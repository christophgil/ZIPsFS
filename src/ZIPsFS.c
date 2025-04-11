/*
  ZIPQsFS   Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  It has been developed starting with  fuse-3.14: Filesystem in Userspace  passthrough.c
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS_notes.org  log.c
*/
// (defun Copy_working() (interactive) (shell-command (concat  (file-name-directory (buffer-file-name) ) "Copy_working.sh")))
// (buffer-filepath)
//__asm__(".symver realpath,realpath@GLIBC_2.2.5");
#define VFILE_SFX_INFO "@SOURCE.TXT"
#define PATH_IS_FILE_INFO(vp,vp_l) ENDSWITH(vp,vp_l,VFILE_SFX_INFO)
#define HOMEPAGE "https://github.com/christophgil/ZIPsFS"
#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#ifndef PATH_MAX // in OpenSolaris
#define PATH_MAX 1024
#endif
// ---
#include <sys/types.h>
#include <unistd.h> /// ???? lseek
#include <getopt.h>
// ---
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif  // !MAP_ANONYMOUS
// ---
#include "config.h"
#include <dirent.h>
#include <fuse.h>
#include <zip.h>
#include <stdatomic.h>
#ifdef __USE_GNU
#include <gnu/libc-version.h>
#endif
#if WITH_EXTRA_ASSERT
#define ASSERT(...) (assert(__VA_ARGS__))
#else
#define ASSERT(...)
#endif
//////////////////////////////
/// FUSE 1 or  FUSE 2      ///
//////////////////////////////
/* If compiled with ZIPsFS.compile.sh, then the FUSE_MAJOR_V and FUSE_MINOR_V are detected with print_fuse_version.c */
#ifndef FUSE_MAJOR_V
#define FUSE_MAJOR_V 3
#define FUSE_MINOR_V 999
#endif
// ---
#if FUSE_MAJOR_V>2
#define WITH_FUSE_3 1
#define COMMA_FILL_DIR_PLUS ,0
#else
#define WITH_FUSE_3 0
#define COMMA_FILL_DIR_PLUS
#endif

#define HOOK_MSTORE_CLEAR(m)   {char mpath[PATH_MAX+1];mstore_file(mpath,m,-1);warning(WARN_DIRCACHE,mpath,"Clearing struct mstore %s ",m->name);}
// ---
////////////////////
/// Early Macros ///
////////////////////
#define WITH_DEBUG_MALLOC 1
#include "cg_log.h"
#include "ZIPsFS_version.h"
#include "cg_utils.h"
#include "cg_ht_v7.h"
#include "cg_textbuffer.h"
#include "cg_cpu_usage_pid.h"
#include "ZIPsFS_configuration.h"
#include "ZIPsFS.h"
// ---
#define PROFILED(x) x
#include "generated_ZIPsFS.inc"
#include "cg_profiler.h"
// ---
#include "cg_pthread.c"
#include "cg_debug.c"
#include "cg_utils.c"
#include "cg_ht_v7.c"
#include "cg_log.c"
#include "cg_cpu_usage_pid.c"
#include "cg_textbuffer.c"


#ifdef __cppcheck__
#define fhandle_busy_start(d) FILE *_fhandle_busy=fopen("abc","r")
#define fhandle_busy_end(d)   fclose(_fhandle_busy)
#else
#define fhandle_busy_start(d) {if (d) d->is_busy++;}   /* Protects fhandle d from being destroyed */
#define fhandle_busy_end(d)   {if (d) d->is_busy--;}
#endif //__cppcheck__

#if WITH_AUTOGEN
#include "ZIPsFS_configuration_autogen.h"
#include "ZIPsFS_autogen_impl.h"
#endif //WITH_AUTOGEN
static char _self_exe[PATH_MAX+1];//,static char _initial_cwd[PATH_MAX+1];
static int _fhandle_n=0,_mnt_l=0;
IF1(WITH_MEMCACHE,static enum enum_when_memcache_zip _memcache_policy=MEMCACHE_SEEK);
static bool _pretendSlow=false;
static char _mkSymlinkAfterStart[MAX_PATHLEN+1]={0},_mnt[PATH_MAX+1];
static struct ht *ht_set_id(const int id,struct ht *ht){
#if WITH_DEBUG_MALLOC
  if (!ht) return NULL;
  assert(id<(1<<MALLOC_TYPE_SHIFT));
  ht->id=MALLOC_TYPE_HT|id;
  if (ht->keystore)    _malloc_is_count_mstore[ht->keystore->id=MALLOC_TYPE_KEYSTORE|id]=true;
  if (ht->valuestore)  _malloc_is_count_mstore[ht->valuestore->id=MALLOC_TYPE_VALUESTORE|id]=true;
#endif //WITH_DEBUG_MALLOC
  return ht;
}
static int _lock_oldstate_unused;
static int zpath_strlen(const struct zippath *zpath,int s){
  return s==0?0:strlen(zpath->strgs+s);
}
static bool zpath_exists(struct zippath *zpath){
  if (!zpath) return false;
  const ino_t ino=zpath->stat_rp.st_ino;
  if ((ino!=0) != (zpath->realpath_l!=0)) warning(WARN_FLAG_ONCE|WARN_FLAG_ERROR,RP(),"Inconsistence ino: %ld  realpath_l: %d",ino,zpath->realpath_l);
  return ino!=0;
}



static MAYBE_INLINE struct fhandle* fhandle_at_index(int i){
  static struct fhandle *_fhandle[FHANDLE_BLOCKS];
#define B _fhandle[i>>FHANDLE_LOG2_BLOCK_SIZE]
  struct fhandle *block=B;
  if (!block){
    block=B=cg_calloc(MALLOC_fhandle,FHANDLE_BLOCK_SIZE,SIZEOF_FHANDLE);
    assert(block!=NULL);
  }
  return block+(i&(FHANDLE_BLOCK_SIZE-1));
#undef B
}
static struct mstore mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
static long _count_stat_fail=0,_count_stat_from_cache=0;
static struct ht
ht_intern_fileext,_ht_valid_chars, ht_inodes_vp,ht_count_getattr
  IF1(WITH_AUTOGEN,,ht_fsize)
  IF1(WITH_STAT_CACHE,,stat_ht)
  IF1(WITH_ZIPINLINE_CACHE,, ht_zinline_cache_vpath_to_zippath);

/////////////////////////////////////////////////////////////////
/// Root paths - the source directories                       ///
/// The root directories are specified as program arguments   ///
/// The _root[0] may be read/write, and can be empty string   ///
/// The others are always  read-only                          ///
/////////////////////////////////////////////////////////////////

static struct rootdata _root[ROOTS]={0}, *_root_writable=NULL;
static const char* rootpath(const struct rootdata *r){
  if (!r) return NULL;
  assert(r->rootpath!=NULL);
  return r->rootpath;
}
static const char* report_rootpath(const struct rootdata *r){
  return !r?"No root" : r->rootpath;
}
static int rootindex(const struct rootdata *r){
  return !r?-1: (int)(r-_root);
}
static int _root_n=0;
/* *** fhandle vars and defs *** */
static char _fWarningPath[2][MAX_PATHLEN+1], _fLogFlags[MAX_PATHLEN+1];
static float _ucpu_usage,_scpu_usage;/* user and system */
static long _count_readzip_memcache=0,_count_readzip_memcache_because_seek_bwd=0,_log_read_max_size=0,_count_SeqInode=0;
static  int64_t _memcache_maxbytes=3L*1000*1000*1000;
static int _unused_int;
static bool _thread_unblock_ignore_existing_pid;
// ---
#include "ZIPsFS_configuration.c"
#include "ZIPsFS_debug.c"
#if WITH_STAT_CACHE
#include "ZIPsFS_cache_stat.c"
#endif //WITH_STAT_CACHE
#include "ZIPsFS_cache.c"
#if WITH_TRANSIENT_ZIPENTRY_CACHES
#include "ZIPsFS_transient_zipentry_cache.c"
#endif
// ---
#include "ZIPsFS_log.c"
#if WITH_MEMCACHE
#include "ZIPsFS_memcache.c"
#endif // WITH_MEMCACHE
// ---
#if WITH_AUTOGEN
#include "ZIPsFS_autogen.c"
#include "ZIPsFS_configuration_autogen.c"
#include "ZIPsFS_autogen_impl.c"
#endif //WITH_AUTOGEN
// ---
#include "ZIPsFS_ctrl.c"
#if WITH_STAT_SEPARATE_THREADS
#include "ZIPsFS_stat_queue.c"
#endif //WITH_STAT_SEPARATE_THREADS
// ---
#ifndef WITH_PROFILER
#define WITH_PROFILER 0
#endif
// ---
#if WITH_PROFILER
#include "generated_profiler.c"
#endif // WITH_PROFILER
// ---
////////////////////////////////////////
/// lock,pthread, synchronization   ///
////////////////////////////////////////
static pid_t _pid;
static pthread_mutex_t _mutex[NUM_MUTEX];
static void init_mutex(void){
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  RLOOP(i,mutex_roots+_root_n){
    pthread_mutex_init(_mutex+i,&_mutex_attr_recursive);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consider ZIP file 20220802_Z1_AF_001_30-0001_SWATH_P01-Rep4.wiff2.Zip
// 20220802_Z1_AF_001_30-0001_SWATH_P01-Rep4.wiff.scan  will be stored as *.wiff.scan in the table-of-content
// * denotes the PLACEHOLDER_NAME
//
// This will save space in cache.  Many ZIP files will have identical table-of-content and thus kept only once in the cache.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char *simplify_fname(char *s,const char *u, const char *zipfile){
  ASSERT(s!=NULL);
#if WITH_DIRCACHE_OPTIMIZE_NAMES
  const int ulen=cg_strlen(u);
  if (strchr(u,PLACEHOLDER_NAME)){
    static int alreadyWarned=0;
    if(!alreadyWarned++) warning(WARN_CHARS,zipfile,"Found PLACEHOLDER_NAME in file name");
  }
  s[0]=0;
  if (!ulen) return u;
  if (zipfile && (ulen<MAX_PATHLEN) && *zipfile){
    const char *replacement=zipfile+cg_last_slash(zipfile)+1, *dot=cg_strchrnul(replacement,'.');
    const int replacement_l=dot-replacement;
    const char *posaddr=dot && replacement_l>5?cg_memmem(u,ulen,replacement,replacement_l):NULL;
    if (posaddr && posaddr<replacement+replacement_l){
      const int pos=posaddr-u;
      memcpy(s,u,pos);
      s[pos]=PLACEHOLDER_NAME;
      memcpy(s+pos+1,u+pos+replacement_l,ulen-replacement_l-pos);
      s[ulen-replacement_l+1]=0;
      return s;
    }
  }
#endif //WITH_DIRCACHE_OPTIMIZE_NAMES
  return u;
}/* simplify_fname*/
/*  Reverses of simplify_fname().    Replaces PLACEHOLDER_NAME by name of zipfile */
static bool unsimplify_fname(char *n,const char *zipfile){
#if WITH_DIRCACHE_OPTIMIZE_NAMES
  const char *placeholder=strchr(n,PLACEHOLDER_NAME);
  if (placeholder){
    const char *replacement=zipfile+cg_last_slash(zipfile)+1;
    const int n_l=cg_strlen(n),pos=placeholder-n,replacement_l=((char*)cg_strchrnul(replacement,'.'))-replacement;
    int dst=n_l+replacement_l;/* Start copying with terminal zero */
    if (dst>=MAX_PATHLEN) return false;
    for(;--dst>=0;){
      const int src=dst<pos?dst:  dst<pos+replacement_l?-1: dst-replacement_l+1;
      if (src>=0) n[dst]=n[src];
    }
    memcpy(n+pos,replacement,replacement_l);
  }
#endif //WITH_DIRCACHE_OPTIMIZE_NAMES
  return true;
}
//////////////////////
// struct directory //
//////////////////////
#define directory_is_stack(d) ((d)->core.fname==(d)->_stack_fname)
static void directory_init(struct directory *d,uint32_t flags,const char *realpath,const int realpath_l, struct rootdata *r){
#define C(field,type) d->core.field=d->_stack_##field
  C_FILE_DATA();
#undef C
  d->dir_realpath=realpath;
  d->dir_realpath_l=realpath_l;
  d->core.files_l=0;
  d->root=r;
  d->dir_flags=flags;
  mstore_init(&d->filenames,"",4096|MSTORE_OPT_MALLOC);
  d->files_capacity=DIRECTORY_FILES_CAPACITY;
}
#define directory_new(dir,flags,realpath,realpath_l,r)  struct directory dir={0};directory_init(&dir,flags,realpath,realpath_l,r)
static void directory_destroy(struct directory *d){
  if (d && (0==(d->dir_flags&(DIRECTORY_IS_DIRCACHE)))){
#define C(field,type) if(d->core.field!=d->_stack_##field) cg_free_null(MALLOC_directory_field,d->core.field);
    C_FILE_DATA();
#undef C
    mstore_destroy(&d->filenames);
  }
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
  cg_thread_assert_locked(mutex_dircache); ASSERT(n0!=NULL); ASSERT(dir!=NULL);
  if (cg_empty_dot_dotdot(n0)) return;
  struct directory_core *d=&dir->core;
#define L d->files_l
#define B dir->files_capacity
  if (B<=L){
    B=2*L;
    const bool is_stack=directory_is_stack(dir);
#define C(f,type)  if (d->f) d->f=is_stack?memcpy(cg_malloc(MALLOC_directory_field,B*sizeof(type)),d->f,L*sizeof(type)):   realloc(d->f,B*sizeof(type));
    C_FILE_DATA();
#undef C
  }
  static char buf_for_s[MAX_PATHLEN+1];
  const char *s=simplify_fname(buf_for_s,n0,dir->dir_realpath);
  const int len=cg_pathlen_ignore_trailing_slash(s);
  // ASSERT(len>0);
  if (d->fflags) d->fflags[L]=flags|(s[len]=='/'?DIRENT_ISDIR:0);
#define C(name) if (d->f##name) d->f##name[L]=name
  C(mtime);  C(size);  C(crc);  C(inode);
#undef C
  if (crc) ASSERT(NULL!=d->fcrc);
  ASSERT(NULL!=d->fname);
  d->fname[L++]=(char*)mstore_addstr(&dir->filenames,s,len);
#undef B
#undef L
}
////////////
/// stat ///
////////////

static int stat_cache_opts_for_root(struct rootdata *r){
  if (r) return ((r->features&ROOT_REMOTE)?STAT_CACHE_ROOT_IS_REMOTE:0)|(r==_root_writable?STAT_CACHE_ROOT_IS_WRITABLE:0);
  return 0;
}
static bool stat_from_cache_or_syscall(const int opt, const char *path,const int path_l,const ht_hash_t hash,struct stat *stbuf){
  cg_thread_assert_not_locked(mutex_fhandle);
  ASSERT(NULL!=path);   ASSERT(strlen(path)>=path_l);
  IF1(WITH_STAT_CACHE,
      if (stat_from_cache(opt,stbuf,path,path_l,hash)) return true;
      if (!(opt&STAT_ALSO_SYSCALL)) return false);
  const int res=lstat(path,stbuf);
  LOCK(mutex_fhandle,inc_count_getattr(path,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS));
  if (res) return false;
  assert(stbuf->st_ino!=0);
  IF1(WITH_STAT_CACHE,stat_to_cache(stbuf,path,path_l,hash));
  return true;
}/*stat_from_cache_or_syscall()*/
static bool stat_from_cache_or_syscall_or_async(const char *rp, struct stat *stbuf,struct rootdata *r){
  const int rp_l=cg_strlen(rp);
  const ht_hash_t hash=hash32(rp,rp_l);
  if (!WITH_STAT_SEPARATE_THREADS || !(r&&r->features&ROOT_REMOTE)) return stat_from_cache_or_syscall(STAT_ALSO_SYSCALL|stat_cache_opts_for_root(r),rp,rp_l,hash,stbuf);
  if (stat_from_cache_or_syscall(0,rp,rp_l,hash,stbuf)) return true;
  return IF1(WITH_STAT_SEPARATE_THREADS,stat_queue_and_wait(rp,rp_l,hash,stbuf,r)||) false;
}
//////////////////////
/// Infinity loops ///
//////////////////////
static int observe_thread(struct rootdata *r, const int thread){
  LF();
  const int flag=
    thread==PTHREAD_RESPONDING?LOG_INFINITY_LOOP_RESPONSE:
    thread==PTHREAD_STATQUEUE?LOG_INFINITY_LOOP_STAT:
    thread==PTHREAD_MEMCACHE?LOG_INFINITY_LOOP_MEMCACHE:
    thread==PTHREAD_DIRCACHE?LOG_INFINITY_LOOP_DIRCACHE:
    thread==PTHREAD_MISC0?LOG_INFINITY_LOOP_MISC:
    0;
  if (flag) IF_LOG_FLAG(flag) log_verbose("Thread: %s  Root: %s ",PTHREAD_S[thread],rootpath(r));
  while(r->thread_pretend_blocked[thread]) usleep(1000*100);
  return (r->thread_when_loop_deciSec[thread]=deciSecondsSinceStart());
}
static void root_start_thread(struct rootdata *r,int ithread){
  r->thread_pretend_blocked[ithread]=false;
  if (atomic_fetch_add(&r->thread_starting[ithread],0)){
    log_warn("r->thread_starting[%s] >0 Not going to start thread.",PTHREAD_S[ithread]);
    return;
  }
  atomic_fetch_add(&r->thread_starting[ithread],1);
  void *(*f)(void *)=NULL;
  switch(ithread){
#if WITH_STAT_SEPARATE_THREADS
  case PTHREAD_STATQUEUE:
    if (!(r->features&ROOT_REMOTE)) return;
    f=&infloop_statqueue;
    break;
#endif
    IF1(WITH_MEMCACHE,case PTHREAD_MEMCACHE: f=&infloop_memcache;break);
  case PTHREAD_RESPONDING: f=&infloop_responding; break;
  case PTHREAD_DIRCACHE: f=&infloop_dircache; break;
  case PTHREAD_MISC0: f=&infloop_misc0; break;
  }
  const int count=r->thread_count_started[ithread]++;
  if (count) warning(WARN_THREAD,report_rootpath(r),"pthread_start %s function: %p",PTHREAD_S[ithread],f);
  ASSERT(f!=NULL);
  if (f){
    if (pthread_create(&r->thread[ithread],NULL,f,(void*)r)){
#define C "Failed thread_create '%s'  Root: %d",PTHREAD_S[ithread],rootindex(r)
      if (count) warning(WARN_THREAD|WARN_FLAG_EXIT|WARN_FLAG_ERRNO,rootpath(r),C); else DIE(C);
#undef C
    }
  }
  atomic_fetch_add(&r->thread_starting[ithread],-1);
}



static void *infloop_responding(void *arg){
  struct rootdata *r=arg;
  init_infloop(r,PTHREAD_RESPONDING);
  for(int j=0;;j++){
    //if (r->features&ROOT_REMOTE) log_msg(" R%d.%d ",rootindex(r),j);
    const int now=deciSecondsSinceStart();
    if (!statvfs(rootpath(r),&r->statvfs)){ /* sigar_file_system_usage_get() is using statvfs() which blocks on a stale NFS mounts. */
      const int diff=observe_thread(r,PTHREAD_RESPONDING)-now;
      if (diff>ROOT_WARN_STATFS_TOOK_LONG_SECONDS*10) log_warn("\n statfs %s took %'ld ms\n",rootpath(r),100L*diff);
    }
    usleep(1000*ROOT_OBSERVE_EVERY_MSECONDS_RESPONDING);
  }
  return NULL;
}
static void *infloop_misc0(void *arg){
  LF();
  struct rootdata *r=arg;
  init_infloop(r,PTHREAD_MISC0);
  for(int j=0;;j++){
    observe_thread(r,PTHREAD_MISC0);
    usleep(1000*1000);
    LOCK_NCANCEL(mutex_fhandle,fhandle_destroy_those_that_are_marked());
    IF1(WITH_AUTOGEN,if (!(j&0xFff)) autogen_cleanup());
    //   if (!(j&0xFF)) log_msg(" MI ");
    if (!(j&3)){
      static struct pstat pstat1,pstat2;
      cpuusage_read_proc(&pstat2,getpid());
      cpuusage_calc_pct(&pstat2,&pstat1,&_ucpu_usage,&_scpu_usage);
      pstat1=pstat2;
      IF_LOG_FLAG(LOG_INFINITY_LOOP_RESPONSE) if (_ucpu_usage>40||_scpu_usage>40) log_verbose("pid: %d cpu_usage user: %.2f system: %.2f\n",getpid(),_ucpu_usage,_scpu_usage);
    }
  }
}

///////////////////////////////////////////////
/// Capability to unblock requires that     ///
/// pthreads have different gettid() and    ///
/// /proc- file system                      ///
///////////////////////////////////////////////
static void init_infloop(struct rootdata *r, const int ithread){
  LF();
  IF_LOG_FLAG(LOG_INFINITY_LOOP_RESPONSE) log_entered_function("Thread: %s  Root: %s ",PTHREAD_S[ithread],rootpath(r));
  IF1(WITH_CANCEL_BLOCKED_THREADS,
      pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,&_unused_int);
      if (r) r->thread_pid[ithread]=gettid(); assert(_pid!=gettid()); assert(cg_pid_exists(gettid())));
}
#if WITH_CANCEL_BLOCKED_THREADS
static void *infloop_unblock(void *arg){
  usleep(1000*1000);
  if (_pid==gettid()){ warning(WARN_THREAD,"","Threads not using own process IDs. No unblock of blocked threads"); return NULL;}
  if (*_mkSymlinkAfterStart){
    struct stat stbuf;
    PROFILED(lstat)(_mkSymlinkAfterStart,&stbuf);
    if (((S_IFREG|S_IFDIR)&stbuf.st_mode) && !(S_IFLNK&stbuf.st_mode)){
      warning(WARN_MISC,""," Cannot make symlink %s =>%s  because %s is a file or dir\n",_mkSymlinkAfterStart,_mnt,_mkSymlinkAfterStart);
      EXIT(1);
    }
  }
  while(true){
    const int seconds=2;
    usleep(1000*1000*seconds);
    foreach_root1(r){
      RLOOP(t,PTHREAD_LEN){
        if (!r->thread_is_run[t]) continue;
        const int threshold=
          t==PTHREAD_STATQUEUE?10*UNBLOCK_AFTER_SECONDS_THREAD_STATQUEUE:
          t==PTHREAD_MEMCACHE?10*UNBLOCK_AFTER_SECONDS_THREAD_MEMCACHE:
          t==PTHREAD_DIRCACHE?10*UNBLOCK_AFTER_SECONDS_THREAD_DIRCACHE:
          t==PTHREAD_RESPONDING?10*UNBLOCK_AFTER_SECONDS_THREAD_RESPONDING:
          0;
        if (threshold && r->thread[t] && (deciSecondsSinceStart()-MAX_int(r->thread_when_loop_deciSec[t],r->thread_when_canceled_deciSec[t]))>MAX_int(threshold,10*2*seconds)){
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
            r->thread_when_canceled_deciSec[t]=deciSecondsSinceStart();
            root_start_thread(r,t);
          }
        }
        usleep(1000*1000);
      }
    }
  }
  return NULL;
}
#endif //WITH_CANCEL_BLOCKED_THREADS


/////////////////////////////////////////////////////////////////////////////////////////////
/// 1. Return true for local file roots.
/// 2. Return false for remote file roots (starting with double slash) that has not responded for long time
/// 2. Wait until last respond of root is below threshold.
////////////////////////////////////////////////////////////////////////////////////////////
static void log_root_blocked(struct rootdata *r,const bool blocked){
  if (r->blocked!=blocked){
    warning(WARN_ROOT|WARN_FLAG_ERROR,rootpath(r),"Remote root %s"ANSI_RESET"\n",blocked?ANSI_FG_RED"not responding.":ANSI_FG_GREEN"responding again.");
    r->blocked=blocked;
  }
}
static bool wait_for_root_timeout(struct rootdata *r){
  if (!(r->features&ROOT_REMOTE)){ log_root_blocked(r,false);return true;}
  //const bool debug=filepath_contains_blocking(rootpath(r));
  const int N=10000;

  RLOOP(iTry,N){
    const float delay=(deciSecondsSinceStart()-r->thread_when_loop_deciSec[PTHREAD_RESPONDING])/10.0;
    if (delay>ROOT_SKIP_UNLESS_RESPONDED_WITHIN_SECONDS) break;
    if (delay<ROOT_LAST_RESPONSE_MUST_BE_WITHIN_SECONDS){ log_root_blocked(r,false);return true; }
    cg_sleep_ms(ROOT_LAST_RESPONSE_MUST_BE_WITHIN_SECONDS/N,"");
    LF();
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
//////////////////////
/////    Utils   /////
//////////////////////
/* *** Stat **** */
#define ST_BLKSIZE 4096
static void stat_set_dir(struct stat *s){
  if (s){
    mode_t *m=&(s->st_mode);
    ASSERT(S_IROTH>>2==S_IXOTH);
    if(!(*m&S_IFDIR)){
      s->st_size=ST_BLKSIZE;
      s->st_nlink=1;
      *m=(*m&~S_IFMT)|S_IFDIR|((*m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
    }
  }
}
static void stat_init(struct stat *st, int64_t size,const struct stat *uid_gid){
  const bool isdir=size<0;
  cg_clear_stat(st);
  st->st_mode=isdir?(S_IFDIR|0777):(S_IFREG|0666);
  st->st_nlink=1;
  if (!isdir){
    st->st_size=size;
    st->st_blocks=(size+511)>>9;
  }
  st->st_blksize=ST_BLKSIZE;
  if (uid_gid){
    st->st_gid=uid_gid->st_gid;
    st->st_uid=uid_gid->st_uid;
    st->ST_MTIMESPEC=uid_gid->ST_MTIMESPEC;
  }else{
    // geteuid()  effective user ID of the calling process.
    // getuid()   real user ID of the calling process.
    // getgid()   real group ID of the calling process.
    // getegid()  effective group ID of the calling process.
    st->st_gid=getgid();
    st->st_uid=getuid();
  }
}
/* *** Array *** */
#define Nth(array,i,defaultVal) (array?array[i]:defaultVal)
#define Nth0(array,i) Nth(array,i,0)
/*
  (progn
  (defun W(f)
  (save-excursion
  (beginning-of-buffer)
  (query-replace (concat " " f "(const ")   (concat " PROFILED(" f ")(const "))
  ))
  (W "xmp_getattr")
  (W "xmp_access")
  (W "xmp_open")
  (W "xmp_readdir")
  (W "xmp_lseek")
  (W "xmp_read")
  (W "xmp_release")
  )
*/

////////////////////////////////////////////////////////////////////////////////////////////////
/// The struct zippath is used to identify the real path from the virtual path               ///
/// All strings like virtualpath are stacked in strgs                                        ///
/// strgs is initially on stack and may later go to heap.                                    ///
/// A new one is created with zpath_newstr() and one or several calls to zpath_strncat()     ///
/// The length of the created string is finally obtained with zpath_commit()                 ///
////////////////////////////////////////////////////////////////////////////////////////////////
static int zpath_newstr(struct zippath *zpath){
  assert(zpath!=NULL);
  const int n=(zpath->current_string=++zpath->strgs_l);
  zpath->strgs[n]=0;
  return n;
}
static bool zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=MIN_int(cg_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){ warning(WARN_LEN|WARN_FLAG_MAYBE_EXIT,"zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len); return false;}
    cg_strncpy(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return true;
}
#define zpath_strcat(zpath,s)  zpath_strncat(zpath,s,9999)
static int zpath_commit_hash(const struct zippath *zpath, ht_hash_t *hash){
  const int l=zpath->strgs_l-zpath->current_string;
  if (hash) *hash=hash32(zpath->strgs+zpath->current_string,l);
  return l;
}

#define ZPATH_COMMIT_HASH(zpath,x) zpath->x##_l=zpath_commit_hash(zpath,&zpath->x##_hash)
#define zpath_commit(zpath) zpath_commit_hash(zpath,NULL)
#define zpath_assert_strlen(zpath)  _zpath_assert_strlen(__func__,__FILE_NAME__,__LINE__,zpath)
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath){
  bool e=false;
#define C(a)  if (zpath_strlen(zpath,zpath->a)!=zpath->a##_l && (e=true)) log_error(#a" != "#a"_l   %d!=%d\n",zpath_strlen(zpath,zpath->a),zpath->a##_l);
  C(virtualpath);C(virtualpath_without_entry);C(entry_path);
#undef C
  if (e){
    log_zpath("Error ",zpath);
    log_warn("zpath_assert_strlen in %s at "ANSI_FG_BLUE"%s:%d\n"ANSI_RESET,fn,file,line);
    ASSERT(0);
  }
}

static void zpath_reset_keep_VP(struct zippath *zpath){
#define x_l_hash(x) x=x##_l=x##_hash
  zpath->entry_path=zpath->entry_path_l=x_l_hash(zpath->virtualpath_without_entry)=x_l_hash(zpath->realpath)=0;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_L()=EP_L()=zpath->zipcrc32=0; /* Reset. Later probe a different realpath */
  zpath->root=NULL;
  zpath->strgs_l=zpath->virtualpath+VP_L();
  zpath->flags=(zpath->flags&ZP_STARTS_AUTOGEN);
}
static void zpath_init(struct zippath *zpath, const char *vp){
  assert(zpath!=NULL);
  assert(vp!=NULL);
  memset(zpath,0,SIZEOF_ZIPPATH);
  const int vp_l=strlen(vp);
  const int l=cg_pathlen_ignore_trailing_slash(vp);
  zpath->virtualpath=zpath->strgs_l=1;  /* To distinguish from virtualpath==0  meaning not defined we use 1*/
  zpath->virtualpath_hash=hash_value_strg(vp);
  zpath->virtualpath_l=vp_l;
  const char *NOCSC=cg_strcasestr(vp,"$NOCSC$");/* Windows no-client-side-cache */
  int removed=0;
  if (NOCSC){
    zpath_strncat(zpath,vp,NOCSC-vp);
    zpath_strcat(zpath,NOCSC+(removed=7));
  }else{
    zpath_strcat(zpath,vp);
  }
  zpath->strgs[zpath->virtualpath+(VP_L()=l-removed)]=0;
  zpath_reset_keep_VP(zpath);
  IF1(WITH_AUTOGEN,if (virtualpath_startswith_autogen(vp,vp_l)) zpath->flags|=ZP_STARTS_AUTOGEN);
}
#if WITH_AUTOGEN
static void autogen_zpath_init(struct zippath *zpath,const char *path){
  zpath_init(zpath,path);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath,rootpath(zpath->root=_root_writable));
  zpath_strcat(zpath,path);
  ZPATH_COMMIT_HASH(zpath,realpath);
}
#endif //WITH_AUTOGEN

static bool zpath_stat(struct zippath *zpath,struct rootdata *r){
  if (!zpath) return false;
  if (zpath->stat_rp.st_ino){
  }else{
    const bool success=stat_from_cache_or_syscall_or_async(RP(),&zpath->stat_rp,r);
    if (!success) return false;
    zpath->stat_vp=zpath->stat_rp;
  }
  return true;
}
#define warning_zip(path,e,txt){\
    const int ze=!e?0:zip_error_code_zip(e), se=!e?0:zip_error_code_system(e);\
    char s[1024];*s=0;  if (se) strerror_r(se,s,1023);\
    warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH,path,"%s    sys_err: %s %s zip_err: %s %s",txt,!e?"e is NULL":!se?"":error_symbol(se),s, !ze?"":error_symbol_zip(ze), !ze?"":zip_error_strerror(e));}

static struct zip *zip_open_ro(const char *orig){
  struct zip *zip=NULL;
  if (orig){
    LF();
    if (!cg_endsWithZip(orig,0)){
      IF_LOG_FLAG(LOG_ZIP) log_verbose("zip_open_ro %s ",orig);
      return NULL;
    }
    IF_LOG_FLAG(LOG_ZIP){ static int count; log_verbose("Going to zip_open(%s) #%d ... ",orig,count);cg_print_stacktrace(0); }
    RLOOP(iTry,2){
      int err;
      zip=zip_open(orig,ZIP_RDONLY,&err);
      LOCK(mutex_fhandle,inc_count_getattr(orig,zip?COUNTER_ZIPOPEN_SUCCESS:COUNTER_ZIPOPEN_FAIL));
      if (zip) break;
      warning(WARN_OPEN|WARN_FLAG_ERRNO,orig,"zip_open_ro() err=%d",err);usleep(1000);
      //cg_print_stacktrace(0);
    }
  }
  return zip;
}
///////////////////////////////////
/// Is the virtualpath a zip entry?
///////////////////////////////////
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *shorten, char *append[]){
  IF1(WITH_AUTOGEN,if (virtualpath_startswith_autogen(vp,vp_l)) return 0);
  char *e;
  const char *b=(char*)vp;
  int ret=0;
  for(int i=4;i<=vp_l;i++){
    e=(char*)vp+i;
    if (i==vp_l || *e=='/'){
      if ((ret=config_virtual_dirpath_to_zipfile(b,e,append))!=INT_MAX){
        if (shorten) *shorten=-ret;
        ret+=i;
        break;
      }
      if (*e=='/') b=e+1;
    }
  }
  return ret==INT_MAX?0:ret;
}
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add_file(struct directory,...)
// Returns true on success.
////////////////////////////////////////////////////////////
static bool directory_from_dircache_zip_or_filesystem(struct directory *mydir,const struct stat *rp_stat/*current stat or cached*/){
  const char *rp=mydir->dir_realpath;
  if (!rp || !*rp) return false;
  const bool isZip=mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE;
  //if (isZip && !ENDSWITH(mydir->dir_realpath,mydir->dir_realpath_l,".Zip")) {dde_print("mydir->dir_realpath:%s\n",mydir->dir_realpath);EXIT(9);}
  int result=0; /* Undefined:0 OK:1 Fail:-1 */
#if WITH_DIRCACHE
  const bool doCache=config_advise_cache_directory_listing(rp,mydir->dir_realpath_l,mydir->root->features&ROOT_REMOTE,isZip,rp_stat->ST_MTIMESPEC);
  if (doCache)  LOCK_NCANCEL(mutex_dircache,result=dircache_directory_from_cache(mydir,rp_stat->ST_MTIMESPEC)?1:0);
#endif
  if (!result && (mydir->dir_flags&DIRECTORY_TO_QUEUE)){ /* Read zib dir asynchronously. Initially we see in the listing name.Content. Later we will see single files. */
    LOCK_NCANCEL(mutex_dircachejobs, ht_only_once(&mydir->root->dircache_queue,rp,0));
    result=-1;
  }
  if (!result){ /* Really read from zip */
    result=-1;
    mydir->core.files_l=0;
    if (isZip){
      struct zip *zip=zip_open_ro(rp);
      if (zip){
        const int SB=256,N=PROFILED(zip_get_num_entries)(zip,0);
        struct zip_stat s[SB]; /* reduce num of pthread lock */
        mydir->core.finode=NULL;
        for(int k=0;k<N;){
          int i=0;
          for(;i<SB && k<N;k++) if (!zip_stat_index(zip,k,0,s+i)) i++;
          lock(mutex_dircache);
          FOR(j,0,i){
            const char *n=s[j].name;
            const int nl=strlen(n);
            if (config_do_not_list_file(rp,n,nl)) continue;
            directory_add_file(s[j].comp_method?DIRENT_IS_COMPRESSED:0,mydir,0,n,s[j].size,s[j].mtime,s[j].crc);
            //if (!s[j].crc && *n) if (!ENDSWITH(n,nl,".sqlite-journal") && n[nl-1]!='/') warning(WARN_STAT,rp," s[j].crc is 0 n=%s size=%zu",n,s[j].size);
          }
          unlock(mutex_dircache);
        }
        result=1;
        zip_close(zip);
      }
    }else/*isZip*/{
      DIR *dir=opendir(rp);
      LOCK(mutex_fhandle,inc_count_getattr(rp,dir?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL));
      if (!dir){
        log_errno("%s",rp);
      }else{  /* Really read from file system */
        struct dirent *de;
        mydir->core.fsize=NULL;
        mydir->core.fcrc=NULL;
        while((de=readdir(dir))){
          const char *n=de->d_name;
          if (config_do_not_list_file(rp,n,strlen(n))) continue;
          bool isdir;
#if defined(HAS_DIRENT_D_TYPE) && ! HAS_DIRENT_D_TYPE
          char fpath[PATH_MAX+1]; /* OpenSolaris */
          snprintf(fpath,PATH_MAX,"%s/%s",rp,n);
          isdir=cg_is_dir(fpath);
#else
          isdir=(de->d_type==(S_IFDIR>>12)); /* BSD, Linux, MacOSX */
#endif
          LOCK(mutex_dircache,directory_add_file(isdir?DIRENT_ISDIR: 0,mydir,de->d_ino,n,0,0,0));
        }
        result=1;
      }
      closedir(dir);
    }
    if (result==1){
      mydir->core.mtim=rp_stat->ST_MTIMESPEC;
      config_exclude_files(rp,mydir->dir_realpath_l,mydir->core.files_l, mydir->core.fname,mydir->core.fsize);
      IF1(WITH_DIRCACHE,if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(mydir)));
    }
  }/*!result from cache*/
  return result==1;
}

/* Reading zip dirs asynchroneously */
static void *infloop_dircache(void *arg){
  struct rootdata *r=arg;
  init_infloop(r,PTHREAD_DIRCACHE);
  char path[MAX_PATHLEN+1];
  struct stat stbuf;
  struct directory mydir={0}; /* Put here otherwise use of stack var after ... */
  while(true){
    observe_thread(r,PTHREAD_DIRCACHE);
    *path=0;
    lock_ncancel(mutex_dircachejobs);/*Pick path from an entry and put in stack variable path */
    struct ht_entry *ee=r->dircache_queue.entries;
    RLOOP(i,r->dircache_queue.capacity){
      const char *k=ee[i].key;
      if (k){
        cg_strncpy(path,k,MAX_PATHLEN);
        ht_clear_entry(&r->dircache_queue, ee+i);
        break;
      }}
    unlock_ncancel(mutex_dircachejobs);/*Pick path from an entry and put in stack variable path */
    if (*path && stat_from_cache_or_syscall_or_async(path,&stbuf,r)){
      directory_init(&mydir,DIRECTORY_IS_ZIPARCHIVE,path,strlen(path),r);
      directory_from_dircache_zip_or_filesystem(&mydir,&stbuf);
      directory_destroy(&mydir);
    }
    usleep(1000*10);
  }
}





////////////////////////////////
/// Special Files           ///
//////////////////////////////
#define startsWithDir_ZIPsFS(path)  !strncmp(path,DIR_ZIPsFS,DIR_ZIPsFS_L)
static bool isSpecialFile(const char *slashFn, const char *path,const int path_l){
  return slashFn && *slashFn &&
    path && startsWithDir_ZIPsFS(path) &&
    path_l-DIR_ZIPsFS_L==strlen(slashFn) &&
    !strcmp(path+DIR_ZIPsFS_L,slashFn);
}
static bool find_realpath_special_file(struct zippath *zpath){
  FOR(i,0,2){
    if (isSpecialFile(SPECIAL_FILES[i+SFILE_LOG_WARNINGS],VP(),VP_L())){
      zpath->realpath=zpath_newstr(zpath);
      zpath_strcat(zpath,_fWarningPath[i]);
      ZPATH_COMMIT_HASH(zpath,realpath);
      if (zpath_stat(zpath,NULL)){
        zpath->stat_vp.st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
        return true;
      }
    }
  }
  zpath_reset_keep_VP(zpath);
  return false;
}
static int whatSpecialFile(const char *vp,const int vp_l){
  if (startsWithDir_ZIPsFS(vp)){
    FOR(i,0,SFILE_L)  if (isSpecialFile(SPECIAL_FILES[i],vp,vp_l)) return i;
    if (!vp[DIR_ZIPsFS_L]) return SFILE_L;
  }
  return -1;
}


/////////////////////////////////////////////////////////////////////
//
// The following functions are used to search for a real path
// for a given virtual path,
//
////////////////////////////////////////////////////////////
/* Returns true on success */
static bool _test_realpath(struct zippath *zpath, struct rootdata *r){
  assert(r!=NULL);
  LF();
  //zpath_assert_strlen(zpath);
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  zpath->root=r;
  {
    const char *vp0=VP0_L()?VP0():VP(); /* Virtual path without zip entry */
    if (!zpath_strcat(zpath,rootpath(r)) || strcmp(vp0,"/") && !zpath_strcat(zpath,vp0)) return false;
    ZPATH_COMMIT_HASH(zpath,realpath);
  }
  if (!zpath_stat(zpath,r)) return false;
  if (ZPATH_IS_ZIP()){
    if (!cg_endsWithZip(RP(),0)){
      IF_LOG_FLAG(LOG_REALPATH) log_verbose("!cg_endsWithZip rp: %s\n",RP());
      return false;
    }
    if (EP_L() && filler_readdir_zip(0,zpath,NULL,NULL,NULL)) {
      return false; /* This sets the file attribute of zip entry  */
    }
  }
  return true;
}
static bool test_realpath(struct zippath *zpath, struct rootdata *r){
  if (!_test_realpath(zpath,r)){ zpath_reset_keep_VP(zpath); return false;}
  return true;
}
/* Uses different approaches and calls test_realpath */
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r){
  LF();
  //if (zpath->flags&ZP_STARTS_AUTOGEN) return false;
  zpath->entry_path=zpath_newstr(zpath);
  zpath->flags|=ZP_ZIP;
  if (!zpath_strcat(zpath,vp+cg_last_slash(vp)+1)) return false;
  EP_L()=zpath_commit(zpath);
  const bool ok=test_realpath(zpath,r);
  IF_LOG_FLAG(LOG_ZIP_INLINE) if (cg_is_regular_file(RP()))    log_exited_function("rp: %s vp: %s ep: %s ok: %s",RP(),vp,EP(),yes_no(ok));
  return ok;
}
static bool find_realpath_nocache(struct zippath *zpath,struct rootdata *r){
  const char *vp=VP(); /* At this point, Only zpath->virtualpath is defined. */
  char *append="";
  {
    if (!wait_for_root_timeout(r)){
      return false;
    }
    if (VP_L()){ /* ZIP-file is a folder (usually ZIP-file.Content in the virtual file system. */
      int shorten=0;
      const int zip_l=virtual_dirpath_to_zipfile(vp,VP_L(),&shorten,&append);
      if (zip_l){ /* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name.  */
        zpath->virtualpath_without_entry=zpath_newstr(zpath);
        if (!zpath_strncat(zpath,vp,zip_l) || !zpath_strcat(zpath,append)) return false;
        ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
        zpath->entry_path=zpath_newstr(zpath);
        {
          const int pos=VP0_L()+shorten+1-cg_strlen(append);
          if (pos<VP_L()) zpath_strcat(zpath,vp+pos);
        }
        EP_L()=zpath_commit(zpath);
        zpath->flags|=ZP_ZIP;
        zpath_assert_strlen(zpath);
        const bool t=test_realpath(zpath,r);
        if (t){
          if (!EP_L()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
          return true;
        }
      }
#if WITH_ZIPINLINE
      for(int rule=0;;rule++){ /* ZIP-entries inlined. ZIP-file itself not shown in listing. Occurs for Sciex mass-spec */
        const int len=config_containing_zipfile_of_virtual_file(rule,vp,VP_L(),&append);
        if (!len) break;
        zpath_reset_keep_VP(zpath);
        zpath->virtualpath_without_entry=zpath_newstr(zpath);
        if (!zpath_strncat(zpath,vp,len) || !zpath_strcat(zpath,append)) return false;
        ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
        const bool ok=find_realpath_try_inline(zpath,vp,r);
        if (ok) return true;
      }
#endif //WITH_ZIPINLINE
    }
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  const bool t=test_realpath(zpath,r);
  return t;
} /*find_realpath_nocache*/


static bool find_realpath_any_root1(struct zippath *zpath,const long which_roots){
  const char *vp=VP();
  const int vp_l=VP_L();
  zpath_reset_keep_VP(zpath);
  bool debug=ENDSWITH(vp,vp_l,"wiff");
  if (vp_l){
#define R() foreach_root1(r) if (which_roots&(1<<rootindex(r)))
#if WITH_ZIPINLINE_CACHE
    R(){
      char zip[MAX_PATHLEN+1]; *zip=0;
      LOCK(mutex_dircache,const char *z=zinline_cache_vpath_to_zippath(vp,vp_l); if (z) strcpy(zip,z));
      if (*zip && !strncmp(zip,r->rootpath,r->rootpath_l) && zip[r->rootpath_l]=='/'  && wait_for_root_timeout(r)){
        zpath_reset_keep_VP(zpath);
        zpath->virtualpath_without_entry=zpath_newstr(zpath);
        if (!zpath_strcat(zpath,zip+r->rootpath_l)) return false;
        ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
        if (find_realpath_try_inline(zpath,vp,r)) return true;
      }
    }
#endif //WITH_ZIPINLINE_CACHE
  }
  R() if (find_realpath_nocache(zpath,r)) return true;
  //  //if (!onlyThisRoot && !config_not_report_stat_error(vp)) warning(WARN_GETATTR|WARN_FLAG_ONCE_PER_PATH,vp,"fail");
  return false;
#undef R
}/*find_realpath_any_root1*/
#if WITH_EXTRA_ASSERT
static bool debug_trigger_vp(const char *vp,const int vp_l){
  return  !strcmp("/PRO2/Data/50-0139",vp) ||
    !strcmp("/PRO2/Data",vp) ||
    !strcmp("/PRO2",vp) ||
    ENDSWITH(vp,vp_l,".d") ||
    ENDSWITH(vp,vp_l,".tdf") ||
    ENDSWITH(vp,vp_l,".tdf_bin");
}
#else
#define debug_trigger_vp(...) false
#endif

static long search_file_which_roots(const char *vp,const int vp_l,const bool path_starts_with_autogen){
#if WITH_AUTOGEN
  if (path_starts_with_autogen){
    INIT_STRUCT_AUTOGEN_FILES(ff,vp,vp_l-(ENDSWITH(vp,vp_l,".log")?4:0));
    if (autogen_realinfiles(&ff)) return 1;
  }
#endif //WITH_AUTOGEN
  return config_search_file_which_roots(vp,vp_l);
}
static bool find_realpath_any_root(const int opt,struct zippath *zpath,const struct rootdata *onlyThisRoot){
  const char *vp=VP();
  const int vp_l=VP_L();
  const bool path_starts_autogen=false IF1(WITH_AUTOGEN, || (0!=(zpath->flags&ZP_STARTS_AUTOGEN)));
  if (!onlyThisRoot && find_realpath_special_file(zpath)) return true;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,const int trans=transient_cache_find_realpath(opt,zpath,vp,vp_l); if (trans) return trans==1);
  const int virtualpath=zpath->virtualpath;
  bool found=false; /* which_roots==01 means only Zero-th root */
  FOR(cut01,0,path_starts_autogen?2:1){
    if (0!=(opt&(cut01?FINDRP_AUTOGEN_CUT_NOT:FINDRP_AUTOGEN_CUT))) continue;
    const long roots=(onlyThisRoot?(1<<rootindex(onlyThisRoot)):-1) & ((path_starts_autogen&&!cut01)?01:search_file_which_roots(vp,vp_l,path_starts_autogen));
    if (!roots && !cut01) continue;
    IF1(WITH_AUTOGEN, if(cut01){ zpath->virtualpath=virtualpath+DIR_AUTOGEN_L; zpath->virtualpath_l=vp_l-DIR_AUTOGEN_L;});
    int sleep_ms=0;
    FOR(i,0,config_num_retries_getattr(vp,vp_l,&sleep_ms)){
      if ((found=find_realpath_any_root1(zpath, roots))){
        if (i) warning(WARN_RETRY,vp,"find_realpath_any_root succeeded on retry %d",i);
        goto found;
      }
      cg_sleep_ms(sleep_ms,"");
    }
  }
  if (found) assert(zpath->realpath!=0);
 found:
  zpath->virtualpath=virtualpath;/*Restore*/
  zpath->virtualpath_l=vp_l;
  {
#if WITH_TRANSIENT_ZIPENTRY_CACHES
    lock(mutex_fhandle);
    struct zippath *zp=transient_cache_get_or_create_zpath(true,vp,vp_l);
    if (zp){
      if (found) *zp=*zpath;
      else if (!onlyThisRoot) zp->flags|=ZP_DOES_NOT_EXIST; /* Not found in any root. */
    }
    unlock(mutex_fhandle);
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
  }
  if (!found && !path_starts_autogen && !onlyThisRoot && !config_not_report_stat_error(vp,vp_l)){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,vp,"found is false   root: %s",report_rootpath(onlyThisRoot));
  }
  return found;
} /*find_realpath_any_root*/

///////////////////////////////////////////////////////////
// Data associated with file handle.
// Motivation: When the same file is accessed from two different programs,
// We see different fi->fh
// Wee use this as a key to obtain a data structure "fhandle"
//
// Conversely, fuse_get_context()->private_data returns always the same pointer address even for different file handles.
//
static const struct fhandle FHANDLE_EMPTY={0};

static bool fhandle_can_destroy(struct fhandle *d){
  ASSERT_LOCKED_FHANDLE();
  if (!d || d->is_busy>0) return false;
#if WITH_MEMCACHE
  const struct memcache *m=d->memcache;
  if (m){ /* Not close when serving as file-content-cache for other fhandle instances  with identical path */
    IF1(WITH_MEMCACHE,if (m->memcache_status==memcache_reading||d->is_memcache_store>0) return false);
    const ht_hash_t hash=D_VP_HASH(d);
    foreach_fhandle(id,e){
      if (e==d || (e->flags&FHANDLE_FLAG_DESTROY_LATER)) continue;
      //if (!m->memcache_status && /* Otherwise two with same path, each having a cache could stick for ever */
      if (D_VP_HASH(e)==hash && !strcmp(D_VP(d),D_VP(e))) return false;
    }
  }
#endif //WITH_MEMCACHE
  return true;
}
////////////////////////////////////////////////////////////
/// path can be NULL and is to evict from page cache     ///
////////////////////////////////////////////////////////////
#define fhandle_destroy_and_evict_from_pagecache(fh,path,path_l) _fhandle_destroy(fhandle_get(path,fh),fh,path,path_l);
static void _fhandle_destroy(struct fhandle *d,const int fd_evict,const char *path_evict,const int path_evict_l){
  LF();
  ASSERT_LOCKED_FHANDLE();
  IF1(WITH_EVICT_FROM_PAGECACHE,bool do_evict=path_evict?config_advise_evict_from_filecache(path_evict,path_evict_l, d?D_EP(d):NULL,d?D_EP_L(d):0):false);
  if (d){
    if (!fhandle_can_destroy(d)){ /* When reading to RAM cache */
      d->flags|=FHANDLE_FLAG_DESTROY_LATER;
      IF1(WITH_EVICT_FROM_PAGECACHE,do_evict=false);
    }else{
      IF1(WITH_AUTOGEN,const uint64_t fhr=d->fh_real;d->fh_real=0;if (fhr) close(fhr));
#if WITH_TRANSIENT_ZIPENTRY_CACHES
      if (d->ht_transient_cache){
        foreach_fhandle(ie,e) if (d->ht_transient_cache==e->ht_transient_cache) goto keep_transient_cache;
        ht_destroy(d->ht_transient_cache);
        cg_free(MALLOC_fhandle,d->ht_transient_cache);
        d->ht_transient_cache=0;
      }
    keep_transient_cache:
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
      IF1(WITH_MEMCACHE,memcache_free(d));
      IF1(WITH_EVICT_FROM_PAGECACHE,if (do_evict){const ht_hash_t hash=D_RP_HASH(d); foreach_fhandle(ie,e) if (d!=e && D_RP_HASH(e)==hash){ do_evict=false;break;}});
      fhandle_zip_close(d);
      if (d->zarchive && zip_close(d->zarchive)==-1) warning_zip(D_RP(d),zip_get_error(d->zarchive),"Failed zip_close");
      pthread_mutex_destroy(&d->mutex_read);
      IF1(WITH_AUTOGEN, aimpl_maybe_reset_atime_in_future(d));
      memset(d,0,SIZEOF_FHANDLE);
    }
  }
  IF1(WITH_EVICT_FROM_PAGECACHE,if (do_evict){maybe_evict_from_filecache(fd_evict,path_evict,path_evict_l); IF_LOG_FLAG(LOG_EVICT_FROM_CACHE) log_verbose(ANSI_MAGENTA"Evicted %s"ANSI_RESET,path_evict);});
}
static void fhandle_destroy(struct fhandle *d){
  if (d) _fhandle_destroy(d,0,D_RP(d),D_RP_L(d));
}
static void fhandle_destroy_those_that_are_marked(void){
  ASSERT_LOCKED_FHANDLE();
  foreach_fhandle(id,d){
    if (d->flags&FHANDLE_FLAG_DESTROY_LATER  && fhandle_can_destroy(d)) fhandle_destroy(d);
  }

}

static void fhandle_init(struct fhandle *d, struct zippath *zpath){
  memset(d,0,SIZEOF_FHANDLE);
  pthread_mutex_init(&d->mutex_read,NULL);
  const char *path=VP();
  d->zpath=*zpath;
  d->filetypedata=filetypedata_for_ext(path,d->zpath.root);
  d->flags|=FHANDLE_FLAG_ACTIVE; /* Important; This must be the last assignment */
}
static struct fhandle* fhandle_create(const uint64_t fh, struct zippath *zpath){
  ASSERT_LOCKED_FHANDLE();
  struct fhandle *d=NULL;
  { foreach_fhandle_also_emty(ie,e) if (!e->flags){ d=e; break;}} /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhandle_n>=FHANDLE_MAX){ warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_ERROR,VP(),"Excceeding FHANDLE_MAX");return NULL;}
    d=fhandle_at_index(_fhandle_n++);
  }
  fhandle_init(d,zpath);
  d->fh=fh;
#if WITH_TRANSIENT_ZIPENTRY_CACHES
  if ((d->zpath.flags&ZP_ZIP) && config_advise_transient_cache_for_zipentries(VP(),VP_L())){
    d->flags|=FHANDLE_FLAG_WITH_TRANSIENT_ZIPENTRY_CACHES;
    foreach_fhandle(ie,e){
      if (d!=e && FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(d,e) && NULL!=(d->ht_transient_cache=e->ht_transient_cache)) break;
    }
  }
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
#if WITH_MEMCACHE
  const ht_hash_t hash=D_VP_HASH(d);
  { foreach_fhandle(ie,e) if (d!=e && hash==D_VP_HASH(e) && !strcmp(D_VP(d),D_VP(e))) d->memcache=e->memcache; }
#endif //WITH_MEMCACHE
  d->flags|=FHANDLE_FLAG_ACTIVE;
  return d;
}
static struct fhandle* fhandle_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHANDLE();
  //log_msg(ANSI_FG_GRAY" fhandle %d  %lu\n"ANSI_RESET,op,fh);
  fhandle_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(path);
  foreach_fhandle(id,d){
    if (fh==d->fh && D_VP_HASH(d)==h && !strcmp(path,D_VP(d))) return d;
  }
  return NULL;
}
///////////////////////////////////
/// Auto-Generated files             ///
///////////////////////////////////
/* ******************************************************************************** */
#define FILLDIR_AUTOGEN (1<<0)
#define FILLDIR_IS_DIR_ZIPsFS (1<<1)
static void filldir(const int opt,fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
#if WITH_AUTOGEN
  if (_realpath_autogen && 0!=(opt&FILLDIR_AUTOGEN)){
    autogen_filldir(filler,buf,name,stbuf,no_dups);
    return;
  }
#endif// WITH_AUTOGEN
  IF1(WITH_AUTOGEN,IF1(WITH_AUTOGEN_DIR_HIDDEN, if (0==(opt&FILLDIR_IS_DIR_ZIPsFS) || strcmp(DIRNAME_AUTOGEN,name))))
    if (ht_only_once(no_dups,name,0))    filler(buf,name,stbuf,0 COMMA_FILL_DIR_PLUS);
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t next_inode(void){
  static ino_t seq=1UL<<63;
  return seq++;
}
static ino_t make_inode(const ino_t inode0,struct rootdata *r, const int entryIdx,const char *path){
  const static int SHIFT_ROOT=40,SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");
  if (inode0<(1L<<SHIFT_ROOT) && entryIdx<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| ((rootindex(r)+1L)<<SHIFT_ROOT);
  }else{
    struct ht *ht=&r->ht_inodes;
    const uint64_t key1=inode0+1, key2=entryIdx;
    //const uint64_t key2=(entryIdx<<1)|1; /* Because of the implementation of hash table ht, key2 must not be zero. We multiply by 2 and add 1. */
    LOCK_NCANCEL_N(mutex_inode,
                   if (!ht->capacity) ht_set_id(HT_MALLOC_inodes,ht_init(ht,"inode",HT_FLAG_NUMKEY|16));
                   ino_t inod=(ino_t)ht_numkey_get(ht,key1,key2);
                   if (!inod){ ht_numkey_set(ht,key1,key2,(void*)(inod=next_inode()));_count_SeqInode++;});
    return inod;
  }
}

static ino_t inode_from_virtualpath(const char *vp,const int vp_l){
  static struct ht_entry *e;
  LOCK_NCANCEL(mutex_inode, e=ht_get_entry(&ht_inodes_vp,vp,vp_l,0,true));
  if (!e->value) e->value=(void*)next_inode();
  return (ino_t)e->value;
}
/* ******************************************************************************** */
/* *** Zip *** */
#define SET_STAT() stat_init(st,isdir?-1: Nth0(d.fsize,i),&zpath->stat_rp);  st->st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(d.finode,i,i),rp); zpath->zipcrc32=Nth0(d.fcrc,i);\
  if (Nth0(d.fflags,i)&DIRENT_IS_COMPRESSED) zpath->flags|=ZP_IS_COMPRESSED;\
  st->st_mtime=Nth0(d.fmtime,i)
static int filler_readdir_zip(const int opt,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  const char *rp=RP();
  const int ep_l=EP_L();
  struct rootdata *r=zpath->root;
  if(!ep_l && !filler_maybe_null) return 0; /* When the virtual path is a Zip file then just report success */
  if (!zpath_stat(zpath,r)) return ENOENT;
  directory_new(mydir,DIRECTORY_IS_ZIPARCHIVE,rp,RP_L(),r);
  if (!directory_from_dircache_zip_or_filesystem(&mydir,&zpath->stat_rp)) return ENOENT;
  char u[MAX_PATHLEN+1];
  struct directory_core d=mydir.core;
  FOR(i,0,d.files_l){
    if (cg_empty_dot_dotdot(d.fname[i]) || !unsimplify_fname(strcpy(u,d.fname[i]),rp)) continue;
    int len=cg_pathlen_ignore_trailing_slash(u);
    if (len>=MAX_PATHLEN){ warning(WARN_STR|WARN_FLAG_ERROR,u,"Exceed MAX_PATHLEN"); continue;}
    int isdir=(Nth0(d.fflags,i)&DIRENT_ISDIR)!=0, not_at_the_first_pass=0;
    while(len){
      if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
        const int slash=cg_last_slash(u);
        if (slash<0) break;
        u[slash]=0;
        isdir=1;
      }
      if (!(len=cg_strlen(u))) break;
      /* For ZIP files, readdir_iterate gives all zip entries. They contain slashes. */
      if (filler_maybe_null){
        if (ep_l &&  /* The parent folder is not just a ZIP file but a ZIP file with a zip entry stored in zpath->entry_path */
            (len<=ep_l || strncmp(EP(),u,ep_l) || u[ep_l]!='/')) continue; /* u must start with  zpath->entry_path */
        const char *n0=(char*)u+(ep_l?ep_l+1:0); /* Remove zpath->entry_path from the left of u. Note that zpath->entry_path has no trailing slash. */
        if (strchr(n0,'/') || ht_sget(no_dups,n0)) continue;
        struct stat stbuf, *st=&stbuf;
        SET_STAT();
        assert_validchars(VALIDCHARS_FILE,n0,strlen(n0),"n0");
        filldir(opt,filler_maybe_null,buf,n0,st,no_dups);
      }else if (ep_l==len && !strncmp(EP(),u,len)){ /* ---  filler_readdir_zip() has been called from test_realpath(). The goal is to set zpath->stat_vp --- */
        struct stat *st=&zpath->stat_vp;
        SET_STAT();
        return 0;
      }
    }/* while len */
  }
  return filler_maybe_null?0:ENOENT;
}/*filler_readdir_zip*/
#undef SET_STAT
static int filler_readdir(const int opt,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  if (!rp || !*rp) return 0;
  if (ZPATH_IS_ZIP()) return filler_readdir_zip(opt,zpath,buf,filler,no_dups);
  struct rootdata *r=zpath->root; ASSERT(r!=NULL);
  struct stat stbuf;
  if (zpath->stat_rp.st_mtime){
    struct stat st;
    char direct_rp[MAX_PATHLEN+1], virtual_name[MAX_PATHLEN+1];
    directory_new(dir,0,rp,RP_L(),r);
    if (directory_from_dircache_zip_or_filesystem(&dir,&zpath->stat_rp)){
      char u[MAX_PATHLEN+1],u2[MAX_PATHLEN+1]; /*buffers for unsimplify_fname() */
      const struct directory_core d=dir.core;
      FOR(i,0,d.files_l){
        if (cg_empty_dot_dotdot(d.fname[i])) continue;
        if (!unsimplify_fname(strcpy(u,d.fname[i]),rp)) continue;
        const int u_l=strlen(u);
        if (0==(opt&FILLDIR_AUTOGEN) && no_dups && ht_get(no_dups,u,u_l,0)) continue;
        bool inlined=false; /* Entries of ZIP file appear directory in the parent of the ZIP-file. */
#if WITH_ZIPINLINE
        if (config_skip_zipfile_show_zipentries_instead(u,u_l) && (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,u))){
          const int direct_rp_l=strlen(direct_rp);
          directory_new(dir2,DIRECTORY_IS_ZIPARCHIVE|DIRECTORY_TO_QUEUE,direct_rp,direct_rp_l,r);
          if (stat_from_cache_or_syscall_or_async(direct_rp,&stbuf,r) && directory_from_dircache_zip_or_filesystem(&dir2,&stbuf)){
            FOR(j,0,dir2.core.files_l){/* Embedded zip file */
              const char *n2=dir2.core.fname[j];
              if (n2 && !strchr(n2,'/')){
                stat_init(&st,(Nth0(dir2.core.fflags,j)&DIRENT_ISDIR)?-1:Nth0(dir2.core.fsize,j),&zpath->stat_rp);
                st.st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(dir2.core.finode,j,j),RP());
                if (unsimplify_fname(strcpy(u2,n2),direct_rp) && config_containing_zipfile_of_virtual_file(0,u2,strlen(u2),NULL)) filldir(opt,filler,buf,u2,&st,no_dups);
              }
            }
            inlined=true;
          }
          directory_destroy(&dir2);
        }
#endif
        if (!inlined){
          stat_init(&st,(Nth0(d.fflags,i)&DIRENT_ISDIR)?-1:Nth0(d.fsize,i),NULL);
          st.st_ino=make_inode(zpath->stat_rp.st_ino,r,0,RP());
          if (!config_do_not_list_file(rp,u,u_l)){
            IF1(WITH_ZIPINLINE,if (config_also_show_zipfile_in_listing(u,u_l))) filler(buf,u,&st,0  COMMA_FILL_DIR_PLUS);
            config_zipfilename_to_virtual_dirname(virtual_name,u,u_l);
            // if (strlen(virtual_name)!=u_l || cg_endsWithZip(u,u_l))
            st.st_mode=(st.st_mode&~S_IFMT)|S_IFDIR; /* ZIP files as  directory */
            filldir(opt,filler,buf,virtual_name,&st,no_dups);
          }
        }
      }
    }
    directory_destroy(&dir);
  }
  return 0;
}



static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;} // cppcheck-suppress [constParameterCallback]
static int xmp_statfs(const char *path, struct statvfs *stbuf){ return minus_val_or_errno(statvfs(_root->rootpath,stbuf));}
/************************************************************************************************/
static int has_sufficient_storage_space(const char *path){
  const int slash=cg_last_slash(path);
  if (slash<=0) return EINVAL;
  if (!cg_recursive_mk_parentdir(path)){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,path,"failed cg_recursive_mk_parentdir");
    return EPERM;
  }
  char parent[PATH_MAX+1]; cg_stpncpy0(parent,path,slash);
  struct statvfs st;
  if (statvfs(parent,&st)){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,parent,"Going return EIO");
    return EIO;
  }
  const long free=st.f_frsize*st.f_bavail, total=st.f_frsize*st.f_blocks;
  if (config_has_sufficient_storage_space(path,free,total)) return 0;
  warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH,parent,"%s: Available: %'ld GB  Total: %'ld GB",strerror(ENOSPC),free>>30,total>>30);
  return ENOSPC;
}

/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
static int realpath_mk_parent(char *realpath,const char *path){
  if (!_root_writable) return EACCES;/* Only first root is writable */
  const int path_l=strlen(path), slash=cg_last_slash(path);
  if (config_not_overwrite(path,path_l)){
    bool found;FIND_REALPATH(path);
    if (found && zpath->root>0){ warning(WARN_OPEN|WARN_FLAG_ONCE,RP(),"It is only allowed to overwrite files in root 0");return EACCES;}
  }
  assert(_root_writable->rootpath_l+path_l<MAX_PATHLEN);
  if (slash>0){
    char parent[MAX_PATHLEN+1];
    cg_strncpy(parent,path,slash);
    bool found;FIND_REALPATH(parent);
    if (!found) return ENOENT;
    strcpy(stpcpy(realpath,_root_writable->rootpath),parent);
    const int res=has_sufficient_storage_space(realpath);
    if (res) return res;
  }
  strcpy(stpcpy(realpath,_root_writable->rootpath),path);
  return 0;
}
/********************************************************************************/
// FUSE FUSE 3.0.0rc3 The high-level init() handler now receives an additional struct fuse_config pointer that can be used to adjust high-level API specific configuration options.
#define DO_LIBFUSE_CACHE_STAT 0
#define EVAL(a) a
#define EVAL2(a) EVAL(a)
void *xmp_init(struct fuse_conn_info *conn IF1(WITH_FUSE_3,,struct fuse_config *cfg)){ // cppcheck-suppress staticFunction
  //void *x=fuse_apply_conn_info_opts;  //cfg-async_read=1;
#if WITH_FUSE_3
  cfg->use_ino=1;
  IF1(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
#endif
  return NULL;
}


/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted
// Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now all receive an additional struct fuse_file_info pointer (which, however, may be NULL even if the file is currently open).
#if FUSE_MAJOR_V>=2 && FUSE_MINOR_V>9
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null){ /* NOT_TO_GENERATED_HEADER */
  LOG_FUSE(path);
  const int res=_xmp_getattr(path,stbuf,fi_or_null);
  LOG_FUSE_RES(path,res);
  return res;
}
#else
static int xmp_getattr(const char *path, struct stat *stbuf){ /* NOT_TO_GENERATED_HEADER */
  const int res=_xmp_getattr(path,stbuf,NULL);
  LOG_FUSE_RES(path,res);
  return res;
}
#endif
static int PROFILED(_xmp_getattr)(const char *path, struct stat *stbuf, void *fi_or_null){
  const int path_l=strlen(path);
  if (PATH_IS_FILE_INFO(path,path_l)){ stat_init(stbuf,PATH_MAX,0); return 0; }
  int err=0;
  if (trigger_files(false,path,path_l)){
    stat_init(stbuf,0,NULL);
    return 0;
  }
  if (path_l==DIR_ZIPsFS_L && !strcmp(path,DIR_ZIPsFS)){
    stat_init(stbuf,0,NULL);
    stat_set_dir(stbuf);
    stbuf->st_ino=inode_from_virtualpath(path,path_l);
    time(&stbuf->st_mtime);
    return 0;
  }
  {
    const int i=whatSpecialFile(path,path_l);
    if (i>=SFILE_BEGIN_VIRTUAL){
      struct textbuffer b={0};special_file_content(&b,i);
      const int b_l=textbuffer_length(&b);
      stat_init(stbuf,b_l?b_l:i==SFILE_INFO?(_info_capacity?_info_capacity:100000):0,NULL);
      time(&stbuf->st_mtime);
      stbuf->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
      stbuf->st_ino=inode_from_virtualpath(path,path_l);
      if (ENDSWITH(path,path_l,".command")) stbuf->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);
      return 0;
    }
  }
  bool found;FIND_REALPATH(path);
  if (found){
    *stbuf=zpath->stat_vp;
  }else{
#if WITH_AUTOGEN
    const long size=autogen_estimate_filesize(path,path_l);
    if(_realpath_autogen && size>0){
      stat_init(stbuf,size,&zpath->stat_rp);
      stbuf->st_ino=inode_from_virtualpath(path,path_l);
      return 0;
    }
#endif //WITH_AUTOGEN
    err=ENOENT;
  }
  LOCK(mutex_fhandle,inc_count_getattr(path,err?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS));
  IF1(DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (err) debug_track_false_getattr_errors(path,path_l));
  if (config_file_is_readonly(path,path_l)) stbuf->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  return -err;
}
static int PROFILED(xmp_access)(const char *path, int mask){
  LOG_FUSE(path);
  const int path_l=strlen(path);
  if (whatSpecialFile(path,path_l)>=SFILE_BEGIN_VIRTUAL) return 0;
  NEW_ZIPPATH(path);
  int res=find_realpath_any_root(0,zpath,NULL)?0:ENOENT;
  assert(VP()[0]=='/'||VP()[0]==0);
  if (res==-1) res=ENOENT;
  if (!res && (mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) res=access(RP(),(mask&~X_OK)|R_OK);
  if (res) report_failure_for_tdf(path);
  LOCK(mutex_fhandle,inc_count_getattr(path,res?COUNTER_ACCESS_FAIL:COUNTER_ACCESS_SUCCESS));
  if (debug_trigger_vp(path,path_l)) log_verbose("%s returning %d",path,minus_val_or_errno(res));
  return minus_val_or_errno(res);
}
static int xmp_utimens(const char *path, const struct timespec ts[2],struct fuse_file_info *fi){
  (void) fi;    /* don't use utime/utimes since they follow symlinks */
  bool found; FIND_REALPATH(path);
  if (!found) return -ENOENT;
  if (zpath->root!=_root_writable) return -EPERM;
  const int res=utimensat(0,RP(),ts,AT_SYMLINK_NOFOLLOW);
  return res==-1?-errno:0;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  LOG_FUSE(path);
  bool found;FIND_REALPATH(path);
  LOG_FUSE_RES(path,found);
  if (!found) return -ENOENT;
  const int n=readlink(RP(),buf,size-1);
  return n==-1?-errno: (buf[n]=0);
}
static int xmp_unlink(const char *path){
  LOG_FUSE(path);
  bool found;FIND_REALPATH(path);
  LOG_FUSE_RES(path,found);
  return !found?-ENOENT: !ZPATH_ROOT_WRITABLE()?-EACCES:  minus_val_or_errno(unlink(RP()));
}
static int xmp_rmdir(const char *path){
  LOG_FUSE(path);
  bool found;FIND_REALPATH(path);
  LOG_FUSE_RES(path,found);
  return !ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(rmdir(RP()));
}
#define LOG2_FD_ZIP_MIN 20
#define FD_ZIP_MIN (1<<LOG2_FD_ZIP_MIN)
//#define FHANDLE_FLAG_CHANGED_TO_WRITE (1<<(LOG2_FD_ZIP_MIN-1))

static uint64_t next_fh(){
  static uint64_t next_fh=FD_ZIP_MIN;
  if (++next_fh==0xFFffFFffFFffFFffL) next_fh=FD_ZIP_MIN;
  return next_fh;
}


static int xmp_open(const char *path, struct fuse_file_info *fi){
  LOG_FUSE(path);
  ASSERT(fi!=NULL);
  const int path_l=strlen(path);
  {
    const int i=whatSpecialFile(path,path_l);
    if (i==SFILE_INFO) LOCK(mutex_special_file,++_info_count_open;make_info(MAKE_INFO_HTML|MAKE_INFO_ALL));
    if (i>=SFILE_BEGIN_VIRTUAL) return 0;
  }
  if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))) return create_or_open(path,0775,fi);
  bool found;FIND_REALPATH(path);
  int64_t handle=0;
  IF1(WITH_MEMCACHE, if (!found)  handle=fhandle_set_filesource_info(zpath));
  if (handle)  return (fi->fh=handle)>0?0:ENOENT;
  IF1(WITH_AUTOGEN,  if (found && _realpath_autogen && (zpath->flags&ZP_STARTS_AUTOGEN) && autogen_remove_if_not_up_to_date(zpath)) found=false);
  if (found){
    if (ZPATH_IS_ZIP()){
      while(zpath){
        LOCK(mutex_fhandle,if (fhandle_create(handle=next_fh(),zpath)) zpath=NULL); /* zpath is now stored in fhandle */
        if (zpath) log_verbose("Going to sleep and retry fhandle_create %s ...",path);usleep(1000*1000);
      }
    }else{
      const int64_t fh_real=handle=open(RP(),fi->flags);
      if (handle<=0) warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"open:  fh=%d",handle);
      if (has_proc_fs() && !cg_check_path_for_fd("my_open_fh",RP(),handle)) handle=-1;
      if (handle>0){ IF1(WITH_AUTOGEN,LOCK(mutex_fhandle,if (zpath->flags&ZP_STARTS_AUTOGEN) fhandle_create(handle=next_fh(),zpath)->fh_real=fh_real)); }
    }
  }else{
#if WITH_AUTOGEN
    INIT_STRUCT_AUTOGEN_FILES(ff,path,path_l);
    if (_realpath_autogen && (zpath->flags&ZP_STARTS_AUTOGEN) && autogen_realinfiles(&ff)>=0){
      LOCK(mutex_fhandle, autogen_zpath_init(zpath,path); fhandle_create(handle=next_fh(),zpath)->flags|=FHANDLE_FLAG_IS_AUTOGEN);
      zpath=NULL;
      found=true;
    }
#endif //WITH_AUTOGEN
    if (!found && report_failure_for_tdf(path)){
      log_zpath("Failed ",zpath);
      warning(WARN_OPEN|WARN_FLAG_MAYBE_EXIT,path,"FIND_REALPATH failed");
    }
  }
  if (!found || handle==-1){
    if (!config_not_report_stat_error(path,path_l)) warning(WARN_GETATTR,path,"found:%s handle:%llu",success_or_fail(found),handle);
    return !found?-ENOENT:-errno;
  }
  fi->fh=handle;
  return 0;
}/*xmp_open*/


static int xmp_truncate(const char *path, off_t size IF1(WITH_FUSE_3,,struct fuse_file_info *fi)){
  LOG_FUSE(path);
  int res;
  IF1(WITH_FUSE_3,if (fi)    res=ftruncate(fi->fh,size); else)
    {
      bool found;FIND_REALPATH(path);
      res=!ZPATH_ROOT_WRITABLE()?EACCES: found?truncate(RP(),size):ENOENT;
    }
  return minus_val_or_errno(res);
}
/////////////////////////////////
// Readdir
/////////////////////////////////
/** unsigned int cache_readdir:1; FOPEN_CACHE_DIR Can be filled in by opendir. It signals the kernel to  enable caching of entries returned by readdir(). */
#if FUSE_MAJOR_V>=3 && FUSE_MINOR_V>5 /* FUSE 3.5 Added a new cache_readdir flag to fuse_file_info to enable caching of readdir results. */
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  LOG_FUSE(path);
  const int res=_xmp_readdir(path,buf,filler,offset,fi);
  LOG_FUSE_RES(path,res);
  return res;
}
#else
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){ return _xmp_readdir(path,buf,filler,offset,fi);}
#endif

static int PROFILED(_xmp_readdir)(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
  (void)offset;(void)fi;
  struct ht no_dups_={0}, *xmp_readdir_no_dups=&no_dups_;
  ht_set_id(HT_MALLOC_without_dups,ht_init_with_keystore_dim(xmp_readdir_no_dups,"xmp_readdir_no_dups",HT_FLAG_COUNTMALLOC|8,4096));
  NEW_ZIPPATH(path);
  const int opt0=strcmp(path,DIR_ZIPsFS)?0:FILLDIR_IS_DIR_ZIPsFS;
  int opt=opt0;
  bool ok=false;
  const int path_l=strlen(path);
  FOR(cut_autogen,0,(zpath->flags&ZP_STARTS_AUTOGEN)?2:1){
    foreach_root1(r){
      if (find_realpath_any_root(opt|(cut_autogen?FINDRP_AUTOGEN_CUT:FINDRP_AUTOGEN_CUT_NOT),zpath,r)){ /* FINDRP_AUTOGEN_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
        opt=FINDRP_NOT_TRANSIENT_CACHE|opt0; /* Transient cache only once */
        filler_readdir(0,zpath,buf,filler,xmp_readdir_no_dups);
        IF1(WITH_AUTOGEN, if (zpath->flags&ZP_STARTS_AUTOGEN) filler_readdir(FILLDIR_AUTOGEN,zpath,buf,filler,xmp_readdir_no_dups));
        ok=true;
        if (!cut_autogen && !ENDSWITH(path,path_l,EXT_CONTENT) && config_readir_no_other_roots(RP(),RP_L())) break;
      }
    }
  }
  if (*path=='/' && !path[1]){ /* Folder ZIPsFS in the root */
    filler(buf,&DIR_ZIPsFS[1],NULL,0  COMMA_FILL_DIR_PLUS);
    ok=true;
  }else if (opt0&FILLDIR_IS_DIR_ZIPsFS){ /* Childs of folder ZIPsFS */
    FOR(i,0,SFILE_L){
      IF1(WITH_AUTOGEN,if (WITH_AUTOGEN_DIR_HIDDEN && i==SFILE_DEBUG_CTRL) continue);
      filldir(0,filler,buf,i==SFILE_DEBUG_CTRL?DIRNAME_AUTOGEN:SPECIAL_FILES[i]+1,NULL,xmp_readdir_no_dups);
    }
    ok=true;
  }
  ht_destroy(xmp_readdir_no_dups);
  return ok?0:-1;
}
/////////////////////////////////////////////////////////////////////////////////
// With the following methods, new  new files,links, dirs etc.  are created  ///
/////////////////////////////////////////////////////////////////////////////////
static int xmp_mkdir(const char *path, mode_t mode){
  if (!_root_writable) return -EACCES;
  char real_path[MAX_PATHLEN+1];
  int res=realpath_mk_parent(real_path,path);
  if (!res) res=mkdir(real_path,mode);
  return minus_val_or_errno(res);
}
static int create_or_open(const char *path, mode_t mode, struct fuse_file_info *fi){
  if (PATH_IS_FILE_INFO(path,strlen(path))) return -EPERM;
  char real_path[MAX_PATHLEN+1];
  {
    const int res=realpath_mk_parent(real_path,path);
    if (res) return -res;
  }
  bool found;FIND_REALPATH(path);
  if (!found){
    if (!(fi->fh=MAX_int(0,open(real_path,fi->flags|O_CREAT,mode)))){
      log_errno("open(%s,%x,%u) returned -1\n",real_path,fi->flags,mode); cg_log_file_mode(mode); cg_log_open_flags(fi->flags); log_char('\n');
      return -errno;
    }
  }else{ /* Maybe not writing only reading. We will call open() in xmp_read() or xmp_write() */
    LOCK(mutex_fhandle,fhandle_create(fi->fh=next_fh(),zpath)->flags|=FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE);
  }
  return 0;
}
static int xmp_create(const char *path, mode_t mode,struct fuse_file_info *fi){ /* O_CREAT|O_RDWR goes here */
  LOG_FUSE(path);
  const int res=create_or_open(path,mode,fi);
  LOG_FUSE_RES(path,res);
  return res;
}
static int xmp_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  LOG_FUSE(path);
  if (!_root_writable) return -EACCES;
  int res=0;
  long fd;
  errno=0;
  if(!fi){
    char real_path[MAX_PATHLEN+1];
    if ((res=realpath_mk_parent(real_path,path))) return -res;
    fd=MAX_int(0,open(real_path,O_WRONLY));
  }else{
    fd=fi->fh;
    LOCK_N(mutex_fhandle, struct fhandle *d=fhandle_get(path,fi->fh); fhandle_busy_start(d));
    if (d) fd_for_fhandle(&fd,d,fi->flags);
    LOCK_N(mutex_fhandle, fhandle_busy_end(d));
  }
  if (fd<=0) return errno?-errno:-EIO;
  if ((res=pwrite(fd,buf,size,offset))==-1) res=-errno;
  if (!fi) close(fd);
  return res;
}
///////////////////////////////
// Functions with two paths ///
///////////////////////////////
static int xmp_symlink(const char *target, const char *path){ // target,link
  LOG_FUSE(path);
  if (!_root_writable) return -EACCES;
  char real_path[MAX_PATHLEN+1];
  if (!(realpath_mk_parent(real_path,path)) && symlink(target,real_path)==-1) return -errno;
  return 0;
}

static int xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags)){
  LF();
  IF_LOG_FLAG(LOG_FUSE_METHODS_ENTER) log_entered_function("%s -> %s",old_path,neu_path);
  const int res=_xmp_rename(old_path,neu_path IF1(WITH_FUSE_3,,flags));
  LOG_FUSE_RES(old_path,res);
  return res;
}
static int _xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags)){
  bool eexist=false;
#if WITH_GNU && WITH_FUSE_3
  if (flags&RENAME_NOREPLACE){
    bool found;FIND_REALPATH(neu_path);
    if (found) eexist=true;
  }else if (flags) return -EINVAL;
#endif // WITH_GNU
  bool found;FIND_REALPATH(old_path);
  if (!found) return -ENOENT;
  if (eexist) return -EEXIST;
  if (!ZPATH_ROOT_WRITABLE()) return -EACCES;
  char neu[MAX_PATHLEN+1];
  int res=realpath_mk_parent(neu,neu_path);
  if (!res) res=rename(RP(),neu);
  return minus_val_or_errno(res);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////
#if FUSE_MAJOR_V>2
static off_t xmp_lseek(const char *path, const off_t off, const int whence, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  LOG_FUSE(path);
  ASSERT(fi!=NULL);
  int ret=off;
  lock(mutex_fhandle);
  struct fhandle* d=fhandle_get(path,fi->fh);
  if (d){
    switch(whence){
#if WITH_GNU
    case SEEK_HOLE:ret=(d->offset=d->zpath.stat_vp.st_size);break;
    case SEEK_DATA:
#endif // WITH_GNU
    case SEEK_SET: ret=d->offset=off;break;
    case SEEK_CUR: ret=(d->offset+=off);break;
    case SEEK_END: ret=(d->offset=d->zpath.stat_vp.st_size+off);break;
    }
  };
  unlock(mutex_fhandle);
  if (!d) warning(WARN_SEEK|WARN_FLAG_ONCE,path,"d is NULL");
  return ret;
}
#endif
/* Read size bytes from zip entry.   Invoked only from fhandle_read() unless memcache_is_advised(d) */



////////////////////////////////////////////////////////////////////
// Wrapping zip_fread, zip_ftell and zip_fseek                     //
// Because zip_ftell and zip_fseek do not work on compressed data  //
/////////////////////////////////////////////////////////////////////

static off_t fhandle_zip_ftell(const struct fhandle *d){
  return d->zip_fread_position;
}
/* If successful, the number of bytes actually read is returned. When zip_fread() is called after reaching the end of the file, 0 is returned. In case of error, -1 is returned. */
static off_t fhandle_zip_fread(struct fhandle *d, void *buf,  zip_uint64_t nbytes, const char *errmsg){
  const off_t read=zip_fread(d->zip_file,buf,nbytes);
  if (read<0){ warning_zip(D_VP(d),zip_file_get_error(d->zip_file)," fhandle_zip_fread()");return -1;}
  d->zip_fread_position+=read;
  return read;
}
/* Returns true on success. May fail for seek backward. */
static bool fhandle_zip_fseek(struct fhandle *d, const off_t offset, const char *errmsg){
  off_t skip=(offset-fhandle_zip_ftell(d));
  if (!skip) return true;
  const bool backward=(skip<0);
  LF(); IF_LOG_FLAG(LOG_ZIP) log_verbose("%p %s offset: %ld  ftell: %ld   diff: %ld  backward: %s"ANSI_RESET,d, D_VP(d),offset,fhandle_zip_ftell(d),skip, backward?ANSI_FG_RED"Yes":ANSI_FG_GREEN"No");
  const int fwbw=backward?FHANDLE_FLAG_SEEK_BW_FAIL:FHANDLE_FLAG_SEEK_FW_FAIL;
  if (!(d->flags&fwbw) && !zip_file_is_seekable(d->zip_file)) d->flags|=(FHANDLE_FLAG_SEEK_BW_FAIL|FHANDLE_FLAG_SEEK_FW_FAIL);
  if (!(d->flags&fwbw) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
    d->zip_fread_position=offset;
    return true;
  }
  d->flags|=fwbw;
  if (backward) return false;
  char buf[4096]; /* Read to the respective position */
  const long num=MIN(sizeof(buf),skip);
  while(0<(skip=(offset-fhandle_zip_ftell(d)))){
    if (fhandle_zip_fread(d,buf,num,"")<0){
      if (fhandle_not_yet_logged(FHANDLE_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,D_VP(d),"fhandle_zip_fread returns value<0   skip=%ld bufsize=%zu",skip,num);
      return false;
    }
  }
  return true;
}
static zip_file_t *fhandle_zip_open(struct fhandle *d,const char *msg){
  cg_thread_assert_not_locked(mutex_fhandle);
  zip_file_t *zf=d->zip_file;
  if (!zf){
    d->zip_fread_position=0;
    const struct zippath *zpath=&d->zpath;
    struct zip *z=d->zarchive;
    if (!z && !(d->zarchive=z=zip_open_ro(RP()))){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"Failed zip_open");
    }else{
      zf=d->zip_file=zip_fopen(z,EP(),ZIP_RDONLY);
      if (!zf) warning_zip(RP(),zip_get_error(z),"zip_fopen()");
      fhandle_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL);
    }
  }
  return zf;
}
static void fhandle_zip_close(struct fhandle *d){
  zip_file_t *z=d->zip_file;
  d->zip_file=NULL;
  if (z) zip_file_error_clear(z);
  if (z && zip_fclose(z)) warning_zip(D_RP(d),zip_file_get_error(z),"Failed zip_fclose");   /* heap-use-after-free */
}

static off_t fhandle_read_zip(const char *path, char *buf, const off_t size, const off_t offset,struct fhandle *d,struct fuse_file_info *fi){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!fhandle_zip_open(d,__func__)){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"xmp_read_fhandle_zip fhandle_zip_open returned -1");
    return -1;
  }
  IF1(WITH_MEMCACHE, if (!(_memcache_policy==MEMCACHE_NEVER || !tdf_or_tdf_bin(D_VP(d)))) warning(WARN_MEMCACHE|WARN_FLAG_ERROR,D_VP(d),"tdf or tdf_bin should be memcache"));
  if (offset!=fhandle_zip_ftell(d)){   /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    if (!fhandle_zip_fseek(d,offset,"")){
#if WITH_MEMCACHE
      if (_memcache_policy!=MEMCACHE_NEVER){ /* Worst case=seek backward - consider using cache */
        const off_t num=memcache_waitfor(d,offset+size);
        if (num>0){
          fi->direct_io=1;
          fhandle_zip_close(d);
          _count_readzip_memcache_because_seek_bwd++;
          fhandle_counter_inc(d,ZIP_READ_CACHE_SUCCESS);
          return memcache_read(buf,d,offset,num);
        }
        fhandle_counter_inc(d,ZIP_READ_CACHE_FAIL);
      }
#endif //WITH_MEMCACHE
    }
  }
  const off_t pos=fhandle_zip_ftell(d);
  if (offset<pos){ /* Worst case=seek backward - need reopen zip file */
    warning(WARN_SEEK,path,ANSI_FG_RED"fhandle_zip_ftell() - going to reopen zip"ANSI_RESET);
    fhandle_zip_close(d);
    fhandle_zip_open(d,"REWIND");
    if (!fhandle_zip_fseek(d,offset,"REWIND")) return -1;
  }else if (offset>pos){
    warning(WARN_SEEK,D_VP(d),"should not happen offset=%ld > pos=%ld ",offset,pos);
    return -1;
  }
  const off_t num=fhandle_zip_fread(d,buf,size,"");
  fhandle_counter_inc(d,num<0?ZIP_READ_NOCACHE_FAIL:!num?ZIP_READ_NOCACHE_ZERO:ZIP_READ_NOCACHE_SUCCESS);
  return num;
}/*xmp_read_fhandle_zip*/
static uint64_t fd_for_fhandle(long *fd, struct fhandle *d,const int open_flags){
  if (d){
    assert(d->is_busy>0);
    if (d->flags&FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE){
      d->fh_real=open(D_RP(d),open_flags);
      d->flags&=~FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE;
    }/* Auto-generated data into cg_textbuffer */
    if (d->fh_real){ *fd=d->fh_real; return true; }
  }
  return false;
}

static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  LF(); IF_LOG_FLAG(LOG_READ_BLOCK) log_entered_function("%s Offset: %'ld +%'d ",path,(long)offset,(int)size);
  const int bytes=_xmp_read(path,buf,size,offset,fi);
  IF_LOG_FLAG(LOG_READ_BLOCK) log_exited_function("%s Offset: %'ld +%'d Bytes: %'d %s",path,(long)offset,(int)size,bytes,success_or_fail(bytes>0));
  return bytes;
}
static int _xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  const int path_l=strlen(path);
  long fd=fi->fh;
  int nread=0;
  {
    const int i=whatSpecialFile(path,path_l);
    if (i>0){
      LOCK(mutex_special_file,
           if(i==SFILE_INFO && _info_count_open<=0) warning(WARN_FLAG_ONCE|WARN_FLAG_ERROR|WARN_MISC,path,"xmp_read: _info_count_open=%d",_info_count_open);
           if (i>=SFILE_BEGIN_VIRTUAL) nread=read_special_file(i,buf,size,offset));
      return nread;
    }
  }
  LOCK_N(mutex_fhandle,
         struct fhandle *d=fhandle_get(path,fd);
         if (d){ d->accesstime=time(NULL); if (d->fh_real) fd=d->fh_real;}
         fhandle_busy_start(d));
  if (d){
    IF1(WITH_MEMCACHE,struct memcache *m=d->memcache);
    if (false IF1(WITH_AUTOGEN, ||(d->flags&FHANDLE_FLAG_IS_AUTOGEN) && _realpath_autogen && !d->fh_real)){
#if WITH_AUTOGEN
      if (!d->autogen_state){autogen_run(d); m=d->memcache;}
      if (d->autogen_state!=AUTOGEN_SUCCESS){ nread=-d->autogen_error; goto done_d;}
      if (!(m && m->txtbuf)) d->flags|=FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE;
#endif //WITH_AUTOGEN
    }
    if (fd_for_fhandle(&fd,d,O_RDONLY)){
      if (fd<0) nread=-EIO;
      goto done_d;
    }
    IF1(WITH_MEMCACHE,if (d->flags&FHANDLE_FLAG_MEMCACHE_COMPLETE){ nread=memcache_read(buf,d,offset,offset+size); goto done_d; });
    assert(d->is_busy>=0);
    if ((d->zpath.flags&ZP_ZIP)!=0){      /* Important for libzip. At this point we see 2 threads reading the same zip entry.*/
      if (size>(int)_log_read_max_size) _log_read_max_size=(int)size;
      nread=-1;
      IF1(WITH_MEMCACHE,
          if (!(d->flags&(FHANDLE_FLAG_WITH_MEMCACHE|FHANDLE_FLAG_WITHOUT_MEMCACHE))) d->flags|=(memcache_is_advised(d)?FHANDLE_FLAG_WITH_MEMCACHE:FHANDLE_FLAG_WITHOUT_MEMCACHE);
          if (d->flags&FHANDLE_FLAG_WITH_MEMCACHE) nread=memcache_read_fhandle(buf,size,offset,d,fi));
      if (nread<0){
        pthread_mutex_lock(&d->mutex_read); /* Observing here same/different struct fhandle instances and various pthread_self() */
        nread=fhandle_read_zip(D_VP(d),buf,size,offset,d,fi);
        pthread_mutex_unlock(&d->mutex_read);
      }
    }
    LOCK_N(mutex_fhandle, const char *status=IF0(WITH_MEMCACHE,"!WITH_MEMCACHE")IF1(WITH_MEMCACHE,MEMCACHE_STATUS_S[!m?0:m->memcache_status]); if (nread>0) d->n_read+=nread);
    if (nread<0 && !config_not_report_stat_error(path,path_l)){
      warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"nread<0:  d=%p  off=%ld size=%zu  nread=%d  n_read=%llu  memcache_status:%s"ANSI_RESET,d,offset,size,nread,d->n_read,status);
    }
  }
 done_d:
  if (!nread && (!d IF1(WITH_AUTOGEN,|| d->fh_real))){
    if (!fd){
      nread=-errno;
    }else if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK_REG_FILE:"ANSI_RESET" offset: %'lld ",(LLD)offset),log_msg("Failed %s fd=%llu\n",path,(LLU)fd);
      nread=-1;
    }else if ((nread=pread(fd,buf,size,offset))==-1){
      nread=-errno;
    }
  }
  if (d) LOCK(mutex_fhandle,fhandle_busy_end(d));
  return nread;
}/*xmp_read*/
static int xmp_release(const char *path, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  ASSERT(fi!=NULL);
  const int path_l=strlen(path), special=whatSpecialFile(path,path_l);
  if (special>=SFILE_BEGIN_VIRTUAL) return 0;
  const uint64_t fh=fi->fh;
  if (fh>=FD_ZIP_MIN){
    LOCK(mutex_fhandle,fhandle_destroy_and_evict_from_pagecache(fh,path,path_l));
  }else if (fh>2 && close(fh)){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,path,"my_close_fh %llu",fh);
    cg_print_path_for_fd(fh);
  }
  return 0;
}
static int xmp_flush(const char *path, struct fuse_file_info *fi){
  LOG_FUSE(path);
  return whatSpecialFile(path,strlen(path))>=SFILE_BEGIN_VIRTUAL?0:
    fi->fh<FD_ZIP_MIN?fsync(fi->fh):
    0;
}
static void exit_ZIPsFS(void){
  log_verbose("Going to exit ...");

  /* osxfuse and netbsd needs two parameters:  void fuse_unmount(const char *mountpoint, struct fuse_chan *ch); */
#if FUSE_MAJOR_V>=3
  if (fuse_get_context() && fuse_get_context()->fuse) fuse_unmount(fuse_get_context()->fuse);
#endif
  fflush(stderr);
}
int main(const int argc,const char *argv[]){
  _pid=getpid();
  IF1(WITH_CANCEL_BLOCKED_THREADS,assert(_pid==gettid()), assert(cg_pid_exists(_pid)));

  fprintf(stderr,"MAX_PATHLEN: %d\n",MAX_PATHLEN);
  fprintf(stderr,"has_proc_fs: %s\n",yes_no(has_proc_fs()));
  if (!realpath(*argv,_self_exe)) DIE("Failed realpath %s",*argv);
  init_mutex();
  init_sighandler(argv[0],(1L<<SIGSEGV)|(1L<<SIGUSR1)|(1L<<SIGABRT),stderr);
  gettimeofday(&_startTime,NULL);
  {
    _warning_color[WARN_THREAD]=ANSI_FG_RED;
    _warning_color[WARN_GETATTR]=ANSI_FG_MAGENTA;
    _warning_color[WARN_CHARS]=ANSI_YELLOW;
    _warning_color[WARN_DEBUG]=ANSI_MAGENTA;
    for(int i=0;MY_WARNING_NAME[i];i++) _warning_channel_name[i]=(char*)MY_WARNING_NAME[i];
  }
  int colon=0;
  FOR(i,1,argc) if (!strcmp(":",argv[i])){ colon=i; break;}
  fprintf(stderr,ANSI_INVERSE""ANSI_UNDERLINE"This is %s"ANSI_RESET" Version: "ZIPSFS_VERSION"\nCompiled: %s %s  PID: "ANSI_FG_WHITE ANSI_BLUE"%d"ANSI_RESET"\n",path_of_this_executable(),__DATE__,__TIME__,_pid);
  IF1(WITH_GNU,fprintf(stderr,"gnu_ggnu_get_libc_version: %s\n",gnu_get_libc_version()));
#if defined(__has_feature)
#  if __has_feature(address_sanitizer)
  puts_stderr("Compiled with sanitizer\n");
#else
  puts_stderr("Compiled without sanitizer\n");
#  endif
#endif
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in fprintf */
  ASSERT(S_IXOTH==(S_IROTH>>2));
  IF1(WITH_ZIPINLINE,config_containing_zipfile_of_virtual_file_test());
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr); S(access); S(utimens);
  S(readlink);
  //S(opendir);
  S(readdir);   S(mkdir);
  S(symlink); S(unlink);
  S(rmdir);   S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  IF1(WITH_FUSE_3,S(lseek));
#undef S
  static struct option l_option[]={
    { "help",      0, NULL, 'h'},
    { "version",   0, NULL, 'V'},
    {NULL,0,NULL,0}
  };
  bool isBackground=false;
  for(int c;(c=getopt_long(argc,(char**)argv,"+aAbqT:nkhVs:c:S:l:L:",l_option,NULL))!=-1;){  /* The initial + prevents permutation of argv */
    switch(c){
    case 'a':  assert(!WITH_EXTRA_ASSERT); assert(!WITH_ASSERT_LOCK);
    case 'A':  if (WITH_AUTOGEN==(c=='a')) DIE("Macro WITH_AUTOGEN should be %d due to option -%c\n",!WITH_AUTOGEN,c);break; // cppcheck-suppress [knownConditionTrueFalse]
    case 'V': exit(0);break;
    case 'T':{
      const int i=atoi(optarg);
      if (0<=i && i<=CG_PRINT_STACKTRACE_TEST_MAX) cg_print_stacktrace_test(i);
      else log_error("Option -T requires numbers between 0 and %d",CG_PRINT_STACKTRACE_TEST_MAX);
      exit_ZIPsFS();
    } break;
    case 'b': isBackground=true; break;
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 'S': _pretendSlow=true; break;
    case 's': cg_strncpy(_mkSymlinkAfterStart,optarg,MAX_PATHLEN); break;
    case 'h': ZIPsFS_usage();
      return 0;
    case 'l':
#if WITH_MEMCACHE
      if ((_memcache_maxbytes=cg_atol_kmgt(optarg))<(1<<22)){
        log_error("Option -l: _memcache_maxbytes is too small %s\n",optarg);
        return 1;
      }
#endif //WITH_MEMCACHE
      break;
    case 'L':{
#if ! defined(HAS_RLIMIT) || HAS_RLIMIT

      static struct rlimit _rlimit={0};
      _rlimit.rlim_cur=_rlimit.rlim_max=cg_atol_kmgt(optarg);
      log_msg("Setting rlimit to %llu MB \n",(LLU)_rlimit.rlim_max>>20);
      setrlimit(RLIMIT_AS,&_rlimit);
#else
      log_error("Option -L not available on this platform.");
#endif
    } break;
#if WITH_MEMCACHE
    case 'c':{
      bool ok=false;
      const char *s;
      for(int i=0;!ok && (s=rm_pfx_us(WHEN_MEMCACHE_S[i]));i++){
        if ((ok=!strcasecmp(s,optarg))) _memcache_policy=i;
      }
      if (!ok){ log_error("Wrong option -c %s\n",optarg); ZIPsFS_usage(); return 1;}
    } break;
#endif //WITH_MEMCACHE
    }
  }
  if (!getuid() || !geteuid()){
    log_strg("Running ZIPsFS as root opens unacceptable security holes.\n");
    if (isBackground) DIE("It is only allowed in foreground mode  with option -f.");
    fprintf(stderr,"Do you accept the risks [Enter / Ctrl-C] ?\n");cg_getc_tty();
  }
  if (!colon){ log_error("No single colon found in parameter list\n"); suggest_help(); return 1;}
  if (colon==argc-1){ log_error("Expect mount point after single colon\n"); suggest_help(); return 1;}
  ASSERT(MAX_PATHLEN<=PATH_MAX);
  _mnt_l=strlen(realpath(argv[argc-1],_mnt));
  {
    struct stat st;
    if (PROFILED(stat)(_mnt,&st)){
      if (isBackground) DIE("Directory does not exist: %s",_mnt);
      fprintf(stderr,"Create non-existing folder %s  [Enter / Ctrl-C] ?\n",_mnt);
      cg_getc_tty();
      cg_recursive_mkdir(_mnt);
    }else{
      if (!S_ISDIR(st.st_mode)) DIE("Not a directory: %s",_mnt);
    }
  }
  { /* dot_ZIPsFS */
    char dot_ZIPsFS[MAX_PATHLEN+1], dirOldLogs[MAX_PATHLEN+1], tmp[MAX_PATHLEN];
    {
      {
        char *d=dot_ZIPsFS+strlen(cg_copy_path(dot_ZIPsFS,"~/.ZIPsFS"));
        strcat(d,_mnt); while(*++d) if (*d=='/') *d='_';
      }
      snprintf(dirOldLogs,MAX_PATHLEN,"%s%s",dot_ZIPsFS,"/oldLogs");
      cg_recursive_mkdir(dirOldLogs);
      FILE *f;
      if (!(f=fopen(strcpy(stpcpy(tmp,dot_ZIPsFS),"/pid.txt"),"w"))){
        perror(tmp);
      }else{
        fprintf(f,"%d\n",_pid);
        fclose(f);
      }
    }
    FOR(i,0,2){
      snprintf(_fWarningPath[i],MAX_PATHLEN,"%s%s",dot_ZIPsFS,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
      struct stat stbuf;
      if (!PROFILED(lstat)(_fWarningPath[i],&stbuf) && stbuf.st_size){ /* Save old logs with a mtime in file name. */
        const time_t t=stbuf.st_mtime;
        struct tm lt;
        localtime_r(&t,&lt);
        snprintf(tmp,MAX_PATHLEN,"%s%s",dirOldLogs,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
        strftime(strrchr(tmp,'.'),22,"_%Y_%m_%d_%H:%M:%S",&lt);
        strcat(tmp,".log");
        if (rename(_fWarningPath[i],tmp)) DIE("rename %s %s",_fWarningPath[i],tmp);
        const pid_t pid=fork();
        assert(pid>=0);
        if (pid>0){
          //int status;
          waitpid(pid,/*&status*/NULL,0);
        }else{
          assert(!execlp("gzip","gzip","-f","--best",tmp,(char*)NULL));
          assert(1);
        }
      }
      _fWarnErr[i]=fopen(_fWarningPath[i],"w");
    }
    warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);

    {
      static char ctrl_sh[MAX_PATHLEN+1];
      snprintf(ctrl_sh,MAX_PATHLEN,"%s/%s",dot_ZIPsFS,"ZIPsFS_CTRL.sh");
      snprintf(tmp,MAX_PATHLEN,"%s/cachedir",dot_ZIPsFS); mstore_set_base_path(tmp);
      SPECIAL_FILES[SFILE_DEBUG_CTRL]=ctrl_sh;
      {
        struct textbuffer b={0};
        special_file_content(&b,SFILE_DEBUG_CTRL);
        const int fd=open(ctrl_sh,O_RDONLY);
        if (fd<0 || textbuffer_differs_from_filecontent_fd(&b,fd)){
          log_msg("Going to write %s ...\n",ctrl_sh);
          textbuffer_write_file(&b,ctrl_sh,0770);
        }else{
          log_msg(ANSI_FG_GREEN"Up-to-date %s\n"ANSI_RESET,ctrl_sh);
        }
        if (fd>0) close(fd);
      }
    }
    {
      snprintf(_fLogFlags,MAX_PATHLEN,"%s/%s",dot_ZIPsFS,"log_flags.conf");
      FILE *f;
      if (!cg_file_exists(_fLogFlags) && (f=fopen(_fLogFlags,"w"))) fclose(f);
      snprintf(tmp,MAX_PATHLEN,"%s.readme",_fLogFlags);
      if ((f=fopen(tmp,"w"))){
        fprintf(f,
                "f_log_flag=%s\n"
                "# The file specifies additional logs that go exclusively to stderr.\n"
                "# Users can enter a decimal number which is a bit-mask.\n"
                "# Changes take immediate effect\n#\n"
                "# MEANING OF BITS AND SHELL ALIASES:\n#\n",_fLogFlags);
        FOR(i,0,LOG_FLAG_LENGTH) fprintf(f,"alias %30s='echo $((1<<%d))  >$f_log_flag'\n",LOG_FLAG_S[i],i);
        fprintf(f,"#\n# Note: To activate the aliases, this script can be sourced in bash.  Run\n#    . %s\n",tmp);
        fclose(f);
      }
    }
  }
  FOR(i,optind,colon){ /* Source roots are given at command line. Between optind and colon */
    if (_root_n>=ROOTS) DIE("Exceeding max number of ROOTS %d.  Increase constant  LOG2_ROOTS   in configuration.h and recompile!\n",ROOTS);
    const char *p=argv[i];
    if (!*p){ log_warn("Command line argument # %d is empty. %s\n",optind,i==optind?"Consequently, there will be no writable root.":"");  continue;}
    struct rootdata *r=_root+_root_n++;
    if (i==optind)  (_root_writable=r)->features|=ROOT_WRITABLE;
    {
      int slashes=-1;while(p[++slashes]=='/');
      if (slashes>1){ r->features|=ROOT_REMOTE; p+=(slashes-1); }
    }
    log_msg("Going to obtain realpath of  %s ...  ",p);
    r->rootpath_l=cg_strlen(realpath(p,r->rootpath));
    //pthread_mutex_init(&r->mutex_zip_fread,NULL);
    log_msg("Realpath is  %s\n",r->rootpath);
    if (!r->rootpath_l) r->rootpath_l=cg_strlen(realpath(p,r->rootpath));
    if (!r->rootpath_l && i!=optind){perror("");      DIE("realpath '%s':  %s  is empty\n",p,r->rootpath);}
    ht_set_mutex(mutex_dircache,ht_init(&r->dircache_ht,"dircache",HT_FLAG_KEYS_ARE_STORED_EXTERN|12));
    ht_set_mutex(mutex_dircachejobs,ht_init(&r->dircache_queue,"dircache_queue",8));
#if WITH_DIRCACHE || WITH_STAT_CACHE
    ht_init_interner_file(&r->dircache_ht_fname,"dircache_ht_fname",16,DIRECTORY_CACHE_SIZE);
    ht_set_mutex(mutex_dircache,&r->dircache_ht_fname);
    ht_set_mutex(mutex_dircache,ht_init_interner_file(&r->dircache_ht_fnamearray,"dircache_ht_fnamearray",HT_FLAG_BINARY_KEY|12,DIRECTORY_CACHE_SIZE));
    mstore_set_mutex(mutex_dircache,mstore_init(&r->dircache_mstore,"dircache_mstore",MSTORE_OPT_MMAP_WITH_FILE|DIRECTORY_CACHE_SIZE));
#endif
  }/* Loop roots */

  log_msg("\n\nRoots:\n");
  foreach_root1(r) log_msg("\t%s\t%s\n",rootpath(r),!cg_strlen(rootpath(r))?"":(r->features&ROOT_REMOTE)?"Remote":(r->features&ROOT_WRITABLE)?"Writable":"Local");

  { /* Storing information per file type for the entire run time */
    mstore_set_mutex(mutex_fhandle,mstore_init(&mstore_persistent,"persistent",0x10000));
    ht_set_mutex(mutex_fhandle,ht_init_interner(&ht_intern_fileext,"ht_intern_fileext",8,4096));
    ht_set_mutex(mutex_validchars,ht_init(&_ht_valid_chars,"validchars",HT_FLAG_NUMKEY|12));
    ht_init_with_keystore_dim(&ht_inodes_vp,"inode_from_virtualpath",16,0x100000|MSTORE_OPT_MMAP_WITH_FILE);
    IF1(WITH_AUTOGEN,if (!ht_fsize.capacity) ht_init(&ht_fsize,"autogen-fsize",HT_FLAG_NUMKEY|9));
  }
  IF1(WITH_ZIPINLINE_CACHE, ht_set_mutex(mutex_dircache,ht_init(&ht_zinline_cache_vpath_to_zippath,"zinline_cache_vpath_to_zippath",HT_FLAG_NUMKEY|16)));
  IF1(WITH_STAT_CACHE,ht_set_mutex(mutex_dircache,ht_init(&stat_ht,"stat",16)));
  log_strg("\n"ANSI_INVERSE"Roots"ANSI_RESET"\n");
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  bool warn=false;
  if (*_mkSymlinkAfterStart){
    _mkSymlinkAfterStart[cg_pathlen_ignore_trailing_slash(_mkSymlinkAfterStart)]=0;
    if (*_mkSymlinkAfterStart=='/') fprintf(stderr,RED_WARNING": "ANSI_FG_BLUE"%s"ANSI_RESET" is an absolute path. You might be unable to export the file tree with NFS and Samba. Press Enter to continue anyway!\n",_mkSymlinkAfterStart); cg_getc_tty();
    const int err=cg_symlink_overwrite_atomically(_mnt,_mkSymlinkAfterStart);
    char rp[PATH_MAX+1];
    if (err || !realpath(_mkSymlinkAfterStart,rp)){
      char cwd[MAX_PATHLEN+1];
      warning(WARN_MISC,_mkSymlinkAfterStart,"Working-dir: %s  cg_symlink_overwrite_atomically(%s,%s); %s",getcwd(cwd,MAX_PATHLEN),_mnt,_mkSymlinkAfterStart,strerror(err));
      EXIT(1);
    }else if(!cg_is_symlink(_mkSymlinkAfterStart)){
      warning(WARN_MISC,_mkSymlinkAfterStart," not a symlink");
      EXIT(1);
    }else{
      log_msg(GREEN_SUCCESS"Created symlink %s -->%s\n",_mkSymlinkAfterStart,rp);
    }
    *_mkSymlinkAfterStart=0;
  }

#define C(var,x) if (var!=x){warn=true;fprintf(stderr,RED_WARNING"%s is  %d  instead of  %d\n",#var,var,x); warning(WARN_MISC,#var,"is  %d instead of %d",var,x);}
  C(WITH_DIRCACHE,1);
  C(WITH_MEMCACHE,1);
  C(WITH_TRANSIENT_ZIPENTRY_CACHES,1);
  C(WITH_ZIPINLINE,1);
  C(WITH_ZIPINLINE_CACHE,1);
  C(WITH_STAT_CACHE,1);
  C(WITH_DIRCACHE_OPTIMIZE_NAMES,1);
  C(PLACEHOLDER_NAME,7);
  C(WITH_PTHREAD_LOCK,1);
  C(WITH_ASSERT_LOCK,0);
  C(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,0);
  C(WITH_STAT_SEPARATE_THREADS,1);
  C(DEBUG_DIRCACHE_COMPARE_CACHED,0);
  C(DEBUG_TRACK_FALSE_GETATTR_ERRORS,0);
  C(WITH_AUTOGEN,1);
  IF1(WITH_EVICT_FROM_PAGECACHE,C(WITH_EVICT_FROM_PAGECACHE,1));
  C(WITH_EXTRA_ASSERT,0);
  C(HAS_BACKTRACE,1);
  C(WITH_POPEN_NOSHELL,0);
#undef C
  if (!HAS_BACKTRACE){
    fprintf(stderr,"Warning: No stack-traces can be written in case of a program error.\n");
    warn=true;
  }else if (!HAS_ADDR2LINE && !HAS_ATOS){
    fprintf(stderr,"For better stack-traces (debugging) it is recommended to install "ANSI_FG_BLUE IF1(IS_APPLE,"atos")IF0(IS_APPLE,"addr2line (package binutils)")ANSI_RESET".\n");
    warn=true;
  }
#if WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
  if (DIRECTORY_CACHE_SIZE*NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE<64*1024*1024){
    log_msg(RED_WARNING"Small file attribute and directory cache of only %d\n",NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE*NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE/1024);
    warn=true;
  }
#endif
  IF1(WITH_AUTOGEN,if (!cg_is_member_of_group("docker")){ log_warn(HINT_GRP_DOCKER); warn=true;});
  if (warn && !strstr(_mnt,"/cgille/")){ fprintf(stderr,"Press enter\n");cg_getc_tty();}
  if (isBackground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=_cg_is_none_interactive=true;
  foreach_root1(r){
    RLOOP(t,PTHREAD_LEN){
      //if (ir&&t==PTHREAD_MISC0) continue; /* Only one instance */
      if (r!=_root&&t==PTHREAD_MISC0) continue; /* Only one instance */
      r->thread_is_run[t]=true;
      root_start_thread(r,t);
    }
  }
#if WITH_AUTOGEN
  char realpath_autogen_heap[MAX_PATHLEN+1]; // cppcheck-suppress unassignedVariable
  if (_root_writable){
    //    strcat(strcpy(_realpath_autogen=realpath_autogen_heap,_root_writable->rootpath),DIR_AUTOGEN);
    stpcpy(stpcpy(_realpath_autogen=realpath_autogen_heap,_root_writable->rootpath),DIR_AUTOGEN);
    aimpl_init();
  }
#endif
  static pthread_t thread_unblock;
  IF1(WITH_CANCEL_BLOCKED_THREADS,pthread_create(&thread_unblock,NULL,&infloop_unblock,NULL));
  _textbuffer_memusage_lock=_mutex+mutex_textbuffer_usage;
  log_msg("Running %s with PID %d. Going to fuse_main() ...\n",argv[0],_pid);
  cg_free(MALLOC_TESTING,cg_malloc(MALLOC_TESTING,10));
  _fuse_argv[_fuse_argc++]="";
  if (!isBackground) _fuse_argv[_fuse_argc++]="-f";
  FOR(i,colon+1,argc) _fuse_argv[_fuse_argc++]=argv[i];
  const int fuse_stat=fuse_main(_fuse_argc,(char**)_fuse_argv,&xmp_oper,NULL);
  log_msg(RED_WARNING" fuse_main returned %d\n",fuse_stat);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF1(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS();
}
// napkon      astra zenika Agathe  find
////////////////////////////////////////////////////////////////////////////
// SSMetaData zipinfo //s-mcpb-ms03/slow2/incoming/Z1/Data/30-0089/20230719_Z1_ZW_027_30-0089_Serum_EAD_14eV_3A_OxoIDA_rep_01.wiff2.Zip
//
// DIE   DIE_DEBUG_NOW   DEBUG_NOW   log_debug_now  log_entered_function log_exited_function strchr
//
// _GNU_SOURCE PATH_MAX
// SIGILL SIGINT   ST_NOATIME
// HAVE_CONFIG_H  HAS_EXECVPE HAS_UNDERSCORE_ENVIRON HAS_ST_MTIM HAS_POSIX_FADVISE
// https://wiki.smartos.org/
// malloc calloc strdup  free  mmap munmap   readdir opendir
// MMAP_INC(...)  MUNMAP_INC(...)  MALLOC_INC(...)  FREE_INC(...)
// cg_mmap ht_destroy cg_strdup cg_free cg_free_null
// ZIPSFS_FINAL_REPORT mmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmmm WITH_DEBUG_MALLOC
//
// opendir  locked config_autogen_size_of_not_existing_file crc32
// limits strtoOAk SIZE_POINTER   MALLOC_MSTORE_IMBALANCE
// unlink   cg_waitpid_logtofile_return_exitcode
// strncpy stpncpy  mempcpy strcat _thisPrg
// strrchr autogen_rinfiles
// analysis.tdf-shm analysis.tdf-wal
// lstat stat
//     if (!strcmp("/s-mcpb-ms03.charite.de/incoming/PRO3/Maintenance/202403/20240326_PRO3_LRS_004_MA_HEK500ng-5minHF_001_V11-20-HSoff_INF-G.d.Zip",rp)) {  log_zpath("DDDDDDDDDD",zpath);}
// SIGTERM
//  statvfs    f_bsize  f_frsize  f_blocks f_bfree f_bavail
// config_autogen_estimate_filesize   autogen_rinfiles
// stat foreach_fhandle EIO local  fseek seek open
// fdescriptor fdscrptr fhandle fhandle_read_zip  #include "generated_profiler_names.c" LOCK
// mutex_fhandle
