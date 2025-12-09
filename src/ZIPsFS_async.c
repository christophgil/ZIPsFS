//////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                ///
/// running stat() asynchronously in separate thread to avoid blocking ///
/// COMPILE_MAIN=ZIPsFS                                                ///
//////////////////////////////////////////////////////////////////////////
enum enum_async_job_status{ASYNC_JOB_IDLE,ASYNC_JOB_SUBMITTED,ASYNC_JOB_PICKED};
#define OK_OR_TIMEOUT(code_ok,code_timeout)    LOCK(mutex_async,if (id==ID){code_ok;G=0;}else{code_timeout;})
//#define DIE_IF_TIMEOUT(path) IF0(IS_CHECKING_CODE, if (!finished) DIE_DEBUG_NOW("!finished %s ",path))
#define DIE_IF_TIMEOUT(path)
#define ASYNC_WAIT(path,t)  for(int j=1; G==ASYNC_JOB_PICKED && ((j%255) || (time(NULL)-(t)<=TO));j++) usleep(256);\
  LOCK_N(mutex_async, const bool finished=!G; ID++);\
  if (!finished) log_warn("Timeout G=%d  %s root: '%s': path: '%s'   %'ld  <= %d",G,ASYNC_S[A],rootpath(r),path   ,time(NULL)-(t),TO)

#define TO (A==ASYNC_STAT?STAT_TIMEOUT_SECONDS:A==ASYNC_OPENFILE?OPENFILE_TIMEOUT_SECONDS:A==ASYNC_OPENZIP?OPENZIP_TIMEOUT_SECONDS:A==ASYNC_READDIR?READDIR_TIMEOUT_SECONDS:0)
#define R()  struct rootdata *r=zpath->root; if (!r || ROOT_NOT_RESPONDING(r)) continue  // r is null e.g. for warnings.log
#define L() pthread_mutex_lock(r->async_mtx+A)
#define UL() LOCK(mutex_async, G=0); pthread_mutex_unlock(r->async_mtx+A)
#define G           r->async_go[A]
#define ID     r->async_task_id[A]
/*
  This variant leads to timeout on     find mnt/
  #define WAIT_PICKED() { time_t time0=time(NULL); for(int j=1; G==ASYNC_JOB_SUBMITTED && ((j%255) || ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC)-time0<=TO);j++) usleep(256);}
*/
#define WAIT_PICKED(code) LOCK_N(mutex_async,++ID;code;G=ASYNC_JOB_SUBMITTED);\
  for(int j=1; G==ASYNC_JOB_SUBMITTED  && ((j%255) || time(NULL)-ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC)<=TO);j++) usleep(256)
#define SET_PICKED(code)\
  int id;\
  lock(mutex_async);\
  const int go=G;\
  if (go==ASYNC_JOB_SUBMITTED){ code; id=ID;G=ASYNC_JOB_PICKED;}\
  unlock(mutex_async);\
  if (go!=ASYNC_JOB_SUBMITTED) return false;

static bool directory_rp_stat(struct directory *dir){
  if (dir->dir_zpath.stat_rp.st_ino) return true;
  if (!DIR_RP_L(dir)) return false;
  if (!stat(DIR_RP(dir),&dir->dir_zpath.stat_rp)) return true;
  warning(WARN_STAT|WARN_FLAG_ERRNO,DIR_RP(dir),"stat");
  return false;
}
static bool readdir_now(struct directory *dir){
  if (!readdir_from_zip(dir) && !readir_from_filesystem(dir)) return false;
  directory_rp_stat(dir);
  dir->core.dir_mtim=dir->dir_zpath.stat_rp.ST_MTIMESPEC;
  //IF1(WITH_ZIPENTRY_PLACEHOLDER,directory_any_file_with_placeholder(dir));
  return (dir->dir_is_success=true);
}
/*
  G_on is set in async_xxx() to signal async_periodically_xxx() to perform task.
  async_xxx() waits till G_picked has been invoked by async_periodically_xxx().
  From know on wait till timeout.
  Increment ID. Then async_periodically_xxx() must not write back results any more.
  When finished async_periodically_xxx(). async_xxx() knows that finished.
  Cleanup resources: if Time-out (id!=ID) then async_periodically_xxx() is responsible for cleaning up
*/

#if WITH_DIRCACHE
static void directory_to_queue(const struct directory *dir){
  root_start_thread(DIR_ROOT(dir),PTHREAD_ASYNC,false);
  LOCK(mutex_dircache_queue,  ht_only_once(&DIR_ROOT(dir)->dircache_queue,DIR_RP(dir),0));
}
static bool async_periodically_dircache(struct rootdata *r){
  bool success=false;
  char path[PATH_MAX+1];
  struct stat stbuf={0};
  while(true){
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
    if (!*path) break;
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
      assert(DIR_ROOT(dir)==r);
      if (readdir_now(dir)){
        success=true;
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
  struct stat st=empty_stat;
  struct strg path;
  SET_PICKED(r->async_stat=empty_stat; path=r->async_stat_path; assert(path.s!=NULL));
  const bool success=stat_direct(&st,&path,r);
  OK_OR_TIMEOUT(r->async_stat=success?st:empty_stat,);
  return success;
}
#endif //WITH_TIMEOUT_STAT
static bool async_stat(const struct strg *path,struct stat *st,struct rootdata *r){
  if (r && ROOT_NOT_RESPONDING(r)) return false;
  IF1(WITH_TIMEOUT_STAT,if (!isRootRemote(r))) return stat_direct(st,path,r);
#if WITH_TIMEOUT_STAT
  assert(path); assert(path->s); assert(*path->s);
  L();
  WAIT_PICKED(r->async_stat_path=*path);
  const time_t t0=time(NULL);
  ASYNC_WAIT(path->s,t0);
  DIE_IF_TIMEOUT(path->s);
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
  char path[PATH_MAX+1];
  int flags;
  //log_entered_function("'%s' G:%d",rootpath(r),G);
  SET_PICKED(strncpy(path,r->async_openfile_path,PATH_MAX);  assert(*path);    flags=r->async_openfile_flags);
  const int fd=open(path,flags);
  bool timeout=false;
  OK_OR_TIMEOUT(r->async_openfile_fd=fd,timeout=true);
  if (timeout && fd>0) close(fd);
  return fd>0;
}
static int async_openfile(struct zippath *zpath,const int flags){
  do{
    R();
    if (!isRootRemote(r)) return open(RP(),flags);
    L();
    WAIT_PICKED(r->async_openfile_flags=flags;  strcpy(r->async_openfile_path,RP()));
    const time_t t0=time(NULL);
    ASYNC_WAIT(RP(),t0);
    DIE_IF_TIMEOUT(RP());
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
  struct async_zipfile zip;
  //log_debug_now("%s",rootpath(r));
  SET_PICKED(zip=*r->async_zipfile);
  openzip_now(&zip);
  const bool success=zip.zf!=NULL;
  OK_OR_TIMEOUT(*r->async_zipfile=zip,closezip_now(&zip);zip.zf=NULL);
  return success;
}
static void async_openzip(struct async_zipfile *zip){
  assert(zip);
  struct zippath *zpath=&zip->azf_zpath;
  do{
    R();
    if (!r->remote){ openzip_now(zip);break;}
    L();
    ASSERT(EP_L());
    WAIT_PICKED(r->async_zipfile=zip);
    const time_t t0=time(NULL);
    ASYNC_WAIT(RP(),t0);
    UL();
    if (finished && zip->zf) break;
    DIE_IF_TIMEOUT(RP());
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

static void directory_copy(struct directory *dst,const struct directory *src, struct rootdata *r){
  lock(mutex_dircache);
  const int n=src->core.files_l;
  directory_ensure_capacity(dst,n,n);
  //directory_print(src,5);
  const struct directory_core *d=&src->core;
  assert(NULL!=src->ht_intern_names);
  FOR(i,0,n){
    directory_add(Nth0(d->fflags,i)|DIRENT_DIRECT_NAME,dst,  Nth0(d->finode,i), d->fname[i],Nth0(d->fsize,i),Nth0(d->fmtime,i),Nth0(d->fcrc,i));
  }
  dst->dir_is_success=src->dir_is_success;
  ASSERT(src->core.files_l==dst->core.files_l);
  //directory_print(dst,5);
  unlock(mutex_dircache);
}

static inline bool async_periodically_readdir(struct rootdata *r){
  static struct directory dir={0}; dir.ht_intern_names=&r->dircache_ht_fname;
  SET_PICKED(assert(r->async_dir!=NULL); directory_init_zpath(&dir,&r->async_dir->dir_zpath));
  const bool success=readdir_now(&dir);
  OK_OR_TIMEOUT(if (success) directory_copy(r->async_dir,&dir,r),);
  //log_debug_now("'%s'  %s files_l: %d ",DIR_RP(&dir),success?GREEN_SUCCESS:RED_FAIL, dir.core.files_l);    directory_print(&dir,5);
  return success;
}
static bool readdir_async(struct directory *dir){
  assert(dir!=NULL);
  struct zippath *zpath=&dir->dir_zpath;
  do{
    R();
    ASSERT(RP_L());
    if (!r->with_timeout||dir->async_never) return readdir_now(dir);
    L();
    WAIT_PICKED(r->async_dir=dir);
    root_update_time(r,-PTHREAD_ASYNC);
    ASYNC_WAIT(RP(),ROOT_WHEN_ITERATED(r,PTHREAD_ASYNC));
    //log_debug_now("%p '%s'   finished:%d  success:%d files_l: %d same: %d",dir,DIR_RP(dir),finished,dir->dir_is_success, dir->core.files_l, dir==r->async_dir);  directory_debug_filenames("dir/",dir);
    UL();
    if (finished) return dir->dir_is_success;
    DIE_IF_TIMEOUT(RP());
  }while(find_realpath_other_root(&dir->dir_zpath));
  return false;
}
#else
#define readdir_async(dir) readdir_now(dir)
#endif //WITH_TIMEOUT_READDIR
#undef A
#undef ASYNC_WAIT
#undef OK_OR_TIMEOUT
#undef L
#undef UL
#undef R
#undef G
#undef ID
#undef TO
#undef WAIT_PICKED
/* ================================================================================ */


/******************************/
/* Thread and Infinity loops  */
/******************************/
static void root_start_thread(struct rootdata *r,const enum enum_root_thread t,const bool evenIfAlreadyStarted){
  lock(mutex_start_thread);
  if (!r->thread_already_started[t] ||evenIfAlreadyStarted){
    log_verbose("Going to start thread %s / %s",r->rootpath,PTHREAD_S[t]);
    void *(*f)(void *)=IF1(WITH_PRELOADFILERAM,t==PTHREAD_PRELOAD?&infloop_preloadfile:) t==PTHREAD_ASYNC?infloop_async:  t==PTHREAD_MISC?infloop_misc: NULL;
    if (f){
      const int count=r->thread_count_started[t]++;
      if (pthread_create(&r->thread[t],NULL,f,(void*)r)){
#define C "Failed thread_create '%s'  Root: %d",PTHREAD_S[t],rootindex(r)
        if (count) warning(WARN_THREAD|WARN_FLAG_EXIT|WARN_FLAG_ERRNO,rootpath(r),C); else DIE(C);
#undef C
      }
      if (count) warning(WARN_THREAD,report_rootpath(r),"pthread_start %s function: %p",PTHREAD_S[t],f);
      r->thread_already_started[t]=true;
    }
  }
  unlock(mutex_start_thread);
}
static void log_infinity_loop(const struct rootdata *r, const enum enum_root_thread t){
  const int flag=
    t==PTHREAD_PRELOAD?LOG_INFINITY_LOOP_PRELOADFILERAM:
    t==PTHREAD_ASYNC?LOG_INFINITY_LOOP_DIRCACHE:
    t==PTHREAD_MISC?LOG_INFINITY_LOOP_MISC: 0;
  IF_LOG_FLAG(flag) log_verbose("Thread: %s  Root: %s ",PTHREAD_S[t],rootpath(r));
}

static void root_update_time(struct rootdata *r, int t){
  const bool success=t>0;
  if (t<0) t=-t;
  assert(r!=NULL);
  const time_t now=time(NULL);
  if (success) atomic_store(r->thread_when_success+t,now);
  atomic_store(r->thread_when+t,now);
}


static void *infloop_async(void *arg){
  struct rootdata *r=arg;
  assert(r!=NULL);
  init_infloop(r,PTHREAD_ASYNC);
  long nanos=ASYNC_SLEEP_USECONDS*1000; /* The shorter the higher the responsiveness, but increased CPU usage */
  for(int loop=0; ;loop++){
    cg_nanosleep(nanos);
    if (nanos<ASYNC_SLEEP_USECONDS*1000)      nanos++;
    bool success=IF01(IS_CHECKING_CODE,false,rand());
    IF1(WITH_DIRCACHE, if (!(loop&255) && async_periodically_dircache(r)) success=true);
    if (r->with_timeout){
      /* Reduce waiting when many subsequent request */
      IF1(WITH_TIMEOUT_STAT, if ((async_periodically_stat(r))){ nanos=MAX(ASYNC_SLEEP_USECONDS*1000L/50,1000*400);  success=true; sched_yield();});
      if (!(loop&15)){ /* less often */
#define C(with,fn) IF1(with,if (fn(r)){success=true;});
        C(WITH_TIMEOUT_READDIR,async_periodically_readdir);
        C(WITH_TIMEOUT_OPENZIP,async_periodically_openzip);
        C(WITH_TIMEOUT_OPENFILE,async_periodically_openfile);
#undef C
      }
    }
    if (r->remote){
      if (!success && !(loop&255) && ROOT_SUCCESS_SECONDS_AGO(r)>MAX(1,ROOT_RESPONSE_WITHIN_SECONDS/4)){
        success=!statvfs(rootpath(r),&r->statvfs);
        if (!success) log_verbose(RED_FAIL"statvfs(%s)",rootpath(r));
      }
      if (!(loop&255)||success) root_update_time(r,success?PTHREAD_ASYNC:-PTHREAD_ASYNC);
    }
    if (!(loop&0x1023)) log_infinity_loop(r,PTHREAD_ASYNC);
  }
}

static bool _cleanup_running;
static void *_cleanup_files_runnable(void *arg){
  _cleanup_running=true;
    //const pid_t pid0=getpid();
    if (fork()){
      perror(_cleanup_script);
    }else{
      execlp("bash","bash",_cleanup_script,(char*)0);
      exit(errno);
    }
  _cleanup_running=false;
  return NULL;
}
static void *infloop_misc(void *arg){
  struct rootdata *r=arg;
  init_infloop(r,PTHREAD_MISC);
  for(int j=0;;j++){
    log_infinity_loop(r,PTHREAD_MISC);
    root_update_time(r,-PTHREAD_MISC);
    usleep(1000*1000);
    LOCK_NCANCEL(mutex_fhandle,fhandle_destroy_those_that_are_marked());
#if WITH_AUTOGEN||WITH_PRELOADFILEDISK
    if (_root_writable && (j&0xFff)==256){
      static pthread_t t;
      if (!_cleanup_running && cg_file_exists(_cleanup_script)) pthread_create(&t,NULL,&_cleanup_files_runnable,NULL);
    }
#endif //WITH_AUTOGEN||WITH_PRELOADFILEDISK
    if (!(j&3)){
      static struct pstat pstat1,pstat2;
      cpuusage_read_proc(&pstat2,getpid());
      cpuusage_calc_pct(&pstat2,&pstat1,&_ucpu_usage,&_scpu_usage);
      pstat1=pstat2;
      IF_LOG_FLAG(LOG_INFINITY_LOOP_RESPONSE) if (_ucpu_usage>40||_scpu_usage>40) log_verbose("pid: %d cpu_usage user: %.2f system: %.2f\n",getpid(),_ucpu_usage,_scpu_usage);
    }
    log_flags_update();
    //if (!(j&63)) debug_report_zip_count();
    IF1(WITH_CANCEL_BLOCKED_THREADS,unblock_periodically());
  }
}


/* Each remote root has its own  infloop_preloadfile thread. Loading file content into RAM asynchronously. */
#if WITH_PRELOADFILERAM || WITH_PRELOADFILEDISK
static void *infloop_preloadfile(void *arg){
  struct rootdata *r=arg;
  init_infloop(r,PTHREAD_PRELOAD);
  /* pthread_cleanup_push(infloop_preloadfile_start,r); Does not work because pthread_cancel not working when root blocked. */
  for(int i=0;;i++){
    struct fHandle *dm=NULL,*dl=NULL,*d=NULL; // cppcheck-suppress constVariablePointer
    {
      lock(mutex_fhandle);
      foreach_fhandle(id,e){
        IF1(WITH_PRELOADFILERAM,if (e->preloadfileram && e->preloadfileram->preloadfileram_status==preloadfileram_queued && e->preloadfileram->m_zpath.root==r) d=dm=e);
        if (e->zpath.root==r && e->flags&FHANDLE_FLAG_LCOPY_QUEUE) d=dl=e; // cppcheck-suppress unreadVariable
      }
      unlock(mutex_fhandle);
    }
    if (d && wait_for_root_timeout(r)){
      {
        lock(mutex_fhandle);
        atomic_fetch_add(&d->is_preloading,1); /* Prevents destruction */
        IF1(WITH_PRELOADFILERAM,  if (dm && preloadfileram_queued!=preloadfileram_get_status(dm))  dm=NULL;   preloadfileram_set_status(dm,preloadfileram_reading));
        IF1(WITH_PRELOADFILEDISK, if (dl && !(dl->flags&FHANDLE_FLAG_LCOPY_QUEUE))  dl=NULL; if (dl) { dl->flags|=FHANDLE_FLAG_LCOPY_RUN; dl->flags&=~FHANDLE_FLAG_LCOPY_QUEUE;});
        unlock(mutex_fhandle);
      }
      IF1(WITH_PRELOADFILERAM,  if (dm) preloadfileram_now(dm,r));
      IF1(WITH_PRELOADFILEDISK, if (dl){ char dst[MAX_PATHLEN+1];   if (preloadfiledisk_realpath(&dl->zpath,dst)) preloadfiledisk_now(dst,NULL,dl);});
      {
        lock(mutex_fhandle);
        atomic_fetch_add(&d->is_preloading,-1);
        IF1(WITH_PRELOADFILERAM,  preloadfileram_set_status(dm,preloadfileram_done));
        unlock(mutex_fhandle);
      }
    }/* if (d) */
    root_update_time(r,-PTHREAD_PRELOAD);
    usleep(100*1000);
  }
}



// PRELOADFILE_TIMEOUT_SECONDS     PREFETCH_FILE_TIMEOUT_SECONDS
// PTHREAD_PRELOAD             PTHREAD_PREFETCH_FILE
// preloadfile_time_exceeded()     prefetch_file_timeout()
// infloop_preloadfile() infloop_prefetch_file()
// config_advise_cache_in_ram() config_advise_prefetch_file_to_ram
#if PRELOADFILE_TIMEOUT_SECONDS
bool preloadfile_time_exceeded(const char *func, const struct fHandle *d,const int counter){
  lock(mutex_fhandle);
  struct rootdata *r=d->zpath.root;
  assert(r!=NULL);
  time_t t=time(NULL)-ROOT_WHEN_ITERATED(r,PTHREAD_PRELOAD);
  unlock(mutex_fhandle);
  if (t>PRELOADFILE_TIMEOUT_SECONDS){
      warning(WARN_PRELOADFILE,D_RP(d),"%s Timeout > "STRINGIZE(PRELOADFILE_TIMEOUT_SECONDS)" s",func);
      COUNTER1_INC(counter);
      return true;
  }
  return false;
}
#else
#define preloadfile_time_exceeded(...) false
#endif //PRELOADFILE_TIMEOUT_SECONDS

#endif //WITH_PRELOADFILERAM || WITH_PRELOADFILEDISK
///////////////////////////////////////////////
/// Capability to unblock requires that     ///
/// pthreads have different gettid() and    ///
/// /proc- file system                      ///
///////////////////////////////////////////////
static void init_infloop(const struct rootdata *r, const enum enum_root_thread ithread){
  IF_LOG_FLAG(LOG_INFINITY_LOOP_RESPONSE)log_entered_function("Thread: %s  Root: %s ",PTHREAD_S[ithread],rootpath(r));
  IF1(WITH_CANCEL_BLOCKED_THREADS,
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&_unused_int);
      if (r) r->thread_pid[ithread]=gettid(); assert(_pid!=gettid()); assert(cg_pid_exists(gettid())));
}
#if WITH_CANCEL_BLOCKED_THREADS
static void unblock_periodically(){
  static yes_zero_no_t threads_use_own_PID=0;
  if (!threads_use_own_PID){
    threads_use_own_PID=YES;
    if (_pid==gettid()){ threads_use_own_PID=NO;  warning(WARN_THREAD,"","Threads not using own process IDs. No unblock of blocked threads");}
  }
  if (threads_use_own_PID==NO) return;
  foreach_root(r){
    FOR(t,1,PTHREAD_LEN){
      if (t==PTHREAD_MISC || !r->thread_already_started[t]) continue;
      const int threshold=t==PTHREAD_PRELOAD?UNBLOCK_AFTER_SECONDS_THREAD_PRELOADFILERAM: t==PTHREAD_ASYNC?UNBLOCK_AFTER_SECONDS_THREAD_ASYNC: 0;
      if (!threshold || !r->thread_count_started[t] || !r->thread[t]) continue;
      const long now=time(NULL), last=MAX_long(ROOT_WHEN_ITERATED(r,t),r->thread_when_canceled[t]);
      if (!last) continue;
      if (now-last>threshold){
        log_verbose("%s  %s  now-last:%ld > threshold: %d ",r->rootpath, PTHREAD_S[t],now-last,threshold);
        pthread_cancel(r->thread[t]); /* Double cancel is not harmful: https://stackoverflow.com/questions/7235392/is-it-safe-to-call-pthread-cancel-on-terminated-thread */
        usleep(1000*1000);
        const pid_t pid=r->thread_pid[t];
        bool not_restart=pid && cg_pid_exists(pid);
        if (not_restart && _thread_unblock_ignore_existing_pid){
          warning(WARN_THREAD,rootpath(r),"Going to start thread %s even though pid %ld still exists.",PTHREAD_S[t],(long)pid);
          _thread_unblock_ignore_existing_pid=not_restart=false;
        }
        char proc[99]; sprintf(proc,"/proc/%ld",(long)pid);
        if (not_restart){
          log_warn("Not yet starting thread because process  still exists %s",proc);
        }else{
          warning(WARN_THREAD|WARN_FLAG_SUCCESS,proc,"Going root_start_thread(%s,%s) because process does not exist any more",rootpath(r),PTHREAD_S[t]);
          r->thread_when_canceled[t]=time(NULL);
          root_start_thread(r,t,true);
        }
      }
    }
  }
}
#endif //WITH_CANCEL_BLOCKED_THREADS




/***********************************************************************************************************/
/* 1. Return true for local file roots.                                                                    */
/* 2. Return false for remote file roots (starting with double slash) that has not responded for long time */
/* 2. Wait until last respond of root is below threshold.                                                  */
/***********************************************************************************************************/
static void log_root_blocked(struct rootdata *r,const bool blocked){
  if (r && r->blocked!=blocked){
    warning(WARN_ROOT|WARN_FLAG_ERROR,rootpath(r),"Remote root %s"ANSI_RESET"\n",blocked?ANSI_FG_RED"not responding.":ANSI_FG_GREEN"responding again.");
    r->blocked=blocked;
  }
}
static bool wait_for_root_timeout(struct rootdata *r){
  if (!r || !r->remote){ log_root_blocked(r,false);return true;}
  const int N=10000;
  RLOOP(iTry,N){
    const time_t delay=ROOT_SUCCESS_SECONDS_AGO(r);
    //log_debug_now("delay: %ld ",delay);
    if (delay>ROOT_GIVEUP_AFTER_SECONDS) break;
    if (delay<ROOT_RESPONSE_WITHIN_SECONDS){
      log_root_blocked(r,false);
      return true;
    }
    cg_sleep_ms(ROOT_RESPONSE_WITHIN_SECONDS/N,"");
    bool log=iTry>N/2 && iTry%(N/10)==0;
    IF_LOG_FLAG(LOG_INFINITY_LOOP_RESPONSE) log=true;
    if (log) log_verbose("%s %d/%d\n",rootpath(r),iTry,N);
  }
  r->log_count_delayed_periods++;
  r->log_count_delayed++;
  log_root_blocked(r,true);
  r->log_count_delayed_periods++;
  return false;
}
