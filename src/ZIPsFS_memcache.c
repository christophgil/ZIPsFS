/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Caching file data in ZIP entries in RAM                   ///
/////////////////////////////////////////////////////////////////
#include "cg_crc32.c"
//#define memcache_set_status(m,status) {m->memcache_status=(status);}


static bool  memcache_is_queued(struct memcache *m){
  return (m && m->memcache_l && m->memcache_status==memcache_queued);
    }

static void memcache_set_status(struct memcache *m,const int status){
  ASSERT_LOCKED_FHDATA();
  if (m) m->memcache_status=status;
}

static struct memcache *memcache_new(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  if (!d->memcache){
    static int id;
    (d->memcache=cg_calloc(MALLOC_memcache,1,sizeof(struct memcache)))->id=++id;
    const ht_hash_t hash=D_VP_HASH(d);
    foreach_fhdata(ie,e){
      if (D_VP_HASH(e)==hash && !strcmp(D_VP(d),D_VP(e))) e->memcache=d->memcache;
    }
    d->memcache->txtbuf=textbuffer_new(MALLOC_memcache_textbuffer);
  }
  return d->memcache;
}


///////////////////////////////////////////
/// Reading the cached bytes in the RAM ///
///////////////////////////////////////////

/* Called from memcache_read_fhdata() fhdata_read_zip()  and xmp_read() */
static off_t memcache_read(char *buf,const struct fhdata *d,const off_t from,off_t to){
  cg_thread_assert_not_locked(mutex_fhdata);
  struct memcache *m=!d?NULL:d->memcache;
  if (!m || !m->txtbuf) return 0;
  //  assert(d->flags&FHDATA_FLAGS_HEAP
  LOCK_N(mutex_fhdata,const off_t already=m->memcache_already);
  //log_entered_function(" vp: %s from: %'ld  to: %'ld already: %'ld",D_VP(d),(long)from,(long)to,(long)already);
  to=MIN(to,already);
  if (to==from) return 0; // Returning -1 causes operation not permitted.
  if (to>from){
    textbuffer_copy_to(m->txtbuf,from,to,buf);
    return to-from;
  }
  return already<=from && m->memcache_status==memcache_done?-1:0;
}
/* Invoked from xmp_read, where struct fhdata *d=fhdata_get(path,fd) */
static off_t memcache_read_fhdata(char *buf, const off_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi){
  const struct zippath *zpath=&d->zpath;
  LOCK_N(mutex_fhdata, struct memcache *m=d->memcache; const bool is_memcache=m && m->memcache_status || memcache_is_advised(d) && (m=memcache_new(d)));
  if (is_memcache){
    //    const clock_t clock0=clock();
    //     LOCK(mutex_fhdata,counter_rootdata_t *counter=filetypedata_for_ext(D_VP(d),ROOTd(d));if (counter) counter->wait+=(clock()-clock0));
    const off_t already=memcache_waitfor(d,offset+size);
    if (offset==m->memcache_l) return 0; /* Otherwise md5sum fails with EPERM */
    if (already>offset && m->memcache_l){
      const off_t num=memcache_read(buf,d,offset,MIN(already,size+offset));
      if (num>0) _count_readzip_memcache++;
      if (num>0 || m->memcache_l>=offset) return num;
    }
  }
  return -1;
}





/* Those that are queued will be processed in a different thread. See infloop_memcache() */
bool memcache_fhdata_to_queue(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  const off_t st_size=d->zpath.stat_vp.st_size;
  if (!st_size){ warning(WARN_MEMCACHE,D_VP(d),"st_size is 0");return false;}
  struct memcache *m=memcache_new(d);
  m->memcache_already=0;
  { /* Exceeding max bytes in RAM ? */
    const off_t u=textbuffer_memusage_get(0)+textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP);
    if (u+st_size>_memcache_maxbytes*((d->flags&FHDATA_FLAGS_URGENT)?3:2)){
      log_warn("_memcache_maxbytes reached: currentMem=%'lld+%'lld  _memcache_maxbytes=%'lld \n",(LLD)u,(LLD)st_size,(LLD)_memcache_maxbytes);
      return false;
    }
  }
  char *bb=textbuffer_first_segment_with_min_capacity(st_size>SIZE_CUTOFF_MMAP_vs_MALLOC?TEXTBUFFER_MUNMAP:0,m->txtbuf,st_size);
  if (!bb || bb==MAP_FAILED){
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache failed using %s",D_RP(d),(d->flags&FHDATA_FLAGS_HEAP)?"Malloc":"Mmap");
    return false;
  }
  m->memcache_l=st_size;
  memcache_set_status(m,memcache_queued);
  return true;
}
/* Compare with crc32 stored in ZIP file */

static bool fhdata_check_crc32(struct fhdata *d){
  //PROFILER_B(Test1);
  const off_t st_size=d->zpath.stat_vp.st_size;
  assert(d->memcache);
  {
    LOCK_N(mutex_fhdata,const off_t a=d->memcache->memcache_already);
    if (a!=st_size) warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"Not equal  memcache_already: %ld and st_size: %ld",a,st_size);
  }
  const uint32_t crc=cg_crc32(d->memcache->txtbuf->segment[0],st_size,0,_mutex+mutex_crc);
  if (d->zpath.zipcrc32!=crc){
    warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"crc32-mismatch!  recorded-in-ZIP: %x != computed: %x    size=%zu  rp=%s",d->zpath.zipcrc32,crc,st_size,D_RP(d));
    return false;
  }
  //PROFILER_E(Test1);
  return true;
}
struct zip_file {
  zip_error_t error; /* error information */
  zip_source_t *src; /* data source */
};

#define MEMCACHE_READ_ERROR 1
static int memcache_store_try(struct fhdata *d){
#define A() LOCK(mutex_fhdata,ASSERT(!fhdata_can_destroy(d));  assert(D_VP(d)!=NULL); assert(d->zpath.root!=NULL); assert(d->memcache); assert(d->memcache->memcache_status==memcache_reading))
  bool verbose=false;
  //log_entered_function(ANSI_FG_MAGENTA"%s  \n"ANSI_RESET,D_VP(d));
  assert(cg_strlen(D_VP(d)));
  if (!d->zpath.root){ log_zpath("root is null",&d->zpath); return -1;}
  A();
  int ret=0;
  const off_t st_size=d->zpath.stat_vp.st_size;
  LOCK_N(mutex_fhdata,struct memcache *m=memcache_new(d));
  m->memcache_already_current=0;
  char rp[MAX_PATHLEN+1],path[MAX_PATHLEN+1];
  strcpy(rp,D_RP(d));
  strcpy(path,D_VP(d));
  struct zip *za=zip_open_ro(rp);
  if (!za){
    warning(WARN_MEMCACHE|WARN_FLAG_ERRNO|WARN_FLAG_ERROR,rp,"memcache_zip_entry: Failed zip_open d=%p",d);
  }else{
    zip_file_t *zf=zip_fopen(za,D_EP(d),ZIP_RDONLY);
    fhdata_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL);
    if (!zf){
      warning(WARN_MEMCACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_fopen d=%p",d);
    }else{
      const int64_t start=currentTimeMillis();
      char *buffer=textbuffer_first_segment_with_min_capacity(st_size>SIZE_CUTOFF_MMAP_vs_MALLOC?TEXTBUFFER_MUNMAP:0,m->txtbuf,st_size);
      for(zip_int64_t n;st_size>m->memcache_already_current;){
        if (d->flags&FHDATA_FLAGS_DESTROY_LATER){
          LOCK(mutex_fhdata, if (fhdata_can_destroy(d)) d->flags|=FHDATA_FLAGS_INTERRUPTED);
          fhdata_counter_inc(d,COUNT_MEMCACHE_INTERRUPT);
        }
        if (d->flags&FHDATA_FLAGS_INTERRUPTED) break;
        FOR(retry,0,RETRY_ZIP_FREAD){
          const off_t n_max=MIN_long(MEMCACHE_READ_BYTES_NUM,st_size-m->memcache_already_current);
          assert(n_max>=0); assert(m->memcache_already_current+n_max<=st_size);
          if ((n=zip_fread(zf,buffer+m->memcache_already_current,n_max))>0){
            observe_thread(d->zpath.root,PTHREAD_MEMCACHE);
            if (retry){ warning(WARN_RETRY,path,"succeeded on retry: %d/%d  n=%ld",retry,RETRY_ZIP_FREAD-1,n); fhdata_counter_inc(d,COUNT_RETRY_ZIP_FREAD);}
            break;
          }
          log_verbose("Retry: %d Going sleep 100 ...",retry); usleep(100*1000);
        }/* for retry */
        if (n<=0){ ret=MEMCACHE_READ_ERROR; warning(WARN_MEMCACHE,path,"memcache_zip_entry: n<0  d=%p  read=%'zu st_size=%'ld",d,m->memcache_already_current,st_size);break;}
        const off_t c=m->memcache_already_current+=n;
        if (c>m->memcache_already) m->memcache_already=c;
      }/*for memcache_already_current*/
      if (m->memcache_already_current==st_size){ /* Completely read */
        //assert(ret!=MEMCACHE_READ_ERROR);
        if (fhdata_check_crc32(d)){
          fhdata_counter_inc(d,ZIP_READ_CACHE_CRC32_SUCCESS);
          LOCK(mutex_fhdata, A();m->memcache_took_mseconds=currentTimeMillis()-start;memcache_set_status(m,memcache_done));
          log_succes("memcache_done  d= %p %s    in %'llu mseconds   ret: %d\n",d,path,(LLU)(currentTimeMillis()-start), ret);
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
    zip_fclose(zf);
  }/*za!=NULL*/
  zip_close(za);
  return ret;
#undef A
}



/* Invoked from infloop_memcache */
static void memcache_store(struct fhdata *d){
  if (!d) return;
  assert(cg_strlen(D_VP(d)));
  log_entered_function("%s",D_VP(d));
  cg_thread_assert_not_locked(mutex_fhdata);
  FOR(retry,0,NUM_MEMCACHE_STORE_RETRY){
    const int ret=memcache_store_try(d);
    if (retry && ret<0){
      LOCK(mutex_fhdata,if (d->memcache->memcache_already==d->zpath.stat_vp.st_size){warning(WARN_RETRY,D_VP(d),"memcache_store succeeded on retry %d",retry);fhdata_counter_inc(d,COUNT_RETRY_MEMCACHE);});
    }
    if ((d->flags&FHDATA_FLAGS_INTERRUPTED) || ret!=MEMCACHE_READ_ERROR) break;
    log_verbose("Going to sleep 1000 ms and retry  %s ...",D_VP(d)); usleep(1000*1000);
    find_realpath_again_fhdata(d); /* If root is blocked there is a chance to get the file from a different root */
  }
}
#define WITH_RESCUE_BLOCKED_MEMCACHE 1
static void *infloop_memcache(void *arg){
  struct rootdata *r=arg;
  //log_entered_function("%s", rootpath(r));
  /* pthread_cleanup_push(infloop_memcache_start,r); Does not work because pthread_cancel not working when root blocked. */
  LOCK_NCANCEL(mutex_fhdata,if (r->memcache_d){ r->memcache_d->flags|=FHDATA_FLAGS_INTERRUPTED;
      //       log_msg(RED_WARNING" FHDATA_FLAGS_INTERRUPTED %s",D_VP(r->memcache_d));
      //     log_msg("Simple %s","aaa");
    });
#if WITH_RESCUE_BLOCKED_MEMCACHE
  foreach_fhdata(id,d){
    LOCK_NCANCEL_N(mutex_fhdata,
                   struct memcache *m=d->memcache;
                   const bool rescue=m && m->memcache_l && d->zpath.root==r && m->memcache_status==memcache_reading);
    if (rescue) log_msg("Going to rescue  %s\n",D_RP(d));
    if (rescue && (find_realpath_again_fhdata(d))){
      log_zpath("after find_realpath_again_fhdata ",&d->zpath);
      LOCK_N(mutex_fhdata, memcache_fhdata_to_queue(d));
    }
  }
#endif
  for(int i=0;;i++){
    struct fhdata *d=NULL;
    lock(mutex_fhdata);
    r->memcache_d=NULL;
    foreach_fhdata(id,e){
      if (memcache_is_queued(e->memcache) && e->zpath.root==r){
        assert(cg_strlen(D_VP(e)));
        memcache_set_status(e->memcache,memcache_reading);
        (r->memcache_d=d=e)->is_memcache_store++;
        break; /* e is protected from being destroyed and thus is d within memcache_store() and memcache_store_try(). See fhdata_can_destroy(). */
      }
    }
    if (d) assert(!fhdata_can_destroy(d));
    unlock(mutex_fhdata);
    if (d){
      if (wait_for_root_timeout(r)) memcache_store(d);
      LOCK(mutex_fhdata,d->is_memcache_store--; r->memcache_d=NULL);
    }
    observe_thread(r,PTHREAD_MEMCACHE); /* Allow to rescue blocked */
    usleep(20*1000);
  }
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
  ASSERT(d->zpath.root!=NULL);
  LOCK_N(mutex_fhdata, struct memcache *m=memcache_new(d); if (!m->memcache_status) memcache_fhdata_to_queue(d));
  while(memcache_is_queued(m)||m->memcache_status==memcache_reading){
    LOCK_N(mutex_fhdata,off_t a=m->memcache_already);
    if (a>=min_fill) return a;
    usleep(10*1000);
  }
  LOCK_N(mutex_fhdata,const off_t a=m->memcache_already);
  return a;
}


//////////////////
/// Destructor ///
//////////////////


/* Called from fhdata_destroy() */
static void memcache_free(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  struct memcache *m=d->memcache;
  if (m){
    foreach_fhdata(id,e) if (e!=d && e->memcache==m) return; /* Shared by other? */
    textbuffer_destroy(m->txtbuf);
    cg_free_null(MALLOC_memcache_textbuffer,m->txtbuf);
    cg_free(MALLOC_memcache,m);
    //    foreach_fhdata_also_emty(id2,e) if (e->memcache==m) e->memcache=NULL;
  }
}
