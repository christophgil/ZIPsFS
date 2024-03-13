/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Caching file data in ZIP entries in RAM                   ///
/////////////////////////////////////////////////////////////////
#include "cg_crc32.c"


static size_t memcache_read(char *buf,const struct fhdata *d, const off_t from,  off_t to){
  ASSERT_LOCKED_FHDATA();
  if (!d || !d->memcache2) return 0;
  to=MIN(to,d->memcache_already);
  if (to==from) return 0; // Returning -1 causes operation not permitted.
  if (to>from){
    textbuffer_copy_to(d->memcache2,from,to,buf);
    return to-from;
  }
  return d->memcache_already<=from && d->memcache_status==memcache_done?-1:0;
}
static void memcache_free(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  struct textbuffer *c=d->memcache2;
  d->memcache2=NULL;
  const size_t len=d->memcache_l;
  if (c){
    textbuffer_destroy(c);
    FREE(c);
    fhdata_set_memcache_status(d,d->memcache_took_mseconds=d->memcache_l=0);
  }
}
bool memcache_malloc_or_mmap(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  const int64_t st_size=d->zpath.stat_vp.st_size;
  if (!st_size){ warning(WARN_MEMCACHE,D_VP(d),"st_size is 0");return false;}
  d->memcache_already=0;
  {
    const int64_t u=textbuffer_memusage_get(0)+textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP);
    if (u+st_size>_memcache_maxbytes*((d->flags& FHDATA_FLAGS_URGENT)?3:2)){
      log_warn("_memcache_maxbytes reached: currentMem=%'ld+%'ld  _memcache_maxbytes=%'ld \n",u,st_size,_memcache_maxbytes);
      return false;
    }
  }
  d->memcache2=textbuffer_new();
  if (st_size>SIZE_CUTOFF_MMAP_vs_MALLOC) d->memcache2->flags|=TEXTBUFFER_MMAP;
  char *bb=textbuffer_malloc(d->memcache2,st_size);
  if (bb){
    d->memcache_l=st_size;
  }else{
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache %p failed using %s",(void*)mmap,(d->flags&FHDATA_FLAGS_HEAP)?"malloc":"mmap");
    //log_mem(stderr);
  }
  return bb!=NULL && bb!=MAP_FAILED;
}
static bool fhdata_check_crc32(struct fhdata *d){
  const size_t st_size=d->zpath.stat_vp.st_size;
  assert(d->memcache_already==st_size);
  const uint32_t crc=cg_crc32(d->memcache2->segment[0],st_size,0,_mutex+mutex_crc);
  if (d->zpath.zipcrc32!=crc){
    warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"crc32-mismatch!  recorded-in-ZIP: %x != computed: %x    size=%zu  rp=%s",d->zpath.zipcrc32,crc,st_size,D_RP(d));
    return false;
  }
  return true;
}

#define MEMCACHE_READ_ERROR 1
static int memcache_store_try(struct fhdata *d){
#define A() ASSERT(d->memcache_status==memcache_reading);/*This keeps d alive*/ assert(D_VP(d)!=NULL); assert(d->zpath.root!=NULL)
  bool verbose=false;
  if (verbose) log_entered_function("%s\n",D_VP(d));
  assert(d->is_xmp_read>0);A();
  int ret=0;
  const int64_t st_size=d->zpath.stat_vp.st_size;
  d->memcache_already_current=0;
  assert(d->memcache2!=NULL);
  char rp[MAX_PATHLEN+1],path[MAX_PATHLEN+1];
  strcpy(rp,D_RP(d));
  strcpy(path,D_VP(d));
  struct zip *za=zip_open_ro(rp);
  if (!za){
    warning(WARN_MEMCACHE|WARN_FLAG_ERRNO|WARN_FLAG_ERROR,rp,"memcache_zip_entry: Failed zip_open d=%p",d);
  }else{
    zip_file_t *zf=zip_fopen(za,D_EP(d),ZIP_RDONLY);
    LOCK(mutex_fhdata,fhdata_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL));
    if (!zf){
      warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_fopen d=%p",d);
    }else{
      const int64_t start=currentTimeMillis();
#define RETRY_ZIP_FREAD 3
#define N() MIN_long(MEMCACHE_READ_BYTES_NUM,st_size-d->memcache_already_current)
      for(zip_int64_t n;N()>0;){
        if (d->flags&FHDATA_FLAGS_DESTROY_LATER){ LOCK(mutex_fhdata, if (fhdata_can_destroy(d)) d->flags|=FHDATA_FLAGS_INTERRUPTED); fhdata_counter_inc(d,COUNT_MEMCACHE_INTERRUPT);}
        if (d->flags&FHDATA_FLAGS_INTERRUPTED) break;
        FOR(retry,0,RETRY_ZIP_FREAD){
          if ((n=zip_fread(zf,d->memcache2->segment[0]+d->memcache_already_current,N()))>0){
            if (retry){
              warning(WARN_RETRY,path,"zip_fread succeeded retry: %d/%d  n=%ld",retry,RETRY_ZIP_FREAD-1,n);
              LOCK(mutex_fhdata,fhdata_counter_inc(d,COUNT_RETRY_ZIP_FREAD));
            }
            break;
          }
          usleep(100*1000);
        }
        if (n<=0){ ret=MEMCACHE_READ_ERROR; warning(WARN_MEMCACHE,path,"memcache_zip_entry: n<0  d=%p  read=%'zu st_size=%'ld",d,d->memcache_already_current,st_size);break;}
        if ((d->memcache_already_current+=n)>d->memcache_already) d->memcache_already=d->memcache_already_current;
      }/*for*/
      if (d->memcache_already_current==st_size){ /* Completely read */
        //assert(ret!=MEMCACHE_READ_ERROR);
        if (fhdata_check_crc32(d)){
          LOCK(mutex_fhdata, A();d->memcache_took_mseconds=currentTimeMillis()-start;fhdata_counter_inc(d,ZIP_READ_CACHE_CRC32_SUCCESS); fhdata_set_memcache_status(d,memcache_done));
          //log_succes("memcache_done  d= %p %s %p  st_size=%zu  in %'ld mseconds\n",d,path,d->memcache,d->memcache_already_current,d->memcache_took_mseconds);
          if (verbose) log_exited_function("%s  d->memcache_already_current==st_size  crc32 OK\n",D_RP(d));
        }else{
          LOCK(mutex_fhdata,A();fhdata_counter_inc(d,ZIP_READ_CACHE_CRC32_FAIL);fhdata_set_memcache_status(d,d->memcache_already_current=d->memcache_already=0));
          if (verbose) log_exited_function("%s  d->memcache_already_current==st_size  crc32 wrong\n",D_RP(d));
        }
      }else{
        if (!(d->flags&FHDATA_FLAGS_INTERRUPTED)) warning(WARN_MEMCACHE|WARN_FLAG_ERROR,path,"d->memcache_already_current!=st_size:  %zu!=%ld",d->memcache_already_current,st_size);
        LOCK(mutex_fhdata,fhdata_set_memcache_status(d,memcache_done));
        if (verbose) log_exited_function("%s  d->memcache_already_current!=st_size\n",D_RP(d));
      }
    }/*zf!=NULL*/
    zip_fclose(zf);
  }/*zf!=NULL*/
  zip_close(za);
  return ret;
#undef N
#undef A
}

/* Invoked from infloop_memcache */
static void memcache_store(struct fhdata *d){
  if (!d) return;
  cg_thread_assert_not_locked(mutex_fhdata);
  assert(d->memcache_status==memcache_reading);
  LOCK(mutex_fhdata,assert(!fhdata_can_destroy(d)));
  FOR(retry,0,NUM_MEMCACHE_STORE_RETRY){
    const int ret=memcache_store_try(d);
    if (retry && !ret && d->memcache_already==d->zpath.stat_vp.st_size){
      warning(WARN_RETRY,D_VP(d),"memcache_store succeeded on retry %d",retry);
      LOCK(mutex_fhdata,fhdata_counter_inc(d,COUNT_RETRY_MEMCACHE));
    }
    if ((d->flags&FHDATA_FLAGS_INTERRUPTED) || ret!=MEMCACHE_READ_ERROR) break;
    usleep(1000*1000);
  }
  maybe_evict_from_filecache(0,D_RP(d),D_RP_L(d),D_VP(d),D_VP_L(d));
}

static void *infloop_memcache(void *arg){
#define D(x) LOCK(mutex_fhdata,r->memcache_d=x)
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_memcache_start,r);
  LOCK_NCANCEL(mutex_fhdata,if (r->memcache_d) r->memcache_d->flags|=FHDATA_FLAGS_INTERRUPTED);
  while(true){
    if (!wait_for_root_timeout(r)) continue;
    D(NULL);
    LOCK(mutex_fhdata,
         foreach_fhdata_also_emty(id,d){
               observe_thread(r,PTHREAD_MEMCACHE);
           if (memcache_is_queued(d)){
             D(d);
             fhdata_set_memcache_status(d,memcache_reading);break;
           }
           /* d is protected from being destroyed if  memcache_reading */
         });
    if (r->memcache_d) memcache_store(r->memcache_d);
    D(NULL);
    observe_thread(r,PTHREAD_MEMCACHE);
    usleep(10000);
  }
  pthread_cleanup_pop(0);
#undef D
}

static bool memcache_is_advised(const struct fhdata *d){
  if (!d || config_advise_not_cache_zipentry_in_ram(D_VP(d),D_VP_L(d)) || _memcache_policy==MEMCACHE_NEVER) return false;
  return
    _memcache_policy==MEMCACHE_COMPRESSED &&   (d->zpath.flags&ZP_IS_COMPRESSED)!=0 ||
    _memcache_policy==MEMCACHE_ALWAYS ||
    config_advise_cache_zipentry_in_ram(d->zpath.stat_vp.st_size,D_VP(d),D_VP_L(d),(d->zpath.flags&ZP_IS_COMPRESSED)!=0);
}
/* Returns fhdata instance with enough memcache data */
static struct fhdata FHDATA_WAIT;
static struct fhdata *select_fhdata_with_memcache(const char *path, const ht_hash_t hash,const size_t min_fill){
  cg_thread_assert_locked(mutex_fhdata);
  struct fhdata *wait=NULL;
  foreach_fhdata_also_emty(id,d){
    if (d->memcache_l && D_VP_HASH(d)==hash && !strcmp(path,D_VP(d))){
      if (d->memcache_already>=min_fill || d->memcache_status==memcache_done) return d;
      wait=&FHDATA_WAIT;
    }
  }
  return wait;
}
#define MEMCACHE_WAITFOR(d) while(memcache_is_queued(d)||d->memcache_status==memcache_reading){ if(d->memcache_already>=min_fill)return d;usleep(10*1000);} if (d->memcache_already>=min_fill||d->memcache_status==memcache_done) return d;

static struct fhdata *memcache_waitfor(struct fhdata *d,size_t min_fill){
  cg_thread_assert_not_locked(mutex_fhdata);
  ASSERT(d->is_xmp_read>0);
  MEMCACHE_WAITFOR(d);
  struct fhdata *d2=NULL;
  bool queued=false;
  while(true){
    LOCK(mutex_fhdata, d2=select_fhdata_with_memcache(D_VP(d),D_VP_HASH(d),min_fill); if ((queued=!d2 && memcache_malloc_or_mmap(d))) fhdata_set_memcache_status(d,memcache_queued));
    if (d2!=&FHDATA_WAIT) break;
    usleep(100*1000);
  }
  if (queued){
    MEMCACHE_WAITFOR(d);
    return d;
  }
  if (d2){
    LOCK(mutex_fhdata,assert(!fhdata_can_destroy(d2)));
  }
  return d2;
}
