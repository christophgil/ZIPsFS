/*
  ZIPsFS   Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  It has been developed starting with  fuse-3.14: Filesystem in Userspace  passthrough.c
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS_notes.org  log.c
*/
#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#ifdef __linux__
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include "config.h"
#include <limits.h>
#include <math.h>
#include <fuse.h>
#include <pthread.h>
#include <dirent.h>
#include <math.h>
#include <stdbool.h>
#ifdef __FreeBSD__
#include <sys/un.h>
#endif
#include <fcntl.h> // provides posix_fadvise
#include <sys/mman.h>
#include <fuse.h>
#define fill_dir_plus 0
#include <zip.h>
#define FOR(from,var,to) for(int var=from;var<to;var++)
#define RLOOP(var,from) for(int var=from;--var>=0;)
#define LOG_STREAM stdout
#define A1(x) C(x,INIT)C(x,STR)C(x,INODE)C(x,THREAD)C(x,MALLOC)C(x,ROOT)C(x,OPEN)C(x,READ)C(x,ZIP_FREAD)C(x,READDIR)C(x,SEEK)C(x,ZIP)C(x,GETATTR)C(x,STAT)C(x,FHDATA)C(x,DIRCACHE)C(x,MEMCACHE)C(x,FORMAT)C(x,MISC)C(x,DEBUG)C(x,CHARS)C(x,RETRY)C(x,LEN)
#define A2(x) C(x,nil)C(x,queued)C(x,reading)C(x,done)C(x,interrupted)
#define A3(x) C(x,NEVER)C(x,SEEK)C(x,RULE)C(x,COMPRESSED)C(x,ALWAYS)
#define A4(x) C(x,nil)C(x,fhdata)C(x,dircachejobs)C(x,log_count)C(x,crc)C(x,inode)C(x,memUsage)C(x,dircache)C(x,idx)C(x,statqueue)C(x,validchars)C(x,special_file)C(x,validcharsdir)C(x,roots) //* mutex_roots must be last */
#define A5(x) C(x,NIL)C(x,QUEUED)C(x,FAILED)C(x,OK)
#define A6(x) C(x,DIRCACHE)C(x,MEMCACHE)C(x,STATQUEUE)C(x,LEN)
#define C(x,a) x##a,
enum warnings{A1(WARN_)};
enum memcache_status{A2(memcache_)};
enum when_memcache_zip{A3(MEMCACHE_)};
enum mutex{A4(mutex_)};
enum statqueue_status{A5(STATQUEUE_)};
enum root_thread{A6(PTHREAD_)};
#undef C
#define C(x,a) #a,
static const char *MY_WARNING_NAME[]={A1()NULL};
static const char *MEMCACHE_STATUS_S[]={A2()NULL};
static const char *WHEN_MEMCACHE_S[]={A3()NULL};
static const char *MUTEX_S[]={A4()NULL};
static const char *STATQUEUE_STATUS_S[]={A5()NULL};
static const char *PTHREAD_S[]={A6()NULL};
#undef C
#undef A1
#undef A2
#undef A3
#include "cg_ht_v7.c"
#include "cg_utils.c"
#include "cg_log.c"
//#define PLACEHOLDER_NAME 0x1A
#define PLACEHOLDER_NAME '*'
#include "cg_debug.c"
#include "ZIPsFS_configuration.h"
#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000
#define DEBUG_ABORT_MISSING_TDF 1
#define SIZE_POINTER sizeof(char *)
#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
#define IS_STAT_READONLY(st) !(st->st_mode&(S_IWUSR|S_IWGRP|S_IWOTH))
///////////////////////////
//// Structs and enums ////
///////////////////////////
static int _fhdata_n=0, _log_count_lock=0;
enum fhdata_having{having_any,having_memcache,having_no_memcache};
static enum when_memcache_zip _memcache_policy=MEMCACHE_SEEK;
static bool _pretendSlow=false;
static int64_t _memcache_maxbytes=3L*1000*1000*1000;
static char _mkSymlinkAfterStart[MAX_PATHLEN+1]={0},_mnt[MAX_PATHLEN+1];
#define HOMEPAGE "https://github.com/christophgil/ZIPsFS"
///////////////
/// pthread ///
///////////////
static pthread_key_t _pthread_key;
#define THREADS_PER_ROOT 3
static int _oldstate_not_needed;
#define LOCK(mutex,code) lock(mutex);code;unlock(mutex)
#define LOCK_NCANCEL(mutex,code) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&_oldstate_not_needed);lock(mutex);code;unlock(mutex);pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&_oldstate_not_needed);
#define DIRENT_ISDIR (1<<0)
#define DIRENT_IS_COMPRESSED (1<<1)
#define C_FILE_DATA_WITHOUT_NAME()    C(fsize,size_t); C(finode,uint64_t); C(fmtime,uint64_t); C(fcrc,uint32_t); C(fflags,uint8_t);
#define C_FILE_DATA()    C(fname,char *); C_FILE_DATA_WITHOUT_NAME();
#define C(field,type) type *field;
struct directory_core{
  struct timespec mtim;
  int files_l;
  C_FILE_DATA();
};
#undef C
#define S_DIRECTORY_CORE sizeof(struct directory_core)
#define DIRECTORY_FILES_CAPACITY 256 /* Capacity on stack. If exceeds then variables go to heap */
#define DIRECTORY_IS_HEAP (1<<0)
#define DIRECTORY_IS_DIRCACHE (1<<1)
#define DIRECTORY_IS_ZIPARCHIVE (1<<2)
#define DIRECTORY_TO_QUEUE (1<<3)
#define ROOTd(d) (_root+d->zpath.root)
#define ROOTS (1<<LOG2_ROOTS)

struct directory{
  uint32_t dir_flags;
  const char *dir_realpath;
  struct rootdata *root;
#define C(field,type) type _stack_##field[DIRECTORY_FILES_CAPACITY];
  C_FILE_DATA();
#undef C
  struct directory_core core; // Only this data goes to dircache.
  struct mstore filenames;
  int files_capacity;
};
struct zippath{
#define C(s) char *s; int s##_l
  C(strgs); /* Contains all strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  C(virtualpath);
  C(virtualpath_without_entry);  /*  Let Virtualpath be "/foo.zip/bar". virtualpath_without_entry will be "/foo.zip". */
  C(entry_path); /*  Let Virtualpath be "/foo.zip/bar". entry_path will be "bar". */
#undef C
  char *realpath;
  zip_uint32_t rp_crc;
  int root;         /* index of root */
  int current_string; /* The String that is currently build within strgs with zpath_newstr().  With zpath_commit() this marks the end of String. */
  struct stat stat_rp,stat_vp;
  uint32_t flags;
};
#define ZP_ZIP (1<<2)
#define ZP_STRGS_ON_HEAP (1<<3)
#define ZP_IS_COMPRESSED (1<<4)
#define ZP_NEVER_STATCACHE_IN_FHDATA (1<<5)
#define ZPATH_IS_ZIP() ((zpath->flags&ZP_ZIP)!=0)
#define LOG_FILE_STAT() log_file_stat(zpath->realpath,&zpath->stat_rp),log_file_stat(zpath->virtualpath,&zpath->stat_vp)
#define VP() zpath->virtualpath
#define EP() zpath->entry_path
#define EP_LEN() zpath->entry_path_l
#define VP_LEN() zpath->virtualpath_l
#define RP() zpath->realpath
#define VP0() zpath->virtualpath_without_entry
#define VP0_LEN() zpath->virtualpath_without_entry_l
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,NULL)?0:ENOENT;

enum enum_count_getattr{
  COUNTER_STAT_FAIL,COUNTER_STAT_SUCCESS,
  COUNTER_OPENDIR_FAIL,COUNTER_OPENDIR_SUCCESS,
  COUNTER_GETATTR_FAIL,COUNTER_GETATTR_SUCCESS,
  COUNTER_ACCESS_FAIL,COUNTER_ACCESS_SUCCESS,
  COUNTER_READDIR_SUCCESS,COUNTER_READDIR_FAIL,
  COUNTER_ROOTDATA_INITIALIZED,enum_count_getattr_length};

enum enum_counter_rootdata{
  ZIP_OPEN_SUCCESS,ZIP_OPEN_FAIL,
  //
  ZIP_READ_NOCACHE_SUCCESS, ZIP_READ_NOCACHE_ZERO, ZIP_READ_NOCACHE_FAIL,
  ZIP_READ_NOCACHE_SEEK_SUCCESS, ZIP_READ_NOCACHE_SEEK_FAIL,
  //
  ZIP_READ_CACHE_SUCCESS, ZIP_READ_CACHE_FAIL,
  ZIP_READ_CACHE_CRC32_SUCCESS, ZIP_READ_CACHE_CRC32_FAIL,
  //
  COUNT_RETRY_STAT,
  COUNT_RETRY_MEMCACHE,
  COUNT_RETRY_ZIP_FREAD,
  //
  counter_rootdata_num};
#define FILETYPEDATA_NUM 1024
#define FILETYPEDATA_FREQUENT_NUM 64
struct counter_rootdata{
  const char *ext;
  uint32_t counts[counter_rootdata_num];
  long rank;
};
typedef struct counter_rootdata counter_rootdata_t;

#define ZPATH_STRGS 4096
/* The struct fhdata holds data associated with a file descriptor.
   Stored in a linear list _fhdata. The list may have holes.
   Memcache: It may also contain the cached file content of the zip entry.
   Only one of all instances with a specific virtual path should store the cached zip entry  */
struct fhdata{
  uint64_t fh; /* Serves togehter with path as key to find the instance in the linear array.*/
  char *path; /* The virtual path serves as key*/
  ht_hash_t path_hash; /*Serves as key*/
  struct zip *zarchive;
  zip_file_t *zip_file;
  struct zippath zpath;
  volatile time_t accesstime;
  struct stat *statcache_for_subdirs_of_path;
  int statcache_for_subdirs_of_path_l;
  volatile bool close_later;
  volatile bool memcache_is_heap,memcache_is_urgent;
  uint8_t already_logged;
  volatile enum memcache_status memcache_status;
  char *memcache;  /* Zip entries are loaded into RAM */
  volatile size_t memcache_l,memcache_already,memcache_already_current;
  int64_t memcache_took_mseconds;
  volatile int64_t offset,n_read;
  volatile int is_xmp_read; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex_read; /* Costs 40 Bytes */
  counter_rootdata_t *filetypedata;
};
#define FHDATA_MAX 3333
#define foreach_fhdata(d) for(struct fhdata *d=_fhdata+_fhdata_n;--d>=_fhdata;)
#define foreach_fhdata_path_not_null(d)  foreach_fhdata(d) if (d->path)
static struct fhdata _fhdata[FHDATA_MAX]={0};
#define STATQUEUE_ENTRY_N 256
struct statqueue_entry{
  volatile int time;
  char rp[MAX_PATHLEN+1];
  volatile int rp_l;
  volatile ht_hash_t rp_hash;
  struct stat stat;
  volatile enum statqueue_status status;
};
struct rootdata{
  char *root_path;
  int root_path_l;
  int index; /* Index in array _roots */
  uint32_t features;
  struct statfs statfs;
  uint32_t log_count_delayed,log_count_restarted;
  volatile uint32_t statfs_took_deciseconds; /* how long did it take */
  char dircache_files[MAX_PATHLEN+1];
  struct mstore dircache;
  struct ht dircache_queue, dircache_ht;
  IF1(IS_DIRCACHE, struct cache_by_hash dircache_byhash_name, dircache_byhash_dir);
  pthread_t pthread[PTHREAD_LEN];
  int pthread_count_started[PTHREAD_LEN];
  int pthread_when_response_deciSec; /* When last response. Overflow after after few years does not matter. */
  int pthread_when_loop_deciSec[PTHREAD_LEN]; /* Detect blocked loop. */
  struct statqueue_entry statqueue[STATQUEUE_ENTRY_N];
  bool debug_pretend_blocked[PTHREAD_LEN];
  struct fhdata *memcache_d;
  struct ht ht_filetypedata;
  counter_rootdata_t filetypedata_dummy, filetypedata_all, filetypedata[FILETYPEDATA_NUM],filetypedata_frequent[FILETYPEDATA_FREQUENT_NUM];
  bool filetypedata_initialized;
};
static struct rootdata _root[ROOTS]={0}, *_writable_root=NULL;
#define foreach_root(ir,r)    int ir=0;for(struct rootdata *r=_root; ir<_root_n; r++,ir++)
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int _root_n=0;
enum memusage{memusage_mmap,memusage_malloc,memusage_n,memusage_get_curr,memusage_get_peak};
#define Nth(array,i,defaultVal) (array?array[i]:defaultVal)
#define Nth0(array,i) Nth(array,i,0)
//////////////////////////////////////
// stat cache
/////////////////////////////////////
struct cached_stat{
  int when_read_decisec;
  ino_t st_ino;
  struct timespec st_mtim;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};
static struct mstore mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
static long  _count_stat_fail=0, _count_stat_from_cache=0;
static struct ht ht_intern_temporarily, ht_intern_strg;
IF1(IS_STAT_CACHE,static struct ht stat_ht);
IF1(IS_ZIPINLINE_CACHE,static struct ht zipinline_cache_virtualpath_to_zippath_ht);
IF1(DO_REMEMBER_NOT_EXISTS,static struct ht remember_not_exists_ht;)

  static int _memusage_count[2*memusage_n];
//////////////////////////////////////////////////////////////////////////
// The struct zippath is used to identify the real path from the virtual path
// All strings are stacked in char* field ->strgs.
// strgs is initially on stack and may later go to heap.
/* *** fhdata vars and defs *** */
///////////////////////////////////////////////////////////
// The root directories are specified as program arguments
// The _root[0] may be read/write, and can be empty string
// The others are read-only
//
// #include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#define DIR_ZIPsFS "/ZIPsFS"
#define DIR_ZIPsFS_L (sizeof(DIR_ZIPsFS)-1)
static const char *SPECIAL_FILES[]={"/warnings_and_errors.log","/file_system_info.html","/ZIPsFS.command","/Readme.html",NULL};  /* SPECIAL_FILES[SFILE_LOG_WARNINGS] is first!*/
enum enum_special_files{SFILE_LOG_WARNINGS,SFILE_FS_INFO,SFILE_CTRL,SFILE_README,SFILE_L};
static char _fwarn[MAX_PATHLEN+1];
#define MSTORE_ADDSTR(m,str,len) (mstore_assert_lock(m),mstore_addstr(m,str,len))
#define MSTORE_ADD(m,src,bytes,align) (mstore_assert_lock(m),mstore_add(m,src,bytes,align))
#define MSTORE_ADD_REUSE(m,str,len,hash,byhash) (mstore_assert_lock(m),mstore_add_reuse(m,str,len,hash,byhash))
#define MSTORE_DESTROY(m) (mstore_assert_lock(m),mstore_destroy(m))
#define MSTORE_CLEAR(m) (mstore_assert_lock(m),mstore_clear(m))
#define MSTORE_USAGE(m) (mstore_assert_lock(m),mstore_usage(m))
#define MSTORE_COUNT_SEGMENTS(m) (mstore_assert_lock(m),mstore_count_segments(m))
#define assert_validchars(t,s,len,msg) _assert_validchars(t,s,len,msg,__func__)
#define PRETEND_BLOCKED(thread) while(r->debug_pretend_blocked[thread]) usleep(1000*1000);
static pthread_mutex_t _mutex[mutex_roots+ROOTS];
#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#include "ZIPsFS_configuration.c"
#include "ZIPsFS_debug.c"
#include "ZIPsFS_log.c"
#include "ZIPsFS_cache.c"
#include "ZIPsFS_ctrl.c"
/* With DO_ASSERT_LOCK 50 nanosec. Without 25 nanosec. Not recursive 30 nanosec. */
////////////////////////////////////////
/// lock, pthread, synchronization   ///
////////////////////////////////////////
static void lock(int mutex){
  _log_count_lock++;
  pthread_mutex_lock(_mutex+mutex);
  IF1(DO_ASSERT_LOCK, mutex_count(mutex,1));
}
static void unlock(int mutex){
  IF1(DO_ASSERT_LOCK,mutex_count(mutex,-1));
  pthread_mutex_unlock(_mutex+mutex);
}
static void init_mutex(){
  pthread_key_create(&_pthread_key,NULL);
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  RLOOP(i,mutex_roots+_root_n){
    pthread_mutex_init(_mutex+i,&_mutex_attr_recursive);
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Consider ZIP file 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff2.Zip
// 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff.scan  will be stored as *.wiff.scan in the table-of-content
// * denotes the PLACEHOLDER_NAME
//
// This will save space in cache.  Many ZIP files will have identical table-of-content and thus kept only once in the cache.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if IS_DIRCACHE_OPTIMIZE_NAMES
static const char *simplify_name(char *s,const char *u, const char *zipfile){
  ASSERT(u!=NULL);
  ASSERT(s!=NULL);
  ASSERT(s!=u);
  const int ulen=my_strlen(u);
  if (strchr(u,PLACEHOLDER_NAME)){
    static int alreadyWarned=0;
    if(!alreadyWarned++) warning(WARN_CHARS,zipfile,"Found PLACEHOLDER_NAME in file name");
  }
  s[0]=0;
  if (!ulen) return u;
  if (zipfile && (ulen<MAX_PATHLEN) && *zipfile){
    const char *replacement=zipfile+last_slash(zipfile)+1, *dot=strchrnul(replacement,'.');
    const int replacement_l=dot-replacement;
    const char *posaddr=dot && replacement_l>5?memmem(u,ulen,replacement,replacement_l):NULL;
    if (posaddr){
      const int pos=posaddr-u;
      memcpy(s,u,pos);
      s[pos]=PLACEHOLDER_NAME;
      memcpy(s+pos+1,u+pos+replacement_l,ulen-replacement_l-pos);
      s[ulen-replacement_l+1]=0;
      return s;
    }
  }
  return u;
}
#endif //IS_DIRCACHE_OPTIMIZE_NAMES
/* Reverse of simplify_name():  Replace PLACEHOLDER_NAME by name of zipfile */
#if IS_DIRCACHE_OPTIMIZE_NAMES
static const char *unsimplify_name(char *u,const char *s,const char *zipfile){
  ASSERT(u!=NULL);  ASSERT(s!=NULL);  ASSERT(s!=u);
  char *placeholder=strchr(s,PLACEHOLDER_NAME);
  const int slen=my_strlen(s);
  if (IS_DIRCACHE_OPTIMIZE_NAMES && placeholder){
    const char *replacement=zipfile+last_slash(zipfile)+1, *dot=strchrnul(replacement,'.');
    const int pos=placeholder-s,replacement_l=dot-replacement,ulen=slen-1+replacement_l;
    if (ulen<MAX_PATHLEN){
      memcpy(u,s,pos);
      memcpy(u+pos,replacement,replacement_l);
      memcpy(u+pos+replacement_l,s+pos+1,slen-pos-1);
      u[ulen]=0;
      assert_validchars(VALIDCHARS_PATH,u,ulen,"");
      return u;
    }
  }
  return s;
}
#endif //IS_DIRCACHE_OPTIMIZE_NAMES
//////////////////////
// struct directory //
//////////////////////
static void mstore_assert_lock(struct mstore *m){
  if (m->debug_mutex){
    assert_locked(m->debug_mutex);
  }else{
    if (!m->debug_thread) m->debug_thread=(pthread_t)pthread_self();
    else ASSERT(m->debug_thread==(uint64_t)pthread_self());
  }
}
static void directory_init(uint32_t flags,struct directory *d, const char *realpath, struct rootdata *r){
#define C(field,type) if (!d->core.field) d->core.field=d->_stack_##field
  C_FILE_DATA();
#undef C
  d->dir_realpath=realpath;
  d->root=r;
  d->dir_flags=flags;
  d->core.files_l=0;
  mstore_init(&d->filenames,NULL,4096|MSTORE_OPT_MALLOC);
  d->files_capacity=DIRECTORY_FILES_CAPACITY;
}
static void directory_destroy(struct directory *d){
  if (d && (d->dir_flags&(DIRECTORY_IS_DIRCACHE|DIRECTORY_IS_HEAP))==DIRECTORY_IS_HEAP){
#define C(field,type) FREE(d->core.field)
    C_FILE_DATA();
#undef C
    MSTORE_DESTROY(&d->filenames);
  }
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
  assert_locked(mutex_dircache);
  ASSERT(n0!=NULL); ASSERT(dir!=NULL); ASSERT(!(dir->dir_flags&DIRECTORY_IS_DIRCACHE));
  if (empty_dot_dotdot(n0)) return;
  struct directory_core *d=&dir->core;
#define L d->files_l
#define B dir->files_capacity
  if (B<=L){
    B=2*L;
    const bool heap=(dir->dir_flags&DIRECTORY_IS_HEAP)!=0;
#define C(f,type)  if (d->f) d->f=heap?realloc(d->f,B*sizeof(type)):memcpy(malloc(B*sizeof(type)),d->f,L*sizeof(type));
    C_FILE_DATA();
#undef C
    dir->dir_flags|=DIRECTORY_IS_HEAP;
  }
  static char buf_for_s[MAX_PATHLEN+1];
#if IS_DIRCACHE_OPTIMIZE_NAMES
  const char *s=simplify_name(buf_for_s,n0,dir->dir_realpath);
#else
  const char *s=n0;
#endif
  const int len=pathlen_ignore_trailing_slash(s);
  ASSERT(len>0);
  if (d->fflags) d->fflags[L]=flags|(s[len]=='/'?DIRENT_ISDIR:0);
#define C(name) if (d->f##name) d->f##name[L]=name
  C(mtime);  C(size);  C(crc);  C(inode);
#undef C
  ASSERT(d->fname!=NULL);
  const char *n=d->fname[L++]=(char*)MSTORE_ADDSTR(&dir->filenames,s,len);
  ASSERT(n!=NULL);
  //ASSERT(len==strlen(n));
#undef B
#undef L
}

/////////////////////////////////////////
////////////
/// stat ///
////////////
static bool stat_maybe_cache(bool alsoSyscall, const char *path,const int path_l,struct stat *stbuf){
  uint32_t valid_seconds=0;
  const int now=deciSecondsSinceStart();
  const ht_hash_t hash=hash32(path,path_l);
  struct cached_stat *st=NULL;
  ASSERT(path!=NULL);
  ASSERT(strlen(path)>=path_l);
#if IS_STAT_CACHE
  if (0<(valid_seconds=config_file_attribute_valid_seconds(true,path,path_l))){
    LOCK(mutex_dircache,st=ht_get(&stat_ht,path,path_l,hash));
    if (st && valid_seconds){
      valid_seconds=config_file_attribute_valid_seconds(IS_STAT_READONLY(st),path,path_l);
      if (now-st->when_read_decisec<valid_seconds*10L){
#define C(f) stbuf->st_##f=st->st_##f
        C(ino);C(mtim);C(mode);C(uid);C(gid);
#undef C
        _count_stat_from_cache++;
        return true;
      }
    }
  }
#endif //IS_STAT_CACHE
  if (!alsoSyscall) return false;
  const int res=stat(path,stbuf);
  LOCK(mutex_fhdata,inc_count_getattr(path,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS));
  if (res) return false;
#define C(f) st->st_##f=stbuf->st_##f
#if IS_STAT_CACHE
  LOCK(mutex_dircache,
       if (!st) st=mstore_malloc(&_root->dircache,sizeof(struct cached_stat),8);
       C(ino);C(mtim);C(mode);C(uid);C(gid);
       st->when_read_decisec=now;
       ht_set(&stat_ht, ht_internalize_string(&ht_intern_temporarily,path,path_l,hash),path_l,hash,st);
       assert(st==ht_get(&stat_ht,path,path_l,hash));
       assert(st->st_mtim.tv_nsec==stbuf->st_mtim.tv_nsec));
#endif //IS_STAT_CACHE
#undef C
  return true;
}
///////////////////////////////////////////////////////////////////////
/// statqueue - running stat() in separate thread to avoid blocking ///
///////////////////////////////////////////////////////////////////////
#define foreach_statqueue_entry(i,q) struct statqueue_entry *q=r->statqueue;for(int i=0;i<STATQUEUE_ENTRY_N;q++,i++)
#define STATQUEUE_ADD_FOUND_SUCCESS (STATQUEUE_ENTRY_N+1)
#define STATQUEUE_ADD_FOUND_FAILURE (STATQUEUE_ENTRY_N+2)
#define STATQUEUE_ADD_NO_FREE_SLOT (STATQUEUE_ENTRY_N+3)
#define STATQUEUE_DONE(status) (status==STATQUEUE_FAILED||status==STATQUEUE_OK)
static int statqueue_add(struct rootdata *r, const char *rp, int rp_l, ht_hash_t rp_hash, struct stat *stbuf){
  assert_locked(mutex_statqueue); ASSERT(rp_l<MAX_PATHLEN);
#if IS_STAT_CACHE
  {
    LOCK(mutex_dircache,const bool ok=stat_maybe_cache(false,rp,rp_l,stbuf));
    if (ok) return 0;
  }
#endif //IS_STAT_CACHE
  const int time=deciSecondsSinceStart();
  { /* Maybe there is already this path in the queue */
    foreach_statqueue_entry(i,q){
      if (STATQUEUE_DONE(q->status) && q->time+STATQUEUE_TIMEOUT_SECONDS*10>time && q->rp_l==rp_l && q->rp_hash==rp_hash && !strncmp(q->rp,rp,MAX_PATHLEN)){
        if (q->status==STATQUEUE_OK){ *stbuf=q->stat; return STATQUEUE_ADD_FOUND_SUCCESS;}
        return STATQUEUE_ADD_FOUND_FAILURE;
        if (q->status==STATQUEUE_QUEUED && q->time+STATQUEUE_TIMEOUT_SECONDS*10/2>time) return i;
      }
    }
  }
  foreach_statqueue_entry(i,q){
    if (!q->status || q->time+STATQUEUE_TIMEOUT_SECONDS*10<time){
      memcpy(q->rp,rp,rp_l);
      q->rp[q->rp_l=rp_l]=0;
      q->rp_hash=rp_hash;
      q->status=STATQUEUE_QUEUED;
      return i;
    }
  }
  return STATQUEUE_ADD_NO_FREE_SLOT;
}
static bool _statqueue_stat(const char *path, struct stat *stbuf,struct rootdata *r){
  const int len=my_strlen(path), TRY=1000*100;
  const ht_hash_t hash=hash32(path,len);
  enum statqueue_status status=0;
  int i;
  while(true){
    LOCK(mutex_statqueue,i=statqueue_add(r,path,len,hash,stbuf));
    if (i!=STATQUEUE_ADD_NO_FREE_SLOT) break;
    usleep(STATQUEUE_TIMEOUT_SECONDS*1000L*1000/10);
  }
  if (i==STATQUEUE_ADD_FOUND_FAILURE) return false;
  if (i==STATQUEUE_ADD_FOUND_SUCCESS) return true;
  struct statqueue_entry *q=r->statqueue+i;
  RLOOP(try,TRY){
    status=0;
    if (STATQUEUE_DONE(q->status)){
      LOCK(mutex_statqueue,
           if (STATQUEUE_DONE(q->status) && hash==q->rp_hash && q->rp_l==len && !strncmp(q->rp,path,MAX_PATHLEN)){
             *stbuf=q->stat;
             status=q->status;});
    }
    if (STATQUEUE_DONE(status)){
      if (status!=STATQUEUE_OK && hash==q->rp_hash && stat_maybe_cache(true,path,len,stbuf)) {
        LOCK(mutex_statqueue,if (hash==q->rp_hash) warning(WARN_STAT,path,"STAT BEIM ZWEITEN MAL"));
      }
      return status==STATQUEUE_OK;
    }
    usleep(MAX((STATQUEUE_TIMEOUT_SECONDS*1000*1000)/TRY,1));
  }
  return false; /* Timeout */
}
static bool statqueue_stat(const char *path, struct stat *stbuf,struct rootdata *r){
  const int path_l=my_strlen(path);
  if (!r || !(r->features&ROOT_REMOTE)) return stat_maybe_cache(true,path,path_l,stbuf);
  if (stat_maybe_cache(false,path,path_l,stbuf)) return true;
  FOR(0,try,4){
    if (_statqueue_stat(path,stbuf,r)){
      if (try){
        warning(WARN_RETRY,path,"Succeeded on attempt %d\n",try);
        LOCK(mutex_fhdata,rootdata_counter_inc(path,COUNT_RETRY_STAT,r));
      }
      return true;
    }
  }
  return false;
}
//////////////////////
/// Infinity loops ///
//////////////////////
static void infloop_statqueue_start(void *arg){ pthread_start(arg,PTHREAD_STATQUEUE);}
static void infloop_dircache_start(void *arg){ pthread_start(arg,PTHREAD_DIRCACHE);}
#if IS_MEMCACHE
static void infloop_memcache_start(void *arg){
  pthread_start(arg,PTHREAD_MEMCACHE);
}
#endif //IS_MEMCACHE
static void pthread_start(struct rootdata *r,int ithread){
  r->debug_pretend_blocked[ithread]=false;
  void *(*f)(void *);
  switch(ithread){
  case PTHREAD_STATQUEUE:
    if (!(r->features&ROOT_REMOTE)) return;
    f=&infloop_statqueue;
    break;
#if IS_MEMCACHE
  case PTHREAD_MEMCACHE:
    f=&infloop_memcache;
    break;
#endif //IS_MEMCACHE
  case PTHREAD_DIRCACHE:
    f=&infloop_dircache;
    break;
  }
  if (r->pthread_count_started[ithread]++) warning(WARN_THREAD,r->root_path,"pthread_start %s ",PTHREAD_S[ithread]);
  if (pthread_create(&r->pthread[ithread],NULL,f,(void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT,r->root_path,"Failed thread_create %s root=%d ",PTHREAD_S[ithread],r->index);
}
#define IS_Q() (q->status==STATQUEUE_QUEUED)
#define W r->pthread_when_response_deciSec
static void *infloop_statqueue(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_statqueue_start,r);
  char path[MAX_PATHLEN+1];
  struct stat stbuf;
  for(int j=0;true;j++){
    foreach_statqueue_entry(i,q){
      if IS_Q(){
          ht_hash_t hash;
          int l=*path=0;
          LOCK_NCANCEL(mutex_statqueue,if IS_Q(){ memcpy(path,q->rp,(l=q->rp_l)+1);hash=q->rp_hash;});
          path[l]=0;
          if (*path){
            const bool ok=stat_maybe_cache(true,path,l,&stbuf);
            if (ok) r->pthread_when_loop_deciSec[PTHREAD_STATQUEUE]=W=deciSecondsSinceStart();
            LOCK_NCANCEL(mutex_statqueue,
                         if (IS_Q() && l==q->rp_l && hash==q->rp_hash && !memcmp(path,q->rp,l)){
                           q->time=deciSecondsSinceStart();
                           q->stat=stbuf;
                           q->status=ok?STATQUEUE_OK:STATQUEUE_FAILED;});
          }
        }
    }
    if (!(j&8191)){ LOCK_NCANCEL(mutex_fhdata,fhdata_destroy_those_that_are_marked()); }
    if (!(j&1023)){
      const int now=deciSecondsSinceStart();
      statfs(r->root_path,&r->statfs);
      r->pthread_when_loop_deciSec[PTHREAD_STATQUEUE]=deciSecondsSinceStart();
      if ((r->statfs_took_deciseconds=(W=deciSecondsSinceStart())-now)>ROOT_OBSERVE_EVERY_SECONDS*10*4) log_warn("\nstatfs %s took %'ld ms\n",r->root_path,100L*(W-now));
    }
    PRETEND_BLOCKED(PTHREAD_STATQUEUE);
    usleep(1000*1000*ROOT_OBSERVE_EVERY_SECONDS/1024);
  }
  pthread_cleanup_pop(0);
}
#undef W
#undef IS_Q
static void *infloop_unblock(void *arg){
  usleep(1000*1000);
  if (*_mkSymlinkAfterStart){
    {
      struct stat stbuf;
      lstat(_mkSymlinkAfterStart,&stbuf);
      if (((S_IFREG|S_IFDIR)&stbuf.st_mode) && !(S_IFLNK&stbuf.st_mode))
        {
          warning(WARN_MISC,""," Cannot make symlink %s => %s  because %s is a file or dir\n",_mkSymlinkAfterStart,_mnt,_mkSymlinkAfterStart);
          exit(1);
        }
    }
    const int err=symlink_overwrite_atomically(_mnt,_mkSymlinkAfterStart);
    char rp[PATH_MAX];
    if (err || !realpath(_mkSymlinkAfterStart,rp)){
      warning(WARN_MISC,_mkSymlinkAfterStart," symlink_overwrite_atomically(%s,%s); %s",_mnt,_mkSymlinkAfterStart,strerror(err));
      exit(1);
    }else if(!is_symlink(_mkSymlinkAfterStart)){
      warning(WARN_MISC,_mkSymlinkAfterStart," not a symlink");
      exit(1);
    }else{
      log_msg(GREEN_SUCCESS"Created symlink %s --> %s\n",_mkSymlinkAfterStart,rp);
    }
    *_mkSymlinkAfterStart=0;
  }
  static int threshold[PTHREAD_LEN];
  threshold[PTHREAD_STATQUEUE]=10*UNBLOCK_AFTER_SEC_THREAD_STATQUEUE;
  threshold[PTHREAD_MEMCACHE]=10*UNBLOCK_AFTER_SEC_THREAD_MEMCACHE;
  threshold[PTHREAD_DIRCACHE]=10*UNBLOCK_AFTER_SEC_THREAD_DIRCACHE;
  while(true){
    usleep(1000*1000);
    int time=deciSecondsSinceStart();
    foreach_root(i,r){
      RLOOP(t,PTHREAD_LEN){
        ASSERT(threshold[t]>0);
        if (r->pthread[t] && (r->features&ROOT_REMOTE)&& (time-r->pthread_when_loop_deciSec[t]>threshold[t])){
          warning(WARN_THREAD,r->root_path,"Going to pthread_cancel() %s\n",PTHREAD_S[t]);
          r->pthread_when_loop_deciSec[t]=time+10*threshold[t];
          pthread_cancel(r->pthread[t]);
        }
      }
    }
  }
  return NULL;
}
/////////////////////////////////////////////////////////////////////////////////////////////
/// 1. Return true for local file roots.
/// 2. Return false for remote file roots (starting with double slash) that have not responde for long time
/// 2. Wait until last respond of root is below threshold.
static bool wait_for_root_timeout(struct rootdata *r){
  if (!(r->features&ROOT_REMOTE)) return true;
  RLOOP(try,99){
    const int delay=deciSecondsSinceStart()-r->pthread_when_response_deciSec;
    if (delay>ROOT_OBSERVE_TIMEOUT_SECONDS*10) break;
    if (delay<ROOT_OBSERVE_EVERY_SECONDS*10*2) return true;
    usleep(1000*1000*ROOT_OBSERVE_EVERY_SECONDS/10);
    if (try%10==0) log_msg("<%s %d>",__func__,try);
  }
  warning(WARN_ROOT|WARN_FLAG_ONCE_PER_PATH,r->root_path,"Remote root not responding.");
  r->log_count_delayed++;
  return false;
}
//////////////////////
/////    Utils   /////
//////////////////////
/********************************************************************************/
/* *** Stat **** */
#define ST_BLKSIZE 4096
static void stat_set_dir(struct stat *s){
  if(s){
    mode_t *m=&(s->st_mode);
    ASSERT(S_IROTH>>2==S_IXOTH);
    if(!(*m&S_IFDIR)){
      s->st_size=ST_BLKSIZE;
      s->st_nlink=1;
      *m=(*m&~S_IFMT)|S_IFDIR|((*m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
    }
  }
}
static void init_stat(struct stat *st, int64_t size,struct stat *uid_gid){
  const bool isdir=size<0;
  clear_stat(st);
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
    st->st_mtim=uid_gid->st_mtim;
  }else{
    // geteuid() //returns the effective user ID of the calling process.
    // getuid() //returns the real user ID of the calling process.
    // getgid() //returns the real group ID of the calling process.
    // getegid() //returns the effective group ID of the calling process.
    st->st_gid=getgid();
    st->st_uid=getuid();
  }
}
////////////////////////////////////////////////////////////////////////////////////////////
/// zpath                                                                                ///
/// All strings like virtualpath are within strgs which initially lives on the stack     ///
/// A new one is created with zpath_newstr() and one or several calls to zpath_strncat() ///
/// The length of the created string is finally obtained with zpath_commit()             ///
////////////////////////////////////////////////////////////////////////////////////////////
static char *zpath_newstr(struct zippath *zpath){
  char *s=zpath->strgs+(zpath->current_string=++zpath->strgs_l);
  *s=0;
  return s;
}
static bool zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=min_int(my_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){ warning(WARN_LEN|WARN_FLAG_MAYBE_EXIT,"zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len); return false;}
    my_strncpy(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return true;
}
#define zpath_strcat(zpath,s)  zpath_strncat(zpath,s,9999)
static int zpath_commit(const struct zippath *zpath){
  return zpath->strgs_l-zpath->current_string;
}
#define zpath_assert_strlen(zpath)  _zpath_assert_strlen(__func__,__FILE__,__LINE__,zpath)
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath){
  bool e=false;
#define C(X)    if (my_strlen(X())!=X##_LEN() && (e=true)) log_error(#X"()!="#X"_LEN()  %u!=%d\n",my_strlen(X()),X##_LEN());
  C(VP);C(EP);
#undef C
#define C(a)  if (my_strlen(zpath->a)!=zpath->a##_l && (e=true)) log_error(#a"!=a"#a"_l   %u!=%d\n",my_strlen(zpath->a),zpath->a##_l);
  C(virtualpath);C(virtualpath_without_entry);C(entry_path);
#undef C
  if (e){
    log_zpath("Error ",zpath);
    DIE(ANSI_FG_RED"zpath_assert_strlen"ANSI_RESET" in %s at "ANSI_FG_BLUE"%s:%d\n"ANSI_RESET,fn,file,line);
  }
}
static void zpath_stack_to_heap(struct zippath *zpath){ /* When the zpath object is stored in fhdata */
  ASSERT_LOCKED_FHDATA();
  zpath->flags|=ZP_STRGS_ON_HEAP;
  const char *stack=zpath->strgs;
  if (!(zpath->strgs=(char*)malloc(zpath->strgs_l+1))) log_abort("malloc");
  memcpy(zpath->strgs,stack,zpath->strgs_l+1);
  const int64_t d=(int64_t)(zpath->strgs-stack); /* Diff new addr-old addr */
#define C(a) if (zpath->a) zpath->a+=d
  C(virtualpath); C(virtualpath_without_entry); C(entry_path); C(realpath);
#undef C
}

static void zpath_reset_keep_VP(struct zippath *zpath){ /* Reset to. Later probe a different realpath */
  VP0()=EP()=RP()=NULL;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_LEN()=EP_LEN()=zpath->rp_crc=0;
  zpath->root=-1;
  zpath->strgs_l=VP_LEN();
  zpath->flags=0;
}
static void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack){
  ASSERT(virtualpath!=NULL);
  const int l=pathlen_ignore_trailing_slash(virtualpath);
  if (!l) ASSERT(*virtualpath=='/' && !virtualpath[1]);
  zpath->virtualpath=zpath->strgs=strgs_on_stack;
  const char *NOCSC=strcasestr(virtualpath,"$NOCSC$");
  int removed=0;
  if (NOCSC){
    zpath_strncat(zpath,virtualpath,NOCSC-virtualpath);
    zpath_strcat(zpath,NOCSC+(removed=7));
  }else{
    zpath_strcat(zpath,virtualpath);
  }
  zpath->virtualpath[VP_LEN()=l-removed]=0;
  zpath_reset_keep_VP(zpath);
}
static void zpath_destroy(struct zippath *zpath){
  if (zpath){
    clear_stat(&zpath->stat_rp);
    clear_stat(&zpath->stat_vp);
    if (zpath->flags&ZP_STRGS_ON_HEAP) FREE2(zpath->strgs);
    memset(zpath,0,sizeof(struct zippath));
  }
}
static bool zpath_stat(struct zippath *zpath,struct rootdata *r){
  if (!zpath) return false;
  if (!zpath->stat_rp.st_ino){
    if (!statqueue_stat(RP(),&zpath->stat_rp,r)) return false;
    zpath->stat_vp=zpath->stat_rp;
  }
  return true;
}
static struct zip *zip_open_ro(const char *orig){
  struct zip *zip=NULL;
  if (orig){
    RLOOP(try,2){
      int err;
      if ((zip=zip_open(orig,ZIP_RDONLY,&err))) break;
      warning(WARN_OPEN,orig,"zip_open_ro err=%d",err);
      usleep(1000);
    }
  }
  return zip;
}
///////////////////////////////////
/// Is the virtualpath a zip entry?
///////////////////////////////////
static int zip_contained_in_virtual_path(const char *path, int *shorten, char *append[]){
  const int len=my_strlen(path);
  char *e,*b=(char*)path;
  int ret=0,i;
  for(i=4;i<=len;i++){
    e=(char*)path+i;
    if (i==len || *e=='/'){
      if ((ret=config_virtualpath_is_zipfile(b,e,append))!=INT_MAX){
        if (shorten) *shorten=-ret;
        ret+=i;
        break;
      }
      if (*e=='/')  b=e+1;
    }
  }
  return ret==INT_MAX?0:ret;
}
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add_file(struct directory, ...)
// Returns true on success.
////////////////////////////////////////////////////////////
static bool directory_from_dircache_zip_or_filesystem(struct directory *mydir,const struct stat *st){
  const char *rp=mydir->dir_realpath;
  if (!rp || !*rp) return false;
  bool debug=endsWithZip(rp,strlen(rp)) && (mydir->dir_flags&DIRECTORY_TO_QUEUE);
  int result=0;
  IF1(IS_DIRCACHE,LOCK_NCANCEL(mutex_dircache,result=dircache_directory_from_cache(mydir,st->st_mtim)?1:0));
  if (!result && (mydir->dir_flags&DIRECTORY_TO_QUEUE)){ /* Read zib dir asynchronously */
    LOCK_NCANCEL(mutex_dircachejobs,ht_set(&mydir->root->dircache_queue,rp,0,0,"X"));
    result=-1;
  }
  if(!result){
    result=-1;
    if (mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
      struct zip *zip=zip_open_ro(rp);
      if (zip){
        struct zip_stat sb;
        const int n=zip_get_num_entries(zip,0);
        mydir->core.finode=NULL;
        FOR(0,k,n){
          if (!zip_stat_index(zip,k,0,&sb)){
            LOCK(mutex_dircache,directory_add_file((sb.comp_method?DIRENT_IS_COMPRESSED:0),mydir,0,sb.name,sb.size,sb.mtime,sb.crc));
          }
        }
        result=1;
        zip_close(zip);
      }
    }else{
      DIR *dir=opendir(rp);
      LOCK(mutex_fhdata, inc_count_getattr(rp,dir?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL));
      if (!dir){
        perror("Unable to read directory");
      }else{
        struct dirent *de;
        mydir->core.fsize=NULL;
        mydir->core.fcrc=NULL;
        while((de=readdir(dir))){
          LOCK(mutex_dircache,directory_add_file((de->d_type==(S_IFDIR>>12))?DIRENT_ISDIR:0,mydir,de->d_ino,de->d_name,0,0,0));
        }
        result=1;
      }
      closedir(dir);
    }
    if (result>0){
      mydir->core.mtim=st->st_mtim;
#if IS_DIRCACHE
      if ((mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE) || config_cache_directory(rp,mydir->root->features&ROOT_REMOTE,st->st_mtim)){
        LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(mydir));
      }
#endif
    }
  }
  return result>0;
}
/* Reading zip dirs asynchroneously */
static void *infloop_dircache(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_dircache_start,r);
  char path[MAX_PATHLEN+1];
  struct directory mydir={0};
  struct stat stbuf;
  while(true){
    *path=0;
    struct ht_entry *ee=r->dircache_queue.entries;
    LOCK_NCANCEL(mutex_dircachejobs,
                 RLOOP(i,r->dircache_queue.capacity){
                   if (ee[i].key){
                     my_strncpy(path,ee[i].key,MAX_PATHLEN);
                     FREE2(ee[i].key);
                     ht_clear_entry(ee+i);
                     break;
                   }});
    if (*path && statqueue_stat(path,&stbuf,r)){
      directory_init(DIRECTORY_IS_ZIPARCHIVE,&mydir,path,r);
      int n=directory_from_dircache_zip_or_filesystem(&mydir,&stbuf);
    }
    r->pthread_when_loop_deciSec[PTHREAD_DIRCACHE]=deciSecondsSinceStart();
    PRETEND_BLOCKED(PTHREAD_DIRCACHE);
    usleep(1000*10);
  }
  pthread_cleanup_pop(0);
}
////////////////////////////////
/// Special Files           ///
//////////////////////////////
#define  startsWithDir_ZIPsFS(path)  !strncmp(path,DIR_ZIPsFS,DIR_ZIPsFS_L)
static bool isSpecialFile(const char *slashFn, const char *path){
  return startsWithDir_ZIPsFS(path) && !strcmp(path+DIR_ZIPsFS_L,slashFn);
}
static bool find_realpath_special_file(struct zippath *zpath){
  if (isSpecialFile(SPECIAL_FILES[SFILE_LOG_WARNINGS],VP())){
    zpath->realpath=zpath_newstr(zpath);
    zpath_strcat(zpath,_fwarn);
    if (zpath_stat(zpath,NULL)){
      zpath->stat_vp.st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
      return true;
    }
  }
  return false;
}
static int whatSpecialFile(const char *vp){
  if (startsWithDir_ZIPsFS(vp)){
    FOR(0,i,SFILE_L)  if (isSpecialFile(SPECIAL_FILES[i],vp)) return i;
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
/* Previously VP0_LEN(), EP() ,EP_LEN() etc. had been set in find_realpath_any_root().
   Constructs zpath->realpath and checks whether exists.
   Returns true on success */
static bool test_realpath(struct zippath *zpath, struct rootdata *r){
  zpath_assert_strlen(zpath);
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  zpath->root=r->index;
  {
    const char *vp0=VP0_LEN()?VP0():VP(); /* Virtual path without zip entry */
    if (!zpath_strcat(zpath,r->root_path) || strcmp(vp0,"/") && !zpath_strcat(zpath,vp0)) return false;
  }
  if (!zpath_stat(zpath,r)) return false; // xxxxx
  if (ZPATH_IS_ZIP()){
    if (!endsWithZip(RP(),0)) return false;
    if (EP_LEN()) return 0==filler_readdir_zip(r,zpath,NULL,NULL,NULL); /* This sets the file attribute of zip entry  */
  }
  return true;
}
/* Uses different approaches and calls test_realpath */
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r){
  EP()=zpath_newstr(zpath);
  zpath->flags|=ZP_ZIP;
  if (!zpath_strcat(zpath,vp+last_slash(vp)+1)) return false;
  EP_LEN()=zpath_commit(zpath);
  return test_realpath(zpath,r);
}
static bool find_realpath_any_root(struct zippath *zpath,const struct rootdata *onlyThisRoot){
  if (!onlyThisRoot && find_realpath_special_file(zpath)) return true;
  const char *vp=VP(); /* At this point, Only zpath->virtualpath is defined. */
#if IS_STATCACHE_IN_FHDATA
  if (!(zpath->flags&ZP_NEVER_STATCACHE_IN_FHDATA) && *vp){
    struct stat stbuf;
    struct zippath *zp[1]; /* Returned by stat_for_virtual_path() */
    if (stat_for_virtual_path(vp,&stbuf,zp)){
      if (!strcmp(vp,"/PRO1")) log_debug_now("stat_for_virtual_path %s \n",vp);
      zpath_reset_keep_VP(zpath);
      VP0()=zpath_newstr(zpath); zpath_strcat(zpath,zp[0]->virtualpath_without_entry); VP0_LEN()=zpath_commit(zpath);
      RP()=zpath_newstr(zpath); zpath_strcat(zpath,zp[0]->realpath);
      {
        const int ep_off=zp[0]->virtualpath_l-zp[0]->entry_path_l;
        assert(ep_off>=0);
        EP()=(char*)vp+ep_off;
        EP_LEN()=VP_LEN()-ep_off;
      }
      zpath->stat_vp=stbuf;
      zpath->stat_rp=zp[0]->stat_rp;
      zpath->flags|=ZP_ZIP;
      zpath->root=zp[0]->root;
      return true;
    }
  }
#endif //IS_STATCACHE_IN_FHDATA
  char *append="";
#if IS_ZIPINLINE_CACHE
  const char *cached_zip=zipinline_cache_virtualpath_to_zippath(vp,VP_LEN());
  if (cached_zip){
    const int l=strlen(cached_zip);
    foreach_root(ir,r){
      if (!wait_for_root_timeout(r)) continue;
      if (!strncmp(cached_zip,r->root_path,r->root_path_l) && cached_zip[r->root_path_l]=='/'){
        zpath_reset_keep_VP(zpath);
        VP0()=zpath_newstr(zpath);
        if (!zpath_strcat(zpath,cached_zip+r->root_path_l)) return false;
        VP0_LEN()=zpath_commit(zpath);
        if (find_realpath_try_inline(zpath,vp,r)) return true;
      }
    }
  }
#endif //IS_ZIPINLINE_CACHE
  foreach_root(ir,r){
    if (onlyThisRoot && onlyThisRoot!=r || !wait_for_root_timeout(r)) continue;
    if (VP_LEN()){
      { /* ZIP-file is a folder (usually ZIP-file.Content in the virtual file system. */
        zpath_reset_keep_VP(zpath);
        int shorten=0;
        const int zip_l=zip_contained_in_virtual_path(vp,&shorten,&append);
        if (zip_l){ /* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name.  */
          VP0()=zpath_newstr(zpath);
          if (!zpath_strncat(zpath,vp,zip_l) || !zpath_strcat(zpath,append)) return false;
          VP0_LEN()=zpath_commit(zpath);
          EP()=zpath_newstr(zpath);
          {
            const int pos=VP0_LEN()+shorten+1-my_strlen(append);
            if (pos<VP_LEN()) zpath_strcat(zpath,vp+pos);
          }
          EP_LEN()=zpath_commit(zpath);
          zpath->flags|=ZP_ZIP;
          zpath_assert_strlen(zpath);
          if (test_realpath(zpath,r)){
            if (!EP_LEN()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
            return true;
          }
        }
      }
      { /* ZIP-entries inlined. ZIP-file itself not shown in listing. Occurs for Sciex mass-spec */
        for(int rule=0;;rule++){
          const int len=config_zipentry_to_zipfile(rule,vp,&append);
          if (!len) break;
          zpath_reset_keep_VP(zpath);
          VP0()=zpath_newstr(zpath);
          if (!zpath_strncat(zpath,vp,len) || !zpath_strcat(zpath,append)) return false;
          VP0_LEN()=zpath_commit(zpath);
          if (find_realpath_try_inline(zpath,vp,r)) return true;
        }
      }
    }
    { /* Just a file */
      zpath_reset_keep_VP(zpath);
      if (test_realpath(zpath,r)) return true;
    }
  }/*loop root*/
  return false;
}
//2023_my_data_zip_1.d.Zip
///////////////////////////////////////////////////////////
// Data associated with file handle.
// Motivation: When the same file is accessed from two different programs,
// We see different fi->fh
// Wee use this as a key to obtain a data structure "fhdata"
//
// Conversely, fuse_get_context()->private_data returns always the same pointer address even for different file handles.
//
static const struct fhdata FHDATA_EMPTY={0};
static bool fhdata_can_destroy(struct fhdata *d){
  ASSERT_LOCKED_FHDATA();
  /* No close when serving as mem-cache for other instances  with identical path */
  return d && d->is_xmp_read<=0 && !(d->memcache_l && fhdata_by_virtualpath(d->path,d->path_hash,d,having_no_memcache));
}
static void fhdata_zip_close(bool alsoZipArchive,struct fhdata *d){
  {
    zip_file_t *z=d->zip_file;
    d->zip_file=NULL;
    if (z && zip_fclose(z)) warning(WARN_FHDATA,d->path,"Failed zip_fclose");
  }
  if (alsoZipArchive){
    struct zip *z=d->zarchive;
    d->zarchive=NULL;
    if (z && zip_close(z)==-1) warning(WARN_FHDATA,d->path,"Failed zip_close");
  }
}
static void memcache_free(struct fhdata *d){
  char *c=d->memcache;
  d->memcache=NULL;
  const size_t len=d->memcache_l;
  if (c && len){
    //log_cache("Going to release cache %p %zu\n",c,len);
    if(d->memcache_is_heap){
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
static void fhdata_destroy(struct fhdata *d){
  if (!d) return;
  ASSERT_LOCKED_FHDATA();
  if (d->memcache_status==memcache_queued) d->memcache_status=0;
  if (!fhdata_can_destroy(d) || d->memcache_status==memcache_reading){ /* When reading to RAM cache */
    d->close_later=true;
  }else{
    memcache_free(d);
    //log_msg(ANSI_FG_GREEN"Going to release fhdata d=%p path=%s fh=%lu "ANSI_RESET,d,snull(d->path),d->fh);
    FREE2(d->path);
    fhdata_zip_close(true,d);
    zpath_destroy(&d->zpath);
    FREE2(d->statcache_for_subdirs_of_path);
    pthread_mutex_destroy(&d->mutex_read);
    memset(d,0,sizeof(struct fhdata));
  }
}
static void fhdata_destroy_those_that_are_marked(){
  ASSERT_LOCKED_FHDATA();
  foreach_fhdata_path_not_null(d){
    if (d->close_later) fhdata_destroy(d);
  }
}
static zip_file_t *fhdata_zip_open(struct fhdata *d,const char *msg){
  assert_not_locked(mutex_fhdata);
  ASSERT(d->path!=NULL);
  zip_file_t *zf=d->zip_file;
  if (!zf){
    const struct zippath *zpath=&d->zpath;
    struct zip *z=d->zarchive;
    if (!z && !(d->zarchive=z=zip_open_ro(RP()))){
      warning(WARN_OPEN,RP(),"Failed zip_open");
    }else if (!(zf=d->zip_file=zip_fopen(z,EP(),ZIP_RDONLY))){
      warning(WARN_OPEN,RP(),"Failed zip_fopen");
    }
    LOCK(mutex_fhdata,fhdata_counter_inc(d,z?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL));
  }
  return zf;
}

static struct fhdata* fhdata_create(const char *path,const uint64_t fh, struct zippath *zpath){
  ASSERT_LOCKED_FHDATA();
  struct fhdata *d=NULL;
  foreach_fhdata(e) if (!e->path) d=e; /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhdata_n>=FHDATA_MAX){ warning(WARN_STR,path,"Excceeding FHDATA_MAX");return NULL;}
    d=_fhdata+_fhdata_n++;
  }
  memset(d,0,sizeof(struct fhdata));
  pthread_mutex_init(&d->mutex_read,NULL);
  d->path_hash=hash_value_strg(path);
  d->fh=fh;
  d->path=strdup(path); /* Important; This must be the  last assignment */
  zpath_stack_to_heap(zpath);
  MAYBE_ASSERT(zpath->realpath!=NULL);
  d->zpath=*zpath;
  assert(zpath->root>=0);
  d->filetypedata=filetypedata_for_ext(path,_root+zpath->root);
  return d;
}
static struct fhdata* fhdata_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHDATA();
  //log_msg(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  fhdata_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(path);
  foreach_fhdata_path_not_null(d){
    if (fh==d->fh && d->path_hash==h && !strcmp(path,d->path)) return d;
  }
  return NULL;
}
static struct fhdata *fhdata_by_virtualpath(const char *path,ht_hash_t path_hash, const struct fhdata *not_this,const enum fhdata_having having){
  ASSERT_LOCKED_FHDATA();
  if(path!=NULL){
    foreach_fhdata_path_not_null(d){
      if (d==not_this ||
          having==having_memcache && !d->memcache_status ||
          having==having_no_memcache && d->memcache_status) continue; /* Otherwise two with same path, each having a cache could stick for ever */
      if (d->path_hash==path_hash && !strcmp(path,d->path)) return d;
    }
  }
  return NULL;
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t make_inode(const ino_t inode0,const struct rootdata *r, const int entryIdx,const char *path){
  const static int SHIFT_ROOT=40, SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");;
  if (inode0<(1L<<SHIFT_ROOT) && entryIdx<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| ((r->index+1L)<<SHIFT_ROOT);
  }else{
    static struct ht hts[ROOTS]={0};
    struct ht *ht=hts+r->index;
#define key1_2 inode0,entryIdx
    static uint64_t seq=1UL<<63;
    LOCK_NCANCEL(mutex_inode,
                 if (!ht->capacity) ht_init(ht,HT_FLAG_INTKEY|16);
                 uint64_t inod=(uint64_t)ht_get_int(ht,key1_2);
                 if (!inod){ ht_set_int(ht,key1_2,(void*)(inod=++seq));_count_SeqInode++;});
    return inod;
  }
#undef key1_2
}
/* ******************************************************************************** */
/* *** Zip *** */
#define SET_STAT() init_stat(st,isdir?-1: Nth0(d.fsize,i),&zpath->stat_rp);  st->st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(d.finode,i,i),rp); zpath->rp_crc=Nth0(d.fcrc,i); \
  if (Nth0(d.fflags,i)&DIRENT_IS_COMPRESSED) zpath->flags|=ZP_IS_COMPRESSED; \
  st->st_mtime=Nth0(d.fmtime,i)
static int filler_readdir_zip(struct rootdata *r,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  log_mthd_orig();
  const char *rp=RP();
  const int ep_len=EP_LEN();
  //if (endsWithZip(rp,0)) log_entered_function("read_zipdir rp=%s filler=%p  vp=%s  entry_path=%s   ep_len=%d\n",rp,filler_maybe_null,VP(),EP(),ep_len);
  if(!ep_len && !filler_maybe_null) return 0; /* When the virtual path is a Zip file then just report success */
  if (!zpath_stat(zpath,r)) return ENOENT;
  struct directory mydir={0};
  directory_init(DIRECTORY_IS_ZIPARCHIVE,&mydir,rp,r);
  if (!directory_from_dircache_zip_or_filesystem(&mydir,&zpath->stat_rp)) return ENOENT;
  char u[MAX_PATHLEN+1];
  struct directory_core d=mydir.core;
  FOR(0,i,d.files_l){
    IF1(IS_DIRCACHE_OPTIMIZE_NAMES,{ const char *n=d.fname[i]; if (n==unsimplify_name(u,n,rp)) strcpy(u,n);});
    int len=pathlen_ignore_trailing_slash(u);
    if (len>=MAX_PATHLEN){ warning(WARN_STR,u,"Exceed MAX_PATHLEN"); continue;}
    int isdir=(Nth0(d.fflags,i)&DIRENT_ISDIR)!=0, not_at_the_first_pass=0;
    while(len){
      if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
        const int slash=last_slash(u);
        if (slash<0) break;
        u[slash]=0;
        isdir=1;
      }
      if (!(len=my_strlen(u))) break;
      /* For ZIP files, readdir_iterate gives all zip entries. They contain slashes. */
      if (filler_maybe_null){
        if (ep_len &&  /* The parent folder is not just a ZIP file but a ZIP file with a zip entry stored in zpath->entry_path */
            (len<=ep_len || strncmp(EP(),u,ep_len) || u[ep_len]!='/')) continue; /* u must start with  zpath->entry_path */
        const char *n0=(char*)u+(ep_len?ep_len+1:0); /* Remove zpath->entry_path from the left of u. Note that zpath->entry_path has no trailing slash. */
        if (strchr(n0,'/') || !config_filter_files_in_listing(rp,n0) || !config_zipentry_filter(VP0(),EP()) || ht_set(no_dups,n0,0,0,"")) continue;
        struct stat stbuf, *st=&stbuf;
        SET_STAT();
        assert_validchars(VALIDCHARS_FILE,n0,strlen(n0),"n0");
        filler_maybe_null(buf,n0,st,0,fill_dir_plus);
      }else if (ep_len==len && !strncmp(EP(),u,len)){ /* ---  filler_readdir_zip() has been called from test_realpath(). The goal is to set zpath->stat_vp --- */
        struct stat *st=&zpath->stat_vp;
        SET_STAT();
        return 0;
      }
    }// while len
  }
  return filler_maybe_null?0:ENOENT;
}
#undef SET_STAT
static int filler_readdir(struct rootdata *r,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  log_mthd_invoke();
  if (!rp || !*rp) return 0;
  if (ZPATH_IS_ZIP()) return filler_readdir_zip(r,zpath,buf,filler,no_dups);
  struct stat stbuf;
  //stat_maybe_cache(true,rp,strlen(rp),&stbuf);  if(stbuf.st_mtime!=zpath->stat_rp.st_mtime) warning(WARN_READDIR,rp,"file_mtime(rp)!=zpath->stat_rp.st_mtime");
  if (zpath->stat_rp.st_mtime){
    log_mthd_orig();
    struct stat st;
    char direct_rp[MAX_PATHLEN+1], virtual_name[MAX_PATHLEN+1];
    struct directory dir={0},dir2={0};
    directory_init(0,&dir,rp,r);
    if (directory_from_dircache_zip_or_filesystem(&dir,&zpath->stat_rp)){
      char buf_for_u[MAX_PATHLEN+1], buf_for_u2[MAX_PATHLEN+1];
      const struct directory_core d=dir.core;
      FOR(0,i,d.files_l){
        const char *u=
          IF1(IS_DIRCACHE_OPTIMIZE_NAMES,unsimplify_name(buf_for_u,d.fname[i],rp))
          IF0(IS_DIRCACHE_OPTIMIZE_NAMES,d.fname[i]);
        if (empty_dot_dotdot(u) || ht_set(no_dups,u,0,0,"")) continue;
        bool inlined=false; /* Entries of ZIP file appear directory in the parent of the ZIP-file. */
        if (config_zipentries_instead_of_zipfile(u) && (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,u))){
          directory_init(DIRECTORY_IS_ZIPARCHIVE|DIRECTORY_TO_QUEUE,&dir2,direct_rp,r);
          if (statqueue_stat(direct_rp,&stbuf,r) && directory_from_dircache_zip_or_filesystem(&dir2,&stbuf)){
            const struct directory_core d2=dir2.core;
            FOR(0,j,d2.files_l){/* Embedded zip file */
              const char *n2=d2.fname[j];
              if (n2 && !strchr(n2,'/')){
                init_stat(&st,(Nth0(d2.fflags,j)&DIRENT_ISDIR)?-1:Nth0(d2.fsize,j),&zpath->stat_rp);
                st.st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(d2.finode,j,j),RP());
                const char *u2=IF0(IS_DIRCACHE_OPTIMIZE_NAMES,n2) IF1(IS_DIRCACHE_OPTIMIZE_NAMES,unsimplify_name(buf_for_u2,n2,direct_rp));
                if (config_zipentry_to_zipfile(0,u2,NULL)) filler(buf,u2,&st,0,fill_dir_plus);
              }
            }
            inlined=true;
          }
        }
        if (!inlined){
          char *append="";
          init_stat(&st,(Nth0(d.fflags,i)&DIRENT_ISDIR)?-1:Nth0(d.fsize,i),NULL);
          st.st_ino=make_inode(zpath->stat_rp.st_ino,r,0,RP());
          if (config_filter_files_in_listing(rp,u)){
            if (config_also_show_zipfile_in_listing(u)) filler(buf,u,&st,0,fill_dir_plus);
            filler(buf,config_zipfilename_real_to_virtual(virtual_name,u),&st,0,fill_dir_plus);
          }
        }
      }
    }
    directory_destroy(&dir2);
    directory_destroy(&dir);
  }
  //  log_exited_function("impl_readdir \n");
  return 0;
}
static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;}
static int xmp_statfs(const char *path, struct statvfs *stbuf){ return minus_val_or_errno(statvfs(_root->root_path,stbuf));}
/************************************************************************************************/
/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
static int realpath_mk_parent(char *realpath,const char *path){
  if (!_writable_root) return EACCES;/* Only first root is writable */
  const int slash=last_slash(path);
  //log_entered_function("%s slash=%d  \n  ",path,slash);
  if (config_not_overwrite(path)){
    int res;FIND_REALPATH(path);
    const bool exist=!res && zpath->root>0;
    zpath_destroy(zpath);
    if (exist){ warning(WARN_OPEN|WARN_FLAG_ONCE,RP(),"It is only allowed to overwrite files in root 0");return EACCES;}
  }
  if (slash>0){
    char parent[MAX_PATHLEN+1];
    my_strncpy(parent,path,slash);
    int res;FIND_REALPATH(parent);
    if (!res){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,_writable_root->root_path),parent,MAX_PATHLEN);
      recursive_mkdir(realpath);
    }
    zpath_destroy(zpath);
    if (res) return ENOENT;
  }
  strncat(strcpy(realpath,_writable_root->root_path),path,MAX_PATHLEN);
  log_exited_function("success realpath=%s \n",realpath);
  return 0;
}
/********************************************************************************/
#define DO_LIBFUSE_CACHE_STAT 0
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  //void *x=fuse_apply_conn_info_opts;  //cfg->async_read=1;
  cfg->use_ino=1;
  IF1(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
  return NULL;
}
/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null){
  trigger_files(path);
  bool done=false;
  int err=0;
  {
    const int i=whatSpecialFile(path);
    if (i>0){ /* Start 1 to skip SFILE_LOG_WARNINGS!*/
      const char *content=special_file_content(i);
      init_stat(stbuf,content?strlen(content):_size_info,NULL);
      time(&stbuf->st_mtime);
      if (!path[DIR_ZIPsFS_L]) stat_set_dir(stbuf);
      if (i==SFILE_CTRL) stbuf->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);

      stbuf->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
      done=true;
      return 0;
    }
  }
#if DO_REMEMBER_NOT_EXISTS
  const bool remember=do_remember_not_exists(path);
  if(!done && remember){
    LOCK(mutex_fhdata,const bool not_exist=NULL!=ht_get(&remember_not_exists_ht,path,0,0));
    if (not_exist){ done=true;err=ENOENT;}
  }
#endif //DO_REMEMBER_NOT_EXISTS
  if (!done) err=stat_for_virtual_path(path,stbuf,NULL)?0:ENOENT;
  LOCK(mutex_fhdata,
       inc_count_getattr(path,err?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS);
       IF1(DO_REMEMBER_NOT_EXISTS,if (err && remember) ht_set(&remember_not_exists_ht,path,0,0,""));
       );
  return -err;
}
static int xmp_access(const char *path, int mask){
  if (whatSpecialFile(path)>=0) return 0;
  log_count_b(xmp_access);
  log_mthd_orig();
  int res;FIND_REALPATH(path);
  if (res==-1) res=ENOENT;
  if (!res && (mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) res=access(RP(),(mask&~X_OK)|R_OK);
  zpath_destroy(zpath);
  report_failure_for_tdf(res,path);
  log_count_e(xmp_access_,path);
  //log_exited_function("$s %d\n",path,res);
  LOCK(mutex_fhdata,inc_count_getattr(path,res?COUNTER_ACCESS_FAIL:COUNTER_ACCESS_SUCCESS));
  return minus_val_or_errno(res);
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  log_mthd_orig();
  int res;FIND_REALPATH(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  zpath_destroy(zpath);
  return res<0?-errno:res;
}
static int xmp_unlink(const char *path){
  log_mthd_orig();
  int res;FIND_REALPATH(path);
  if (!res) res=unlink(RP());
  zpath_destroy(zpath);
  return minus_val_or_errno(res);
}
static int xmp_rmdir(const char *path){
  log_mthd_orig();
  int res;FIND_REALPATH(path);
  if (!res) res=rmdir(RP());
  zpath_destroy(zpath);
  return minus_val_or_errno(res);
}

#define FD_ZIP_MIN (1<<20)
static int xmp_open(const char *path, struct fuse_file_info *fi){ /* Operation not permitted */
  ASSERT(fi!=NULL);
  if (fi->flags&O_WRONLY) return create_or_open(path,0775,fi);
  static ht_hash_t warn_lst;
#define C(x)  if (fi->flags&x){ ht_hash_t warn_hash=hash_value_strg(path)+x; if (warn_lst!=warn_hash) log_warn("%s %s\n",#x,path); warn_lst=warn_hash;}
  C(O_RDWR);  C(O_CREAT);
#undef C
  if (whatSpecialFile(path)>=0) return 0;
  //if (tdf_or_tdf_bin(path)) log_entered_function("%s\n",path);
  log_mthd_orig();
  log_count_b(xmp_open_);
  uint64_t handle=0;
  if (config_keep_file_attribute_in_cache(path)) fi->keep_cache=1;
  int res;FIND_REALPATH(path);
  if (!res){
    if (ZPATH_IS_ZIP()){
      static uint64_t _next_fh=FD_ZIP_MIN;
      LOCK(mutex_fhdata, if (fhdata_create(path,handle=fi->fh=_next_fh++,zpath)) zpath=NULL);
    }else{
      handle=open(RP(),fi->flags);
      if (handle<=0) warning(WARN_OPEN,RP(),"open:  fh=%d",handle);
      if (!check_path_for_fd("my_open_fh",RP(),handle)) handle=-1;
    }
  }else if (report_failure_for_tdf(res,path)){
    log_zpath("Failed ",zpath);
    warning(WARN_OPEN|WARN_FLAG_MAYBE_EXIT,path,"FIND_REALPATH res=%d",res);
  }
  zpath_destroy(zpath);
  log_count_e(xmp_open_,path);
  if (res || handle==-1){
    if (!config_not_report_stat_error(path)) warning(WARN_GETATTR,path,"res=%d handle=%llu",res,handle);
    return res=-1?-ENOENT:-errno;
  }
  fi->fh=handle;
  return 0;
}
static int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi){
  log_entered_function("%s\n",path);
  int res;
  if (fi!=NULL){
    res=ftruncate(fi->fh,size);
  }else{
    FIND_REALPATH(path);
    if (!res) res=truncate(RP(),size);
    zpath_destroy(zpath);
  }
  return minus_val_or_errno(res);
}
/////////////////////////////////
// Readdir
/////////////////////////////////
/** unsigned int cache_readdir:1; FOPEN_CACHE_DIR Can be filled in by opendir. It signals the kernel to  enable caching of entries returned by readdir(). */
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  log_count_b(xmp_readdir_);
  (void)offset;(void)fi;(void)flags;
  //log_entered_function("%s \n",path);
  log_mthd_orig();
  bool ok=false;
  struct ht no_dups={0};
  ht_init_with_keystore_dim(&no_dups,8,4096);
  foreach_root(ir,r){
    if (!wait_for_root_timeout(r)){
      warning(WARN_READDIR,path,"!wait_for_root_timeout");
      continue;
    }
    NEW_ZIPPATH(path);
    if (find_realpath_any_root(zpath,r)){
      filler_readdir(r,zpath,buf,filler,&no_dups);
      ok=true;
    }
    zpath_destroy(zpath);
  }
  if (*path=='/' && !path[1]){
    filler(buf,&DIR_ZIPsFS[1],NULL,0,fill_dir_plus);
    ok=true;
  }else if (!strcmp(path,DIR_ZIPsFS)){
    FOR(0,i,SFILE_L) filler(buf,SPECIAL_FILES[i]+1,NULL,0,fill_dir_plus);
    ok=true;
  }
  ht_destroy(&no_dups);
  log_count_e(xmp_readdir_,path);
  //log_exited_function("%s %d\n",path,ok);
  LOCK(mutex_fhdata,inc_count_getattr(path,ok?COUNTER_READDIR_SUCCESS:COUNTER_READDIR_FAIL));
  return ok?0:-1;
}
/////////////////////////////////////////////////////////////////////////////////
// With the following methods, new  new files, links, dirs etc.  are created  ///
/////////////////////////////////////////////////////////////////////////////////
static int xmp_mkdir(const char *create_path, mode_t mode){
  if (!_writable_root) return -EACCES;
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW" %s \n",create_path);
  char real_path[MAX_PATHLEN+1];
  int res=realpath_mk_parent(real_path,create_path);
  if (!res) res=mkdir(real_path,mode);
  return minus_val_or_errno(res);
}
static int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLUE""ANSI_YELLOW"=========="ANSI_RESET" %s\n",create_path);
  char real_path[MAX_PATHLEN+1];
  {
    const int res=realpath_mk_parent(real_path,create_path);
    if (res) return -res;
  }
  if (!(fi->fh=max_int(0,open(real_path,fi->flags|O_CREAT,mode)))){
    log_warn("open(%s,%x,%u) returned -1\n",real_path,fi->flags,mode); log_file_mode(mode); perror(""); log_open_flags(fi->flags); log_msg("\n");
    return -errno;
  }
  return 0;
}
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  return create_or_open(create_path,mode,fi);
}
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  if (!_writable_root) return -EACCES;
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"======== %s  fi= %p\n"ANSI_RESET,create_path,fi);
  int res=0;
  uint64_t fd;
  if(!fi){
    char real_path[MAX_PATHLEN+1];
    if ((res=realpath_mk_parent(real_path,create_path))) return -res;
    fd=max_int(0,open(real_path,O_WRONLY));
  }else{
    fd=fi->fh;
  }
  if (!fd) return -errno;
  if ((res=pwrite(fd,buf,size,offset))==-1) res=-errno;
  if (!fi) close(fd);
  return res;
}
///////////////////////////////
// Functions with two paths ///
///////////////////////////////
static int xmp_symlink(const char *target, const char *create_path){ // target,link
  if (!_writable_root) return -EACCES;
  log_entered_function("%s %s\n",target,create_path);
  char real_path[MAX_PATHLEN+1];
  int res;
  if (!(realpath_mk_parent(real_path,create_path)) && (res=symlink(target,real_path))==-1) return -errno;
  return -res;
}
static int xmp_rename(const char *old_path, const char *neu_path, uint32_t flags){ // from,to
  if (!_writable_root) return -EACCES;
  log_entered_function("from=%s to=%s \n",old_path,neu_path);
  if (flags) return -EINVAL;
  char old[MAX_PATHLEN+1],neu[MAX_PATHLEN+1];
  strcat(strcpy(old,_writable_root->root_path),old_path);
  int res=access(old,W_OK);
  if (!res && !(res=realpath_mk_parent(neu,neu_path))) res=rename(old,neu);
  return minus_val_or_errno(res);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  log_entered_function("%s %ld",path,off);
  int ret=off;
  LOCK(mutex_fhdata,
       struct fhdata* d=fhdata_get(path,fi->fh);
       if (d){
         switch(whence){
         case SEEK_DATA:
         case SEEK_SET: ret=d->offset=off;break;
         case SEEK_CUR: ret=(d->offset+=off);break;
         case SEEK_END: ret=(d->offset=d->zpath.stat_vp.st_size+off);break;
         case SEEK_HOLE:ret=(d->offset=d->zpath.stat_vp.st_size);break;
         }
       });
  if (!d) warning(WARN_SEEK|WARN_FLAG_ONCE,path,"d is NULL");
  return ret;
}

/* Read size bytes from zip entry.   Invoked only from fhdata_read() unless memcache_is_advised(d) */
static int fhdata_read_zip(const char *path, char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi){
  if (!fhdata_zip_open(d,__func__)){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"xmp_read_fhdata_zip fhdata_zip_open returned -1");
    return -1;
  }
  { /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    const long diff=((long)offset)-zip_ftell(d->zip_file);
    if (diff){
      if (zip_file_is_seekable(d->zip_file) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
      }
#if IS_MEMCACHE
      else if (diff<0 && _memcache_policy!=MEMCACHE_NEVER){ /* Worst case = seek backward - consider using cache */
        d->memcache_is_urgent=true;
        const struct fhdata *d2=memcache_waitfor(d,size,offset);
        if (d2){
          fi->direct_io=1;
          fhdata_zip_close(false,d);
          _count_readzip_memcache_because_seek_bwd++;
          LOCK(mutex_fhdata,const int num=memcache_read(buf,d2,size,offset);fhdata_counter_inc(d,ZIP_READ_CACHE_SUCCESS));
          return num;
        }
        LOCK(mutex_fhdata,fhdata_counter_inc(d,ZIP_READ_CACHE_FAIL));
      }
#endif //IS_MEMCACHE
    }
  }
  {
    const int64_t pos=zip_ftell(d->zip_file);
    if (offset<pos){ /* Worst case = seek backward - need reopen zip file */
      warning(WARN_SEEK,path,"Seek bwd - going to reopen zip\n");
      fhdata_zip_close(false,d);
      fhdata_zip_open(d,"SEEK bwd.");
    }
  }
  for(int64_t diff;1;){
    const int64_t pos=zip_ftell(d->zip_file);
    if (pos<0){ warning(WARN_SEEK,path,"zip_ftell returns %ld",pos);return -1;}
    if (!(diff=offset-pos)) break;
    if (diff<0){ warning(WARN_SEEK,path,"diff<0 should not happen %ld",diff);return -1;}
    if (zip_fread(d->zip_file,buf,MIN(size,diff))<0){
      if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,path,"zip_fread returns value<0    diff=%ld size=%zu",diff,MIN(size,diff));

      return -1;
    }
  }
  // if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_VIA_ZIP,d)){ log_msg("xmp_read_fhdata_zip %s size=%zu offset=%lu fh=%lu \n",path,size,offset,d->fh); }
  const int num=(int)zip_fread(d->zip_file,buf,size);
  if (num<0) warning(WARN_ZIP,path," zip_fread(%p,%p,%zu) returned %d\n",d->zip_file,buf,size,num);
  LOCK(mutex_fhdata,fhdata_counter_inc(d,num<0?ZIP_READ_NOCACHE_FAIL:!num?ZIP_READ_NOCACHE_ZERO:ZIP_READ_NOCACHE_SUCCESS));
  return num;
}/*xmp_read_fhdata_zip*/
///////////////////////////////////////////////////////////////////
/// Invoked from xmp_read, where struct fhdata *d=fhdata_get(path,fd);
/// Previously, in xmp_open() struct fhdata *d  had been made by fhdata_create if and only if ZPATH_IS_ZIP()
static int fhdata_read(char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi){
  ASSERT(d->is_xmp_read>0);  assert_not_locked(mutex_fhdata);
  if (size>(int)_log_read_max_size) _log_read_max_size=(int)size;
  const struct zippath *zpath=&d->zpath;
  ASSERT(ZPATH_IS_ZIP());
#if IS_MEMCACHE
  if (memcache_is_advised(d) || d->memcache_status){
    struct fhdata *d2=memcache_waitfor(d,size,offset);
    if (d2 && d2->memcache_l){
      int num=0;
      LOCK(mutex_fhdata, if (0<=(num=memcache_read(buf,d2,size,offset))) _count_readzip_memcache++);
      if(num>0 || d2->memcache_l<=offset){
        if (num<0) log_debug_now("num=%d  size=%zu offset=%zu  memcache_l=%zu \n",num,size,offset, d2->memcache_l);
        return num;
      }
    }
  }

  // size=4096 offset=76075008  memcache_l=76075008   (+ 4096 offset=76075008 76075008)
#endif //IS_MEMCACHE
  return fhdata_read_zip(d->path,buf,size,offset,d,fi);
}/*xmp_read_fhdata*/
static int64_t _debug_offset;


static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  const uint64_t fd=fi->fh;
  //log_msg("\n");log_entered_function(ANSI_BLUE" %lx "ANSI_RESET" %s size=%zu offset=%'lu next=%'lu   fh=%lu\n",pthread_self(),path,size,offset,offset+size,fd);
  log_mthd_orig();

  int res=read_special_file(path,buf,size,offset);
  if (res) return res;
  LOCK(mutex_fhdata,
       struct fhdata *d=fhdata_get(path,fd);
       if (d){
         d->accesstime=time(NULL);
         if (!d->path){
           warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"d=%p path is NULL",d);
           d=NULL;
         }else if (!d->zpath.realpath){
           ASSERT(fd==d->fh);
           log_zpath("",&d->zpath);
           warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"d->zpath.realpath is NULL  d=%p zpath= %p",d,&d->zpath);
           d=NULL;
         }
         if (d) d->is_xmp_read++; /* Prevent fhdata_destroy() */
       });
  if (d){
    { // Important for libzip. At this point we see 2 threads reading the same zip entry.
      pthread_mutex_lock(&d->mutex_read);
      res=fhdata_read(buf,size,offset,d,fi);
      pthread_mutex_unlock(&d->mutex_read);
    }
    LOCK(mutex_fhdata,
         if (res>0) d->n_read+=res;
         d->is_xmp_read--;
         const int64_t n=d->n_read;
         const char *status=MEMCACHE_STATUS_S[d->memcache_status]);
    if (res<0 && !config_not_report_stat_error(path)){
      warning(WARN_MEMCACHE|WARN_FLAG_ONCE_PER_PATH,path,"res<0:  d=%p  off=%ld size=%zu  res=%d  n_read=%llu  memcache_status=%s"ANSI_RESET,d,offset,size,res,n,status);
    }
  }else{
    if (!fd){
      res=-errno;
    }else if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",offset),log_msg("Failed %s fd=%llu\n",path,fd);
      res=-1;
    }else if ((res=pread(fd,buf,size,offset))==-1){
      res=-errno;
    }
  }
  return res;
}
static int xmp_release(const char *path, struct fuse_file_info *fi){
  //log_entered_function("path=%s\n",path);
  ASSERT(fi!=NULL);
  log_mthd_orig();
  const uint64_t fh=fi->fh;
  if (whatSpecialFile(path)>=0) return 0;
  if (fh>=FD_ZIP_MIN){
    LOCK(mutex_fhdata,struct fhdata *d=fhdata_get(path,fh);fhdata_destroy(d));
  }else if (fh>2){
    maybe_evict_from_filecache(fh,path,NULL);
    if (close(fh)){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,path,"my_close_fh %llu",fh);
      print_path_for_fd(fh);
    }
  }
  //log_exited_function(ANSI_FG_GREEN"%s fh=%llu"ANSI_RESET"\n",path,fh);
  return 0;
}
static int xmp_flush(const char *path,  struct fuse_file_info *fi){
  return whatSpecialFile(path)>0?0:
    fi->fh<FD_ZIP_MIN?fsync(fi->fh):
    0;
}
int main(int argc,char *argv[]){
  init_mutex();
  init_sighandler(argv[0],(1L<<SIGSEGV)|(1L<<SIGUSR1)|(1L<<SIGABRT),stdout);
  gettimeofday(&_startTime,NULL);
  {
    _warning_color[WARN_THREAD]=ANSI_FG_RED;
    _warning_color[WARN_GETATTR]=ANSI_FG_MAGENTA;
    _warning_color[WARN_CHARS]=ANSI_YELLOW;
    _warning_color[WARN_DEBUG]=ANSI_MAGENTA;
    for(int i=0;MY_WARNING_NAME[i];i++) _warning_channel_name[i]=(char*)MY_WARNING_NAME[i];
  }
  int colon=0;
  bool foreground=false;
  FOR(0,i,argc){
    if(!strcmp(":",argv[i])) colon=i;
    foreground|=colon && !strcmp("-f",argv[i]);
  }
  if (!colon){ log_warn("No single colon found in parameter list\n"); usage(); return 1;}
  if (colon==argc-1){ log_warn("Expect mount point after single colon \n"); usage(); return 1;}
  log_msg(ANSI_INVERSE""ANSI_UNDERLINE"This is %s  main(...)"ANSI_RESET"\nCompiled: %s %s\n",path_of_this_executable(),__DATE__,__TIME__);
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  ASSERT(S_IXOTH==(S_IROTH>>2));
  config_zipentry_to_zipfile_test();
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr); S(access);
  S(readlink);
  //S(opendir);
  S(readdir);   S(mkdir);
  S(symlink); S(unlink);
  S(rmdir);   S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  S(lseek);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log_msg("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  bool clearDB=true;
  //    argv_fuse[argc_fuse++]="--fuse-flag";    argv_fuse[argc_fuse++]="sync_read";
  for(int c;(c=getopt(argc,argv,"+qnks:c:S:l:L:"))!=-1;){  // :o:sfdh
    switch(c){
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 'S': _pretendSlow=true; break;
    case 's': strncpy(_mkSymlinkAfterStart,optarg,MAX_PATHLEN); break;
    case 'h': usage();break;
    case 'l': if ((_memcache_maxbytes=atol_kmgt(optarg))<1<<22){
        log_error("Option -l: _memcache_maxbytes is too small %s\n",optarg);
        return 1;
      }
      break;
    case 'L':{
      static struct rlimit _rlimit={0};
      _rlimit.rlim_cur=_rlimit.rlim_max=atol_kmgt(optarg);
      log_msg("Setting rlimit to %lu MB \n",_rlimit.rlim_max>>20);
      setrlimit(RLIMIT_AS,&_rlimit);
    } break;
    case 'c':{
      bool ok=false;
      for(int i=0;!ok && WHEN_MEMCACHE_S[i];i++) if((ok=!strcasecmp(WHEN_MEMCACHE_S[i],optarg))) _memcache_policy=i;
      if (!ok){ log_error("Wrong option -c %s\n",optarg); usage(); return 1;}
    } break;
    }
  }
  log_msg("FUSE_MAJOR_VERSION=%d FUSE_MINOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  ASSERT(MAX_PATHLEN<=PATH_MAX);
  char dot_ZIPsFS[MAX_PATHLEN+1];
  strncpy(_mnt,argv[argc-1],MAX_PATHLEN);
  { /* dot_ZIPsFS */
    char *dir=dot_ZIPsFS+sprintf(dot_ZIPsFS,"%s/.ZIPsFS/",getenv("HOME"));
    strcat(dir,_mnt);
    for(;*dir;dir++) if (*dir=='/') *dir='_';
    snprintf(_fwarn,MAX_PATHLEN,"%s%s",dot_ZIPsFS,SPECIAL_FILES[SFILE_LOG_WARNINGS]);
    warning(WARN_INIT,_fwarn,"");
    char ctrl[MAX_PATHLEN];
    const int v=1;
    if (0){
    FOR(1,i,v){
      sprintf(ctrl,"%s/ZIPsFS_ctrl_v%d.sh",dir,i);
      unlink(ctrl);
    }
    sprintf(ctrl,"%s/ZIPsFS_ctrl_v%d.sh",dir,v);
    FILE *f=fopen(ctrl,"w");
    fputs(
          #include "ZIPsFS_debug_ctrl.bash.inc"
          ,f);
    fclose(f);
    }
  }
  recursive_mkdir(dot_ZIPsFS);
  for(int i=optind;i<colon;i++){ /* source roots are given at command line. Between optind and colon */
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    const char *p=argv[i];
    if (!my_strlen(p)) continue;
    if (_root_n>=ROOTS){ log_error("Maximum number of roots exceeded. Increase constant ROOTS in configuration.h and recompile!\n");return 1;}
    struct rootdata *r=_root+_root_n;
    r->index=_root_n++;
    {
      int slashes=-1;while(p[++slashes]=='/');
      if (slashes>1){
        r->features|=ROOT_REMOTE;
        p+=(slashes-1);
      }
    }
    if (i==optind) (_writable_root=r)->features=ROOT_WRITABLE;
    if (!(r->root_path_l=my_strlen(r->root_path=realpath(p,NULL)))){ log_error("realpath %s   is empty\n",p);return 1;}

    sprintf(r->dircache_files,"%s/cachedir/%d",dot_ZIPsFS,i);
    recursive_mkdir(r->dircache_files);
    mstore_init(&r->dircache,r->dircache_files,DIRECTORY_CACHE_SIZE);
    r->dircache.debug_mutex=mutex_dircache;
    ht_init_with_keystore_dim(&r->dircache_ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|12,4096);
    ht_init(&r->dircache_queue,8);
    assert(r->dircache_queue.keystore==NULL);
    assert((r->dircache_queue.flags&(HT_FLAG_KEYS_ARE_STORED_EXTERN|HT_FLAG_INTKEY))==0);

    IF1(IS_DIRCACHE,cache_by_hash_init(&r->dircache_byhash_name,16,1);
        cache_by_hash_init(&r->dircache_byhash_dir,16,SIZE_POINTER));

  }
  { /* Storing information per file type tor the entire run time */
    mstore_init(&mstore_persistent,NULL,0x10000);
    ht_init_with_keystore_dim(&ht_intern_strg,HT_FLAG_INTERNALIZE_STRING|8,4096);
  }
  IF1(IS_ZIPINLINE_CACHE,ht_init(&zipinline_cache_virtualpath_to_zippath_ht,HT_FLAG_INTKEY|16));
  IF1(DO_REMEMBER_NOT_EXISTS,ht_init_with_keystore_dim(&remember_not_exists_ht,8,4096));
  IF1(IS_STAT_CACHE,ht_init(&stat_ht,0*HT_FLAG_DEBUG|16));
  ht_init_with_keystore(&ht_intern_temporarily,HT_FLAG_INTERNALIZE_STRING|20,&_root->dircache);
  log_msg("\n"ANSI_INVERSE"Roots"ANSI_RESET"\n");
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  log_print_roots(-1); log_strg("\n");
#define C(var,x) if (var!=x) log_msg(RED_WARNING"%s is %d instead of %d\n", #var,var,x)
  C(IS_DIRCACHE,1);
  C(IS_DIRCACHE_OPTIMIZE_NAMES,1);
  C(IS_MEMCACHE,1);
  C(IS_STATCACHE_IN_FHDATA,1);
  C(IS_ZIPINLINE_CACHE,1);
  C(IS_STAT_CACHE,1);
  C(PLACEHOLDER_NAME,7);
  if (DIRECTORY_CACHE_SIZE*DIRECTORY_CACHE_SEGMENTS<64*1024*1024) log_msg(RED_WARNING"Small file attribute and directory cache of only %d\n",DIRECTORY_CACHE_SEGMENTS*DIRECTORY_CACHE_SEGMENTS/1024);
#undef C
  if (!foreground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=true;

  foreach_root(i,r){ /* Note THREADS_PER_ROOT is 3 */
    RLOOP(t,THREADS_PER_ROOT) pthread_start(r,t);
  }
  static pthread_t thread_unblock;
  pthread_create(&thread_unblock,NULL,&infloop_unblock,NULL);
  const int fuse_stat=fuse_main(_fuse_argc=argc-colon,_fuse_argv=argv+colon,&xmp_oper,NULL);
  log_msg("fuse_main returned %d\n",fuse_stat);
  const char *memleak=malloc(123);
  IF1(IS_DIRCACHE,dircache_clear_if_reached_limit_all());
  exit(fuse_stat);
}
