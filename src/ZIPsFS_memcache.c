/* *********************************************
   memcache
   Caching file data in ZIP entries in RAM
   ********************************************* */
#if IS_MEMCACHE
static size_t memcache_read(char *buf,const struct fhdata *d, const size_t size, const off_t offset){
  ASSERT_LOCKED_FHDATA();
  if (!d || !d->memcache) return 0;
  const long n=min_long(size,d->memcache_already-offset);
  if (!n) return 0; /* Returning -1 causes operation not permitted. */
  if (n>0){
    memcpy(buf,d->memcache+offset,min_long(size,d->memcache_already-offset));
    return n;
  }
  return d->memcache_already<=offset && d->memcache_status==memcache_done?-1:0;
}
static void memcache_free(struct fhdata *d){
  char *c=d->memcache;
  d->memcache=NULL;
  const size_t len=d->memcache_l;
  if (c && len){
    //log_cache("Going to release cache %p %zu\n",c,len);
    if((d->flags&FHDATA_FLAGS_HEAP)){
      FREE(c);
      trackMemUsage(memusage_malloc,-len);
    }else{
      if (munmap(c,len)){
        perror("munmap");
      }else{
        //log_succes("munmap\n");
        trackMemUsage(memusage_mmap,-len);
      }
    }
    d->memcache_took_mseconds=d->memcache_l=d->memcache_status=0;
  }
}
bool memcache_malloc_or_mmap(struct fhdata *d){
  const int64_t st_size=d->zpath.stat_vp.st_size;
  d->memcache_already=0;
  //  log_cache("Going to memcache d= %p %s %'ld Bytes\n",d,d->path,st_size);
  {
    const int64_t u=trackMemUsage(memusage_get_curr,0);
    if (u+st_size>_memcache_maxbytes*((d->flags& FHDATA_FLAGS_URGENT)?2:3)){
      log_warn("_memcache_maxbytes reached: currentMem=%'ld+%'ld  _memcache_maxbytes=%'ld \n",u,st_size,_memcache_maxbytes);
      return false;
    }
  }
  if (!st_size) return false;
  if (st_size<SIZE_CUTOFF_MMAP_vs_MALLOC) d->flags|=FHDATA_FLAGS_HEAP;
  char *bb=(d->flags&FHDATA_FLAGS_HEAP)?malloc(st_size):mmap(NULL,st_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
  if (bb && bb!=MAP_FAILED){
    d->memcache=bb;trackMemUsage((d->flags&FHDATA_FLAGS_HEAP)?memusage_malloc:memusage_mmap,st_size);
    d->memcache_l=st_size;
  }else{
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache %p failed using %s",(void*)mmap,(d->flags&FHDATA_FLAGS_HEAP)?"malloc":"mmap");
    log_mem(LOG_STREAM);
  }
  return bb!=NULL && bb!=MAP_FAILED;
}
static bool fhdata_check_crc32(struct fhdata *d){
  const size_t st_size=d->zpath.stat_vp.st_size;
  assert(d->memcache_already==st_size);
  const uint32_t crc=cg_crc32(d->memcache,st_size,0,_mutex+mutex_crc);
  if (d->zpath.rp_crc!=crc){
    warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,d->path,"d->zpath.rp_crc (%x) != crc (%x)",d->zpath.rp_crc,crc);
    return false;
  }
  return true;
}
#define MEMCACHE_READ_ERROR 1
static int memcache_store_try(struct fhdata *d){
  ASSERT(d->memcache_status==memcache_reading);
  int ret=0;
  const int64_t st_size=d->zpath.stat_vp.st_size;
  d->memcache_already_current=0;
  assert(d->memcache!=NULL);
  char rp[MAX_PATHLEN+1],path[MAX_PATHLEN+1];
  strcpy(rp,d->zpath.realpath);
  strcpy(path,d->path);
  struct zip *za=zip_open_ro(rp);
  if (!za){
    warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_open d=%p",d);
  }else{
    zip_file_t *zf=zip_fopen(za,d->zpath.entry_path,ZIP_RDONLY);
    LOCK(mutex_fhdata,fhdata_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL));
    if (!zf){
      warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_fopen d=%p",d);
    }else{
      const int64_t start=currentTimeMillis();
#define RETRY_ZIP_FREAD 3
#define N() min_long(MEMCACHE_READ_BYTES_NUM,st_size-d->memcache_already_current)
      for(zip_int64_t n;N()>0;){
        if (d->flags&FHDATA_FLAGS_DESTROY_LATER){ LOCK(mutex_fhdata, if (fhdata_can_destroy(d)) d->flags|=FHDATA_FLAGS_INTERRUPTED);}
        if (d->flags&FHDATA_FLAGS_INTERRUPTED) break;
        FOR(retry,0,RETRY_ZIP_FREAD){
          if ((n=zip_fread(zf,d->memcache+d->memcache_already_current,N()))>0){
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
        assert (ret!=MEMCACHE_READ_ERROR);
        if (fhdata_check_crc32(d)){ /* Errorneous */
          d->memcache_took_mseconds=currentTimeMillis()-start;
          d->memcache_status=memcache_done; /* From know on d is not memcache_reading and might be destroyed. */
          //log_succes("memcache_done  d= %p %s %p  st_size=%zu  in %'ld mseconds\n",d,path,d->memcache,d->memcache_already_current,d->memcache_took_mseconds);
        }else{
          d->memcache_status=d->memcache_already_current=d->memcache_already=0;
        }
        LOCK(mutex_fhdata,fhdata_counter_inc(d,d->memcache_status==memcache_done?ZIP_READ_CACHE_CRC32_SUCCESS:ZIP_READ_CACHE_CRC32_FAIL));
      }else{
        d->memcache_status=memcache_done;
         if (!(d->flags&FHDATA_FLAGS_INTERRUPTED)) warning(WARN_MEMCACHE,path,"d->memcache_already_current!=st_size:  %zu!=%ld",d->memcache_already_current,st_size);
      }

    }/*zf!=NULL*/
    zip_fclose(zf);
  }/*zf!=NULL*/
  zip_close(za);
  return ret;
#undef N
}

/* Invoked from infloop_memcache */
static void memcache_store(struct fhdata *d){
  if (!d) return;
  assert_not_locked(mutex_fhdata);
  ASSERT(d->memcache_status==memcache_reading);
  assert(!fhdata_can_destroy(d));
#define RETRY_MEMCACHE_STORE 2
  FOR(retry,0,RETRY_MEMCACHE_STORE){
    const int ret=memcache_store_try(d);
    if (retry && !ret && d->memcache_already==d->zpath.stat_vp.st_size){
      warning(WARN_RETRY,d->path,"memcache_store succeeded on retry %d",retry);
      LOCK(mutex_fhdata,fhdata_counter_inc(d,COUNT_RETRY_MEMCACHE));
    }
    if ((d->flags&FHDATA_FLAGS_INTERRUPTED) || ret!=MEMCACHE_READ_ERROR) break;
    usleep(1000*1000);
  }
  maybe_evict_from_filecache(0,d->zpath.realpath,d->path);
}

static void *infloop_memcache(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_memcache_start,r);
  LOCK_NCANCEL(mutex_fhdata,if (r->memcache_d) r->memcache_d->flags|=FHDATA_FLAGS_INTERRUPTED);
  while(true){
    LOCK(mutex_fhdata,r->memcache_d=NULL);
    if (!wait_for_root_timeout(r)) continue;
    LOCK(mutex_fhdata,
         foreach_fhdata_path_not_null(d){
           if (d->memcache_status==memcache_queued){
             LOCK(mutex_fhdata,r->memcache_d=d);
             d->memcache_status=memcache_reading;break;
           }
           /* d is protected from being destroyed if  memcache_reading */
         });
    if (r->memcache_d) memcache_store(r->memcache_d);
    LOCK(mutex_fhdata,r->memcache_d=NULL);
    r->pthread_when_loop_deciSec[PTHREAD_MEMCACHE]=deciSecondsSinceStart();
    PRETEND_BLOCKED(PTHREAD_MEMCACHE);
    usleep(10000);
  }
  pthread_cleanup_pop(0);
}

static bool memcache_is_advised(const struct fhdata *d){
  if (!d || config_not_memcache_zip_entry(d->path) || _memcache_policy==MEMCACHE_NEVER) return false;
  return
    _memcache_policy==MEMCACHE_COMPRESSED &&   (d->zpath.flags&ZP_IS_COMPRESSED)!=0 ||
    _memcache_policy==MEMCACHE_ALWAYS ||
    config_store_zipentry_in_memcache(d->zpath.stat_vp.st_size,d->zpath.virtualpath,(d->zpath.flags&ZP_IS_COMPRESSED)!=0);
}
/* Returns fhdata instance with enough memcache data */

#define ENOUGH(d) (d->memcache_already>=size+offset)
static struct fhdata fhdata_wait;
static struct fhdata *select_fhdata_with_memcache(struct fhdata *d0,const size_t size,const size_t offset){
  assert_locked(mutex_fhdata);
  struct fhdata *wait=NULL;
  foreach_fhdata_path_not_null(d){
    if (d==d0 || !d->memcache_l) continue;
    const int m=d->memcache_status;
    if (m && d->path_hash==d0->path_hash && !strcmp(d0->path,d->path)){
      //log_msg("W %s\n",MEMCACHE_STATUS_S[m]);
      if (ENOUGH(d)) return d;
      if (m==memcache_reading || m==memcache_queued) wait=&fhdata_wait;
    }
  }
  return wait;
}
#define MEMCACHE_WAITFOR(d)  while(d->memcache_status==memcache_queued || d->memcache_status==memcache_reading){ \
    if (ENOUGH(d)) return d;usleep(10*1000); }                          \
  if (ENOUGH(d) || d->memcache_status==memcache_done) return d;

static struct fhdata *memcache_waitfor(struct fhdata *d,size_t size,size_t offset){
  assert_not_locked(mutex_fhdata);
  //log_entered_function(" %s\n",d->path);
  ASSERT(d->is_xmp_read>0);
  MEMCACHE_WAITFOR(d);
  struct fhdata *ret=NULL;
  while(true){
    struct fhdata *d2=NULL;
    LOCK(mutex_fhdata, d2=select_fhdata_with_memcache(d,size,offset));
    //log_msg("W %s\n",!d2?"NuLL":d2==&fhdata_wait?"Wait": MEMCACHE_STATUS_S[d2->memcache_status]);
    if (d2!=&fhdata_wait){
      ret=d2;
      if (ret) assert(!fhdata_can_destroy(ret));
      break;
    }
    usleep(100*1000);
  }
    if (ret) assert(ENOUGH(ret));
  if (!ret && memcache_malloc_or_mmap(d)){ /* Have not found other fhdata having memcache with this path */
    LOCK(mutex_fhdata, if (!d->memcache_status) d->memcache_status=memcache_queued); /* To queue */
    MEMCACHE_WAITFOR(d);
    return d;
  }
  return ret;
}
#undef ENOUGH

#endif //IS_MEMCACHE
