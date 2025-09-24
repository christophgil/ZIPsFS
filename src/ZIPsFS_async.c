//////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                ///
/// running stat() asynchronously in separate thread to avoid blocking ///
/// COMPILE_MAIN=ZIPsFS                                                ///
//////////////////////////////////////////////////////////////////////////
#define TIMEOUT_OR_NOT(code_yes,code_no)  lock(mutex_async); if (id==r->async_task_id[A]){code_no; atomic_store(r->async_go+A,0);}else{code_yes;} unlock(mutex_async)

#define ASYNC_SLEEP_LOOP(path,timeout,time0)  for(int j=1; G==2 && ((j%255) || time(NULL)-(time0)<=timeout);j++)  usleep(256);\
  lock(mutex_async); const bool finished=!G; r->async_task_id[A]++; unlock(mutex_async);\
  if (!finished) log_warn("Timeout %s '%s': '%s'",ASYNC_S[A],rootpath(r),path)\
                   //                   ; else  log_succes("NO_TIMEOUT %s '%s': '%s'",ASYNC_S[A],rootpath(r),path);

#define ASYNC_SLEEP(path,timeout)  WAIT_PICKED(timeout);  time0=time(NULL); ASYNC_SLEEP_LOOP(path,timeout,time0)
#define R()  struct rootdata *r=zpath->root; if (!r || ROOT_NOT_RESPONDING(r)) continue  // r is null e.g. for warnings.log

static bool directory_rp_stat(struct directory *dir){
  if (dir->dir_zpath.stat_rp.st_ino) return true;
  if (!DIR_RP_L(dir)) return false;
  if (!stat(DIR_RP(dir),&dir->dir_zpath.stat_rp)) return true;
  warning(WARN_STAT|WARN_FLAG_ERRNO,DIR_RP(dir),"stat");
  return false;
}
static bool readdir_now(struct directory *dir){
  if (!readdir_from_zip(dir)&&!readir_from_filesystem(dir)) return false;
  directory_rp_stat(dir);
  dir->core.dir_mtim=dir->dir_zpath.stat_rp.ST_MTIMESPEC;
  return (dir->dir_is_success=true);
}
/*
  G_on is set in async_xxx() to signal async_periodically_xxx() to perform task.
  async_xxx() waits till G_picked has been invoked by async_periodically_xxx().
  From know on wait till timeout.
  Increment r->async_task_id[A]. Then async_periodically_xxx() must not write back results any more.
  When finished async_periodically_xxx(). async_xxx() knows that finished.
  Cleanup resources: if Time-out (id!=r->async_task_id[A]) then async_periodically_xxx() is responsible for cleaning up
*/
#define L() pthread_mutex_lock(r->async_mtx+A);assert(!G);r->async_task_id[A]++
#define UL() atomic_store(r->async_go+A,0);pthread_mutex_unlock(r->async_mtx+A)
#define G           atomic_load(r->async_go+A)

#define WAIT_PICKED(timeout) atomic_store(r->async_go+A,1); time_t time0=time(NULL); for(int j=1; G==1 && ((j%255) ||ROOT_WHEN_SUCCESS(r,A)-time0<=timeout);j++) usleep(256)
#define SET_PICKED()  const int id=r->async_task_id[A];atomic_store(r->async_go+A,2)

#if WITH_DIRCACHE
static void directory_to_queue(const struct directory *dir){
  LOCK(mutex_dircache_queue,  ht_only_once(&DIR_ROOT(dir)->dircache_queue,DIR_RP(dir),0));
}
static bool async_periodically_dircache(struct rootdata *r){
  bool success=false;
  char path[PATH_MAX+1];
  struct stat stbuf={0};
  *path=0;
  { /*Pick path from an entry and put in stack variable path */
    lock_ncancel(mutex_dircache_queue);
    struct ht_entry *ee=HT_ENTRIES(&r->dircache_queue);
    RLOOP(i,r->dircache_queue.capacity){
      if (ee[i].key){
        cg_strncpy0(path,ee[i].key,MAX_PATHLEN);
        ht_clear_entry(&r->dircache_queue, ee+i);
        break;
      }}
    unlock_ncancel(mutex_dircache_queue);
  }
  if (*path){
    struct strg strg={0};    strg_init(&strg,path);
    if (IF1(WITH_STAT_CACHE,stat_from_cache(&stbuf,&strg,r)||)  stat_direct(&stbuf,&strg,r)){
      struct directory mydir={0}, *dir=&mydir;
      struct zippath *zpath=directory_init_zpath(dir,NULL);
      zpath->stat_rp=stbuf;
      zpath->realpath=zpath_newstr(zpath);
      zpath_strcat(zpath,path);
      RP_L()=zpath_commit(zpath);
      zpath->root=r;
      zpath->flags|=ZP_ZIP;
      dir->async_never=true;
      if ((success=readdir_now(dir))){
        LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(dir));
      }
      directory_destroy(dir);
    }
  }
  return success;
}
#endif //WITH_DIRCACHE

static void async_zipfile_init(struct async_zipfile *zip,const struct zippath *zpath){
  *zip=async_zipfile_empty;
  zip->azf_zpath=*zpath;
}

static void openzip_now(struct async_zipfile *zip){
  if (!zip) return;
  struct zippath *zpath=&zip->azf_zpath;
  //log_entered_function("rp: '%s' entry: '%s' ",RP(),EP());
  if (!zip->za) zip->za=my_zip_open(RP());
  assert(!zip->zf);
  zip->zf=my_zip_fopen(zip->za,EP(),ZIP_RDONLY,RP());
  LOCK(mutex_fhandle,_rootdata_counter_inc(filetypedata_for_ext(VP(),zpath->root),zip->za?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL));
}
static void closezip_now(struct async_zipfile *z){
  if (!z) return;
  LOCK_N(mutex_async,  zip_file_t *zf=z->zf; z->zf=NULL;  struct zip *za=z->za; z->za=NULL);
  const struct zippath *zpath=&z->azf_zpath;
  my_zip_fclose(zf,RP());
  my_zip_close(za,RP());
}
/* ================================================================================ */
#define A ASYNC_STAT
#if WITH_TIMEOUT_STAT
/* That is the most simple as nothing needs to be closed or destructed */
static inline bool async_periodically_stat(struct rootdata *r){
  if (!G) return false;
    assert(r->async_stat_path.s!=NULL);
  struct stat st=r->async_stat=empty_stat;
  struct strg path=r->async_stat_path;
  SET_PICKED();
  const bool success=stat_direct(&st,&path,r);
  TIMEOUT_OR_NOT(,r->async_stat=st);
  return success;
}
#endif //WITH_TIMEOUT_STAT
static bool async_stat(const struct strg *path,struct stat *st,struct rootdata *r){
  //log_entered_function(" %s",path->s);
  if (r && ROOT_NOT_RESPONDING(r)) return false;
  //log_debug_now(" %s remote: %d",path->s,isRootRemote(r));
  IF1(WITH_TIMEOUT_STAT,if (!isRootRemote(r))) return stat_direct(st,path,r);
#if WITH_TIMEOUT_STAT
  assert(path); assert(path->s); assert(*path->s);
  L();
  r->async_stat_path=*path;
  //log_debug_now("Going ASYNC_SLEEP: %s  STAT_TIMEOUT_SECONDS: %d",path->s,STAT_TIMEOUT_SECONDS);
  ASYNC_SLEEP(path->s,STAT_TIMEOUT_SECONDS);
  //log_debug_now("After ASYNC_SLEEP: %s finished: %d",path->s,finished);
  *st=finished?r->async_stat:empty_stat;
  UL();
  return st->st_ino!=0;
#endif //WITH_TIMEOUT_STAT
}
#undef A
/* ================================================================================ */
#define A ASYNC_OPENFILE
#if WITH_TIMEOUT_OPENFILE
static inline bool async_periodically_openfile(struct rootdata *r){
  if (!G) return false;
  char path[PATH_MAX+1]; strncpy(path,r->async_openfile_path,PATH_MAX);  assert(*path);
  const int flags=r->async_openfile_flags;
  SET_PICKED();
  const int fd=open(path,flags);
  TIMEOUT_OR_NOT(if (fd>0) close(fd), r->async_openfile_fd=fd);
  return fd>0;
}
static int async_openfile(struct zippath *zpath,const int flags){
  //log_entered_function("rp: %s",RP());
  do{
    R();
    if (!isRootRemote(r)) return open(RP(),flags);
    L();
    r->async_openfile_flags=flags;
    strcpy(r->async_openfile_path,RP());
    ASYNC_SLEEP(RP(),OPENFILE_TIMEOUT_SECONDS);
    const int fd=finished?r->async_openfile_fd:0;
    UL();
    if (fd>0) return fd;
  }while(find_realpath_other_root(zpath));
  return 0;
}
#else
#define async_openfile(zpath,flags) open(RP(),flags)
#endif //WITH_TIMEOUT_OPENFILE
#undef A
/* ================================================================================ */
#define A ASYNC_OPENZIP
#if WITH_TIMEOUT_OPENZIP
static inline bool async_periodically_openzip(struct rootdata *r){
  if (!G) return false;
  struct async_zipfile zip=*r->async_zipfile;
  SET_PICKED();
  openzip_now(&zip);
  const bool success=zip.zf!=NULL;
  TIMEOUT_OR_NOT(closezip_now(&zip),*r->async_zipfile=zip);
  return success;
}
static void async_openzip(struct async_zipfile *zip){
  assert(zip);
  struct zippath *zpath=&zip->azf_zpath;
  do{
    R();
    //log_entered_function("rp: '%s' entry: '%s' remote: %d",RP(),EP(),  isRootRemote(r));
    if (!r->remote){ openzip_now(zip);break;}
    L();
    assert(RP_L());  ASSERT(EP_L());
    r->async_zipfile=zip;
    ASYNC_SLEEP(RP(),OPENZIP_TIMEOUT_SECONDS);
    UL();
    if (finished && zip->zf) break;
    //log_exited_function("rp: %s remote: %d %s",RP(),isRootRemote(r), zip->zf?GREEN_SUCCESS:RED_FAIL);
  }while(find_realpath_other_root(zpath));
}
#else
#define async_openzip(zip) openzip_now(zip)
#endif //WITH_TIMEOUT_OPENZIP
#undef A
/* ================================================================================ */
/* For a small number of member files the arrays are not stored on the heap but within the struct.  */
#define A ASYNC_READDIR
#if WITH_TIMEOUT_READDIR
static void directory_copy(struct directory *dst,const struct directory *src){
  *dst=*src;
#define C(f,type)  dst->core.f=dst->_stack_##f;
  if (src->core.fname==src->_stack_fname){ C_FILE_DATA(); }
#undef C
  STRUCT_NOT_ASSIGNABLE_INIT(dst);
}

static inline bool async_periodically_readdir(struct rootdata *r){
  if (!G) return false;
  struct directory dir;
  directory_copy(&dir,r->async_dir);
  SET_PICKED();
  readdir_now(&dir);
  TIMEOUT_OR_NOT(directory_destroy(&dir),directory_copy(r->async_dir,&dir));
  return dir.dir_is_success;
}

static bool readdir_async(struct directory *dir){
  assert(dir!=NULL);
  struct zippath *zpath=&dir->dir_zpath;
  do{
    R();
    ASSERT(RP_L());
    if (!r->remote||dir->async_never) return readdir_now(dir);
    L();
    r->async_dir=dir;
    WAIT_PICKED(READDIR_TIMEOUT_SECONDS);
    directory_update_time(false,dir);
    ASYNC_SLEEP_LOOP(RP(),READDIR_TIMEOUT_SECONDS,atomic_load(r->async_when+A));
    UL();
    if (finished) return dir->dir_is_success;
  }while(find_realpath_other_root(&dir->dir_zpath));

  return false;
}
#else
#define readdir_async(dir) readdir_now(dir)
#endif //WITH_TIMEOUT_READDIR
#undef A
/* ================================================================================ */
#undef ASYNC_SLEEP
#undef ASYNC_SLEEP_LOOP
#undef TIMEOUT_OR_NOT
#undef L
#undef UL
#undef R
#undef G
#undef WAIT_PICKED
