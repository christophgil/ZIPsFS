/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Caching file data in ZIP entries in RAM                   ///
/////////////////////////////////////////////////////////////////
#include "cg_crc32.c"
#define memcache_set_status(m,status) {m->memcache_status=(status);}
#define WITH_ZIP_TELL1 0
static struct memcache *new_memcache(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  if (!d->memcache){
    static int id;
    (d->memcache=calloc(1,sizeof(struct memcache)))->id=++id;
    const ht_hash_t hash=D_VP_HASH(d);
    foreach_fhdata(ie,e){
      if (D_VP_HASH(e)==hash && !strcmp(D_VP(d),D_VP(e))) e->memcache=d->memcache;
    }
  }
  return d->memcache;
}

static off_t memcache_read(char *buf,const struct fhdata *d,const off_t from,off_t to){
  cg_thread_assert_not_locked(mutex_fhdata);
  struct memcache *m=!d?NULL:d->memcache;
  if (!m || !m->memcache2) return 0;
  //  assert(d->flags&FHDATA_FLAGS_HEAP
  LOCK_N(mutex_fhdata,const off_t already=m->memcache_already);
  to=MIN(to,already);
  if (to==from) return 0; // Returning -1 causes operation not permitted.
  if (to>from){
    textbuffer_copy_to(m->memcache2,from,to,buf);
    return to-from;
  }
  return already<=from && m->memcache_status==memcache_done?-1:0;
}
static void memcache_free(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  struct memcache *m=d->memcache;
  if (m){
    textbuffer_destroy(m->memcache2);
    FREE(m->memcache2);
    free(m);
    foreach_fhdata_also_emty(id,e) if (e->memcache==m) e->memcache=NULL;
  }
}
bool memcache_malloc_or_mmap(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  const off_t st_size=d->zpath.stat_vp.st_size;
  if (!st_size){ warning(WARN_MEMCACHE,D_VP(d),"st_size is 0");return false;}
  struct memcache *m=new_memcache(d);
  m->memcache_already=0;
  {
    const off_t u=textbuffer_memusage_get(0)+textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP);
    if (u+st_size>_memcache_maxbytes*((d->flags&FHDATA_FLAGS_URGENT)?3:2)){
      log_warn("_memcache_maxbytes reached: currentMem=%'jd+%'jd  _memcache_maxbytes=%'jd \n",(intmax_t)u,(intmax_t)st_size,(intmax_t)_memcache_maxbytes);
      return false;
    }
  }
  m->memcache2=textbuffer_new();
  if (st_size>SIZE_CUTOFF_MMAP_vs_MALLOC) m->memcache2->flags|=TEXTBUFFER_MMAP;
  char *bb=textbuffer_malloc(m->memcache2,st_size);
  if (bb){
    m->memcache_l=st_size;
  }else{
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache %p failed using %s",(void*)mmap,(d->flags&FHDATA_FLAGS_HEAP)?"malloc":"mmap");
    //log_mem(stderr);
  }
  IF1(WITH_ZIP_TELL1,memset(bb,0,st_size)); // DEBUG_NOW
  return bb!=NULL && bb!=MAP_FAILED;
}
static bool fhdata_check_crc32(struct fhdata *d){
  const off_t st_size=d->zpath.stat_vp.st_size;
  assert(d->memcache);
  {
    LOCK_N(mutex_fhdata,const off_t a=d->memcache->memcache_already);
    if (a!=st_size) warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"Not equal  memcache_already: %ld and st_size: %ld",a,st_size);
  }
  const uint32_t crc=cg_crc32(d->memcache->memcache2->segment[0],st_size,0,_mutex+mutex_crc);
  if (d->zpath.zipcrc32!=crc){
    warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"crc32-mismatch!  recorded-in-ZIP: %x != computed: %x    size=%zu  rp=%s",d->zpath.zipcrc32,crc,st_size,D_RP(d));
    return false;
  }
  return true;
}
struct zip_file {
  zip_error_t error; /* error information */
  zip_source_t *src; /* data source */
};

#define MEMCACHE_READ_ERROR 1
static int memcache_store_try(struct fhdata *d){
#define A() /*ASSERT(d->memcache_status==memcache_reading);*/ LOCK(mutex_fhdata,ASSERT(!fhdata_can_destroy(d)));  assert(D_VP(d)!=NULL); assert(d->zpath.root!=NULL)
  bool verbose=false;
  log_entered_function(ANSI_FG_MAGENTA"%s  \n"ANSI_RESET,D_VP(d));
  A();
  int ret=0;
  const off_t st_size=d->zpath.stat_vp.st_size;
  LOCK_N(mutex_fhdata,  struct memcache *m=new_memcache(d));
  m->memcache_already_current=0;
  assert(m->memcache2!=NULL);
  char rp[MAX_PATHLEN+1],path[MAX_PATHLEN+1];
  strcpy(rp,D_RP(d));
  strcpy(path,D_VP(d));
  struct zip *za=zip_open_ro(rp);
  if (!za){
    warning(WARN_MEMCACHE|WARN_FLAG_ERRNO|WARN_FLAG_ERROR,rp,"memcache_zip_entry: Failed zip_open d=%p",d);
  }else{
    zip_file_t *zf=zip_fopen(za,D_EP(d),ZIP_RDONLY);
    LOCK(mutex_fhdata,m->zipsrc=zf->src);
    fhdata_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL);
    if (!zf){
      warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_fopen d=%p",d);
    }else{
      const int64_t start=currentTimeMillis();
#define RETRY_ZIP_FREAD 3
      for(zip_int64_t n;st_size>m->memcache_already_current;){
        if (d->flags&FHDATA_FLAGS_DESTROY_LATER){
          LOCK(mutex_fhdata, if (fhdata_can_destroy(d)) d->flags|=FHDATA_FLAGS_INTERRUPTED);
          fhdata_counter_inc(d,COUNT_MEMCACHE_INTERRUPT);
        }
        if (d->flags&FHDATA_FLAGS_INTERRUPTED) break;
        FOR(retry,0,RETRY_ZIP_FREAD){
          //          MEMCACHE_READ_BYTES_NUM
#if WITH_ZIP_TELL1
          const long n_max=st_size-m->memcache_already_current;
#else
          const long n_max=MIN_long(64*1024*1024,st_size-m->memcache_already_current);
#endif
          if ((n=zip_fread(zf,m->memcache2->segment[0]+m->memcache_already_current,n_max))>0){
            if (retry){ warning(WARN_RETRY,path,"succeeded on retry: %d/%d  n=%ld",retry,RETRY_ZIP_FREAD-1,n); fhdata_counter_inc(d,COUNT_RETRY_ZIP_FREAD);}
            break;
          }
          log_debug_now("Retry: %d Going sleep 100 ...",retry);
          usleep(100*1000);
        }
        log_debug_now("n=%'ld",n);
        if (n<=0){ ret=MEMCACHE_READ_ERROR; warning(WARN_MEMCACHE,path,"memcache_zip_entry: n<0  d=%p  read=%'zu st_size=%'ld",d,m->memcache_already_current,st_size);break;}
        const off_t c=m->memcache_already_current+=n;
        if (c>m->memcache_already) m->memcache_already=c;
      }/*for*/
      if (m->memcache_already_current==st_size){ /* Completely read */
        //assert(ret!=MEMCACHE_READ_ERROR);
        if (fhdata_check_crc32(d)){
          fhdata_counter_inc(d,ZIP_READ_CACHE_CRC32_SUCCESS);
          LOCK(mutex_fhdata, A();m->memcache_took_mseconds=currentTimeMillis()-start;memcache_set_status(m,memcache_done));
          //log_succes("memcache_done  d= %p %s %p  st_size=%zu  in %'ld mseconds\n",d,path,d->memcache,d->memcache_already_current,d->memcache_took_mseconds);
          if (verbose) log_exited_function("%s  d->memcache_already_current==st_size  crc32 OK\n",D_RP(d));
        }else{
          LOCK(mutex_fhdata,A();memcache_set_status(m,m->memcache_already_current=0);m->memcache_already=0);
          fhdata_counter_inc(d,ZIP_READ_CACHE_CRC32_FAIL);
          if (verbose) log_exited_function("%s  d->memcache_already_current==st_size  crc32 wrong\n",D_RP(d));
        }
      }else{
        if (!(d->flags&FHDATA_FLAGS_INTERRUPTED)) warning(WARN_MEMCACHE|WARN_FLAG_ERROR,path,"d->memcache_already_current!=st_size:  %zu!=%ld",m->memcache_already_current,st_size);
        LOCK(mutex_fhdata,memcache_set_status(m,memcache_done));
        if (verbose) log_exited_function("%s  d->memcache_already_current!=st_size\n",D_RP(d));
      }
    }/*zf!=NULL*/
    LOCK(mutex_fhdata,m->zipsrc=NULL);
    zip_fclose(zf);
  }/*zf!=NULL*/
  zip_close(za);
  log_debug_now("%s memcache_store_try %s  already: %'jd  st_size: %'jd interrup: %s",m->memcache_already>=st_size?GREEN_SUCCESS:RED_FAIL,path,(intmax_t)m->memcache_already,(intmax_t)st_size,yes_no(d->flags&FHDATA_FLAGS_INTERRUPTED));
  return ret;
#undef A
}

/* Invoked from infloop_memcache */
static void memcache_store(struct fhdata *d){
  if (!d) return;
  cg_thread_assert_not_locked(mutex_fhdata);
  FOR(retry,0,NUM_MEMCACHE_STORE_RETRY){
    const int ret=memcache_store_try(d);
    if (retry && !ret){
      LOCK(mutex_fhdata,if (d->memcache->memcache_already==d->zpath.stat_vp.st_size){warning(WARN_RETRY,D_VP(d),"memcache_store succeeded on retry %d",retry);fhdata_counter_inc(d,COUNT_RETRY_MEMCACHE);});
    }
    if ((d->flags&FHDATA_FLAGS_INTERRUPTED) || ret!=MEMCACHE_READ_ERROR) break;
    log_debug_now("Going to sleep 1000 ms and retry  %s ...",D_VP(d));
    usleep(1000*1000);
  }
}

static void *infloop_memcache(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_memcache_start,r);
  LOCK_NCANCEL(mutex_fhdata,if (r->memcache_d){ r->memcache_d->flags|=FHDATA_FLAGS_INTERRUPTED; log_debug_now(RED_WARNING" FHDATA_FLAGS_INTERRUPTED %s",D_VP(r->memcache_d));});
  for(int i=0;;i++){
    if (!wait_for_root_timeout(r)) continue;
    lock(mutex_fhdata);
    r->memcache_d=NULL;
    foreach_fhdata(id,d){
      //     observe_thread(r,PTHREAD_MEMCACHE);
      if (memcache_is_queued(d->memcache)){
        r->memcache_d=d;
        memcache_set_status(d->memcache,memcache_reading);
        break;
      }
      /* d is protected from being destroyed if  memcache_reading */
    };
    unlock(mutex_fhdata);
    if (i%128==0){putc(r->memcache_d!=NULL?'+':'-',stderr);}
    if (r->memcache_d){
      //log_debug_now("Going to memcache_store(%p) r: %d vp: %s  memcache: %p",r->memcache_d,rootindex(r),D_VP(r->memcache_d),r->memcache_d->memcache);
      memcache_store(r->memcache_d);
    }
    LOCK(mutex_fhdata,r->memcache_d=NULL);
    observe_thread(r,PTHREAD_MEMCACHE);
    usleep(10*1000);
  }
  pthread_cleanup_pop(0);
}

static bool memcache_is_advised(const struct fhdata *d){
  if (!d || config_advise_not_cache_zipentry_in_ram(D_VP(d),D_VP_L(d)) || _memcache_policy==MEMCACHE_NEVER) return false;
  return
    _memcache_policy==MEMCACHE_COMPRESSED &&   (d->zpath.flags&ZP_IS_COMPRESSED)!=0 ||
    _memcache_policy==MEMCACHE_ALWAYS ||
    config_advise_cache_zipentry_in_ram(d->zpath.stat_vp.st_size,D_VP(d),D_VP_L(d),(d->zpath.flags&ZP_IS_COMPRESSED)!=0);
}
/* Returns fhdata instance with enough memcache data */

static off_t memcache_waitfor(struct fhdata *d, off_t min_fill){
  cg_thread_assert_not_locked(mutex_fhdata);
  ASSERT(d->is_xmp_read>0);
  LOCK_N(mutex_fhdata, struct memcache *m=new_memcache(d); if (!m->memcache_status && memcache_malloc_or_mmap(d)){memcache_set_status(m,memcache_queued);});
  while(memcache_is_queued(m)||m->memcache_status==memcache_reading){

    LOCK_N(mutex_fhdata,off_t a=m->memcache_already);
#if  WITH_ZIP_TELL1
    lock(mutex_fhdata);
    if (m && m->memcache2->n){
      char *bb=m->memcache2->segment[0];
      off_t bb_l=m->memcache2->segment_e[0];
      for(off_t a1=a; a1<bb_l; a1+=1024*1024) if (bb[a1]) a=a1;
      //if (m->memcache_already<a) log_debug_now("%ld === > %ld ",m->memcache_already,a);
      m->memcache_already=a;
    }
    //           if (m->zipsrc) log_debug_now("zip_source_tell %ld %ld",zip_source_tell(m->zipsrc),m->zipsrc->zip_source);
    unlock(mutex_fhdata);
#endif
    if (a>=min_fill) return a;
    //    log_debug_now("MEMCACHE_WAITFOR sleep %s  already: %zu  need: %zu",D_VP(d),a,min_fill);
    usleep(10*1000);
  }
  LOCK_N(mutex_fhdata,const off_t a=m->memcache_already);
  return a;
}
