/////////////////////////////////////////////////////////
// Zip inline cache                                    //
// Faster /bin/ls with many inlined ZIPs               //
// Can be deaktivated by setting  IS_ZIPINLINE_CACHE to 0 //
/////////////////////////////////////////////////////////
//   time ls -l  $TZ/Z1/Data/30-0072 |nl
//   time ls -l  $TZ/Z1/Data/30-0051 |nl

#if IS_ZIPINLINE_CACHE
static const char *zipinline_cache_virtualpath_to_zippath(const char *vp,const int vp_l){
  assert_locked(mutex_dircache);
  const char *zip=ht_get_int(&zipinline_cache_virtualpath_to_zippath_ht,hash32(vp,vp_l),vp_l);
  if (!zip) return NULL;
  /* validation because we rely on hash only */
  const char *vp_name=vp+last_slash(vp)+1;
  const int len=config_zipentry_to_zipfile(0,vp_name,NULL);
  return strncmp(vp_name,zip+last_slash(zip)+1,len)?NULL:zip;
}
#endif //IS_ZIPINLINE_CACHE
// static void zipinline_cache_set_zip(

//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
#if IS_DIRCACHE
static void dircache_directory_to_cache(const struct directory *dir){
  assert_locked(mutex_dircache);
  struct rootdata *r=dir->root;
  dircache_clear_if_reached_limit(r,DIRECTORY_CACHE_SEGMENTS);
  assert_validchars_direntries(VALIDCHARS_PATH,dir);
  struct directory_core src=dir->core, *d=MSTORE_ADD(&r->dircache,&src,S_DIRECTORY_CORE,SIZE_T);
  RLOOP(i,src.files_l){
    const char *s=src.fname[i];
    ASSERT(s!=NULL);
    const int len=strlen(s);
    (d->fname[i]=(char*)MSTORE_ADD_REUSE(&r->dircache,s,(len+1),hash32(s,len),&r->dircache_byhash_name))[len]=0;
  }
#define C(F,type) if (src.F) d->F=MSTORE_ADD(&r->dircache,src.F,src.files_l*sizeof(type),sizeof(type))
  C_FILE_DATA_WITHOUT_NAME();
#undef C
  /* Save storage when directories look similar.   ZIP files with identical table-of-content after applying simplify_name will have same hash. */
  { /* Using the array of pointers to file names as a key */
    const ht_keylen_t keylen=src.files_l*SIZE_POINTER;
    const ht_hash_t hash=hash32((char*)src.fname,keylen);
    d->fname=(char**)MSTORE_ADD_REUSE(&r->dircache,(char*)d->fname,keylen,hash, &r->dircache_byhash_dir); /* Save the array of pointers */
  }
  const int rp_l=strlen(dir->dir_realpath);
  const char *rp=ht_internalize_string(&ht_intern_temporarily,dir->dir_realpath,rp_l,0); /* The real path of the zip file */
  ht_set(&r->dircache_ht,rp,0,0,d);
#if IS_ZIPINLINE_CACHE
  if (dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
    static char buffer_u[PATH_MAX+1], vp_entry[PATH_MAX+1];
    const char *vp=rp+r->root_path_l;
    const int slash=last_slash(vp),vp_l=rp_l-r->root_path_l;
    strncpy(vp_entry,vp,slash+1);
    RLOOP(i,src.files_l){
      strcpy(vp_entry+slash+1,unsimplify_name(buffer_u,d->fname[i],rp));
      ht_set_int(&zipinline_cache_virtualpath_to_zippath_ht,hash_value_strg(vp_entry),strlen(vp_entry),rp);
      /* We are not storing vp_entry. We  rely on its hash value and accept collisions. */
    }
  }
#endif //IS_ZIPINLINE_CACHE
}
static void dircache_clear_if_reached_limit(struct rootdata *r,size_t limit){
  if (!r) return;
  assert_locked(mutex_dircache);
  const size_t ss=MSTORE_COUNT_SEGMENTS(&r->dircache);
  if (ss>=limit){
    warning(WARN_DIRCACHE,r->root_path,"Clearing directory cache. Cached segments: %zu (%zu) bytes: %zu. Consider to increase DIRECTORY_CACHE_SEGMENTS",ss,limit,MSTORE_USAGE(&r->dircache));
    MSTORE_CLEAR(&r->dircache);
    ht_clear(&r->dircache_ht);

    IF1(IS_DIRCACHE,cache_by_hash_clear(&r->dircache_byhash_name);
        cache_by_hash_clear(&r->dircache_byhash_dir));

    if (r==_root){
      IF1(IS_ZIPINLINE_CACHE,ht_clear(&zipinline_cache_virtualpath_to_zippath_ht));
      IF1(IS_STAT_CACHE,ht_clear(&stat_ht));
      ht_clear(&ht_intern_temporarily);
    }
  }
}
static void dircache_clear_if_reached_limit_all(){
  LOCK(mutex_dircache,foreach_root(i,r)  dircache_clear_if_reached_limit(r,0));
}
#endif //IS_DIRCACHE
#define timespec_equals(a,b) (a.tv_sec==b.tv_sec && a.tv_nsec==b.tv_nsec)
#if IS_DIRCACHE
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim){
  assert_locked(mutex_dircache);
  struct directory_core *s=ht_get(&dir->root->dircache_ht,dir->dir_realpath,0,0);
  if (s){
    if (!timespec_equals(s->mtim,mtim)){/* Cached data not valid any more. */
      ht_set(&dir->root->dircache_ht,dir->dir_realpath,0,0,NULL);
      log_debug_now("diff s-mtime %ld,%ld  %ld,%ld\n",s->mtim.tv_sec,s->mtim.tv_nsec, mtim.tv_sec,mtim.tv_nsec);
    }else{
      dir->core=*s;
      dir->dir_flags=(dir->dir_flags&~DIRECTORY_IS_HEAP)|DIRECTORY_IS_DIRCACHE;
      assert_validchars_direntries(VALIDCHARS_PATH,dir);
      if (DEBUG_NOW!=DEBUG_NOW) log_debug_now("Success %s  %d  timespec_equals: %d nsec: %ld %ld\n",dir->dir_realpath,dir->core.files_l,   timespec_equals(s->mtim, mtim), s->mtim.tv_nsec,mtim.tv_nsec);
      return true;
    }
  }
  return false;
}
#endif //IS_DIRCACHE


/////////////////////////////////////////////////////////////////////////////////////
/// Temporary cache stat of subdirs in fhdata.                                    ///
/// Motivation: We are using software which is sending lots of requests to the FS ///
/////////////////////////////////////////////////////////////////////////////////////
static bool stat_for_virtual_path(const char *path, struct stat *stbuf, struct zippath **return_zpath ){
  memset(stbuf,0,sizeof(struct stat));
  log_count_b(xmp_getattr_);
  const int slashes=count_slash(path);
  ASSERT(slashes>0); /* path starts with a slash */
  bool success=false;
#if IS_STATCACHE_IN_FHDATA
#define C   (d->statcache_for_subdirs_of_path)
#define L   (d->statcache_for_subdirs_of_path_l)
#define D() struct fhdata* d=fhdata_by_subpath(path); if (d && d->path && (d->zpath.flags&ZP_ZIP))
  // bool debug=endsWithDotD(path);
  LOCK(mutex_fhdata,

       D(){
         //          if (debug) log_debug_now("path=%s D() %p \n",path,fhdata_by_subpath(path));
         if (C){
           ASSERT(slashes+1<L);
           if (C[slashes+1].st_ino){
             *stbuf=C[slashes+1];
             success=true;
             if (return_zpath) *return_zpath=&d->zpath;
             _count_stat_cache_from_fhdata++;
           }
         }else{
           const int slashes_vp=count_slash(d->zpath.virtualpath);
           ASSERT(slashes<=slashes_vp);
           C=calloc(L=slashes_vp+2,sizeof(struct stat));
         }
       });
#endif //IS_STATCACHE_IN_FHDATA
  if (return_zpath) return success;
  if (!success){ /* Not found in fhdata cache */
    NEW_ZIPPATH(path);
    zpath->flags|=ZP_NEVER_STATCACHE_IN_FHDATA;
    if((success=find_realpath_any_root(zpath,NULL))){
      *stbuf=zpath->stat_vp;

#if IS_STATCACHE_IN_FHDATA
      LOCK(mutex_fhdata, D() if (C){ ASSERT(slashes+1<L); C[slashes+1]=*stbuf;});
#undef D
#undef L
#undef C
#endif //IS_STATCACHE_IN_FHDATA
    }
    if (!success && !config_not_report_stat_error(path)){ warning(WARN_GETATTR,path,"!success");log_zpath("!success",zpath);}
    zpath_destroy(zpath);
  }
  if (success){
    if (!stbuf->st_size && tdf_or_tdf_bin(path)) warning(WARN_GETATTR|WARN_FLAG_MAYBE_EXIT,path,"xmp_getattr stbuf->st_size is zero ");
    if (!stbuf->st_ino){ log_error("%s stbuf->st_ino is 0 \n",path); success=false;}
  }
  log_count_e(xmp_getattr_,path);
  return success;
}

/* *************************************************************/
/* With the  software we are using we observe many xmp_getattr calls when tdf_bin files are read */
/* This is a cache. We are Looking into the currently open fhdata.  */
#if IS_STATCACHE_IN_FHDATA
static struct fhdata *fhdata_by_subpath(const char *path){
  ASSERT_LOCKED_FHDATA();
  if (*path=='/' && !path[1]) return NULL;
  const int path_l=my_strlen(path);
  struct fhdata *d2=NULL;
  //const bool debug=endsWithDotD(path);
  const bool debug=false;
  RLOOP(need_statcache,2){ /* Preferably to one with a cache */
    foreach_fhdata_path_not_null(d){
      if (!need_statcache || d->memcache){
        const int vp_l=d->zpath.virtualpath_l;
        const char *vp=d->zpath.virtualpath;
        //if (debug) log_debug_now("vp=%s ep_l=%d  vp_l-d->zpath.entry_path_l=%d  path_l=%d\n",vp,d->zpath.entry_path_l,vp_l-d->zpath.entry_path_l,path_l);
        if (vp_l-d->zpath.entry_path_l<=path_l+1 && path_l<=vp_l && vp &&
            (path_l==vp_l||vp[path_l]=='/') && !strncmp(path,vp,path_l)){
          if ((d2=d)->statcache_for_subdirs_of_path){
            if (debug) log_debug_now("fhdata_by_subpath found  %s \n",path);
            return d; /* Preferably one with statcache_for_subdirs_of_path */
          }
        }
      }
    }
  }
  return d2;
}

#endif //IS_STATCACHE_IN_FHDATA
///////////////////////////
///// file i/o cache  /////
///////////////////////////
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const char *zipentryOrNull){
  if (realpath && configuration_evict_from_filecache(realpath,zipentryOrNull)){
    const int fd=fdOrZero?fdOrZero:open(realpath,O_RDONLY);
    if (fd>0){
      posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
      if (!fdOrZero) close(fd);
    }
  }
}



static int64_t trackMemUsage(const enum memusage t,int64_t a){
  static int64_t memUsage=0,memUsagePeak=0;
  if (a && t<memusage_n){
    LOCK(mutex_memUsage,
         if ((memUsage+=a)>memUsagePeak) memUsagePeak=memUsage;
         _memusage_count[2*t+(a<0)]++);
  }
  return t==memusage_get_peak?memUsagePeak:memUsage;
}

/* *********************************************
   memcache
   Caching file data in ZIP entries in RAM
   ********************************************* */
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
  //     LOCK(mutex_fhdata,const int num=(d->memcache_status==memcache_done && d->memcache_already<=offset)?-1:memcache_read(buf,d2,size,offset));
}
bool memcache_malloc_or_mmap(struct fhdata *d){
  const int64_t st_size=d->zpath.stat_vp.st_size;
  d->memcache_already=0;
  //  log_cache("Going to memcache d= %p %s %'ld Bytes\n",d,d->path,st_size);
  {
    const int64_t u=trackMemUsage(memusage_get_curr,0);
    if (u+st_size>_memcache_maxbytes*(d->memcache_is_urgent?2:3)){
      log_warn("_memcache_maxbytes reached: currentMem=%'ld+%'ld  _memcache_maxbytes=%'ld \n",u,st_size,_memcache_maxbytes);
      return false;
    }
  }
  if (!st_size) return false;
  d->memcache_is_heap=(st_size<SIZE_CUTOFF_MMAP_vs_MALLOC);
  char *bb=d->memcache_is_heap?malloc(st_size):mmap(NULL,st_size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
  if (bb && bb!=MAP_FAILED){
    d->memcache=bb;trackMemUsage(d->memcache_is_heap?memusage_malloc:memusage_mmap,st_size);
    d->memcache_l=st_size;
  }else{
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache %p failed using %s",(void*)mmap,d->memcache_is_heap?"malloc":"mmap");
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
#if IS_MEMCACHE
static int memcache_store_try(struct fhdata *d){
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
        if (d->close_later){
          LOCK(mutex_fhdata, if (fhdata_can_destroy(d)) d->memcache_status=memcache_interrupted);
          if (d->memcache_status==memcache_interrupted) break;
        }
        FOR(0,retry,RETRY_ZIP_FREAD){
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
      if (d->memcache_status!=memcache_interrupted){
        if (d->memcache_already_current!=st_size){ /* Partial */
          warning(WARN_MEMCACHE,path,"d->memcache_already_current!=st_size:  %zu!=%ld",d->memcache_already_current,st_size);
        }else{ /* Full */
          assert (ret!=MEMCACHE_READ_ERROR);
          if (fhdata_check_crc32(d)){ /* Errorneous */
            d->memcache_took_mseconds=currentTimeMillis()-start;
            d->memcache_status=memcache_done;
            //log_succes("memcache_done  d= %p %s %p  st_size=%zu  in %'ld mseconds\n",d,path,d->memcache,d->memcache_already_current,d->memcache_took_mseconds);
          }else{
            d->memcache_status=d->memcache_already_current=d->memcache_already=0;
          }
          LOCK(mutex_fhdata,fhdata_counter_inc(d,d->memcache_status==memcache_done?ZIP_READ_CACHE_CRC32_SUCCESS:ZIP_READ_CACHE_CRC32_FAIL));
        }
      }/* !=memcache_interrupted*/
    }/*zf!=NULL*/
    zip_fclose(zf);
  }/*zf!=NULL*/
  zip_close(za);
  return ret;
#undef N
}

static void memcache_store(struct fhdata *d){
  assert_not_locked(mutex_fhdata); ASSERT(d->is_xmp_read>0);
#define RETRY_MEMCACHE_STORE 2
  FOR(0,retry,RETRY_MEMCACHE_STORE){
    const int ret=memcache_store_try(d);
    if (retry && !ret && d->memcache_already==d->zpath.stat_vp.st_size){
      warning(WARN_RETRY,d->path,"memcache_store succeeded on retry %d",retry);
      LOCK(mutex_fhdata,fhdata_counter_inc(d,COUNT_RETRY_MEMCACHE));
    }
    if (d->memcache_status==memcache_interrupted || ret!=MEMCACHE_READ_ERROR) break;
    usleep(1000*1000);
  }
  maybe_evict_from_filecache(0,d->zpath.realpath,d->path);
}

static void *infloop_memcache(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_memcache_start,r);
  if (r->memcache_d){
    LOCK_NCANCEL(mutex_fhdata,r->memcache_d->memcache_status=memcache_interrupted);
  }
  while(true){
    r->memcache_d=NULL;
    if (!wait_for_root_timeout(r)) continue;
    LOCK(mutex_fhdata,
         foreach_fhdata_path_not_null(d){
           if (d->memcache_status==memcache_queued){ (r->memcache_d=d)->memcache_status=memcache_reading;break;}
         });
    if (r->memcache_d) memcache_store(r->memcache_d);
    r->memcache_d=NULL;
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

static struct fhdata *memcache_waitfor(struct fhdata *d,size_t size,size_t offset){
  assert_not_locked(mutex_fhdata);
  //log_entered_function(" %s\n",d->path);
  struct fhdata *ret=NULL;
  if (d && d->path){
    ASSERT(d->is_xmp_read>0);
    if (d->memcache_status==memcache_done){
      ret=d;
    }else{
      struct fhdata *d2=NULL;
      LOCK(mutex_fhdata,
           if (d->memcache_status){
             d2=d;
           }else if (!(d2=fhdata_by_virtualpath(d->path,d->path_hash,NULL,having_memcache))){
             if (memcache_malloc_or_mmap(d)) (d2=d)->memcache_status=memcache_queued;
           });
      if (d2->memcache_status){
        for(int i=0;true;i++){
          if (d2->memcache_status==memcache_done){
            ret=d2;
          }else if (d2->memcache_already>=offset+size){
            LOCK(mutex_fhdata, if (d2->memcache_already>=offset+size) ret=d2);
          }
          if (ret) break;
          if (d2->memcache_status==memcache_interrupted || !d2->memcache_status) return NULL;   /* If wrong crc32 then memcache_status set to 0 */
          usleep(10*1000);
        }
      }
      if (ret && ret->memcache_already<offset+size && ret->memcache_already!=ret->memcache_l){
        warning(WARN_MEMCACHE,d->path,"memcache_done=%s  already<offset+size  memcache_status=%s  already=%'zu / %'zu offset=%'zu size=%'zu",
                yes_no(d2->memcache_status==memcache_done),MEMCACHE_STATUS_S[ret->memcache_status],ret->memcache_already,ret->memcache_l, offset,size);
      }
    }
  }
  return ret;
}
#endif //IS_MEMCACHE
