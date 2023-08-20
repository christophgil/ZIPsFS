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

#define LOG_STREAM stdout

#include "cg_ht_v5.c"
#include "cg_log.c"
#include "cg_utils.c"
//#define PLACEHOLDER_NAME 0x1A
#define PLACEHOLDER_NAME '*'
#include "ZIPsFS_configuration.c"
#include "cg_debug.c"

#define ROOTS (1<<LOG2_ROOTS)
#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000
#define DEBUG_ABORT_MISSING_TDF 1
#define SIZE_POINTER sizeof(char *)
#define ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
///////////////////////////
//// Structs and enums ////
///////////////////////////

#define A1(x) C(x,INIT)C(x,STR)C(x,INODE)C(x,THREAD)C(x,MALLOC)C(x,ROOT)C(x,OPEN)C(x,READ)C(x,READDIR)C(x,SEEK)C(x,ZIP)C(x,GETATTR)C(x,FHDATA)C(x,CACHE)C(x,MISC)C(x,LEN)
#define A2(x) C(x,nil)C(x,queued)C(x,reading)C(x,done)
#define A3(x) C(x,NEVER)C(x,SEEK)C(x,RULE)C(x,ALWAYS)
#define A4(x) C(x,fhdata)C(x,jobs)C(x,log_count)C(x,crc)C(x,inode)C(x,memUsage)C(x,dircache)C(x,idx)C(x,roots) //* mutex_roots must be last */
#define C(x,a) x##a,
enum warnings{A1(WARN_)};
enum memcache_status{A2(memcache_)};
enum when_cache_zip{A3(CACHE_)};
enum mutex{A4(mutex_)};
#undef C
#define C(x,a) #a,
static const char *MY_WARNING_NAME[]={A1()NULL};
static const char *MEMCACHE_STATUS_S[]={A2()NULL};
static const char *WHEN_CACHE_S[]={A3()NULL};
static const char *MUTEX_S[]={A4()NULL};
#undef C
#undef A1
#undef A2
#undef A3

static int _fhdata_n=0;
enum fhdata_having{having_any,having_cache,having_no_cache};
static enum when_cache_zip _memcache_policy=CACHE_SEEK;
static bool _pretendSlow=false;
static int64_t _memcache_maxbytes=3L*1000*1000*1000;
static const char *HOMEPAGE="https://github.com/christophgil/ZIPsFS";
///////////////
/// pthread ///
///////////////
static pthread_mutex_t _mutex[mutex_roots+ROOTS];
static pthread_key_t _pthread_key;
#define DO_ASSERT_LOCK 1
#define THREADS_PER_ROOT 3
/* Count recursive locks with (_mutex+mutex). Maybe inc or dec. */
static int mutex_count(int mutex,int inc){
  int8_t *locked=pthread_getspecific(_pthread_key);
  if (!locked){
    const int R=ROOTS*THREADS_PER_ROOT+99;
    static int i=0;
    static int8_t all_locked[R][mutex_roots+ROOTS];
    pthread_mutex_lock(_mutex+mutex_idx);i++;pthread_mutex_unlock(_mutex+mutex_idx);
    log_debug_now(ANSI_RED"pthread_setspecific for %s %d"ANSI_RESET"\n",MUTEX_S[mutex],i);
    assert(i<R);
    pthread_setspecific(_pthread_key,locked=all_locked[i]);
    assert(locked!=NULL);
  }
  //log_debug_now("mutex_count %s %d \n",MUTEX_S[mutex],locked[mutex]);
  if (inc>0) assert(locked[mutex]++<127);
  if (inc<0) assert(locked[mutex]-->=0);
  return locked[mutex];
}
void lock(int mutex){
  pthread_mutex_lock(_mutex+mutex);
  if (DO_ASSERT_LOCK) mutex_count(mutex,1);
}
void unlock(int mutex){
  if (DO_ASSERT_LOCK) mutex_count(mutex,-1);
  pthread_mutex_unlock(_mutex+mutex);
}
#define SYNCHRONIZE(mutex,code) lock(mutex);code;unlock(mutex)
#define ASSERT_LOCKED_FHDATA() assert_locked(mutex_fhdata)
bool _assert_locked(int mutex,bool yesno,bool log){
  if (!DO_ASSERT_LOCK) return true;
  const int count=mutex_count(mutex,0), ok=(yesno==(count>0));
  if (log || !ok) log_debug_now("_assert_locked %s  %s: %d\n",yes_no(yesno),MUTEX_S[mutex],count);
  return ok;
}
#define assert_locked(mutex)       if (DO_ASSERT_LOCK) assert(_assert_locked(mutex,true,false));
#define assert_not_locked(mutex)   if (DO_ASSERT_LOCK) assert(_assert_locked(mutex,false,false));

#define DIRENT_ISDIR (1<<0)
#define DIRENT_IS_COMPRESSED (1<<1)

#define C_FILE_DATA_WITHOUT_NAME()    C(fsize,size_t); C(finode,uint64_t); C(fmtime,uint64_t); C(fcrc,uint32_t); C(fflags,uint8_t)
#define C_FILE_DATA()    C(fname,char *); C_FILE_DATA_WITHOUT_NAME();

#define C(field,type) type *field;
struct directory_core{
  uint64_t mtime;
  int files_l;
  C_FILE_DATA();
};
#undef C
#define S_DIRECTORY_CORE sizeof(struct directory_core)
#define DIRECTORY_FILES_CAPACITY 256 /* Capacity on stack. If exceeds then variables go to heap */
#define DIRECTORY_IS_HEAP (1<<0)
#define DIRECTORY_IS_CACHE (1<<1)
struct directory{
  uint32_t flags;
  const char *dir_realpath;
  struct rootdata *root;
#define C(field,type) type _stack_##field[DIRECTORY_FILES_CAPACITY];
  C_FILE_DATA();
#undef C
  struct directory_core core; // Only this data goes to cache.
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
  int realpath_pos; /* Position of RP in strgs */
  int current_string; /* The String that is currently build within strgs with zpath_newstr().  With zpath_commit() this marks the end of String. */
  struct stat stat_rp,stat_vp;
  uint32_t flags;
};

/* The struct fhdata holds data associated with a file descriptor.
   Stored in a linear list _fhdata. The list may have holes.
   It may also contain the cached zip entry.
   Only one of all instances with a specific virtual path should store the cached zip entry  */
struct fhdata{
  uint64_t fh; /* Serves togehter with path as key to find the instance in the linear array.*/
  char *path; /* The virtual path serves as key*/
  uint64_t path_hash; /*Serves as key*/
  struct zip *zarchive;
  zip_file_t *zip_file;
  struct zippath zpath;
  time_t accesstime;
  struct stat *cache_stat_subdirs_of_path;
  int cache_stat_subdirs_of_path_l;
  bool close_later,readzip_mutex;
  bool memcache_is_heap,memcache_is_urgent,memcache_failed;
  uint8_t already_logged;
  enum memcache_status memcache_status;
  char *memcache;  /* Zip entries are loaded into RAM */
  size_t memcache_l,memcache_already;
  int64_t memcache_took_mseconds;
  int64_t offset,n_read;
  int is_xmp_read; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
};

struct rootdata{
  char *path;
  int index; /* Index in array _roots */
  uint32_t features;
  struct statfs statfs;
  uint32_t count_delayed;
  uint32_t statfs_when_deciSec; /* When last statfs since start of program. Limitation: Overflow after ~ 5000 days */
  uint32_t statfs_took_deciseconds; /* how long did it take */
  char dircache_files[MAX_PATHLEN];
  struct mstore dircache;
  struct ht dircache_queue, dircache_ht;
  struct cache_by_hash dircache_byhash_name, dircache_byhash_dir;
  pthread_t thread_dircache,thread_observe,thread_memcache;
};
enum memusage{memusage_mmap,memusage_malloc,memusage_n,memusage_get_curr,memusage_get_peak};
#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#define Nth(array,i,defaultVal) (array?array[i]:defaultVal)
#define Nth0(array,i) Nth(array,i,0)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Consider ZIP file 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff2.Zip
// 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff.scan  will be stored as *.wiff.scan in the table-of-content
// * denotes the PLACEHOLDER_NAME
//
// This will save space in cache.  Many ZIP files will have identical table-of-content and thus kept only once in the cache.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char *simplify_name(char *s,const char *u, const char *zipfile){
  assert(u!=NULL);
  assert(s!=NULL);
  assert(s!=u);
  const int ulen=my_strlen(u);
  if (strchr(u,PLACEHOLDER_NAME)){
    static int alreadyWarned=0;
    if(!alreadyWarned++) warning(WARN_MISC,zipfile,"Found SUBSTITUTE in file name");
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
/* Reverse of simplify_name():  Replace PLACEHOLDER_NAME by name of zipfile */
static const char *unsimplify_name(char *u,const char *s,const char *zipfile){
  assert(u!=NULL);  assert(s!=NULL);  assert(s!=u);
  char *placeholder=strchr(s,PLACEHOLDER_NAME);
  const int slen=my_strlen(s);
  if (DIRECTORYCACHE_OPTIMIZE_NAMES_RESTORE && placeholder){
    const char *replacement=zipfile+last_slash(zipfile)+1, *dot=strchrnul(replacement,'.');
    const int pos=placeholder-s,replacement_l=dot-replacement,ulen=slen-1+replacement_l;
    if (ulen<MAX_PATHLEN){
      memcpy(u,s,pos);
      memcpy(u+pos,replacement,replacement_l);
      memcpy(u+pos+replacement_l,s+pos+1,slen-pos-1);
      u[ulen]=0;
      return u;
    }
  }
  return s;
}
//////////////////////
// struct directory //
//////////////////////
#if 0
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d){
  if (!d->fname){ log_error("%s %s %s: d->fname is NULL\n",__func__,func,msg);exit(9);}
  const bool print=(strchr(msg,'/')!=NULL);
  if (print) printf("\n"ANSI_INVERSE"%s Directory %s   files_l=%d\n"ANSI_RESET,func,msg,d->files_l);
  for(int i=0;i<d->files_l;i++){
    const char *n=d->fname[i];
    if (!n){ printf("%s %s: d->fname[%d] is NULL\n",__func__,func,i);exit(9);}
    const int len=strnlen(n,MAX_PATHLEN);
    if (len>=MAX_PATHLEN){ log_error("%s %s %s: strnlen d->fname[%d] is %d\n",__func__,func,msg,i,len);exit(9);}
    const char *s=Nth0(d->fname,i);
    if (print) printf(" (%d)\t%"PRIu64"\t%'zu\t%s\t%p\t%lu\n",i,Nth0(d->finode,i), Nth0(d->fsize,i),s,s,hash_value(s,strlen(s)));
  }
}
#endif


void directory_init(struct directory *d, const char *realpath, struct rootdata *r){
#define C(field,type) if (!d->core.field) d->core.field=d->_stack_##field
  C_FILE_DATA();
#undef C
  d->dir_realpath=realpath;
  d->root=r;
  d->flags=d->core.files_l=0;
  mstore_init(&d->filenames,NULL,4096|MSTORE_OPT_MALLOC);
  d->files_capacity=DIRECTORY_FILES_CAPACITY;
}
static void directory_destroy(struct directory *d){
  if (d && (d->flags&(DIRECTORY_IS_CACHE|DIRECTORY_IS_HEAP))==DIRECTORY_IS_HEAP){
#define C(field,type) FREE(d->core.field)
    C_FILE_DATA();
#undef C
    mstore_destroy(&d->filenames);
  }
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, zip_uint32_t crc){
  assert(n0!=NULL); assert(dir!=NULL); assert(!(dir->flags&DIRECTORY_IS_CACHE));
  if (empty_dot_dotdot(n0)) return;
  char buf_for_s[MAX_PATHLEN];
  const char *s=DIRECTORYCACHE_OPTIMIZE_NAMES?simplify_name(buf_for_s,n0,dir->dir_realpath):n0;
  const int len=pathlen_ignore_trailing_slash(s);
  assert(len>0);
  struct directory_core *d=&dir->core;
#define L d->files_l
#define B dir->files_capacity
  if (B<=L){
    B=2*L;
    const bool heap=(dir->flags&DIRECTORY_IS_HEAP)!=0;
#define C(f,type)  if (d->f) d->f=heap?realloc(d->f,B*sizeof(type)):memcpy(malloc(B*sizeof(type)),d->f,L*sizeof(type));
    C_FILE_DATA();
#undef C
    dir->flags|=DIRECTORY_IS_HEAP;
  }
  if (d->fflags) d->fflags[L]=flags|(s[len]=='/'?DIRENT_ISDIR:0);
#define C(name) if (d->f##name) d->f##name[L]=name
  C(size);
  C(crc);
  C(inode);
#undef C
  assert(d->fname!=NULL);
  const char *n=d->fname[L++]=(char*)mstore_addstr(&dir->filenames,s,len);
  assert(n!=NULL);
  assert(len==strlen(n));
#undef B
#undef L
}
//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
static void dircache_directory_to_cache(const struct directory *dir){
  assert_locked(mutex_dircache);
  if (!DIRECTORYCACHE_PUT) return;
  struct rootdata *r=dir->root;
  dircache_clear_if_reached_limit(r,DIRECTORY_CACHE_SEGMENTS);
  struct directory_core src=dir->core, *d=mstore_add(&r->dircache,&src,S_DIRECTORY_CORE,SIZE_T);
  for(int i=src.files_l;--i>=0;){
    const char *s=src.fname[i];
    assert(s!=NULL);
    const int len=strlen(s);
    (d->fname[i]=(char*)mstore_add_reuse(&r->dircache,s,(len+1),hash_value(s,len),&r->dircache_byhash_name))[len]=0;
  }
#define C(F,type) if (src.F) d->F=mstore_add(&r->dircache,src.F,src.files_l*sizeof(type),sizeof(type))
  C_FILE_DATA_WITHOUT_NAME();
#undef C
  /* Save some memory when directories look similar.
     src.fname s an array of *char.
     We treat it like an char array of length len and compute the hash value.
     ZIP files with identical table-of-content after applying simplify_name will have same hash.
  */
  const uint64_t len=src.files_l*SIZE_POINTER,hash=hash_value((char*)src.fname,len);
  d->fname=(char**)mstore_add_reuse(&r->dircache,(char*)d->fname,len,hash, &r->dircache_byhash_dir);
  //directory_debug_filenames(__func__,dir->dir_realpath,&src);
  //log_debug_now(ANSI_FG_MAGENTA"len=%lu  hash=%lu\n"ANSI_RESET,len,hash);
  ht_set(&r->dircache_ht,dir->dir_realpath,0,0,d);
}

static bool dircache_directory_from_cache(struct directory *dir, const int64_t mtime){
  assert_locked(mutex_dircache);
  if (!DIRECTORYCACHE_GET) return false;
  struct directory_core *s=ht_get(&dir->root->dircache_ht,dir->dir_realpath,0,0);
  if (s){
    if (mtime && s->mtime!=mtime){/* Cached data not valid any more. */
      /*  mstore_free is not implemented
          #define C(field,type) mstore_free(&dir->root->dircache,s->field,s->files_l*sizeof(type));
          C_FILE_DATA();
          #undef C
          mstore_free(&dir->root->dircache,&s,S_DIRECTORY_CORE);
      */
      ht_set(&dir->root->dircache_ht,dir->dir_realpath,0,0,NULL);
    }else{
      dir->core=*s;
      dir->flags=(dir->flags&~DIRECTORY_IS_HEAP)|DIRECTORY_IS_CACHE;
      return true;
    }
  }
  return false;
}
/////////////////////////////////////////
static int _memusage_count[2*memusage_n];
//////////////////////////////////////////////////////////////////////////
// The struct zippath is used to identify the real path from the virtual path
// All strings are stacked in char* field ->strgs.
// strgs is initially on stack and may later go to heap.
#define ZP_ZIP (1<<2)
#define ZP_STRGS_ON_HEAP (1<<3)
#define ZP_IS_COMPRESSED (1<<4)
#define ZPATH_IS_ZIP() ((zpath->flags&ZP_ZIP)!=0)
#define LOG_FILE_STAT() log_file_stat(zpath->realpath,&zpath->stat_rp),log_file_stat(zpath->virtualpath,&zpath->stat_vp)
#define VP() zpath->virtualpath
#define EP() zpath->entry_path
#define EP_LEN() zpath->entry_path_l
#define VP_LEN() zpath->virtualpath_l
#define RP() zpath->realpath
#define VP0() zpath->virtualpath_without_entry
#define VP0_LEN() zpath->virtualpath_without_entry_l
#define ZPATH_STRGS 4096
/* *** fhdata vars and defs *** */
#define FHDATA_MAX 3333
#define foreach_fhdata(d) for(struct fhdata *d=_fhdata+_fhdata_n;--d>=_fhdata;)
#define foreach_fhdata_path_not_null(d)  foreach_fhdata(d) if (d->path)
static struct fhdata _fhdata[FHDATA_MAX]={0};
#define foreach_root(ir,r)    int ir=0;for(struct rootdata *r=_root; ir<_root_n; r++,ir++)
///////////////////////////////////////////////////////////
// The root directories are specified as program arguments
// The _root[0] may be read/write, and can be empty string
// The others are read-only
//
// #include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#define FILE_FS_INFO "/_FILE_SYSTEM_INFO.HTML"
#define FILE_LOG_WARNINGS "/_FILE_SYSTEM_WARNINGS_AND_ERRORS.log"
static char _fwarn[MAX_PATHLEN];
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int _root_n=0;
static struct rootdata _root[ROOTS]={0}, *_writable_root=NULL;
//static struct ht _ht_job_readdir[ROOTS]={0};
static int _count_readzip_cached=0,_count_readzip_regular=0,_count_readzip_seekable[2]={0},_count_readzip_no_seek=0,_count_readzip_seek_fwd=0,_count_readzip_cache_because_seek_bwd=0,
  _count_readzip_reopen_because_seek_bwd=0,_log_read_max_size=0, _count_close_later=0,_countSeqInode=0, _count_getattr_from_cache=0;
static bool fhdata_has_cache(const struct fhdata *d){ return d && d->memcache && d->memcache_l;}
static clock_t _cumul_time_cache=0;
#include "ZIPsFS_debug.c"
#include "ZIPsFS_log.c"
static void *infloop_observe_root(void *arg){
  struct rootdata *r=arg;
  while(1){
    const uint32_t before=deciSecondsSinceStart();
    statfs(r->path,&r->statfs);
    if ((r->statfs_took_deciseconds=(r->statfs_when_deciSec=deciSecondsSinceStart())-before)>ROOT_OBSERVE_EVERY_DECISECONDS*4) log_warn("\nstatfs %s took %'ld ms\n",r->path,100L*(r->statfs_when_deciSec-before));
    usleep(1000*100*ROOT_OBSERVE_EVERY_DECISECONDS);
  }
}
static void start_threads(){
  pthread_key_create(&_pthread_key,NULL);
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  for(int i=mutex_roots+_root_n;--i>=0;){
    pthread_mutex_init(_mutex+i, i!=mutex_dircache? &_mutex_attr_recursive:NULL);
    pthread_mutex_init(_mutex+i,NULL);
  }
  foreach_root(i,r){ /* Note THREADS_PER_ROOT is 3 */
    if (r->features&ROOT_REMOTE && pthread_create(&r->thread_observe,NULL,&infloop_observe_root, (void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT,r->path,"Creating infloop_observe_root %d",i);
    if (pthread_create(&r->thread_dircache,NULL,&infloop_dircache, (void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT,r->path,"Creating infloop_dircache %d",i);
    if (pthread_create(&r->thread_memcache,NULL,&infloop_memcache, (void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT,r->path,"Creating infloop_memcache %d",i);
  }
}
static void dircache_clear_if_reached_limit(struct rootdata *r,size_t limit){
  if (!r) return;
  const size_t ss=mstore_count_segments(&r->dircache);
  if (ss>=limit){
    warning(WARN_CACHE,r->path,"Clearing directory cache. Cached segments: %zu bytes: %zu. Consider to increase DIRECTORY_CACHE_SEGMENTS",ss,mstore_usage(&r->dircache));
    mstore_clear(&r->dircache);
    ht_clear(&r->dircache_ht);
    cache_by_hash_clear(&r->dircache_byhash_name);
    cache_by_hash_clear(&r->dircache_byhash_dir);
  }
}
static void dircache_clear_if_reached_limit_all(){
  SYNCHRONIZE(mutex_dircache,foreach_root(i,r)  dircache_clear_if_reached_limit(r,0));
}
/////////////////////////////////////////////////////////////////////////////////////////////
/// 1. Return true for local file roots.
/// 2. Return false for remote file roots (starting with double slash) that have not responded for long time
/// 2. Wait until last respond of root is below threshold.
static bool wait_for_root_timeout(struct rootdata *r){
  bool ok=!(r->features&ROOT_REMOTE);
  for(int try=99;!ok && --try>=0;){
    const uint32_t delay=deciSecondsSinceStart()-r->statfs_when_deciSec;
    if (delay>ROOT_OBSERVE_EVERY_DECISECONDS*100) break; /* Too long. Give up */
    if (delay<ROOT_OBSERVE_EVERY_DECISECONDS*2){ ok=true; break;}
    usleep(200*100*ROOT_OBSERVE_EVERY_DECISECONDS);
    if (try%10==0) log_msg("<wait_for_root_timeout %d>",try);
  }
  if (!ok){
    warning(WARN_ROOT|WARN_FLAG_ONCE_PER_PATH,r->path,"Remote root not responding.");
    r->count_delayed++;
  }
  return ok;
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
    assert(S_IROTH>>2==S_IXOTH);
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
    st->st_mtime=uid_gid->st_mtime;
  }
}
///////////////////////////
///// file i/o cache  /////
///////////////////////////
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const char *zipentryOrNull){
  if (realpath && configuration_evict_from_filecache(realpath,zipentryOrNull)){
    const int fd=fdOrZero?fdOrZero:open(realpath,O_RDONLY);
    if (fd>0){
      posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
      //execl("/usr/bin/vmtouch","vmtouch","-v",realpath);
      if (!fdOrZero) close(fd);
    }
  }
}
//////////////////////
/////    zpath   /////
//////////////////////
static void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack){
  memset(zpath,0,sizeof(struct zippath));
  assert(virtualpath!=NULL);
  const int l=pathlen_ignore_trailing_slash(virtualpath);
  if (!l) assert(*virtualpath=='/' && !virtualpath[1]);
  zpath->virtualpath=zpath->strgs=strgs_on_stack;
  const char *NOCSC=strcasestr(virtualpath,"$NOCSC$");
  int removed=0;
  if (NOCSC){
    zpath_strncat(zpath,virtualpath,NOCSC-virtualpath);
    zpath_strcat(zpath,NOCSC+(removed=7));
  }else{
    zpath_strcat(zpath,virtualpath);
  }
  zpath->virtualpath[VP_LEN()=(l-removed)]=0;
}
#define zpath_assert_strlen(...) _zpath_assert_strlen(__func__,__VA_ARGS__)
static void _zpath_assert_strlen(const char *fn,const char *title,struct zippath *zpath){
#define C(X)    if (my_strlen(X())!=X##_LEN()){  if (title)log_msg(" %s\n",title);  log_error(#X "=%s  %u!=%d\n",X(),my_strlen(X()),X##_LEN()); log_zpath("Error ",zpath);}
  C(VP);
  C(EP);
#undef C
#define C(a) assert(my_strlen(zpath->a)==zpath->a##_l);
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
#undef C
}
static char *zpath_newstr(struct zippath *zpath){
  char *s=zpath->strgs+(zpath->current_string=++zpath->strgs_l);
  *s=0;
  return s;
}
static int zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=min_int(my_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){ warning(WARN_LEN|WARN_FLAG_MAYBE_EXIT,"zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len); return ENAMETOOLONG;}
    my_strncpy(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return 0;
}
static int zpath_strcat(struct zippath *zpath,const char *s){
  return zpath_strncat(zpath,s,9999);
}
static int zpath_commit(const struct zippath *zpath){
  return zpath->strgs_l-zpath->current_string;
}
static void zpath_stack_to_heap(struct zippath *zpath){ /* When the zpath object is stored in fhdata */
  ASSERT_LOCKED_FHDATA();
  zpath->flags|=ZP_STRGS_ON_HEAP;
  const char *stack=zpath->strgs;
  if (!(zpath->strgs=(char*)malloc(zpath->strgs_l+1))) log_abort("malloc");
  memcpy(zpath->strgs,stack,zpath->strgs_l+1);
  const int64_t d=(int64_t)(zpath->strgs-stack); /* Diff new addr-old addr */
#define C(a) if (zpath->a) zpath->a+=d
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
  C(realpath);
#undef C
}
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,-1)
void zpath_reset_realpath(struct zippath *zpath){ /* keep VP(), VP0() and EP() */
  if (!zpath) return;
  zpath->strgs_l=zpath->realpath_pos;
  clear_stat(&zpath->stat_rp);
  clear_stat(&zpath->stat_vp);
}
void zpath_reset_keep_VP(struct zippath *zpath){ /* Reset to. Later probe a different realpath */
  VP0()=EP()=RP()=NULL;
  VP0_LEN()=EP_LEN()=zpath->rp_crc=0;
  zpath->strgs_l=(int)(VP()-zpath->strgs)+VP_LEN(); /* strgs is behind VP() */
  zpath->flags=0;
}
static void zpath_destroy(struct zippath *zpath){
  if (zpath){
    zpath_reset_realpath(zpath);
    if (zpath->flags&ZP_STRGS_ON_HEAP) FREE2(zpath->strgs);
    memset(zpath,0,sizeof(struct zippath));
  }
}
static int zpath_stat(struct zippath *zpath,const struct rootdata *r){
  if (!zpath) return -1;
  int res=0;
  if (!zpath->stat_rp.st_ino){
    for(int tries=(r && r->features&ROOT_REMOTE)?3:1,try=tries;--try>=0;){
      if (!(res=stat(RP(),&zpath->stat_rp)) || !tdf_or_tdf_bin(VP())) goto ok;
      usleep(1000*100);
    }
  ok:
    if (!res){
      zpath->stat_vp=zpath->stat_rp;
      assert(zpath->stat_rp.st_ino!=0);
    }
  }
  return res;
}
//#define log_seek_ZIP(delta,...)   log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),log_msg(__VA_ARGS__)
#define log_seek(delta,...)  log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),log_msg(__VA_ARGS__)
static struct zip *zip_open_ro(const char *orig){
  if (!orig) return NULL;
  struct zip *zip=NULL;
  for(int try=2;--try>=0;){
    int err;
    if ((zip=zip_open(orig,ZIP_RDONLY,&err))) goto ok;
    warning(WARN_OPEN,orig,"zip_open_ro err=%d",err);
    usleep(1000);
  }
 ok:
  //log_exited_function("zip_open_ro %s\n",orig);
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
#define READDIR_ZIP (1<<1)
#define READDIR_TO_QUEUE (1<<2)
static bool readdir_from_cache_zip_or_dir(const int opt, struct directory *mydir,const int64_t mtime){
  const char *rp=mydir->dir_realpath;
  if (!rp) return false;
  //log_entered_function("rp=%s\n",rp);
  //LOCK(mutex_roots+r->index);
  SYNCHRONIZE(mutex_dircache,int result=dircache_directory_from_cache(mydir,mtime)?1:0);
  if (!result && (opt&READDIR_TO_QUEUE)){ /* Read zib dir asynchronously */
    SYNCHRONIZE(mutex_jobs,ht_set(&mydir->root->dircache_queue,rp,0,0,""));
    result=-1;
  }
  if(!result){
    result=-1;
    if (opt&READDIR_ZIP){
      struct zip *zip=zip_open_ro(rp);
      if (zip){
        struct zip_stat sb;
        const int n=zip_get_num_entries(zip,0);
        mydir->core.finode=NULL;
        for(int k=0; k<n; k++){
          if (!zip_stat_index(zip,k,0,&sb)){ directory_add_file((sb.comp_method?DIRENT_IS_COMPRESSED:0),mydir,0,sb.name,sb.size,sb.crc);}
        }
        result=1;
        zip_close(zip);
      }
    }else{
      DIR *dir=opendir(rp);
      if(dir==NULL) perror("Unable to read directory");
      else{
        struct dirent *de;
        mydir->core.fsize=NULL;
        mydir->core.fcrc=NULL;
        while((de=readdir(dir))){ directory_add_file((de->d_type==(S_IFDIR>>12))?DIRENT_ISDIR:0,mydir,de->d_ino,de->d_name,0,0); }
        result=1;
      }
      closedir(dir);
    }
    if (result>0){
      mydir->core.mtime=mtime;
      if ((opt&READDIR_ZIP) || config_cache_directory(rp,mydir->root->features&ROOT_REMOTE,(currentTimeMillis()-mtime)/1000)){
        SYNCHRONIZE(mutex_dircache,dircache_directory_to_cache(mydir));
        //log_debug_now(ANSI_FG_MAGENTA"Storing directory %s %zu \n\n"ANSI_RESET,rp,mstore_usage(&r->dircache));
      }
    }
  }
  //log_exited_function("result=%d\n",result);
  return result>0;
}
/* Reading zip dirs asynchroneously */
static void *infloop_dircache(void *arg){
  struct rootdata *r=arg;
  char path[MAX_PATHLEN];
  struct directory mydir={0};
  while(1){
    usleep(1000*10);
    int iteration=*path=0;
    struct ht_entry *e;
    SYNCHRONIZE(mutex_jobs,
                e=ht_next(&r->dircache_queue,&iteration);
                if (e && e->key){
                  my_strncpy(path,e->key,MAX_PATHLEN);
                  FREE2(e->key);
                  e->value=NULL;
                }
                );
    if (*path){
      directory_init(&mydir,path,r);
      readdir_from_cache_zip_or_dir(READDIR_ZIP,&mydir,file_mtime(path));
    }
  }
}
/////////////////////////////////////////////////////////////////////
//
// The following functions are used to search for a real path
// for a given virtual path,
//
////////////////////////////////////////////////////////////
//
// Acts on a zpath object for a given root
// Constructs zpath->realpath and checks whether exists.
// Returns true on success
static int test_realpath(struct zippath *zpath, const int root){
  zpath_assert_strlen("",zpath);
  zpath->strgs_l=zpath->realpath_pos; /* Discard previous reaplath */
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  zpath->root=root;
  {
    const char *vp0=VP0_LEN()?VP0():VP(); /* Virtual path without zip entry */
    if (zpath_strcat(zpath,_root[root].path) || strcmp(vp0,"/") && zpath_strcat(zpath,vp0)) return ENAMETOOLONG;
  }
  const int res=zpath_stat(zpath,_root+root);
  if (!res && ZPATH_IS_ZIP()){
    if (!ENDSWITHI(RP(),my_strlen(RP()),".zip")) return -1;
    if (EP_LEN()) return read_zipdir(_root+root,zpath,NULL,NULL,NULL);
  }
  return res;
}
// Iterates over roots and calls test_realpath()
static int test_realpath_any_root(struct zippath *zpath,const int onlyThisRoot, const int notThisRoot){
  zpath->realpath_pos=zpath->strgs_l;
  zpath_assert_strlen("",zpath);
  foreach_root(i,r){
    if (onlyThisRoot!=-1 && i!=onlyThisRoot || notThisRoot!=-1 && i==notThisRoot) continue;
    if (wait_for_root_timeout(r)){
      if (!test_realpath(zpath,i)) return 0;
      zpath_reset_realpath(zpath);
    }
  }
  return -1;
}
// The files FILE_LOG_WARNINGS
bool find_realpath_special_file(struct zippath *zpath){
  if (!strcmp(VP(),FILE_LOG_WARNINGS)){
    zpath->realpath=zpath_newstr(zpath);
    zpath_strcat(zpath,_fwarn);
    zpath_stat(zpath,NULL);
    return true;
  }
  return false;
}
// Uses different approaches and calls test_realpath_any_root
// $NOCSC$
static int find_realpath_any_root(struct zippath *zpath,const int onlyThisRoot){
  zpath_reset_keep_VP(zpath);
  if (find_realpath_special_file(zpath)) return 0;
  if (_writable_root && (onlyThisRoot<0 || onlyThisRoot==_writable_root->index) && !test_realpath_any_root(zpath,_writable_root->index,-1)) return 0;
  zpath_reset_keep_VP(zpath);
  const char *vp=VP(); /* Only zpath->virtualpath is defined */
  int res=-1;
  {  /* ZIP-file from virtual folder name. */
    char *append="";
    int shorten=0;
    const int zip_l=zip_contained_in_virtual_path(vp,&shorten,&append);
    if (zip_l){ /* Bruker MS files. The zip file name without the zip-suffix is the  folder name.  */
      VP0()=zpath_newstr(zpath);
      if (zpath_strncat(zpath,vp,zip_l) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
      VP0_LEN()=zpath_commit(zpath);
      EP()=zpath_newstr(zpath);
      {
        const int pos=VP0_LEN()+shorten+1-my_strlen(append);
        if (pos<VP_LEN()) zpath_strcat(zpath,VP()+pos);
      }
      EP_LEN()=zpath_commit(zpath);
      zpath->flags|=ZP_ZIP;
      zpath_assert_strlen("1",zpath);
      if (!(res=test_realpath_any_root(zpath,onlyThisRoot,-1))){
        if (!EP_LEN()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
      }
    }
  }
  if (res){ /* ZIP-entries inlined. ZIP-file not shown. */
    char *append="";
    for(int k=0,len;res && (len=config_zipentry_to_zipfile(k,vp,&append))!=0;k++){ /* Sciex: Zip entry is directly shown in directory listing. The zipfile is not shown as a folder. */
      zpath_reset_keep_VP(zpath);
      VP0()=zpath_newstr(zpath);
      if (zpath_strncat(zpath,vp,len) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
      VP0_LEN()=zpath_commit(zpath);
      EP()=zpath_newstr(zpath);
      zpath->flags|=ZP_ZIP;
      if (zpath_strcat(zpath,vp+last_slash(vp)+1)) return ENAMETOOLONG;
      EP_LEN()=zpath_commit(zpath);
      zpath_assert_strlen("2",zpath);
      res=test_realpath_any_root(zpath,onlyThisRoot,-1);
    }
  }
  /*DELETED_TABLE_zipfile1*/
  if (res){ /* Just a file */
    zpath_reset_keep_VP(zpath);
    zpath_assert_strlen("4",zpath);
    res=test_realpath_any_root(zpath,onlyThisRoot,0 /* root 0 was already tested at the beginning */);
  }
  return res;
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
  return d && d->is_xmp_read<=0 && !(fhdata_has_cache(d) && fhdata_by_virtualpath(d->path,d->path_hash,d,having_no_cache));
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
    log_cache("Going to release cache %p %zu\n",c,len);
    if(d->memcache_is_heap){
      FREE(c);
      trackMemUsage(memusage_malloc,-len);
    }else{
      if (munmap(c,len)){
        perror("munmap");
      }else{
        log_succes("munmap\n");
        trackMemUsage(memusage_mmap,-len);
      }
    }
    d->memcache_took_mseconds=d->memcache_l=d->memcache_status=d->memcache_failed=0;
  }
}
static void fhdata_destroy(struct fhdata *d){
  if (!d) return;
  ASSERT_LOCKED_FHDATA();
  if (d->memcache) log_entered_function("%s fhdata_can_destroy=%s\n",d->path,yes_no(fhdata_can_destroy(d)));
  if (d->memcache_status==memcache_queued) d->memcache_status=0;
  if (!fhdata_can_destroy(d) || d->memcache_status==memcache_reading){ /* When reading to RAM cache */
    d->close_later=true;
  }else{
    memcache_free(d);
    log_msg(ANSI_FG_GREEN"Going to release fhdata d=%p path=%s fh=%lu "ANSI_RESET,d,snull(d->path),d->fh);
    FREE2(d->path);
    fhdata_zip_close(true,d);
    zpath_destroy(&d->zpath);
    FREE2(d->cache_stat_subdirs_of_path);
    memset(d,0,sizeof(struct fhdata));
  }
}
void fhdata_destroy_those_that_are_marked(){
  ASSERT_LOCKED_FHDATA();
  foreach_fhdata_path_not_null(d){
    if (d->close_later) fhdata_destroy(d);
  }
}
static zip_file_t *fhdata_zip_open(struct fhdata *d,const char *msg){
  assert_not_locked(mutex_fhdata);
  assert(d->path!=NULL);
  zip_file_t *zf=d->zip_file;
  if (!zf){
    const struct zippath *zpath=&d->zpath;
    struct zip *z=d->zarchive;
    if (!z && !(d->zarchive=z=zip_open_ro(RP()))){
      warning(WARN_OPEN,RP(),"Failed zip_open");
    }else if (!(zf=d->zip_file=zip_fopen(z,EP(),ZIP_RDONLY))){
      warning(WARN_OPEN,RP(),"Failed zip_fopen");
    }
  }
  return zf;
}

static struct fhdata* fhdata_create(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHDATA();
  struct fhdata *d=NULL;
  foreach_fhdata(e) if (!e->path) d=e; /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhdata_n>=FHDATA_MAX){ warning(WARN_STR,path,"Excceeding FHDATA_MAX");return NULL;}
    d=_fhdata+_fhdata_n++;
  }
  memset(d,0,sizeof(struct fhdata));
  d->path_hash=hash_value(path,my_strlen(path));
  d->fh=fh;
  d->path=strdup(path); /* Important; This must be the  last assignment */
  return d;
}
static struct fhdata* fhdata_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHDATA();
  //log_msg(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  fhdata_destroy_those_that_are_marked();
  const uint64_t h=hash_value(path,my_strlen(path));
  foreach_fhdata_path_not_null(d){
    if (fh==d->fh && d->path_hash==h && !strcmp(path,d->path)) return d;
  }
  return NULL;
}
static struct fhdata *fhdata_by_virtualpath(const char *path,uint64_t path_hash, const struct fhdata *not_this,const enum fhdata_having having){
  ASSERT_LOCKED_FHDATA();
  if(path!=NULL){
    foreach_fhdata_path_not_null(d){
      if (d==not_this ||
          having==having_cache && !d->memcache_status ||
          having==having_no_cache && d->memcache_status) continue; /* Otherwise two with same path, each having a cache could stick for ever */
      if (d->path_hash==path_hash && !strcmp(path,d->path)) return d;
    }
  }
  return NULL;
}
/* *************************************************************/
/* With the  software we are using we observe many xmp_getattr calls when tdf_bin files are read */
/* This is a cache. We are Looking into the currently open fhdata.  */
static struct fhdata *fhdata_by_subpath(const char *path){
  ASSERT_LOCKED_FHDATA();
  const int len=my_strlen(path);
  struct fhdata *d2=NULL;
  for(int need_cache=2;--need_cache>=0;){ /* Preferably to one with a cache */
    foreach_fhdata_path_not_null(d){
      if (!need_cache || d->memcache){
        const int n=d->zpath.virtualpath_l;
        const char *vp=d->zpath.virtualpath;
        if (len<=n && vp && !strncmp(path,vp,n)){
          if (len==n||vp[len]=='/'){
            d2=d;
            if (d->cache_stat_subdirs_of_path) return d; /* Preferably one with cache_stat_subdirs_of_path */
          }
        }
      }
    }
  }
  return d2;
}
/* ******************************************************************************** */
/* *** Inode *** */
ino_t make_inode(const ino_t inode0,const int  iroot, const int ientry_plus_1,const char *path){
  const static int SHIFT_ROOT=40, SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");;
  if (inode0<(1L<<SHIFT_ROOT) && ientry_plus_1<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((int64_t)ientry_plus_1)<<SHIFT_ENTRY)| ((iroot+1L)<<SHIFT_ROOT);
  }else{
    //log_debug_now("Making inode sequentially inode0=%lu ientry=%d\n",inode0,ientry_plus_1-1);
    static struct ht hts[ROOTS]={0};
    struct ht *ht=hts+iroot;
    assert(sizeof(inode0)==8);
    const uint64_t key1=inode0+1;
    const uint64_t key2=(ientry_plus_1<<1)|1; /* Because of the implementation of hash table ht, key2 must not be zero. We multiply by 2 and add 1. */
    static uint64_t seq=1UL<<63;
    SYNCHRONIZE(mutex_inode,
                if (!ht->capacity) ht_init(ht,HT_FLAG_INTKEY,16);
                uint64_t inod=ht_get_int(ht,key1,key2);
                if (!inod){ ht_set_int(ht,key1,key2,inod=++seq);_countSeqInode++;});
    return inod;
  }
}
/* ******************************************************************************** */
/* *** Zip *** */
#define SET_STAT() init_stat(st,isdir?-1: Nth0(d.fsize,i),&zpath->stat_rp);  st->st_ino=make_inode(zpath->stat_rp.st_ino,r->index,Nth(d.finode,i,i)+1,rp); zpath->rp_crc=Nth0(d.fcrc,i); \
  if (Nth0(d.fflags,i)&DIRENT_IS_COMPRESSED) zpath->flags|=ZP_IS_COMPRESSED
static int read_zipdir(struct rootdata *r,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  log_mthd_orig();
  const char *rp=RP();
  const int ep_len=EP_LEN();
  //log_entered_function("read_zipdir rp=%s filler= %p  vp=%s  entry_path=%s   ep_len=%d\n",rp,filler_maybe_null,VP(),EP(),ep_len);
  if(!ep_len && !filler_maybe_null) return 0; /* When the virtual path is a Zip file then just report success */
  if (zpath_stat(zpath,r)) return ENOENT;
  struct directory mydir={0};
  directory_init(&mydir,rp,r);
  if (!readdir_from_cache_zip_or_dir(READDIR_ZIP,&mydir,zpath->stat_rp.st_mtime)) return ENOENT;
  char u[MAX_PATHLEN];
  struct directory_core d=mydir.core;
  for(int i=0; i<d.files_l;i++){
    {
      const char *n=d.fname[i];
      if (n==unsimplify_name(u,n,rp)) strcpy(u,n);
    }
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
        filler_maybe_null(buf,n0,st,0,fill_dir_plus);
      }else if (ep_len==len && !strncmp(EP(),u,len)){ /* ---  read_zipdir() has been called from test_realpath(). The goal is to set zpath->stat_vp --- */
        struct stat *st=&zpath->stat_vp;
        SET_STAT();
        return 0;
      }
    }// while len
  }
  return filler_maybe_null?0:ENOENT;
}
#undef SET_STAT
static int impl_readdir(struct rootdata *r,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  log_mthd_invoke();
  //log_entered_function("vp=%s rp=%s ZPATH_IS_ZIP=%d  \n",VP(),snull(rp),ZPATH_IS_ZIP());
  if (!rp || !*rp) return 0;
  if (ZPATH_IS_ZIP()) return read_zipdir(r,zpath,buf,filler,no_dups);
  if(file_mtime(rp)!=zpath->stat_rp.st_mtime) warning(WARN_READDIR,rp,"file_mtime(rp)!=zpath->stat_rp.st_mtime");
  const int64_t mtime=zpath->stat_rp.st_mtime;
  if (mtime){
    log_mthd_orig();
    struct stat st;
    char direct_rp[MAX_PATHLEN], virtual_name[MAX_PATHLEN];
    struct directory dir={0},dir2={0};
    directory_init(&dir,rp,r);
    if (readdir_from_cache_zip_or_dir(0,&dir,mtime)){
      char buf_for_u[MAX_PATHLEN], buf_for_u2[MAX_PATHLEN];
      const struct directory_core d=dir.core;
      for(int i=0;i<d.files_l;i++){
        const char *u=unsimplify_name(buf_for_u,d.fname[i],rp);
        if (empty_dot_dotdot(u) || ht_set(no_dups,u,0,0,"")) continue;
        bool inlined=false; /* Entries of ZIP file appear directory in the parent of the ZIP-file. */
        if (config_zipentries_instead_of_zipfile(u) && (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,u))){
          directory_init(&dir2,direct_rp,r);
          if (readdir_from_cache_zip_or_dir(READDIR_ZIP|READDIR_TO_QUEUE,&dir2,file_mtime(direct_rp))){
            const struct directory_core d2=dir2.core;
            for(int j=0;j<d2.files_l;j++){/* Embedded zip file */
              const char *n2=d2.fname[j];
              if (n2 && !strchr(n2,'/')){
                init_stat(&st,(Nth0(d2.fflags,j)&DIRENT_ISDIR)?-1:Nth0(d2.fsize,j),&zpath->stat_rp);
                st.st_ino=make_inode(zpath->stat_rp.st_ino,r->index,Nth(d2.finode,j,j)+1,RP());
                const char *u2=unsimplify_name(buf_for_u2,n2,direct_rp);
                filler(buf,u2,&st,0,fill_dir_plus);
              }
            }
            inlined=true;
          }
        }
        if (!inlined){
          char *append="";
          init_stat(&st,(Nth0(d.fflags,i)&DIRENT_ISDIR)?-1:Nth0(d.fsize,i),NULL);
          st.st_ino=make_inode(zpath->stat_rp.st_ino,r->index,0,RP());
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
  if (!VP_LEN()){
    for(int i=2;--i>=0;){
      const char *s=i==1?FILE_FS_INFO:FILE_LOG_WARNINGS;
      if (!ht_set(no_dups,s,0,0,"")) filler(buf,s+1,NULL,0,fill_dir_plus);
    }
  }
  //log_exited_function("impl_readdir \n");
  return 0;
}
static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;}
static int xmp_statfs(const char *path, struct statvfs *stbuf){ return minus_val_or_errno(statvfs(_root->path,stbuf));}

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
    char parent[MAX_PATHLEN];
    my_strncpy(parent,path,slash);
    int res;FIND_REALPATH(parent);
    if (!res){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,_writable_root->path),parent,MAX_PATHLEN);
      recursive_mkdir(realpath);
    }
    zpath_destroy(zpath);
    if (res) return ENOENT;
  }
  strncat(strcpy(realpath,_writable_root->path),path,MAX_PATHLEN);
  log_exited_function("success realpath=%s \n",realpath);
  return 0;
}
/********************************************************************************/
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  //void *x=fuse_apply_conn_info_opts;  //cfg->async_read=1;
  cfg->use_ino=1;
  cfg->entry_timeout=cfg->attr_timeout=cfg->negative_timeout=2.0;
  return NULL;
}
/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted
#define DO_CACHE_GETATTR 1
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null){
  if (!strcmp(path,FILE_FS_INFO)){
    init_stat(stbuf,MAX_INFO,NULL);
    time(&stbuf->st_mtime);
    return 0;
  }
  debug_triggerd_by_magic_files(path);
  memset(stbuf,0,sizeof(struct stat));
  //log_entered_function("%s \n",path);
  log_count_b(xmp_getattr_);
  const int slashes=count_slash(path);
  assert(slashes>0); /* path starts with a slash */
  int res=-1;
#define C   (d->cache_stat_subdirs_of_path)
#define L   (d->cache_stat_subdirs_of_path_l)
#define D() struct fhdata* d=fhdata_by_subpath(path); if (d && d->path && (d->zpath.flags&ZP_ZIP))
  if (DO_CACHE_GETATTR){ /* Temporary cache stat of subdirs in fhdata. Motivation: We are using software which is sending lots of requests to the FS */
    SYNCHRONIZE(mutex_fhdata,
                D(){
                  if (C){
                    assert(slashes+1<L);
                    if (C[slashes+1].st_ino){
                      *stbuf=C[slashes+1];
                      res=0;
                      _count_getattr_from_cache++;
                    }
                  }else{
                    const int slashes_vp=count_slash(d->zpath.virtualpath);
                    assert(slashes<=slashes_vp);
                    C=calloc(L=slashes_vp+2,sizeof(struct stat));
                  }
                });
  }
  if (res){ /* Not found in fhdata cache */
    FIND_REALPATH(path);
    if(!res){
      *stbuf=zpath->stat_vp;
      if (DO_CACHE_GETATTR){
        SYNCHRONIZE(mutex_fhdata, D() if (C){ assert(slashes+1<L); C[slashes+1]=*stbuf;});
      }
#undef D
#undef L
#undef C
    }
    zpath_destroy(zpath);
  }
  if (res){
    if (!config_not_report_stat_error(path)) warning(WARN_GETATTR,path,"res=%d",res);
  }else{
    if (!stbuf->st_size && tdf_or_tdf_bin(path)) warning(WARN_GETATTR|WARN_FLAG_MAYBE_EXIT,path,"xmp_getattr stbuf->st_size is zero ");
    if (!stbuf->st_ino){
      log_error("%s stbuf->st_ino is 0 \n",path);
      res=-1;
    }
  }
  log_count_e(xmp_getattr_,path);
  //log_exited_function("%s  res=%d ",path,res); log_file_stat(" ",stbuf);
  return res==-1?-ENOENT:-res;
}
static int xmp_access(const char *path, int mask){
  if (!strcmp(path,FILE_FS_INFO)) return 0;
  log_count_b(xmp_access);
  log_mthd_orig();
  int res;FIND_REALPATH(path);
  if (res==-1) res=ENOENT;
  if (!res && (mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) res=access(RP(),(mask&~X_OK)|R_OK);
  zpath_destroy(zpath);
  report_failure_for_tdf(res,path);
  log_count_e(xmp_access_,path);
  //log_exited_function("$s %d\n",path,res);
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
static int64_t trackMemUsage(const enum memusage t,int64_t a){
  static int64_t memUsage=0,memUsagePeak=0;
  if (a && t<memusage_n){
    SYNCHRONIZE(mutex_memUsage,
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
  size_t n=0;
  if (d && offset<d->memcache_already && d->memcache) memcpy(buf,d->memcache+offset,n=min_long(size,d->memcache_already-offset));
  return n;
}
static void memcache_store(struct fhdata *d){
  assert_not_locked(mutex_fhdata); assert(d->is_xmp_read>0);
  const int64_t len=d->zpath.stat_vp.st_size;
  d->memcache_already=0;
  log_cache("Going to cache d= %p %s %'ld Bytes\n",d,d->path,len);
  {
    const int64_t u=trackMemUsage(memusage_get_curr,0);
    if ((d->memcache_failed=(u+len>_memcache_maxbytes*(d->memcache_is_urgent?2:3)))){
      log_warn("_memcache_maxbytes reached: currentMem=%'ld+%'ld  _memcache_maxbytes=%'ld \n",u,len,_memcache_maxbytes);
      return;
    }
  }
  d->memcache_l=len;
  if (!len) return;
  char rp[MAX_PATHLEN],path[MAX_PATHLEN];
  strcpy(rp,d->zpath.realpath);
  strcpy(path,d->path);
  d->memcache_is_heap=(len<SIZE_CUTOFF_MMAP_vs_MALLOC);
  char *bb=d->memcache_is_heap?malloc(len):mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
  if (!bb || bb==MAP_FAILED){
    warning(WARN_MALLOC|WARN_FLAG_ONCE,"Cache %p failed using %s",(void*)mmap,d->memcache_is_heap?"malloc":"mmap");
    log_mem(LOG_STREAM);
    d->memcache_failed=true;
  }else{
    d->memcache=bb;
    trackMemUsage(d->memcache_is_heap?memusage_malloc:memusage_mmap,len);
    struct zip *za=zip_open_ro(rp);
    if (!za){
      warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_open d=%p",d);
    }else{
      zip_file_t *zf=zip_fopen(za,d->zpath.entry_path,ZIP_RDONLY);
      if (!zf){
        warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,rp,"memcache_zip_entry: Failed zip_fopen d=%p",d);
      }else{
        bool ok=true,interrupt=false;
        const int64_t start=currentTimeMillis();
        for(size_t num;  (num=min_long(MEMCACHE_READ_BYTES_NUM,len-d->memcache_already))>0;){
          if (d->close_later){
            SYNCHRONIZE(mutex_fhdata, if ((interrupt=fhdata_can_destroy(d))) memcache_free(d));
            if (interrupt) break;
          }
          int64_t n=zip_fread(zf,bb+d->memcache_already,num);
          if (!n){
            usleep(10000);
            if ((n=zip_fread(zf,bb+d->memcache_already,len-d->memcache_already))<=0) break;
            warning(WARN_CACHE,path,"Previously zip_fread() returned 0 and now it returned %ld",n);
          }
          if (n<0){ ok=false; warning(WARN_CACHE,path,"memcache_zip_entry: n<0  d=%p  read=%'zu len=%'ld",d,d->memcache_already,len);break;}
          d->memcache_already+=n;
        }/*for*/
        if (!interrupt){
          if (d->memcache_already!=len){
            warning(WARN_CACHE,path,"d->memcache_already!=len:  %zu!=%ld",d->memcache_already,len);
          }else if (d->memcache_already){
            SYNCHRONIZE(mutex_crc,const uint32_t crc=cg_crc32(bb,len,0));
            if (d->zpath.rp_crc!=crc) warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,path,"d->zpath.rp_crc (%x) != crc (%x)",d->zpath.rp_crc,crc);
          }
          d->memcache_took_mseconds=currentTimeMillis()-start;
          if(ok) log_succes("mmap d= %p %s %p  len=%zu  in %'ld mseconds\n",d,path,bb,d->memcache_already,d->memcache_took_mseconds);
          d->memcache_status=memcache_done;
          if (!ok) d->memcache_failed=true;
        }
        zip_fclose(zf);
      }/*zf!=NULL*/
      zip_close(za);
      SYNCHRONIZE(mutex_fhdata,maybe_evict_from_filecache(0,rp,path));
    }
  }
}
static void *infloop_memcache(void *arg){
  struct rootdata *r=arg;
  while(true){
    struct fhdata *d2=NULL;
    SYNCHRONIZE(mutex_fhdata,
                foreach_fhdata_path_not_null(d){
                  if (d->memcache_status==memcache_queued){ (d2=d)->memcache_status=memcache_reading;break;}
                });
    if (d2) memcache_store(d2);
    usleep(10000);
  }
}
static bool memcache_is_advised(const struct fhdata *d){
  if (!d || config_not_memcache_zip_entry(d->path) || _memcache_policy==CACHE_NEVER) return false;
  return _memcache_policy==CACHE_ALWAYS || config_store_zipentry_in_cache(d->zpath.stat_vp.st_size,d->zpath.virtualpath,(d->zpath.flags&ZP_IS_COMPRESSED)!=0);
}
/* Returns fhdata instance with enough memcache data */
static struct fhdata *memcache_waitfor(struct fhdata *d,size_t size,size_t offset){
  assert_not_locked(mutex_fhdata);
  //log_entered_function(" %s\n",d->path);
  struct fhdata *ret=NULL;
  if (MEMCACHE_PUT && d && d->path){
    assert(d->is_xmp_read>0);
    if (d->memcache_status==memcache_done){
      ret=d;
    }else{
      struct fhdata *d2=NULL;
      SYNCHRONIZE(mutex_fhdata,
                  if (d->memcache_status) d2=d;
                  else if (!(d2=fhdata_by_virtualpath(d->path,d->path_hash,NULL,having_cache))) (d2=d)->memcache_status=memcache_queued);
      for(int i=0;true;i++){
        if (d2->memcache_status==memcache_done){
          ret=d2;
        }else if (d2->memcache_already>=offset+size){
          SYNCHRONIZE(mutex_fhdata, if (d2->memcache_already>=offset+size) ret=d2);
        }
        if (ret){
          if (ret->memcache_failed) warning(WARN_CACHE,"MEMCACHE_FAILED","d=%p returning d2=%p",d,ret);
          break;
        }
        if (i%1000==999) log_debug_now("waiting %i  %s\n",i,d->path);
        usleep(10*1000);
      }
    }
    if (ret && ret->memcache_already<offset+size && ret->memcache_already!=ret->memcache_l){
      warning(WARN_CACHE,"memcache_already<offset+size","  memcache_status=%s  already=%'zu/%'zu offset=%'zu size=%'zu",MEMCACHE_STATUS_S[ret->memcache_status],ret->memcache_already,ret->memcache_l, offset,size);
    }
  }
  return ret;
}
#define FD_ZIP_MIN (1<<20)
static int xmp_open(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  if (fi->flags&O_WRONLY) return create_or_open(path,0775,fi);
  static uint64_t warn_lst;
#define C(x)  if (fi->flags&x){ uint64_t warn_hash=hash_value(path,my_strlen(path))+x; if (warn_lst!=warn_hash) log_warn("%s %s\n",#x,path); warn_lst=warn_hash;}
  C(O_RDWR);  C(O_CREAT);
#undef C
  if (!strcmp(path,FILE_FS_INFO)) return 0;
  //if (tdf_or_tdf_bin(path)) log_entered_function("%s\n",path);
  log_mthd_orig();
  log_count_b(xmp_open_);
  uint64_t handle=0;
  if (config_keep_file_attribute_in_cache(path)) fi->keep_cache=1;
  int res;FIND_REALPATH(path);
  if (!res){
    if (ZPATH_IS_ZIP()){
      static uint64_t _next_fh=FD_ZIP_MIN;
      SYNCHRONIZE(mutex_fhdata,
                  struct fhdata *d=fhdata_create(path,handle=fi->fh=_next_fh++);
                  if (d){
                    zpath_stack_to_heap(zpath);
                    ASSERT(zpath->realpath!=NULL);
                    d->zpath=*zpath;
                    zpath=NULL;
                  });
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
  if (fi!=NULL) res=ftruncate(fi->fh,size);
  else{
    FIND_REALPATH(path);
    if (!res) res=truncate(RP(),size);
    zpath_destroy(zpath);
  }
  return minus_val_or_errno(res);
}
/////////////////////////////////
//
// Readdir
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  log_count_b(xmp_readdir_);
  (void)offset;(void)fi;(void)flags;
  //log_entered_function("%s \n",path);
  log_mthd_orig();
  int res=-1;
  struct ht no_dups={0};
  ht_init_with_keystore(&no_dups,0,8,12);
  foreach_root(ir,r){
    if (!wait_for_root_timeout(r)) continue;
    NEW_ZIPPATH(path);
    if (!find_realpath_any_root(zpath,r->index)){
      impl_readdir(r,zpath,buf,filler,&no_dups);
      res=0;
    }
    zpath_destroy(zpath);
  }
  ht_destroy(&no_dups);
  log_count_e(xmp_readdir_,path);
  //log_exited_function("%s %d\n",path,res);
  return res;
}
/////////////////////////////////////////////////////////////////////////////////
// With the following methods, new  new files, links, dirs etc.  are created  ///
/////////////////////////////////////////////////////////////////////////////////
static int xmp_mkdir(const char *create_path, mode_t mode){
  if (!_writable_root) return -EACCES;
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW" %s \n",create_path);
  char real_path[MAX_PATHLEN];
  int res=realpath_mk_parent(real_path,create_path);
  if (!res) res=mkdir(real_path,mode);
  return minus_val_or_errno(res);
}
static int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLUE""ANSI_YELLOW"=========="ANSI_RESET" %s\n",create_path);
  char real_path[MAX_PATHLEN];
  const int res=realpath_mk_parent(real_path,create_path);
  if (res) return -res;
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
    char real_path[MAX_PATHLEN];
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
int xmp_symlink(const char *target, const char *create_path){ // target,link
  if (!_writable_root) return -EACCES;
  log_entered_function("%s %s\n",target,create_path);
  char real_path[MAX_PATHLEN];
  int res;
  if (!(realpath_mk_parent(real_path,create_path)) && (res=symlink(target,real_path))==-1) return -errno;
  return -res;
}
int xmp_rename(const char *old_path, const char *neu_path, uint32_t flags){ // from,to
  if (!_writable_root) return -EACCES;
  log_entered_function("from=%s to=%s \n",old_path,neu_path);
  if (flags) return -EINVAL;
  char old[MAX_PATHLEN],neu[MAX_PATHLEN];
  strcat(strcpy(old,_writable_root->path),old_path);
  int res=access(old,W_OK);
  if (!res && !(res=realpath_mk_parent(neu,neu_path))) res=rename(old,neu);
  return minus_val_or_errno(res);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  assert(fi!=NULL);
  log_entered_function("%s %ld",path,off);
  int ret=off;
  SYNCHRONIZE(mutex_fhdata,
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
/* Read size bytes from zip entry.   Invoked only from xmp_read_fhdata() unless memcache_is_advised(d) */
static int xmp_read_fhdata_zip(const char *path, char *buf, const size_t size, const off_t offset,struct fhdata *d){
  assert(d->readzip_mutex);
  if (!fhdata_zip_open(d,"xmp_read")){warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"xmp_read_fhdata_zip fhdata_zip_open returned -1");return -1;}
  { /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    const int64_t diff=offset-zip_ftell(d->zip_file);
    if (!diff){
      _count_readzip_no_seek++;
    }else if (zip_file_is_seekable(d->zip_file) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
      _count_readzip_seekable[diff>0]++;
    }else if (diff<0 && _memcache_policy!=CACHE_NEVER){ /* Worst case = seek backward - consider using cache */
      d->memcache_is_urgent=true;
      const struct fhdata *d2=memcache_waitfor(d,size,offset);
      if (d2){
        fhdata_zip_close(false,d);
        _count_readzip_cache_because_seek_bwd++;
        SYNCHRONIZE(mutex_fhdata,int num=memcache_read(buf,d2,size,offset)); return num;
      }
    }
  }
  {
    const int64_t pos=zip_ftell(d->zip_file);
    if (offset<pos){ /* Worst case = seek backward - need reopen zip file */
      warning(WARN_SEEK,path,"Seek backword - going to reopen zip\n");
      _count_readzip_reopen_because_seek_bwd++;
      fhdata_zip_close(false,d);
      fhdata_zip_open(d,"SEEK bwd.");
    }
  }
  for(int64_t diff;1;){
    const int64_t pos=zip_ftell(d->zip_file);
    if (pos<0){ warning(WARN_SEEK,path,"zip_ftell returns %ld",pos);return -1;}
    if (!(diff=offset-pos)) break;
    if (diff<0){ warning(WARN_SEEK,path,"diff<0 should not happen %ld",diff);return -1;}
    _count_readzip_seek_fwd++;
    if (zip_fread(d->zip_file,buf,MIN(size,diff))<0){
      if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,path,"zip_fread returns value<0    diff=%ld size=%zu",diff,MIN(size,diff));
      return -1;
    }
  }
  // if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_VIA_ZIP,d)){ log_msg("xmp_read_fhdata_zip %s size=%zu offset=%lu fh=%lu \n",path,size,offset,d->fh); }
  const int num=zip_fread(d->zip_file,buf,size);
  if (num<0) warning(WARN_ZIP,path," zip_fread(%p,%p,%zu) returned %d\n",d->zip_file,buf,size,num);
  return num;
}/*xmp_read_fhdata_zip*/
///////////////////////////////////////////////////////////////////
/// Invoked from xmp_read, where struct fhdata *d=fhdata_get(path,fd);
/// Previously, in xmp_open() struct fhdata *d  had been made by fhdata_create if and only if ZPATH_IS_ZIP()
static int xmp_read_fhdata(char *buf, const size_t size, const off_t offset,struct fhdata *d){
  assert(d->is_xmp_read>0);
  assert_not_locked(mutex_fhdata);
  // THINK_ABOUT_LOCK !!!!
  if (size>(int)_log_read_max_size) _log_read_max_size=(int)size;
  int num=-1;
  const struct zippath *zpath=&d->zpath;
  assert(ZPATH_IS_ZIP());
  if (memcache_is_advised(d)){
    struct fhdata *d2=memcache_waitfor(d,size,offset);
    if (!fhdata_has_cache(d2)){
      warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,d->path,"_xmp_read: No cache for d=%p",d);
      log_debug_now("d=%p d2=%p, memcache=%p  memcache_already=%'zu/%'zu size=%'zu offset=%'zu\n",d,d2,d->memcache,d->memcache_already,d->memcache_l,size,offset);
    }
    SYNCHRONIZE(mutex_fhdata,num=memcache_read(buf,d2,size,offset));
  }
  if(num>=0){
    _count_readzip_cached++;
  }else{
    _count_readzip_regular++;
    SYNCHRONIZE(mutex_fhdata,while(d->readzip_mutex) usleep(10*1000);   d->readzip_mutex=true);
    num=xmp_read_fhdata_zip(d->path,buf,size,offset,d);
    d->readzip_mutex=false;
  }
  return num;
}/*xmp_read_fhdata*/
static int64_t _debug_offset;
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  assert(fi!=NULL);
  const uint64_t fd=fi->fh;
  //log_entered_function(ANSI_BLUE" %lx "ANSI_RESET" %s size=%zu offset=%'lu   fh=%lu\n",pthread_self(),path,size,offset,fd);
  log_mthd_orig();
  if (!strcmp(path,FILE_FS_INFO)){
    const int i=min_int(MAX_INFO,print_all_info()); // length of info text
    const int n=max_int(0,min_int(i-offset,min_int(size,i-(int)offset))); // num bytes to be copied
    if (n) memcpy(buf,_info+offset,n);
    return n;
  }
  SYNCHRONIZE(mutex_fhdata,
              struct fhdata *d=fhdata_get(path,fd);
              if (d){
                d->accesstime=time(NULL);
                if (!d->path){
                  warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"d=%p path is NULL",d);
                  d=NULL;
                }else if (!d->zpath.realpath){
                  assert(fd==d->fh);
                  log_zpath("",&d->zpath);
                  warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"d->zpath.realpath is NULL  d=%p zpath= %p",d,&d->zpath);
                  d=NULL;
                }
                if (d) d->is_xmp_read++; /* Prevent fhdata_destroy() */
              });
  int res=-1;
  if (d){
    res=xmp_read_fhdata(buf,size,offset,d);
    SYNCHRONIZE(mutex_fhdata,
                if (res>0) d->n_read+=res;
                d->is_xmp_read--;
                const int64_t n=d->n_read;
                const char *status=MEMCACHE_STATUS_S[d->memcache_status]);
    if (res<0 && !config_not_report_stat_error(path)){
      warning(WARN_CACHE|WARN_FLAG_ONCE_PER_PATH,path,"res<0:  d=%p  off=%ld size=%zu  res=%d  n_read=%llu  memcache_status=%s"ANSI_RESET,d,offset,size,res,n,status);
    }
  }else{
    if (!fd){
      res=-errno;
    }else if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_seek(offset,"Failed %s fd=%llu\n",path,fd);
      res=-1;
    }else if ((res=pread(fd,buf,size,offset))==-1){
      res=-errno;
    }
  }
  //log_exited_function(" %s offset=%'lu  res=%d\n",path,offset,res);
  return res;
}
static int xmp_release(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  log_mthd_orig();
  const uint64_t fh=fi->fh;
  if (fh>=FD_ZIP_MIN){
    SYNCHRONIZE(mutex_fhdata,fhdata_destroy(fhdata_get(path,fh)));
  }else if (fh>2){
    maybe_evict_from_filecache(fh,path,NULL);
    if (close(fh)){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,path,"my_close_fh %llu",fh);
      print_path_for_fd(fh);
    }
  }
  log_exited_function(ANSI_FG_GREEN"%s fh=%llu"ANSI_RESET"\n",path,fh);
  return 0;
}
static int xmp_flush(const char *path, struct fuse_file_info *fi){
  const uint64_t fh=fi->fh;
  return fh<FD_ZIP_MIN?fsync(fh):0;
}

int main(int argc,char *argv[]){
  _thisPrg=argv[0];
  _startTimeMillis=currentTimeMillis();
  _timeAtStart=time(NULL);
  for(int i=0;MY_WARNING_NAME[i];i++) _warning_channel_name[i]=(char*)MY_WARNING_NAME[i];
  //_warning_pfx[WARN

  int colon=0;
  bool foreground=false;
  for(int i=0;i<argc;i++){
    if(!strcmp(":",argv[i])) colon=i;
    foreground|=colon && !strcmp("-f",argv[i]);
  }
  if (!colon){ log_warn("No single colon found in parameter list\n"); usage(); return 1;}
  if (colon==argc-1){ log_warn("Expect mount point after single colon \n"); usage(); return 1;}

  log_msg(ANSI_INVERSE""ANSI_UNDERLINE"This is %s  main(...)"ANSI_RESET"\nCompiled: %s %s\n",path_of_this_executable(),__DATE__,__TIME__);
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  assert(S_IXOTH==(S_IROTH>>2));
  config_zipentry_to_zipfile_test();
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr); S(access);
  S(readlink);S(readdir);   S(mkdir);
  S(symlink); S(unlink);
  S(rmdir);   S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  S(lseek);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log_msg("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  bool clearDB=true;
  //    argv_fuse[argc_fuse++]="--fuse-flag";    argv_fuse[argc_fuse++]="sync_read";
  for(int c;(c=getopt(argc,argv,"+qnksc:l:L:"))!=-1;){  // :o:sfdh
    switch(c){
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 's': _pretendSlow=true; break;
    case 'h': usage();break;
    case 'l':
      if ((_memcache_maxbytes=atol_kmgt(optarg))<1<<22){ log_error("Option -l: _memcache_maxbytes is too small %s\n",optarg); exit(1);}
      break;
    case 'L':{
      static struct rlimit _rlimit={0};
      _rlimit.rlim_cur=_rlimit.rlim_max=atol_kmgt(optarg);
      log_msg("Setting rlimit to %lu MB \n",_rlimit.rlim_max>>20);
      setrlimit(RLIMIT_AS,&_rlimit);
    } break;
    case 'c':{
      bool ok=false;
      for(int i=0;!ok && WHEN_CACHE_S[i];i++) if((ok=!strcasecmp(WHEN_CACHE_S[i],optarg))) _memcache_policy=i;
      if (!ok){ log_error("Wrong option -c %s\n",optarg); usage(); return 1;}
    } break;
    }
  }
  log_msg("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  assert(MAX_PATHLEN<=PATH_MAX);
  char dot_ZIPsFS[MAX_PATHLEN], *mnt=argv[argc-1];
  { /* dot_ZIPsFS */
    char *s=dot_ZIPsFS+sprintf(dot_ZIPsFS,"%s/.ZIPsFS/",getenv("HOME"));
    strcat(s,mnt);
    for(;*s;s++) if (*s=='/') *s='_';
    snprintf(_fwarn,MAX_PATHLEN,"%s%s",dot_ZIPsFS,FILE_LOG_WARNINGS);
    warning(WARN_INIT,_fwarn,"");
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
    if (!my_strlen(r->path=realpath(p,NULL))){ log_error("Real path of non-empty path must not be empty: '%s'\n",p);return 1;}
    sprintf(r->dircache_files,"%s/cachedir/%d",dot_ZIPsFS,i);
    recursive_mkdir(r->dircache_files);
    mstore_init(&r->dircache,r->dircache_files,DIRECTORY_CACHE_SIZE);
    ht_init_with_keystore(&r->dircache_ht,0,12,12);
    ht_init(&r->dircache_queue,0,8);
    cache_by_hash_init(&r->dircache_byhash_name,16,1);
    cache_by_hash_init(&r->dircache_byhash_dir,16,SIZE_POINTER);
  }
  log_msg("\n"ANSI_INVERSE"Roots"ANSI_RESET"\n");
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  print_roots(-1); log_strg("\n");
#define C(var,x) if (var!=x) log_msg(RED_WARNING"%s is %d instead of %d\n", #var,var,x)
  C(DIRECTORYCACHE_PUT,1);
  C(DIRECTORYCACHE_GET,1);
  C(DIRECTORYCACHE_OPTIMIZE_NAMES,1);
  C(DIRECTORYCACHE_OPTIMIZE_NAMES_RESTORE,1);
  C(MEMCACHE_PUT,1);
  C(MEMCACHE_GET,1);
  C(PLACEHOLDER_NAME,7);
#undef C
  start_threads();
  if (!foreground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=true;
  const int fuse_stat=fuse_main(_fuse_argc=argc-colon,_fuse_argv=argv+colon,&xmp_oper,NULL);
  log_msg("fuse_main returned %d\n",fuse_stat);
  const char *memleak=malloc(123);
  dircache_clear_if_reached_limit_all();
  exit(fuse_stat);
}
