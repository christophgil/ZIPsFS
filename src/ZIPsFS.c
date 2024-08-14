/*
  ZIPsFS   Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  It has been developed starting with  fuse-3.14: Filesystem in Userspace  passthrough.c
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS_notes.org  log.c
*/
// (defun Copy_working() (interactive) (shell-command (concat  (file-name-directory (buffer-file-name) ) "Copy_working.sh")))
// (buffer-filepath)
//__asm__(".symver realpath,realpath@GLIBC_2.2.5");

#define FUSE_USE_VERSION 31
#undef _GNU_SOURCE
#include <sys/types.h> // ????
#include <unistd.h> /// ???? lseek

 #include <sys/mman.h>
 #ifndef MAP_ANONYMOUS
 #define MAP_ANONYMOUS MAP_ANON
 #endif  // !MAP_ANONYMOUS
#include "cg_os_dependencies.h"
#include "config.h"
#include <dirent.h>
#include <sys/un.h>
#include <fuse.h>

#include <sys/syscall.h>
#include <zip.h>

#include <stdatomic.h>
#include "ZIPsFS_configuration.h"

#if IS_NETBSD
 #include <sys/param.h>
     #include <sys/mount.h>
#endif //IS_NETBSD


// statvfs statfs
#if WITH_EXTRA_ASSERT
#define ASSERT(...) (assert(__VA_ARGS__))
#else
#define ASSERT(...)
#endif

#ifndef FUSE_MAJOR_V
#define FUSE_MAJOR_V 9
#define FUSE_MINOR_V 999
#endif

#include "cg_utils.h"
#if ! WITH_DIRCACHE
#undef WITH_DIRCACHE_OPTIMIZE_NAMES
#define WITH_DIRCACHE_OPTIMIZE_NAMES 0
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif

#if ! WITH_ZIPINLINE
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif


#define exit_ZIPsFS(res) { _exit_ZIPsFS();EXIT(res);}

// printf puts putchar stdout
#define A1(x) C(x,INIT)C(x,MISC)C(x,STR)C(x,INODE)C(x,THREAD)C(x,MALLOC)C(x,ROOT)C(x,OPEN)C(x,READ)C(x,ZIP_FREAD)C(x,READDIR)C(x,SEEK)C(x,ZIP)C(x,GETATTR)C(x,STAT)C(x,FHDATA)C(x,DIRCACHE)C(x,MEMCACHE)C(x,FORMAT)C(x,DEBUG)C(x,CHARS)C(x,RETRY)C(x,AUTOGEN)C(x,LEN)
#define A2(x) C(x,nil)C(x,queued)C(x,reading)C(x,done)
#define A3(x) C(x,NEVER)C(x,SEEK)C(x,RULE)C(x,COMPRESSED)C(x,ALWAYS)
#define A4(x) C(x,nil)C(x,mutex_count)C(x,mstore_init)C(x,fhdata)C(x,autogen)C(x,autogen_init)C(x,dircachejobs)C(x,log_count)C(x,crc)C(x,inode)C(x,memUsage)C(x,dircache)C(x,idx)C(x,statqueue)C(x,validchars)C(x,special_file)C(x,validcharsdir)C(x,textbuffer_usage)C(x,roots) //* mutex_roots must be last */
#define A5(x) C(x,NIL)C(x,QUEUED)C(x,FAILED)C(x,OK)
#define A6(x) C(x,DIRCACHE)C(x,MEMCACHE)C(x,STATQUEUE)C(x,RESPONDING)C(x,MISC0)C(x,LEN)
#define C(x,a) x##a,

#define EXT_CONTENT  ".Content"
enum warnings{A1(WARN_)};
IF1(WITH_MEMCACHE,enum memcache_status{A2(memcache_)});
enum when_memcache_zip{A3(MEMCACHE_)};
enum mutex{A4(mutex_)};
enum statqueue_status{A5(STATQUEUE_)};
enum root_thread{A6(PTHREAD_)};
#undef C
#define C(x,a) #a,
static const char *MY_WARNING_NAME[]={A1()NULL};
IF1(WITH_MEMCACHE,static const char *MEMCACHE_STATUS_S[]={A2()NULL});
IF1(WITH_MEMCACHE,static const char *WHEN_MEMCACHE_S[]={A3()NULL});
static const char *MUTEX_S[]={A4()NULL};
static const char *STATQUEUE_STATUS_S[]={A5()NULL};
static const char *PTHREAD_S[]={A6()NULL};
static char _self_exe[PATH_MAX+1];

#undef C
#undef A1
#undef A2
#undef A3

//#define PLACEHOLDER_NAME '*' // Better for debugging
#define PLACEHOLDER_NAME 0x07

#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000
#define DEBUG_ABORT_MISSING_TDF 1
#define SIZE_POINTER sizeof(char *)
#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
#define IS_STAT_READONLY(st) !(st.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH))
///////////////////////////
//// Structs and enums ////
///////////////////////////
static int _fhdata_n=0,_mnt_l=0, _wait_for_root_timeout_sleep=1;
static enum when_memcache_zip _memcache_policy=MEMCACHE_SEEK;
static bool _pretendSlow=false;
static int64_t _memcache_maxbytes=3L*1000*1000*1000;
static char _mkSymlinkAfterStart[MAX_PATHLEN+1]={0},_mnt[MAX_PATHLEN+1];

#define HOMEPAGE "https://github.com/christophgil/ZIPsFS"
///////////////
/// pthread ///
///////////////
#define ROOTS (1<<LOG2_ROOTS)
#define NUM_MUTEX   (mutex_roots+ROOTS)
#include "cg_pthread.c"
#include "cg_utils.c"
#include "cg_log.c"
#include "cg_debug.c"
#include "cg_ht_v7.c"
#include "cg_cpu_usage_pid.c"
#include "cg_textbuffer.c"


static int _oldstate_not_needed;
#define DIRENT_ISDIR (1<<0)
#define DIRENT_IS_COMPRESSED (1<<1)
#define C_FILE_DATA_WITHOUT_NAME()    C(fsize,off_t); C(finode,uint64_t); C(fmtime,uint64_t); C(fcrc,uint32_t); C(fflags,uint8_t);
#define C_FILE_DATA()    C(fname,char *); C_FILE_DATA_WITHOUT_NAME();
#define C(field,type) type *field;
#define DEBUG_DIRECTORY_HASH32_OF_FNAME(s)    hash32((char*)s->fname,s->files_l*sizeof(char*))
struct directory_core{
  struct timespec mtim;
  int files_l;
  C_FILE_DATA();
};
#undef C
#define DIRECTORY_FILES_CAPACITY 256 /* Capacity on stack. If exceeds then variables go to heap */
#define DIRECTORY_IS_DIRCACHE (1<<1)
#define DIRECTORY_IS_ZIPARCHIVE (1<<2)
#define DIRECTORY_TO_QUEUE (1<<3)
#define ROOTd(d) (d->zpath.root)
struct directory{
  uint32_t dir_flags;
  const char *dir_realpath;
  int dir_realpath_l;
  struct rootdata *root;
#define C(field,type) type _stack_##field[DIRECTORY_FILES_CAPACITY];
  C_FILE_DATA();
#undef C
  struct directory_core core; // Only this data goes to dircache.
  struct mstore filenames;
  int files_capacity;
};
struct zippath{
#define C(s) int s,s##_l
  char strgs[ZPATH_STRGS];  int strgs_l; /* Contains all strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  C(virtualpath); /* The path in the FUSE fs relative to the mount point */
  C(virtualpath_without_entry);  /*  Let Virtualpath be "/foo.zip.Content/bar". virtualpath_without_entry will be "/foo.zip.Content". */
  C(entry_path); /* Let Virtualpath be "/foo.zip.Content/bar". entry_path will be "bar". */
  C(realpath); /* The physical path of the file. In case of a ZIP entry, the path of the ZIP-file */
#undef C
  ht_hash_t virtualpath_hash,virtualpath_without_entry_hash,realpath_hash;
  zip_uint32_t zipcrc32;
  struct rootdata *root;
  int current_string; /* The String that is currently build within strgs with zpath_newstr().  With zpath_commit() this marks the end of String. */
  struct stat stat_rp,stat_vp;
  uint32_t flags;
};
static void zpath_reset_keep_VP(struct zippath *zpath);
#define ZP_STARTS_AUTOGEN (1<<2)
#define ZP_ZIP (1<<3)
#define ZP_DOES_NOT_EXIST (1<<4)
#define ZP_IS_COMPRESSED (1<<5)
#define ZP_VERBOSE (1<<6)
#define ZP_VERBOSE2 (1<<7)
//#define ZP_TRANSIENT_CACHE_ASSUME_IS_ZIPENTRY (1<<8) #define ZP_TRANSIENT_CACHE_IS_PARENT_OF_ZIP (1<<9)

#define ZPATH_IS_ZIP() ((zpath->flags&ZP_ZIP)!=0)
#define ZPATH_IS_VERBOSE() false //((zpath->flags&ZP_VERBOSE)!=0)
#define ZPATH_IS_VERBOSE2() false //((zpath->flags&ZP_VERBOSE2)!=0)
#define ZPATH_ROOT_WRITABLE() (0!=(zpath->root->features&ROOT_WRITABLE))
#define LOG_FILE_STAT() cg_log_file_stat(zpath->realpath,&zpath->stat_rp),log_file_stat(zpath->virtualpath,&zpath->stat_vp)
#define VP() (zpath->strgs+zpath->virtualpath)
#define EP() (zpath->strgs+zpath->entry_path)
#define EP_L() zpath->entry_path_l
#define VP_L() zpath->virtualpath_l
#define RP() (zpath->strgs+zpath->realpath)
#define RP_L() (zpath->realpath_l)
#define VP0() (zpath->strgs+zpath->virtualpath_without_entry)
#define VP0_L() zpath->virtualpath_without_entry_l
#define D_VP(d) (d->zpath.strgs+d->zpath.virtualpath)
#define D_VP0(d) (d->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP0(d) (d->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP_HASH(d) d->zpath.virtualpath_hash
#define D_EP(d) (d->zpath.strgs+d->zpath.entry_path)
#define D_EP_L(d) d->zpath.entry_path_l
#define D_VP_L(d) d->zpath.virtualpath_l
#define D_RP(d) (d->zpath.strgs+d->zpath.realpath)
#define D_RP_HASH(d) (d->zpath.realpath_hash)
#define D_RP_L(d) (d->zpath.realpath_l)

static int zpath_strlen(struct zippath *zpath,int s){
  return s==0?0:strlen(zpath->strgs+s);
}

static bool zpath_exists(struct zippath *zpath){
  if (!zpath) return false;
  const bool ex=zpath->stat_rp.st_ino;
  if (ex != (zpath->realpath_l!=0)){
    log_verbose(RED_ERROR" exists: %d zpath->realpath_l: %d",ex,zpath->realpath_l);
  }
  return ex;
}
#define NEW_ZIPPATH(virtpath)  struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath);  found=find_realpath_any_root(0,zpath,NULL);
#define FINDRP_VERBOSE (1<<0)
#define FINDRP_AUTOGEN_CUT_NOT (1<<2)
#define FINDRP_NOT_TRANSIENT_CACHE (1<<3)
enum enum_count_getattr{
  COUNTER_STAT_FAIL,COUNTER_STAT_SUCCESS,
  COUNTER_OPENDIR_FAIL,COUNTER_OPENDIR_SUCCESS,
  COUNTER_ZIPOPEN_FAIL,COUNTER_ZIPOPEN_SUCCESS,
  COUNTER_GETATTR_FAIL,COUNTER_GETATTR_SUCCESS,
  COUNTER_ACCESS_FAIL,COUNTER_ACCESS_SUCCESS,
  COUNTER_READDIR_SUCCESS,COUNTER_READDIR_FAIL,
  COUNTER_ROOTDATA_INITIALIZED,enum_count_getattr_length};
enum enum_counter_rootdata{
  ZIP_OPEN_SUCCESS,ZIP_OPEN_FAIL,
  //
  ZIP_READ_NOCACHE_SUCCESS,ZIP_READ_NOCACHE_ZERO, ZIP_READ_NOCACHE_FAIL,
  ZIP_READ_NOCACHE_SEEK_SUCCESS,ZIP_READ_NOCACHE_SEEK_FAIL,
  //
  ZIP_READ_CACHE_SUCCESS,ZIP_READ_CACHE_FAIL,
  ZIP_READ_CACHE_CRC32_SUCCESS,ZIP_READ_CACHE_CRC32_FAIL,
  //
  COUNT_RETRY_STAT,
  COUNT_RETRY_MEMCACHE,
  COUNT_RETRY_ZIP_FREAD,
  //
  COUNT_MEMCACHE_INTERRUPT,
  //
  counter_rootdata_num};
#define FILETYPEDATA_NUM 1024
#define FILETYPEDATA_FREQUENT_NUM 64
struct counter_rootdata{
  const char *ext;
  atomic_uint counts[counter_rootdata_num];
  long rank;
  clock_t wait;
};
typedef struct counter_rootdata counter_rootdata_t;


#define FHDATA_FLAGS_ACTIVE (1<<0)
#define FHDATA_FLAGS_DESTROY_LATER (1<<1)
#define FHDATA_FLAGS_INTERRUPTED (1<<2)
#define FHDATA_FLAGS_HEAP (1<<3) /* memcache heap instead of mmap */
#define FHDATA_FLAGS_URGENT (1<<4) /* memcache heap instead of mmap */
#define FHDATA_FLAGS_WITH_TRANSIENT_ZIPENTRY_CACHES (1<<5) /* Can attach ht_transient_cache */
#define FHDATA_FLAGS_IS_AUTOGEN (1<<6)
enum enum_autogen_state{AUTOGEN_UNINITILIZED,AUTOGEN_SUCCESS,AUTOGEN_FAIL};
////////////////////////////////////////////////////////////////////////////////////////////////////
///   The struct fhdata "file handle data" holds data associated with a file descriptor.
///   Stored in a linear list _fhdata. The list may have holes.
///   Memcache: It may also contain the cached file content of the zip entry.
///   Only one of all instances with a specific virtual path should store the cached zip entry
////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_MEMCACHE
struct memcache{
  volatile enum memcache_status memcache_status;
  struct textbuffer *memcache2;
  volatile off_t memcache_l,memcache_already_current,memcache_already;
  int64_t memcache_took_mseconds;
  int id;
};
#endif // WITH_MEMCACHE
struct fhdata{
  uint64_t fh IF1(WITH_AUTOGEN,,fh_real);
  struct zip *zarchive;
  zip_file_t *zip_file;
  struct zippath zpath;
  volatile time_t accesstime;
  volatile int flags;
  IF1(WITH_AUTOGEN,enum enum_autogen_state autogen_state);
  uint8_t already_logged;
  volatile int64_t offset,n_read;
  volatile atomic_int is_xmp_read; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex_read; /* Costs 40 Bytes */
  counter_rootdata_t *filetypedata;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,struct ht *ht_transient_cache);
  bool debug_not_yet_read;
#if WITH_MEMCACHE
  struct memcache *memcache;
  int is_memcache_store;
#endif //WITH_MEMCACHE
};
#define foreach_fhdata_also_emty(id,d) int id=_fhdata_n;for(struct fhdata *d;--id>=0 && ((d=fhdata_at_index(id))||true);)
#define foreach_fhdata(id,d)  foreach_fhdata_also_emty(id,d) if (d->flags)
#define fhdata_path_eq(d,path,hash) d->zpath.virtualpath_hash==hash && !strcmp(D_VP(d),path)
#define memcache_is_queued(m)  (m && m->memcache_l && m->memcache_status==memcache_queued)
#define find_realpath_again_fhdata(d) zpath_reset_keep_VP(&d->zpath),find_realpath_any_root(0,&d->zpath,NULL)
#define SIZEOF_FHDATA sizeof(struct fhdata)
#define SIZEOF_ZIPPATH sizeof(struct zippath)
#define FHDATA_LOG2_BLOCK_SIZE 5
#define FHDATA_BLOCKS 512
#define FHDATA_BLOCK_SIZE (1<<FHDATA_LOG2_BLOCK_SIZE)
#define FHDATA_MAX (FHDATA_BLOCKS*FHDATA_BLOCK_SIZE)
static MAYBE_INLINE struct fhdata* fhdata_at_index(int i){
  static struct fhdata *_fhdata[FHDATA_BLOCKS];
#define B _fhdata[i>>FHDATA_LOG2_BLOCK_SIZE]
  struct fhdata *block=B;
  if (!block) assert(NULL!=(block=B=calloc(FHDATA_BLOCK_SIZE,SIZEOF_FHDATA)));
  return block+(i&(FHDATA_BLOCK_SIZE-1));
#undef B
}
//////////////////////////////////
/// Queue for file attributes  ///
//////////////////////////////////
#define STATQUEUE_ENTRY_N 128
#define STATQUEUE_FLAGS_VERBOSE (1<<1)
#define STAT_ALSO_SYSCALL (1<<1)
#define STAT_USE_CACHE (1<<2)

#define STAT_USE_CACHE_FOR_ROOT(r) (r!=_root_writable?STAT_USE_CACHE:0)
#if WITH_STAT_SEPARATE_THREADS
struct statqueue_entry{
  volatile int flags,refcount;
  char rp[MAX_PATHLEN+1]; volatile int rp_l;  volatile ht_hash_t rp_hash;
  struct stat stat;
  volatile enum statqueue_status status;
  struct timespec time;
};
#endif
//////////////////////////////////////
// caches
/////////////////////////////////////
static struct mstore mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
static long _count_stat_fail=0,_count_stat_from_cache=0;
static struct ht ht_intern_fileext IF1(WITH_STAT_CACHE,,stat_ht)  IF1(WITH_ZIPINLINE_CACHE,, zipinline_cache_virtualpath_to_zippath_ht);

/////////////////////////////////////////////////////////////////
/// Root paths - the source directories                       ///
/// The root directories are specified as program arguments   ///
/// The _root[0] may be read/write, and can be empty string   ///
/// The others are always  read-only                          ///
/////////////////////////////////////////////////////////////////
struct rootdata{ /* Data for a source directory. ZIPsFS acts like an overlay FS. */
  char rootpath[MAX_PATHLEN+1];
  int rootpath_l;
  uint32_t features;
  struct statvfs statfs;
  uint32_t log_count_delayed,log_count_delayed_periods,log_count_restarted;
  volatile uint32_t statfs_took_deciseconds; /* how long did it take */
  struct ht dircache_queue,dircache_ht; // !!
#if WITH_DIRCACHE || WITH_STAT_CACHE
  struct ht dircache_ht_fname,dircache_ht_dir, dircache_ht_fnamearray;
#endif
  pthread_t pthread[PTHREAD_LEN];
  int pthread_count_started[PTHREAD_LEN];
  int pthread_when_loop_deciSec[PTHREAD_LEN]; /* Detect blocked loop. */
  int pthread_when_canceled_deciSec[PTHREAD_LEN];
  IF1(WITH_STAT_SEPARATE_THREADS,struct statqueue_entry statqueue[STATQUEUE_ENTRY_N]);
  bool debug_pretend_blocked[PTHREAD_LEN];
  IF1(WITH_MEMCACHE,struct fhdata *memcache_d);
  struct ht ht_filetypedata;
  counter_rootdata_t filetypedata_dummy,filetypedata_all, filetypedata[FILETYPEDATA_NUM],filetypedata_frequent[FILETYPEDATA_FREQUENT_NUM];
  bool filetypedata_initialized, blocked;
};
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
#define is_last_root_or_null(r)  (!r||r==_root+_root_n-1)
#define foreach_root(ir,r)    int ir=0;for(struct rootdata *r=_root; ir<_root_n; r++,ir++)
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
#define DIRCACHE(r) r->dircache_ht_dir.keystore
/* *** fhdata vars and defs *** */
/////////////
/// Files ///
/////////////
#define DIR_ZIPsFS "/ZIPsFS"
#define DIR_ZIPsFS_L (sizeof(DIR_ZIPsFS)-1)
#define DIRNAME_AUTOGEN "a"
#define DIR_AUTOGEN DIR_ZIPsFS"/"DIRNAME_AUTOGEN
#define DIR_AUTOGEN_L (sizeof(DIR_AUTOGEN)-1)

#define FN_SET_ATIME "/ZIPsFS_set_file_access_time"
static const char *SPECIAL_FILES[]={"/warnings.log","/errors.log","/file_system_info.html","/ZIPsFS.command","",FN_SET_ATIME".command",FN_SET_ATIME".ps1",FN_SET_ATIME".bat","/Readme.html",NULL};  /* SPECIAL_FILES[SFILE_LOG_WARNINGS] is first!*/
enum enum_special_files{SFILE_LOG_WARNINGS,SFILE_LOG_ERRORS,SFILE_INFO,SFILE_CTRL,SFILE_DEBUG_CTRL,SFILE_SET_ATIME_SH,SFILE_SET_ATIME_PS,SFILE_SET_ATIME_BAT,SFILE_README,SFILE_L};
#define SFILE_BEGIN_VIRTUAL SFILE_INFO
static char _fWarningPath[2][MAX_PATHLEN+1], _debug_ctrl[MAX_PATHLEN+1];
static float _ucpu_usage,_scpu_usage;/* user and system */


#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))


#if WITH_AUTOGEN
#include "ZIPsFS_autogen.c"
#include "ZIPsFS_configuration_autogen.c"
#endif //WITH_AUTOGEN
#include "ZIPsFS_configuration.c"
#include "ZIPsFS_debug.c"
#include "ZIPsFS_log.c"
#include "ZIPsFS_cache.c"
#if WITH_TRANSIENT_ZIPENTRY_CACHES
#include "ZIPsFS_transient_zipentry_cache.c"
#endif
#if WITH_MEMCACHE
#include "ZIPsFS_memcache.c"
#endif
#include "ZIPsFS_ctrl.c"
#if WITH_STAT_SEPARATE_THREADS
#include "ZIPsFS_stat_queue.c"
#endif

////////////////////////////////////////
/// lock,pthread, synchronization   ///
////////////////////////////////////////
static pthread_mutex_t _mutex[NUM_MUTEX];
static void init_mutex(){
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
    //    const char *posaddr=dot && replacement_l>5?strnstr(u,ulen,replacement,replacement_l):NULL;
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
/* Reverse of simplify_fname():  Replace PLACEHOLDER_NAME by name of zipfile */
static bool unsimplify_fname(char *n,const char *zipfile){


  {

    char *n=NULL;
    const char *replacement="";
    const int n_l=cg_strlen(n);
    int  pos=0;
char *s=    cg_strchrnul(replacement,'.');
// int replacement_l=s-replacement;
 int replacement_l=((char*)cg_strchrnul(replacement,'.'))-(replacement);

  }


#if WITH_DIRCACHE_OPTIMIZE_NAMES
  char *placeholder=strchr(n,PLACEHOLDER_NAME);
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
  mstore_init(&d->filenames,4096|MSTORE_OPT_MALLOC);
  d->files_capacity=DIRECTORY_FILES_CAPACITY;
}

#define directory_new(dir,flags,realpath,realpath_l,r)  struct directory dir={0};directory_init(&dir,flags,realpath,realpath_l,r)

static void directory_destroy(struct directory *d){
  if (d && (0==(d->dir_flags&(DIRECTORY_IS_DIRCACHE)))){
#define C(field,type) if(d->core.field!=d->_stack_##field) FREE(d->core.field)
    C_FILE_DATA();
#undef C
    mstore_destroy(&d->filenames);
  }
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
  cg_thread_assert_locked(mutex_dircache); ASSERT(n0!=NULL); ASSERT(dir!=NULL); ASSERT(!(dir->dir_flags&DIRECTORY_IS_DIRCACHE));
  if (cg_empty_dot_dotdot(n0)) return;
  struct directory_core *d=&dir->core;
#define L d->files_l
#define B dir->files_capacity
  if (B<=L){
    B=2*L;
    const bool is_stack=directory_is_stack(dir);
#define C(f,type)  if (d->f) d->f=is_stack?memcpy(malloc(B*sizeof(type)),d->f,L*sizeof(type)):   realloc(d->f,B*sizeof(type));
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
  const char *n=d->fname[L++]=(char*)mstore_addstr(&dir->filenames,s,len);
  ASSERT(NULL!=n);
  //ASSERT(len==strlen(n));
#undef B
#undef L
}
/////////////////////////////////////////
////////////
/// stat ///
////////////

static bool stat_maybe_cache(const bool verbose,int opt, const char *path,const int path_l,const ht_hash_t hash,struct stat *stbuf){
  ASSERT(NULL!=path);   ASSERT(strlen(path)>=path_l);
  const int now=deciSecondsSinceStart();
  IF1(WITH_STAT_CACHE,
      if (0!=(opt&STAT_USE_CACHE) && stat_from_cache(stbuf,path,path_l,hash)) return true;
      if (0==(opt&STAT_ALSO_SYSCALL)) return false);
  static int count;
  //if (strstr(path,"fularchiv")){ log_debug_now("#%d Going lstat(%s,) ...",count++,path); } // cg_print_stacktrace(0);}
  const int res=lstat(path,stbuf);
  LOCK(mutex_fhdata,inc_count_getattr(path,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS));
  if (res) return false;
  assert(stbuf->st_ino!=0);
  IF1(WITH_STAT_CACHE,stat_to_cache(stbuf,path,path_l,hash));
  return true;
}/*stat_maybe_cache*/

static bool stat_cache_or_queue(const bool verbose,const char *rp, struct stat *stbuf,struct rootdata *r){
  const int rp_l=cg_strlen(rp);
  const ht_hash_t hash=hash32(rp,rp_l);  //  log_debug_now("r: %s  ROOT_REMOTE: %s",yes_no(r!=NULL),yes_no(r && (r->features&ROOT_REMOTE)));
  if (!WITH_STAT_SEPARATE_THREADS || !r || !(r->features&ROOT_REMOTE)){
    return stat_maybe_cache(verbose,STAT_ALSO_SYSCALL|STAT_USE_CACHE_FOR_ROOT(r),rp,rp_l,hash,stbuf);
  }else if (stat_maybe_cache(verbose,0,rp,rp_l,hash,stbuf)) return true;
  IF1(WITH_STAT_SEPARATE_THREADS,return stat_queue(false,rp,rp_l,hash,stbuf,r));
  IF0(WITH_STAT_SEPARATE_THREADS,return false);
}
//////////////////////
/// Infinity loops ///
//////////////////////
static int observe_thread(struct rootdata *r, int thread){
  while(r->debug_pretend_blocked[thread]) usleep(100*1000);
  return (r->pthread_when_loop_deciSec[thread]=deciSecondsSinceStart());
}
#if WITH_STAT_SEPARATE_THREADS
static void infloop_statqueue_start(void *arg){
  root_start_thread(arg,PTHREAD_STATQUEUE);
}
#endif //WITH_STAT_SEPARATE_THREADS
static void infloop_responding_start(void *arg){ root_start_thread(arg,PTHREAD_RESPONDING);}
static void infloop_dircache_start(void *arg){ root_start_thread(arg,PTHREAD_DIRCACHE);}

#if WITH_MEMCACHE
static void infloop_memcache_start(void *arg){
  root_start_thread(arg,PTHREAD_MEMCACHE);
}
#endif //WITH_MEMCACHE
static void root_start_thread(struct rootdata *r,int ithread){
  //  log_entered_function("%s %s",rootpath(r),PTHREAD_S[ithread]);
  r->debug_pretend_blocked[ithread]=false;
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
  if (r->pthread_count_started[ithread]++) warning(WARN_THREAD,report_rootpath(r),"pthread_start %s function: %p",PTHREAD_S[ithread],f);
  ASSERT(f!=NULL);
  if (f){
    if (pthread_create(&r->pthread[ithread],NULL,f,(void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT|WARN_FLAG_ERRNO,rootpath(r),"Failed thread_create %s root=%d ",PTHREAD_S[ithread],rootindex(r));
  }
}
static void *infloop_responding(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_responding_start,r);
  //const bool debug=filepath_contains_blocking(rootpath(r));
  for(int j=0;;j++){
    //if (r->features&ROOT_REMOTE) log_msg(" R%d.%d ",rootindex(r),j);
    const int now=deciSecondsSinceStart();
    //if (debug) fprintf(stderr," Going-statfs ");
    statvfs(rootpath(r),&r->statfs);
    //if (debug) fprintf(stderr," Done-statfs ");
    const int diff=observe_thread(r,PTHREAD_RESPONDING)-now;
    if (diff>ROOT_WARN_STATFS_TOOK_LONG_SECONDS*10) log_warn("\nstatfs %s took %'ld ms\n",rootpath(r),100L*diff);
    usleep(1000*ROOT_OBSERVE_EVERY_MSECONDS_RESPONDING);
  }
  pthread_cleanup_pop(0);
  return NULL;
}
static void *infloop_misc0(void *arg){
  for(int j=0;;j++){
    usleep(1000*1000);
    LOCK_NCANCEL(mutex_fhdata,fhdata_destroy_those_that_are_marked());
    IF1(WITH_AUTOGEN,if (!(j&0xFff)) autogen_cleanup());
    //   if (!(j&0xFF)) log_msg(" MI ");
    if (!(j&3)){
      static struct pstat pstat1,pstat2;
      cpuusage_read_proc(&pstat2,getpid());
      cpuusage_calc_pct(&pstat2,&pstat1,&_ucpu_usage,&_scpu_usage);
      pstat1=pstat2;
      if (false) if (_ucpu_usage>40||_scpu_usage>40) log_verbose("pid: %d cpu_usage user: %.2f system: %.2f\n",getpid(),_ucpu_usage,_scpu_usage);
    }
  }
}
static void *infloop_unblock(void *arg){
  usleep(1000*1000);
  if (*_mkSymlinkAfterStart){
    {
      struct stat stbuf;
      lstat(_mkSymlinkAfterStart,&stbuf);
      if (((S_IFREG|S_IFDIR)&stbuf.st_mode) && !(S_IFLNK&stbuf.st_mode)){
        warning(WARN_MISC,""," Cannot make symlink %s =>%s  because %s is a file or dir\n",_mkSymlinkAfterStart,_mnt,_mkSymlinkAfterStart);
        EXIT(1);
      }
    }
    const int err=cg_symlink_overwrite_atomically(_mnt,_mkSymlinkAfterStart);
    char rp[PATH_MAX];
    if (err || !realpath(_mkSymlinkAfterStart,rp)){
      warning(WARN_MISC,_mkSymlinkAfterStart," cg_symlink_overwrite_atomically(%s,%s); %s",_mnt,_mkSymlinkAfterStart,strerror(err));
      EXIT(1);
    }else if(!cg_is_symlink(_mkSymlinkAfterStart)){
      warning(WARN_MISC,_mkSymlinkAfterStart," not a symlink");
      EXIT(1);
    }else{
      log_msg(GREEN_SUCCESS"Created symlink %s -->%s\n",_mkSymlinkAfterStart,rp);
    }
    *_mkSymlinkAfterStart=0;
  }
  while(true){
        usleep(5000*1000);
    foreach_root(i,r){
      if (!(r->features&ROOT_REMOTE)) continue;
      RLOOP(t,PTHREAD_LEN){
        const bool debug=t==PTHREAD_MEMCACHE&& strstr(rootpath(r),"blocking");
        const int threshold=
          t==PTHREAD_STATQUEUE?10*UNBLOCK_AFTER_SECONDS_THREAD_STATQUEUE:
          t==PTHREAD_MEMCACHE?10*UNBLOCK_AFTER_SECONDS_THREAD_MEMCACHE:
          t==PTHREAD_DIRCACHE?10*UNBLOCK_AFTER_SECONDS_THREAD_DIRCACHE:
          t==PTHREAD_RESPONDING?10*UNBLOCK_AFTER_SECONDS_THREAD_RESPONDING:
          0;
        if (!threshold || !r->pthread[t]) continue;
        const int timeago=deciSecondsSinceStart()-MAX_int(r->pthread_when_loop_deciSec[t],r->pthread_when_canceled_deciSec[t]);
        //if (debug) log_debug_now("Waiting: %d / %d  %lu ",timeago,threshold, r->pthread[t]);
        // if (timeago>threshold/10) log_debug_now("%s threshold: %d  timeago: %d %p ",PTHREAD_S[t],threshold,timeago,(void*)r->pthread[t]);
        if (timeago>threshold){
          warning(WARN_THREAD,report_rootpath(r),ANSI_RED"Going to  pthread_cancel() root: %s %d  PTHREAD_S: %s. Last response was %d seconds ago. Threshold: %d"ANSI_RESET"\n",rootpath(r),rootindex(r),PTHREAD_S[t],timeago/10,threshold/10);
          pthread_cancel(r->pthread[t]); /* All but PTHREAD_MEMCACHE will restart themselfes via pthread_cleanup_push() */

          IF1(WITH_MEMCACHE,if (i==PTHREAD_MEMCACHE) infloop_memcache_start(r)); /* pthread_cancel not working when root blocked. */
          r->pthread_when_canceled_deciSec[t]=deciSecondsSinceStart();
        }
      }
    }
  }
  return NULL;
}
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
  RLOOP(try,N){
    const float delay=(deciSecondsSinceStart()-r->pthread_when_loop_deciSec[PTHREAD_RESPONDING])/10.0;
    //if (debug) log_debug_now("ddddddddddddd delay: %3.4f",delay);
    if (delay>ROOT_SKIP_UNLESS_RESPONDED_WITHIN_SECONDS) break;
    if (delay<ROOT_LAST_RESPONSE_MUST_BE_WITHIN_SECONDS){ log_root_blocked(r,false);return true; }
    usleep(_wait_for_root_timeout_sleep=1000*1000*ROOT_LAST_RESPONSE_MUST_BE_WITHIN_SECONDS/N);
    if (try>N/2 && try%(N/10)==0) log_msg("%s %s %d/%d\n",__func__,rootpath(r),try,N);
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
static void stat_init(struct stat *st, int64_t size,struct stat *uid_gid){
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
#define zpath_assert_strlen(zpath)  _zpath_assert_strlen(__func__,__FILE__,__LINE__,zpath)
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath){
  bool e=false;
#define C(a)  if (zpath_strlen(zpath,zpath->a)!=zpath->a##_l && (e=true)) log_error(#a" != "#a"_l   %d!=%d\n",zpath_strlen(zpath,zpath->a),zpath->a##_l);
  C(virtualpath);C(virtualpath_without_entry);C(entry_path);
#undef C
  if (e){
    log_zpath("Error ",zpath);
    log_verbose(ANSI_FG_RED"zpath_assert_strlen"ANSI_RESET" in %s at "ANSI_FG_BLUE"%s:%d\n"ANSI_RESET,fn,file,line);
    ASSERT(0);
  }
}

static void zpath_reset_keep_VP(struct zippath *zpath){ /* Reset to. Later probe a different realpath */
#define x_l_hash(x) x=x##_l=x##_hash
  zpath->entry_path=zpath->entry_path_l=x_l_hash(zpath->virtualpath_without_entry)=x_l_hash(zpath->realpath)=0;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_L()=EP_L()=zpath->zipcrc32=0;
  zpath->root=NULL;
  zpath->strgs_l=zpath->virtualpath+VP_L();
  zpath->flags=(zpath->flags&(ZP_STARTS_AUTOGEN|ZP_VERBOSE|ZP_VERBOSE2));
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

static bool zpath_stat(const bool verbose,struct zippath *zpath,struct rootdata *r){
  if (!zpath) return false;
  if (verbose) log_entered_function("%s root:%s",RP(),report_rootpath(r));
  if (zpath->stat_rp.st_ino){
    if (verbose) log_verbose("stat_rp.st_ino!=0  not calling stat root: %s rp: %s vp: %s\n",report_rootpath(r),RP(),VP());
  }else{
    const bool success=stat_cache_or_queue(verbose,RP(),&zpath->stat_rp,r);
    if (verbose) log_verbose("stat_cache_or_queue returned %s rp: %s vp: %s root: %s\n",success_or_fail(success),RP(),VP(),report_rootpath(r));
    if (!success) return false;
    zpath->stat_vp=zpath->stat_rp;
  }
  return true;
}
static void warning_zip(const char *path, zip_error_t *e,const char *txt){
  if (!path) return;
  const int ze=!e?0:zip_error_code_zip(e), se=!e?0:zip_error_code_system(e);
  char s[1024];*s=0;  if (se) strerror_r(se,s,1023);
  warning(WARN_FHDATA,path,"%s    sys_err: %s %s zip_err: %s %s",txt,!e?"e is NULL":!se?"":error_symbol(se),s, !ze?"":error_symbol_zip(ze), !ze?"":zip_error_strerror(e));
}

static struct zip *zip_open_ro(const char *orig){
  struct zip *zip=NULL;
  if (orig){
    if (!cg_endsWithZip(orig,0)){
      log_verbose("zip_open_ro %s ",orig);
      return NULL;
    }
    //static int count; log_verbose("Going to zip_open(%s) #%d ... ",orig,count);cg_print_stacktrace(0);
    RLOOP(try,2){
      int err;
      zip=zip_open(orig,ZIP_RDONLY,&err);
      LOCK(mutex_fhdata,inc_count_getattr(orig,zip?COUNTER_ZIPOPEN_SUCCESS:COUNTER_ZIPOPEN_FAIL));
      if (zip) break;
      warning(WARN_OPEN|WARN_FLAG_ERRNO,orig,"zip_open_ro() err=%d",err);
      usleep(1000);
    }
  }
  return zip;
}
///////////////////////////////////
/// Is the virtualpath a zip entry?
///////////////////////////////////
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *shorten, char *append[]){
  IF1(WITH_AUTOGEN,if (virtualpath_startswith_autogen(vp,vp_l)) return 0);
  char *e,*b=(char*)vp;
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
static bool _directory_from_dircache_zip_or_filesystem(struct directory *mydir,const struct stat *rp_stat/*current stat or cached*/){
  const char *rp=mydir->dir_realpath;
  if (!rp || !*rp) return false;
  const bool isZip=mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE;
  //if (isZip && !ENDSWITH(mydir->dir_realpath,mydir->dir_realpath_l,".Zip")) {dde_print("mydir->dir_realpath:%s\n",mydir->dir_realpath);EXIT(9);}
  int result=0; /* Undefined:0 OK:1 Fail:-1 */
#if WITH_DIRCACHE
  const bool doCache=config_advise_cache_directory_listing(rp,mydir->dir_realpath_l,mydir->root->features&ROOT_REMOTE,isZip,rp_stat->ST_MTIMESPEC);
  if (doCache){
    LOCK_NCANCEL(mutex_dircache,result=dircache_directory_from_cache(mydir,rp_stat->ST_MTIMESPEC)?1:0);
  }
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
        const int SB=256,N=zip_get_num_entries(zip,0);
        struct zip_stat s[SB]; /* reduce num of pthread lock */
        mydir->core.finode=NULL;
        for(int k=0;k<N;){
          int i=0;
          for(;i<SB && k<N;k++) if (!zip_stat_index(zip,k,0,s+i)) i++;
          LOCK(mutex_dircache, FOR(j,0,i){
              const char *n=s[j].name;
              directory_add_file(s[j].comp_method?DIRENT_IS_COMPRESSED:0,mydir,0,n,s[j].size,s[j].mtime,s[j].crc);
              if (!s[j].crc && *n && n[strlen(n)-1]!='/') warning(WARN_STAT,rp," s[j].crc is 0 n=%s size=%zu",n,s[j].size);
            });
        }
        result=1;
        zip_close(zip);
      } //else log_debug_now("%s  zip is NULL",rp);
    }else{
      //static int count; log_msg("Going to run opendir(%s,) #%d",rp,count++);
      DIR *dir=opendir(rp);
      LOCK(mutex_fhdata,inc_count_getattr(rp,dir?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL));
      if (!dir){
        log_errno("%s",rp);
      }else{  /* Really read from file system */
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
    if (result==1){
      mydir->core.mtim=rp_stat->ST_MTIMESPEC;
      IF1(WITH_DIRCACHE,if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(mydir)));
    }
  }
  return result==1;
}

static bool directory_from_dircache_zip_or_filesystem(struct directory *dir,const struct stat *rp_stat/*current stat or cached*/){
  bool ok=_directory_from_dircache_zip_or_filesystem(dir,rp_stat);
  if (ok && (dir->dir_flags&DIRECTORY_IS_DIRCACHE)){
    //debug_directory_print(dir);
  }
  return ok;
}



/* Reading zip dirs asynchroneously */
static void *infloop_dircache(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_dircache_start,r);
  char path[MAX_PATHLEN+1];
  struct stat stbuf;
  struct directory mydir={0}; /* Put here otherwise use of stack var after ... */
  while(true){
    *path=0;
    struct ht_entry *ee=r->dircache_queue.entries;

    lock_ncancel(mutex_dircachejobs);/*Pick path from an entry and put in stack variable path */
    RLOOP(i,r->dircache_queue.capacity){
      if (ee[i].key){
        cg_strncpy(path,ee[i].key,MAX_PATHLEN);
        FREE2(ee[i].key);
        ht_clear_entry(ee+i);
        break;
      }}
    unlock_ncancel(mutex_dircachejobs);/*Pick path from an entry and put in stack variable path */
    if (*path && stat_cache_or_queue(false,path,&stbuf,r)){
      directory_init(&mydir,DIRECTORY_IS_ZIPARCHIVE,path,strlen(path),r);
      directory_from_dircache_zip_or_filesystem(&mydir,&stbuf);
      directory_destroy(&mydir);
    }
    observe_thread(r,PTHREAD_DIRCACHE);
    usleep(1000*10);
  }
  pthread_cleanup_pop(0);
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
      if (zpath_stat(false,zpath,NULL)){
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
/* Previously VP0_L(), EP() ,EP_L() etc. had been set in find_realpath_any_root(0,).
   Constructs zpath->realpath and checks whether exists.
   Returns true on success */
static bool _test_realpath(const bool verbose,struct zippath *zpath, struct rootdata *r){
  assert(r!=NULL);
  //zpath_assert_strlen(zpath);
  if (verbose) log_entered_function("%s root:%s",VP(),report_rootpath(r));
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  zpath->root=r;
  const char *vp=VP();
  {
    const char *vp0=VP0_L()?VP0():vp; /* Virtual path without zip entry */
    if (!zpath_strcat(zpath,rootpath(r)) || strcmp(vp0,"/") && !zpath_strcat(zpath,vp0)) return false;
    ZPATH_COMMIT_HASH(zpath,realpath);
  }
  if (!zpath_stat(verbose,zpath,r)) return false;
  if (ZPATH_IS_ZIP()){
    if (!cg_endsWithZip(RP(),0)){
      if (ZPATH_IS_VERBOSE()) log_verbose("!cg_endsWithZip rp: %s\n",RP());
      return false;
    }
    if (EP_L() && filler_readdir_zip(0,zpath,NULL,NULL,NULL)) return false; /* This sets the file attribute of zip entry  */
  }
  return true;
}
static bool test_realpath(const bool verbose,struct zippath *zpath, struct rootdata *r){
  if (!_test_realpath(verbose,zpath,r)){ zpath_reset_keep_VP(zpath); return false;}
  return true;
}
/* Uses different approaches and calls test_realpath */
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r){
  //if (zpath->flags&ZP_STARTS_AUTOGEN) return false;
  zpath->entry_path=zpath_newstr(zpath);
  zpath->flags|=ZP_ZIP;
  if (!zpath_strcat(zpath,vp+cg_last_slash(vp)+1)) return false;
  EP_L()=zpath_commit(zpath);
  const bool ok=test_realpath(false,zpath,r);
  if (ZPATH_IS_VERBOSE2() && cg_is_regular_file(RP())){
    log_exited_function("rp: %s vp: %s ep: %s ok: %s",RP(),vp,EP(),yes_no(ok));
  }
  return ok;
}
static bool find_realpath_nocache(const bool verbose,struct zippath *zpath,struct rootdata *r){
  const char *vp=VP(); /* At this point, Only zpath->virtualpath is defined. */
  if (verbose) log_entered_function("%s root: %s",vp,report_rootpath(r));
  char *append="";
  {
    if (!wait_for_root_timeout(r)){
      if (verbose) log_verbose("Going to return false because wait_for_root_timeout %s  root: %s",VP(),report_rootpath(r));
      return false;
    }
    if (VP_L()){ /* ZIP-file is a folder (usually ZIP-file.Content in the virtual file system. */
      // RRR      zpath_reset_keep_VP(zpath);
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
        const bool t=test_realpath(verbose,zpath,r);
        if (verbose) log_verbose("test_realpath %s %s %s",VP(),report_rootpath(r),success_or_fail(t));
        if (t){
          if (!EP_L()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
          return true;
        }
      }
#if WITH_ZIPINLINE
      for(int rule=0;;rule++){ /* ZIP-entries inlined. ZIP-file itself not shown in listing. Occurs for Sciex mass-spec */
        const int len=config_containing_zipfile_of_virtual_file(rule,vp,VP_L(),&append);
        if (ZPATH_IS_VERBOSE2()) log_verbose("rule %d  vp: %s  append: %s config_containing_zipfile_of_virtual_file len: %d", rule,vp,append,len);
        if (!len) break;
        zpath_reset_keep_VP(zpath);
        zpath->virtualpath_without_entry=zpath_newstr(zpath);
        if (!zpath_strncat(zpath,vp,len) || !zpath_strcat(zpath,append)) return false;
        ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
        const bool ok=find_realpath_try_inline(zpath,vp,r);
        if (verbose && cg_is_regular_file(RP())){
          log_verbose("rule %d  vp: %s  append: %s config_containing_zipfile_of_virtual_file len: %d  find_realpath_try_inline returned %s", rule,vp,append,len,yes_no(ok));
          log_zpath("",zpath);
        }
        if (ok) return true;
      }
#endif //WITH_ZIPINLINE
    }
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  const bool t=test_realpath(verbose,zpath,r);
  if (verbose) log_verbose("test_realpath %s %s %s",VP(),report_rootpath(r),success_or_fail(t));
  return t;
} /*find_realpath_nocache*/


static bool find_realpath_any_root1(const bool verbose,const bool path_starts_autogen,struct zippath *zpath,const struct rootdata *onlyThisRoot,const long which_roots){
  const char *vp=VP();
  const int vp_l=VP_L();
  if (verbose) log_entered_function("%s root: %s",vp,report_rootpath(onlyThisRoot));
  if (vp_l){
    if (!onlyThisRoot && find_realpath_special_file(zpath)) return true;
#define R() foreach_root(ir,r) if ((!onlyThisRoot||onlyThisRoot==r) && (which_roots&(1<<ir)) && !(path_starts_autogen && ir))
#if WITH_ZIPINLINE_CACHE
    // RRR zpath_reset_keep_VP(zpath);
    R(){
      if (onlyThisRoot && r!=onlyThisRoot) continue;
      char zip[MAX_PATHLEN+1]; *zip=0;
      LOCK(mutex_dircache,const char *z=zipinline_cache_virtualpath_to_zippath(vp,vp_l); if (z) strcpy(zip,z));
      if (*zip && !strncmp(zip,r->rootpath,r->rootpath_l) && zip[r->rootpath_l]=='/'  && wait_for_root_timeout(r)){
        // RRR zpath_reset_keep_VP(zpath);
        zpath->virtualpath_without_entry=zpath_newstr(zpath);
        if (!zpath_strcat(zpath,zip+r->rootpath_l)) return false;
        ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
        if (find_realpath_try_inline(zpath,vp,r)) return true;
      }
    }
#endif //WITH_ZIPINLINE_CACHE
  }
  R() if (find_realpath_nocache(verbose,zpath,r)) return true;
  //if (!onlyThisRoot && !config_not_report_stat_error(vp)) warning(WARN_GETATTR|WARN_FLAG_ONCE_PER_PATH,vp,"fail");
  if (verbose) log_verbose("Going to return false %s %s",vp,report_rootpath(onlyThisRoot));
  // RRR   zpath_reset_keep_VP(zpath);
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
// #define debug_trigger_vp(...) false
static bool find_realpath_any_root(int opt,struct zippath *zpath,const struct rootdata *onlyThisRoot){
  const char *vp=VP();
  const int vp_l=VP_L();
  //if (debug_trigger_vp(vp,vp_l)) opt|=FINDRP_VERBOSE;
  const bool path_starts_autogen=false IF1(WITH_AUTOGEN, || (0!=(zpath->flags&ZP_STARTS_AUTOGEN)));
  const long which_roots=config_search_file_which_roots(vp,vp_l,path_starts_autogen);
  if (opt&FINDRP_VERBOSE) log_entered_function("%s root: %s",vp,report_rootpath(onlyThisRoot));
#if WITH_TRANSIENT_ZIPENTRY_CACHES
  if (vp_l  && vp && !path_starts_autogen){
    struct zippath cached={0};
    if (!(opt&FINDRP_NOT_TRANSIENT_CACHE)) LOCK(mutex_fhdata,struct zippath *zp=transient_cache_get_or_create_zpath(false,vp,vp_l); if (zp) cached=*zp);
    const int f=cached.flags;
    if (cached.virtualpath && !(f&ZP_DOES_NOT_EXIST) && zpath_exists(&cached)){
      *zpath=cached;
      return true;
    }
    if (f&ZP_DOES_NOT_EXIST){
      if (opt&FINDRP_VERBOSE) log_verbose("Going to return false because ZP_DOES_NOT_EXIST %s",vp);
      return false;
    }
  }
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
  const int virtualpath=zpath->virtualpath;
  const bool cut_autogen=which_roots!=1 && !(opt&FINDRP_AUTOGEN_CUT_NOT) && path_starts_autogen;
  bool found=false;
  IF1(WITH_AUTOGEN,FOR(cut01,0,cut_autogen?2:1)){
    IF1(WITH_AUTOGEN,if(cut01){ zpath->virtualpath=virtualpath+DIR_AUTOGEN_L; zpath->virtualpath_l=vp_l-DIR_AUTOGEN_L;});
    int sleep_milliseconds=0;
    const int n=config_num_retries_getattr(vp,vp_l,&sleep_milliseconds);
    FOR(i,0,n){
      found=find_realpath_any_root1(0!=(opt&FINDRP_VERBOSE),IF1(WITH_AUTOGEN,!cut01&&) path_starts_autogen,zpath,onlyThisRoot,which_roots);
      if (0!=(opt&FINDRP_VERBOSE) && !i) log_verbose("find_realpath_any_root1 %s root: %s returned %s",vp,rootpath(onlyThisRoot),success_or_fail(found));
      if (found){
        if (i) warning(WARN_RETRY,vp,"find_realpath_any_root succeeded on retry %d",i);
        goto found;
      }
      if (sleep_milliseconds){
      log_verbose("Going sleep %d ms ...",sleep_milliseconds);
      usleep(sleep_milliseconds<<10);
      }
    }
  }
  if (found) assert(zpath->realpath!=0);
 found:
  zpath->virtualpath=virtualpath;/*Restore*/
  zpath->virtualpath_l=vp_l;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,LOCK(mutex_fhdata, struct zippath *zp=transient_cache_get_or_create_zpath(true,vp,vp_l);
                                          if (zp){
                                            if (found) *zp=*zpath;
                                            else if (!onlyThisRoot) zp->flags|=ZP_DOES_NOT_EXIST; /* Not found in any root. */
                                          }));
  if (0!=(opt&FINDRP_VERBOSE))  log_verbose("%s   going return: %s   root: %s",vp,success_or_fail(found),report_rootpath(onlyThisRoot));
  if (!found && !onlyThisRoot && !config_not_report_stat_error(vp,vp_l)){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,vp,ANSI_RED"found is false   root: %s"ANSI_RESET,report_rootpath(onlyThisRoot));
  }
  return found;
} /*find_realpath_any_root*/

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
  if (!d || d->is_xmp_read>0) return false;
#if WITH_MEMCACHE
  struct memcache *m=d->memcache;
  if (m){ /* Not close when serving as file-content-cache for other fhdata instances  with identical path */
    IF1(WITH_MEMCACHE,if (m->memcache_status==memcache_reading||d->is_memcache_store>0) return false);
    const ht_hash_t hash=D_VP_HASH(d);
    foreach_fhdata(id,e){
      if (e==d || (e->flags&FHDATA_FLAGS_DESTROY_LATER)) continue;
      //if (!m->memcache_status && /* Otherwise two with same path, each having a cache could stick for ever */
      if (D_VP_HASH(e)==hash && !strcmp(D_VP(d),D_VP(e))) return false;
    }
  }
#endif //WITH_MEMCACHE
  return true;
}

static void fhdata_zip_close(struct fhdata *d){
  // log_entered_function("%s",D_VP(d));
  zip_file_t *z=d->zip_file;
  d->zip_file=NULL;
  if (z) zip_file_error_clear(z);
  if (z && zip_fclose(z)) warning_zip(D_RP(d),zip_file_get_error(z),"Failed zip_fclose");
}

////////////////////////////////////////////////////////////
/// path can be NULL and is to evict from page cache     ///
////////////////////////////////////////////////////////////
#define fhdata_destroy_and_evict_from_pagecache(fh,path,path_l) _fhdata_destroy(fhdata_get(path,fh),fh,path,path_l);
static void _fhdata_destroy(struct fhdata *d,const int fd_evict,const char *path_evict,const int path_evict_l){
  ASSERT_LOCKED_FHDATA();
  bool verbose=false;
  IF1(WITH_EVICT_FROM_PAGECACHE,bool do_evict=path_evict?config_advise_evict_from_filecache(path_evict,path_evict_l, d?D_EP(d):NULL,d?D_EP_L(d):0):false);
  if (d){
    if (!fhdata_can_destroy(d)){ /* When reading to RAM cache */
      d->flags|=FHDATA_FLAGS_DESTROY_LATER;
      IF1(WITH_EVICT_FROM_PAGECACHE,do_evict=false);
    }else{
      IF1(WITH_AUTOGEN,const uint64_t fhr=d->fh_real;d->fh_real=0;if (fhr) close(fhr));
#if WITH_TRANSIENT_ZIPENTRY_CACHES
      if (d->ht_transient_cache){
        foreach_fhdata(ie,e) if (d->ht_transient_cache==e->ht_transient_cache) goto keep_transient_cache;
        ht_destroy(d->ht_transient_cache);
        FREE2(d->ht_transient_cache);
      }
    keep_transient_cache:
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
      IF1(WITH_MEMCACHE,memcache_free(d));
      IF1(WITH_EVICT_FROM_PAGECACHE,if (do_evict){const ht_hash_t hash=D_RP_HASH(d); foreach_fhdata(ie,e) if (d!=e && D_RP_HASH(e)==hash){ do_evict=false;break;}});
      //log_debug_now(ANSI_FG_GREEN"Going to release fhdata d=%p path=%s fh=%lu _fhdata_n=%d"ANSI_RESET,d,snull(D_VP(d)),d->fh,_fhdata_n);
      fhdata_zip_close(d);
      if (d->zarchive && zip_close(d->zarchive)==-1) warning_zip(D_RP(d),zip_get_error(d->zarchive),"Failed zip_close");
      pthread_mutex_destroy(&d->mutex_read);
      memset(d,0,SIZEOF_FHDATA);
    }
  }
  IF1(WITH_EVICT_FROM_PAGECACHE,if (do_evict){maybe_evict_from_filecache(fd_evict,path_evict,path_evict_l); log_verbose(ANSI_MAGENTA"Evicted %s"ANSI_RESET,path_evict);});
}
static void fhdata_destroy(struct fhdata *d){
  if (d) _fhdata_destroy(d,0,D_RP(d),D_RP_L(d));
}
static void fhdata_destroy_those_that_are_marked(){
  ASSERT_LOCKED_FHDATA();
  foreach_fhdata(id,d){
    if (d->flags&FHDATA_FLAGS_DESTROY_LATER  && fhdata_can_destroy(d)) fhdata_destroy(d);
  }

}
static zip_file_t *fhdata_zip_open(struct fhdata *d,const char *msg){
  cg_thread_assert_not_locked(mutex_fhdata);
  zip_file_t *zf=d->zip_file;
  if (!zf){
    const struct zippath *zpath=&d->zpath;
    struct zip *z=d->zarchive;
    if (!z && !(d->zarchive=z=zip_open_ro(RP()))){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"Failed zip_open");
    }else{
      zf=d->zip_file=zip_fopen(z,EP(),ZIP_RDONLY);
      if (!zf) warning_zip(RP(),zip_get_error(z),"zip_fopen()");
      fhdata_counter_inc(d,zf?ZIP_OPEN_SUCCESS:ZIP_OPEN_FAIL);
    }
  }
  return zf;
}

static void fhdata_init(struct fhdata *d, struct zippath *zpath){
  memset(d,0,SIZEOF_FHDATA);
  pthread_mutex_init(&d->mutex_read,NULL);
  const char *path=VP();
  d->zpath=*zpath;
  d->filetypedata=filetypedata_for_ext(path,ROOTd(d));
  d->flags|=FHDATA_FLAGS_ACTIVE; /* Important; This must be the last assignment */
}
static struct fhdata* fhdata_create(const uint64_t fh, struct zippath *zpath){
  ASSERT_LOCKED_FHDATA();
  struct fhdata *d=NULL;
  foreach_fhdata_also_emty(ie,e) if (!e->flags){ d=e; break;} /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhdata_n>=FHDATA_MAX){ warning(WARN_FHDATA|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_ERROR,VP(),"Excceeding FHDATA_MAX");return NULL;}
    d=fhdata_at_index(_fhdata_n++);
  }
  fhdata_init(d,zpath);
  d->fh=fh;
#if WITH_TRANSIENT_ZIPENTRY_CACHES
  if ((d->zpath.flags&ZP_ZIP) && config_advise_transient_cache_for_zipentries(VP(),VP_L())){
    d->flags|=FHDATA_FLAGS_WITH_TRANSIENT_ZIPENTRY_CACHES;
    foreach_fhdata(ie,e){
      if (d!=e && FHDATA_BOTH_SHARE_TRANSIENT_CACHE(d,e) && NULL!=(d->ht_transient_cache=e->ht_transient_cache)) break;
    }
  }
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
#if WITH_MEMCACHE
  {
    const ht_hash_t hash=D_VP_HASH(d);
    foreach_fhdata(ie,e){
      if (d!=e && hash==D_VP_HASH(e) && !strcmp(D_VP(d),D_VP(e))) d->memcache=e->memcache;
    }
  }
#endif //WITH_MEMCACHE
  d->flags|=FHDATA_FLAGS_ACTIVE;
  assert(ROOTd(d)!=NULL);
  return d;
}
static struct fhdata* fhdata_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHDATA();
  //log_msg(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  fhdata_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(path);
  foreach_fhdata(id,d){
    if (fh==d->fh && D_VP_HASH(d)==h && !strcmp(path,D_VP(d))) return d;
  }
  return NULL;
}
///////////////////////////////////
/// Auto-Generated files             ///
///////////////////////////////////
/* ******************************************************************************** */
// FUSE when   Added enum fuse_readdir_flags
// 2.9 not yet DEBUG_NOW
#define READDIR_AUTOGEN (1<<0)
#if FUSE_MAJOR_V>2
#define COMMA_FILL_DIR_PLUS ,0
#else
#define COMMA_FILL_DIR_PLUS
#endif // IS_APPLE

static void filldir(const int opt,fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
#if WITH_AUTOGEN
  if (_realpath_autogen && 0!=(opt&READDIR_AUTOGEN)){
    autogen_filldir(filler,buf,name,stbuf,no_dups);
    return;
  }
#endif
  if (ht_only_once(no_dups,name,0)) filler(buf,name,stbuf,0 COMMA_FILL_DIR_PLUS);
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t next_inode(){
  static ino_t seq=1UL<<63;
  return seq++;
}
static ino_t make_inode(const ino_t inode0,struct rootdata *r, const int entryIdx,const char *path){
  const static int SHIFT_ROOT=40,SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");
  if (inode0<(1L<<SHIFT_ROOT) && entryIdx<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| ((rootindex(r)+1L)<<SHIFT_ROOT);
  }else{
    static struct ht hts[ROOTS]={0};
    struct ht *ht=hts+rootindex(r);
    const uint64_t key1=inode0+1, key2=entryIdx;
    //const uint64_t key2=(entryIdx<<1)|1; /* Because of the implementation of hash table ht, key2 must not be zero. We multiply by 2 and add 1. */
    LOCK_NCANCEL_N(mutex_inode,
                   if (!ht->capacity) ht_init(ht,HT_FLAG_NUMKEY|16);
                   ino_t inod=(ino_t)ht_numkey_get(ht,key1,key2);
                   if (!inod){ ht_numkey_set(ht,key1,key2,(void*)(inod=next_inode()));_count_SeqInode++;});
    return inod;
  }
}
static ino_t inode_from_virtualpath(const char *vp,const int vp_l){
  static struct ht ht={0};
  static struct ht_entry *e;
  LOCK_NCANCEL(mutex_inode,
               if (!ht.capacity) ht_init_with_keystore_file(&ht,"inode_from_virtualpath",0,16,0x100000);
               e=ht_get_entry(&ht,vp,vp_l,0,true));
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
  const int ep_len=EP_L();
  struct rootdata *r=zpath->root;
  if(!ep_len && !filler_maybe_null) return 0; /* When the virtual path is a Zip file then just report success */
  if (!zpath_stat(false,zpath,r)) return ENOENT;
  directory_new(mydir,DIRECTORY_IS_ZIPARCHIVE,rp,RP_L(),r);
  if (!directory_from_dircache_zip_or_filesystem(&mydir,&zpath->stat_rp)) return ENOENT;
  char u[MAX_PATHLEN+1];
  struct directory_core d=mydir.core;
  FOR(i,0,d.files_l){
    if (cg_empty_dot_dotdot(d.fname[i]) || !unsimplify_fname(strcpy(u,d.fname[i]),rp)) continue;
    int len=cg_pathlen_ignore_trailing_slash(u);
    if (len>=MAX_PATHLEN){ warning(WARN_STR|WARN_FLAG_ERROR,u,"Exceed MAX_PATHLEN"); continue;}
    int isdir=(Nth0(d.fflags,i)&DIRENT_ISDIR)!=0,not_at_the_first_pass=0;
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
        if (ep_len &&  /* The parent folder is not just a ZIP file but a ZIP file with a zip entry stored in zpath->entry_path */
            (len<=ep_len || strncmp(EP(),u,ep_len) || u[ep_len]!='/')) continue; /* u must start with  zpath->entry_path */
        const char *n0=(char*)u+(ep_len?ep_len+1:0); /* Remove zpath->entry_path from the left of u. Note that zpath->entry_path has no trailing slash. */
        if (strchr(n0,'/') || config_do_not_list_file(rp,n0,strlen(n0)) || ht_sget(no_dups,n0)) continue;
        struct stat stbuf, *st=&stbuf;
        SET_STAT();
        assert_validchars(VALIDCHARS_FILE,n0,strlen(n0),"n0");
        filldir(opt,filler_maybe_null,buf,n0,st,no_dups);
      }else if (ep_len==len && !strncmp(EP(),u,len)){ /* ---  filler_readdir_zip() has been called from test_realpath(). The goal is to set zpath->stat_vp --- */
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
  struct rootdata *r=zpath->root; ASSERT(r!=NULL);
  if (ZPATH_IS_ZIP()) return filler_readdir_zip(opt,zpath,buf,filler,no_dups);
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
        if (0==(opt&READDIR_AUTOGEN) && ht_get(no_dups,u,u_l,0)) continue;
        bool inlined=false; /* Entries of ZIP file appear directory in the parent of the ZIP-file. */
#if WITH_ZIPINLINE
        if (config_skip_zipfile_show_zipentries_instead(u,u_l) && (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,u))){
          const int direct_rp_l=strlen(direct_rp);
          directory_new(dir2,DIRECTORY_IS_ZIPARCHIVE|DIRECTORY_TO_QUEUE,direct_rp,direct_rp_l,r);
          if (stat_cache_or_queue(false,direct_rp,&stbuf,r) && directory_from_dircache_zip_or_filesystem(&dir2,&stbuf)){
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
            if (strlen(virtual_name)!=u_l || cg_endsWithZip(u,u_l)) st.st_mode=(st.st_mode&~S_IFMT)|S_IFDIR; /* ZIP files as  directory */
            filldir(opt,filler,buf,virtual_name,&st,no_dups);
          }
        }
      }
    }
    directory_destroy(&dir);
  }
  //  log_exited_function("impl_readdir \n");
  return 0;
}
static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;}
static int xmp_statfs(const char *path, struct statvfs *stbuf){ return minus_val_or_errno(statvfs(_root->rootpath,stbuf));}
/************************************************************************************************/
/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
static int realpath_mk_parent(char *realpath,const char *path){
  if (!_root_writable) return EACCES;/* Only first root is writable */
  const int path_l=strlen(path), slash=cg_last_slash(path);
  if (config_not_overwrite(path,path_l)){
    bool found;FIND_REALPATH(path);
    const bool exist=found && zpath->root>0;
    if (exist){ warning(WARN_OPEN|WARN_FLAG_ONCE,RP(),"It is only allowed to overwrite files in root 0");return EACCES;}
  }
  if (slash>0){
    char parent[MAX_PATHLEN+1];
    cg_strncpy(parent,path,slash);
    bool found;FIND_REALPATH(parent);
    if (found){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,_root_writable->rootpath),parent,MAX_PATHLEN);
      cg_recursive_mkdir(realpath);
    }
    if (!found) return ENOENT;
  }
  strncat(strcpy(realpath,_root_writable->rootpath),path,MAX_PATHLEN);
  //log_exited_function("success realpath=%s \n",realpath);
  return 0;
}
/********************************************************************************/
// FUSE FUSE 3.0.0rc3 The high-level init() handler now receives an additional struct fuse_config pointer that can be used to adjust high-level API specific configuration options.
#define DO_LIBFUSE_CACHE_STAT 0
#if FUSE_MAJOR_V>2
#define HAS_FUSE_CONFIG 1
#else
#define HAS_FUSE_CONFIG 0
#endif

#define EVAL(a) a
#define EVAL2(a) EVAL(a)


void *xmp_init(struct fuse_conn_info *conn IF1(HAS_FUSE_CONFIG,,struct fuse_config *cfg)){
  //void *x=fuse_apply_conn_info_opts;  //cfg-async_read=1;
  cfg->use_ino=1;
  #if HAS_FUSE_CONFIG
  IF1(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
  #endif
  return NULL;
}

/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted

// Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now all receive an additional struct fuse_file_info pointer (which, however, may be NULL even if the file is currently open).
#if FUSE_MAJOR_V>=2 && FUSE_MINOR_V>=9
#define PARA_GETATTR ,struct fuse_file_info *fi_or_null
#else
#define PARA_GETATTR
#endif
int xmp_getattr(const char *path, struct stat *stbuf PARA_GETATTR){
  const int path_l=strlen(path);
  int err=0;
  bool debug=false; //IF1(WITH_AUTOGEN,if(!strncmp(path,DIR_AUTOGEN,sizeof(DIR_AUTOGEN)-1)) debug=true);
  if (trigger_files(false,path,path_l)){
    stat_init(stbuf,0,NULL);
    return 0;
  }
  if (ENDSWITH(path,path_l,"-journal") ||ENDSWITH(path,path_l,"-wal")) log_entered_function(ANSI_RED"journal and wal %s"ANSI_RESET,path);
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
    //if (ENDSWITH(path,path_l,"wiff")) log_debug_now("path: %s  %ld\n",path,stbuf->st_size);
  }else{
    IF1(WITH_AUTOGEN,long size;
        if(_realpath_autogen && (size=autogen_size_of_not_existing_file(path,path_l))>=0){
          stat_init(stbuf,size,&zpath->stat_rp);
          stbuf->st_ino=inode_from_virtualpath(path,path_l);
          return 0;
        });
    err=ENOENT;
  }
  LOCK(mutex_fhdata,inc_count_getattr(path,err?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS));
  IF1(DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (err) debug_track_false_getattr_errors(path,path_l));

  if (configuration_file_is_readonly(path,path_l)) stbuf->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  //if (debug_trigger_vp(path,path_l))    if (err) DIE_DEBUG_NOW0("err != 0");
  return -err;
}
static int xmp_access(const char *path, int mask){
  const int path_l=strlen(path);
  if (whatSpecialFile(path,path_l)>=SFILE_BEGIN_VIRTUAL) return 0;
  NEW_ZIPPATH(path);
  int res=find_realpath_any_root(0,zpath,NULL)?0:ENOENT;
  assert(VP()[0]=='/'||VP()[0]==0);
  if (res==-1) res=ENOENT;
  if (!res && (mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) res=access(RP(),(mask&~X_OK)|R_OK);
  if (res) report_failure_for_tdf(path);
  LOCK(mutex_fhdata,inc_count_getattr(path,res?COUNTER_ACCESS_FAIL:COUNTER_ACCESS_SUCCESS));
  if (debug_trigger_vp(path,path_l)){ log_debug_now("%s returning %d",path,minus_val_or_errno(res)); if (false && res) DIE_DEBUG_NOW0("res != 0");}
  return minus_val_or_errno(res);
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  bool found;FIND_REALPATH(path);
  if (!found) return -ENOENT;
  const int n=readlink(RP(),buf,size-1);
  return n==-1?-errno: (buf[n]=0);
}
static int xmp_unlink(const char *path){
  bool found;FIND_REALPATH(path);
  return !ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(unlink(RP()));
}
static int xmp_rmdir(const char *path){
  bool found;FIND_REALPATH(path);
  return !ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(rmdir(RP()));
}


#define LOG2_FD_ZIP_MIN 20
#define FD_ZIP_MIN (1<<LOG2_FD_ZIP_MIN)
#define FHANDLE_FLAG_CHANGED_TO_WRITE (1<<(LOG2_FD_ZIP_MIN-1))
static int xmp_open(const char *path, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  if (tdf_or_tdf_bin(path)) log_entered_function("%s",path);
  const int path_l=strlen(path);
  {
    const int i=whatSpecialFile(path,path_l);
    if (i==SFILE_INFO) LOCK(mutex_special_file,++_info_count_open;make_info());
    if (i>=SFILE_BEGIN_VIRTUAL) return 0;
  }
  if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))){
    return create_or_open(path,0775,fi);
  }
  IF1(WITH_AUTOGEN, if (_realpath_autogen && path!=path) autogen_remove_if_not_up_to_date(path,path_l));
  static ht_hash_t warn_lst;
#define C(x)  if ((fi->flags&x) && !ENDSWITH(path,path_l,".tdf")){ ht_hash_t warn_hash=hash_value_strg(path)+x; if (warn_lst!=warn_hash) log_warn("%s %s\n",#x,path); warn_lst=warn_hash;}
  C(O_RDWR);  C(O_CREAT);
#undef C
  uint64_t handle=0;
  /*if (config_keep_file_attribute_in_cache(path)) fi->keep_cache=1;*/
  static uint64_t next_fh=FD_ZIP_MIN;
  bool found;FIND_REALPATH(path);
  IF1(WITH_AUTOGEN, if (found && _realpath_autogen && (zpath->flags&ZP_STARTS_AUTOGEN) && config_autogen_file_is_invalid(path,path_l,&zpath->stat_vp, _root_writable->rootpath)) found=false);
  if (found){
    if (ZPATH_IS_ZIP()){
      while(zpath){
        LOCK(mutex_fhdata,if (fhdata_create(handle=next_fh++,zpath)) zpath=NULL); /* zpath is now stored in fhdata */
        if (zpath){log_verbose("Going to sleep and retry fhdata_create %s ...",path);usleep(1000*1000);}
      }
    }else{
      ASSERT(zpath!=NULL);
      handle=open(RP(),fi->flags);
      if (handle<=0) warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"open:  fh=%d",handle);
      if (has_proc_fs() && !cg_check_path_for_fd("my_open_fh",RP(),handle)) handle=-1;
    }
  }else{
#if WITH_AUTOGEN
    if (_realpath_autogen && (zpath->flags&ZP_STARTS_AUTOGEN) && autogen_size_of_not_existing_file(path,path_l)>=0){
      //log_debug_now("Going autogen %s",path);
      LOCK(mutex_fhdata,
           autogen_zpath_init(zpath,path);
           struct fhdata *d=fhdata_create(handle=next_fh++,zpath);
           d->flags|=FHDATA_FLAGS_IS_AUTOGEN;
           zpath=NULL;
           found=true);
    }
#endif //WITH_AUTOGEN
    if (!found && report_failure_for_tdf(path)){
      log_zpath("Failed ",zpath);
      warning(WARN_OPEN|WARN_FLAG_MAYBE_EXIT,path,"FIND_REALPATH failed");
    }
  }
  if (!found || handle==-1){
    if (!config_not_report_stat_error(path,path_l)) warning(WARN_GETATTR,path,"found:%s handle:%llu",success_or_fail(found),handle);
    if (debug_trigger_vp(path,path_l)){
      //DIE_DEBUG_NOW("found: %s handle: %lu",success_or_fail(found), handle);
    }
    return !found?-ENOENT:-errno;
  }
  fi->fh=handle;
  return 0;
}/*xmp_open*/


#if IS_ORIG_FUSE
#define PARA_TRUNCATE ,struct fuse_file_info *fi
#else
#define PARA_TRUNCATE
#endif
// IF0(IS_NETBSD||IS_APPLE
int xmp_truncate(const char *path, off_t size PARA_TRUNCATE){
  int res;
  IF1(IS_ORIG_FUSE,if (fi)    res=ftruncate(fi->fh,size); else)
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

// FUSE 3.5 Added a new cache_readdir flag to fuse_file_info to enable caching of readdir results. Supported by kernels 4.20 and newer.
#if FUSE_MAJOR_V>=3 && FUSE_MINOR_V>-5
#define  WITH_FUSE_READDIR_FLAGS 1
#endif


//static int hello_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi IF1(IS_ORIG_FUSE,,enum fuse_readdir_flags flags)){
  (void)offset;(void)fi;
  struct ht no_dups={0};
  ht_init_with_keystore_dim(&no_dups,8,4096);
  NEW_ZIPPATH(path);
  int opt=0;
  bool ok=false;
  const int path_l=strlen(path);
  FOR(cut_autogen,0,(zpath->flags&ZP_STARTS_AUTOGEN)?2:1){
    foreach_root(ir,r){
      if (find_realpath_any_root(opt|(cut_autogen?0:FINDRP_AUTOGEN_CUT_NOT),zpath,r)){ /* FINDRP_AUTOGEN_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
        opt=FINDRP_NOT_TRANSIENT_CACHE; /* Transient cache only once */
        filler_readdir(0,zpath,buf,filler,&no_dups);
        IF1(WITH_AUTOGEN, if (cut_autogen) filler_readdir(READDIR_AUTOGEN,zpath,buf,filler,&no_dups));
        ok=true;
        if (!cut_autogen && !ENDSWITH(path,path_l,EXT_CONTENT) && config_readir_no_other_roots(RP(),RP_L())) break;
      }
    }
  }
  if (*path=='/' && !path[1]){ /* Folder ZIPsFS in the root */
    filler(buf,&DIR_ZIPsFS[1],NULL,0  COMMA_FILL_DIR_PLUS);
    ok=true;
  }else if (!strcmp(path,DIR_ZIPsFS)){ /* Childs of folder ZIPsFS */
    FOR(i,0,SFILE_L) filldir(0,filler,buf,i==SFILE_DEBUG_CTRL?DIRNAME_AUTOGEN:SPECIAL_FILES[i]+1,NULL,&no_dups);
    ok=true;
  }
  ht_destroy(&no_dups);
  LOCK(mutex_fhdata,inc_count_getattr(path,ok?COUNTER_READDIR_SUCCESS:COUNTER_READDIR_FAIL));
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
static int create_or_open(const char *path, mode_t mode,struct fuse_file_info *fi){
  char real_path[MAX_PATHLEN+1];
  {
    const int res=realpath_mk_parent(real_path,path);
    if (res) return -res;
  }
  if (!(fi->fh=MAX_int(0,open(real_path,fi->flags|O_CREAT,mode)))){
    log_errno("open(%s,%x,%u) returned -1\n",real_path,fi->flags,mode); cg_log_file_mode(mode); cg_log_open_flags(fi->flags); log_char('\n');
    return -errno;
  }
  return 0;
}
static int xmp_create(const char *path, mode_t mode,struct fuse_file_info *fi){ /* O_CREAT|O_RDWR goes here */
  return create_or_open(path,mode,fi);
}
static int xmp_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  if (!_root_writable) return -EACCES;
  int res=0;
  uint64_t fd;
  if(!fi){
    char real_path[MAX_PATHLEN+1];
    if ((res=realpath_mk_parent(real_path,path))) return -res;
    fd=MAX_int(0,open(real_path,O_WRONLY));
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
static int xmp_symlink(const char *target, const char *path){ // target,link
  if (!_root_writable) return -EACCES;
  char real_path[MAX_PATHLEN+1];
  if (!(realpath_mk_parent(real_path,path)) && symlink(target,real_path)==-1) return -errno;
  return 0;
}

#if IS_ORIG_FUSE
#define WITH_PARA_FLAGS 1
#else
#define WITH_PARA_FLAGS 0
#endif
int xmp_rename(const char *old_path, const char *neu_path IF1(WITH_PARA_FLAGS,, uint32_t flags)){ // from,to
  #if WITH_GNU && WITH_PARA_FLAGS
  bool eexist=false;
  if (flags&RENAME_NOREPLACE){
    bool found;FIND_REALPATH(neu_path);
    if (found) eexist=true;
  }else if (flags) return -EINVAL;
#endif //WITH_GNU


  bool found;FIND_REALPATH(old_path);
    if (!found) return -ENOENT;
    IF1(WITH_GNU,if (eexist) return -EEXIST);


  if (!ZPATH_ROOT_WRITABLE()) return -EACCES;

  char neu[MAX_PATHLEN+1];
  int res=realpath_mk_parent(neu,neu_path);
  if (!res) res=rename(RP(),neu);
  return minus_val_or_errno(res);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////
#if HAS_FUSE_LSEEK
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  int ret=off;
  LOCK_N(mutex_fhdata,
         struct fhdata* d=fhdata_get(path,fi->fh);
         if (d){
           switch(whence){
           #if __USE_GNU
                        case SEEK_HOLE:ret=(d->offset=d->zpath.stat_vp.st_size);break;
           case SEEK_DATA:
#endif // __USE_GNU
           case SEEK_SET: ret=d->offset=off;break;
           case SEEK_CUR: ret=(d->offset+=off);break;
           case SEEK_END: ret=(d->offset=d->zpath.stat_vp.st_size+off);break;

           }
         });
  if (!d) warning(WARN_SEEK|WARN_FLAG_ONCE,path,"d is NULL");
  return ret;
}
#endif // HAS_FUSE_LSEEK
/* Read size bytes from zip entry.   Invoked only from fhdata_read() unless memcache_is_advised(d) */
static off_t fhdata_read_zip(const char *path, char *buf, const off_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi){
  cg_thread_assert_not_locked(mutex_fhdata);
  if (!fhdata_zip_open(d,__func__)){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"xmp_read_fhdata_zip fhdata_zip_open returned -1");
    return -1;
  }
  { /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    const off_t diff=offset-zip_ftell(d->zip_file);
    if (diff){
      if ( /* zip_file_is_seekable(d->zip_file) && */ !zip_fseek(d->zip_file,offset,SEEK_SET)){}
#if WITH_MEMCACHE
      else if (diff<0 && _memcache_policy!=MEMCACHE_NEVER){ /* Worst case=seek backward - consider using cache */
        d->flags|=FHDATA_FLAGS_URGENT;
        const off_t num=memcache_waitfor(d,offset+size);
        if (num>0){
          fi->direct_io=1;
          fhdata_zip_close(d);
          _count_readzip_memcache_because_seek_bwd++;
          fhdata_counter_inc(d,ZIP_READ_CACHE_SUCCESS);
          return memcache_read(buf,d,offset,num);
        }
        fhdata_counter_inc(d,ZIP_READ_CACHE_FAIL);
      }
#endif //WITH_MEMCACHE
    }
  }
  {
    const int64_t pos=zip_ftell(d->zip_file);
    if (pos<0){ warning_zip(path,zip_file_get_error(d->zip_file),"zip_ftell()");return -1;}
    if (offset<pos){ /* Worst case=seek backward - need reopen zip file */
      warning_zip(path,zip_file_get_error(d->zip_file),"zip_ftell() - going to reopen zip\n");
      fhdata_zip_close(d);
      fhdata_zip_open(d,"SEEK bwd.");
    }
  }
  for(int64_t diff;1;){
    const int64_t pos=zip_ftell(d->zip_file);
    if (pos<0){ warning_zip(path,zip_file_get_error(d->zip_file),"zip_ftell()");return -1;}
    if (!(diff=offset-pos)) break;
    if (diff<0){ warning(WARN_SEEK,path,"diff<0 should not happen %ld",diff);return -1;}
    if (zip_fread(d->zip_file,buf,MIN(size,diff))<0){
      if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,path,"zip_fread returns value<0    diff=%ld size=%zu",diff,MIN(size,diff));
      return -1;
    }
  }
  // if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_VIA_ZIP,d)){ log_msg("xmp_read_fhdata_zip %s size=%zu offset=%lu fh=%lu \n",path,size,offset,d->fh); }
  const off_t num=zip_fread(d->zip_file,buf,size);
  if (num<0) warning_zip(path,zip_file_get_error(d->zip_file)," zip_fread()");
  fhdata_counter_inc(d,num<0?ZIP_READ_NOCACHE_FAIL:!num?ZIP_READ_NOCACHE_ZERO:ZIP_READ_NOCACHE_SUCCESS);
  return num;
}/*xmp_read_fhdata_zip*/
///////////////////////////////////////////////////////////////////
/// Invoked from xmp_read, where struct fhdata *d=fhdata_get(path,fd);
/// Previously, in xmp_open() struct fhdata *d  had been made by fhdata_create if and only if ZPATH_IS_ZIP()
static off_t fhdata_read_zip1(char *buf, const off_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi){
  ASSERT(d->is_xmp_read>0);  cg_thread_assert_not_locked(mutex_fhdata);
  if (size>(int)_log_read_max_size) _log_read_max_size=(int)size;
  const struct zippath *zpath=&d->zpath;
#if WITH_MEMCACHE
  LOCK_N(mutex_fhdata, struct memcache *m=d->memcache; const bool is_memcache=m && m->memcache_status || memcache_is_advised(d) && (m=new_memcache(d)));
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
#endif //WITH_MEMCACHE
  return fhdata_read_zip(D_VP(d),buf,size,offset,d,fi);
}/*xmp_read_fhdata*/
static int64_t _debug_offset;
static bool xmp_read_check_d(struct fhdata *d,const char *path){
  if (!D_VP(d)){
    warning(WARN_FHDATA|WARN_FLAG_ERROR,path,"d=%p path is NULL",d);
    return false;
  }else if (!(d->flags&FHDATA_FLAGS_IS_AUTOGEN) && !D_RP(d)){
    log_zpath("",&d->zpath);
    warning(WARN_FHDATA|WARN_FLAG_ERROR,path,"D_RP(d)! is NULL  d=%p zpath= %p",d,&d->zpath);
    return false;
  }
  return true;
}
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  const int path_l=strlen(path);
  uint64_t fd=fi->fh;
  int res=0;
  {
    const int i=whatSpecialFile(path,path_l);
    if (i>0){
      if (i==SFILE_INFO)   LOCK(mutex_special_file, if(_info_count_open<=0) warning(WARN_FLAG_ONCE|WARN_FLAG_ERROR|WARN_MISC,path,"xmp_read: _info_count_open=%d",_info_count_open));
      if (i>=SFILE_BEGIN_VIRTUAL) LOCK(mutex_special_file,res=read_special_file(i,buf,size,offset));
      return res;
    }
  }
  LOCK_N(mutex_fhdata,
         struct fhdata *d=fhdata_get(path,fd);
         if (d){
           if (!d->debug_not_yet_read && tdf_or_tdf_bin(path)){ d->debug_not_yet_read=true; log_verbose(ANSI_MAGENTA"1st"ANSI_RESET" read %s",path);}
           IF1(WITH_AUTOGEN,if (d->fh_real){fd=d->fh_real;});
           d->accesstime=time(NULL);
           if (!xmp_read_check_d(d,path)) d=NULL;
           else d->is_xmp_read++; /* Prevent fhdata_destroy() */
         });
  if (d){
    IF1(WITH_MEMCACHE,struct memcache *m=d->memcache);
    IF1(WITH_AUTOGEN,if ((d->flags&FHDATA_FLAGS_IS_AUTOGEN) && _realpath_autogen && !d->fh_real){
        if (!d->autogen_state) autogen_run(d);
        if (d->autogen_state!=AUTOGEN_SUCCESS){
          res=-EPIPE;
        }else if (m && m->memcache2){ /* Auto-generated data into cg_textbuffer */
          res=memcache_read(buf,d,offset,offset+size);
        }else{ /* Auto-generated data into plain file */
          d->fh_real=fd=open(D_RP(d),fi->flags);
        }
      }else){
      assert(d->is_xmp_read>=0);
      if ((d->zpath.flags&ZP_ZIP)!=0){ // Important for libzip. At this point we see 2 threads reading the same zip entry.
        pthread_mutex_lock(&d->mutex_read);
        res=fhdata_read_zip1(buf,size,offset,d,fi);
        pthread_mutex_unlock(&d->mutex_read);
      }
      LOCK_N(mutex_fhdata,
             if (res>0) d->n_read+=res;
             assert(d->is_xmp_read>=0);
             const int64_t n=d->n_read;
             const char *status=IF0(WITH_MEMCACHE,"!WITH_MEMCACHE")IF1(WITH_MEMCACHE,MEMCACHE_STATUS_S[!m?0:m->memcache_status]));
      if (res<0 && !config_not_report_stat_error(path,path_l)){
        warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"res<0:  d=%p  off=%ld size=%zu  res=%d  n_read=%llu  memcache_status:%s"ANSI_RESET,d,offset,size,res,n,status);
      }
    }
  }
  if (!d IF1(WITH_AUTOGEN,|| d->fh_real)){
    if (!fd){
      res=-errno;
    }else if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK_REG_FILE:"ANSI_RESET" offset: %'jd ",(intmax_t)offset),log_msg("Failed %s fd=%llu\n",path,(LLU)fd);
      res=-1;
    }else if ((res=pread(fd,buf,size,offset))==-1){
      res=-errno;
    }
  }
  if (d) LOCK(mutex_fhdata,d->is_xmp_read--);
  return res;
}
static int xmp_release(const char *path, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  if (tdf_or_tdf_bin(path)) log_entered_function("%s",path);
  const int path_l=strlen(path);
  const uint64_t fh=fi->fh;
  const int special=whatSpecialFile(path,path_l);
  if (special==SFILE_INFO){
    //LOCK(mutex_special_file,if (--_info_count_open==0){ FREE2(_info); _info_capacity=_info_l=0;});
  }
  if (special>=SFILE_BEGIN_VIRTUAL) return 0;
  if (fh>=FD_ZIP_MIN){
    LOCK(mutex_fhdata,fhdata_destroy_and_evict_from_pagecache(fh,path,path_l));
  }else if (fh>2){
    if (close(fh)){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,path,"my_close_fh %llu",fh);
      cg_print_path_for_fd(fh);
    }
  }
  return 0;
}
static int xmp_flush(const char *path,  struct fuse_file_info *fi){
  const int path_l=strlen(path);
  return whatSpecialFile(path,path_l)>=SFILE_BEGIN_VIRTUAL?0:
    fi->fh<FD_ZIP_MIN?fsync(fi->fh):
    0;
}
static void _exit_ZIPsFS(){
  log_entered_function0("");
  int wroteMsg=0;
  while(true){
    int count=0;
    LOCK(mutex_fhdata,foreach_fhdata(id,d){ count++;break;});
    if (!count) break;
    if (!wroteMsg++) log_strg("Going to wait for all fhdata  to be closed ...\n");
    usleep(count*200);
  }
  if (!wroteMsg++) log_strg("Wait for all fhdata  to be closed "GREEN_DONE"\n");

  if (wroteMsg) log_strg(GREEN_SUCCESS);
#if WITH_DIRCACHE
  foreach_root(i,r) LOCK(mutex_dircache,mstore_clear(DIRCACHE(r));ht_destroy(&r->dircache_ht));
#endif //WITH_DIRCACHE
  IF1(WITH_AUTOGEN,config_autogen_cleanup_before_exit());
}





int main(int argc,char *argv[]){
#define C(var) fprintf(stderr,"  %s: %d\n",#var,var);
  C(IS_LINUX);C(IS_APPLE);C(IS_FREEBSD);C(IS_OPENBSD);C(IS_CLANG);C(WITH_GNU);
  C(HAS_BACKTRACE);C(HAS_UNDERSCORE_ENVIRON);
  #undef C
  fprintf(stderr,"MAX_PATHLEN: %d\n",MAX_PATHLEN);
  fprintf(stderr,"has_proc_fs: %s\n",yes_no(has_proc_fs()));
  fprintf(stderr,"FUSE_MAJOR_VERSION=%d FUSE_MINOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
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
  bool foreground=false;
  FOR(i,0,argc){
    if(!strcmp(":",argv[i])) colon=i;
    foreground|=colon && !strcmp("-f",argv[i]);
  }
  if (!colon){ log_warn0("No single colon found in parameter list\n"); usage(); return 1;}
  if (colon==argc-1){ log_warn0("Expect mount point after single colon \n"); usage(); return 1;}
  log_msg(ANSI_INVERSE""ANSI_UNDERLINE"This is %s  main(...)"ANSI_RESET"\nCompiled: %s %s  PID: "ANSI_FG_WHITE ANSI_BLUE"%d"ANSI_RESET"\n",path_of_this_executable(),__DATE__,__TIME__,getpid());
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in fprintf */
  ASSERT(S_IXOTH==(S_IROTH>>2));
  IF1(WITH_ZIPINLINE,config_containing_zipfile_of_virtual_file_test());
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  IF1(IS_LINUX,S(init));
  S(getattr); S(access);
  S(readlink);
  //S(opendir);
  S(readdir);   S(mkdir);
  S(symlink); S(unlink);
  S(rmdir);   S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  IF1(HAS_FUSE_LSEEK,S(lseek));
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log_strg("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  //    argv_fuse[argc_fuse++]="--fuse-flag";    argv_fuse[argc_fuse++]="sync_read";
  for(int c;(c=getopt(argc,argv,"+qnks:c:S:l:L:"))!=-1;){  // :o:sfdh
    switch(c){
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 'S': _pretendSlow=true; break;
    case 's': strncpy(_mkSymlinkAfterStart,optarg,MAX_PATHLEN); break;
    case 'h': usage();break;
    case 'l': if ((_memcache_maxbytes=cg_atol_kmgt(optarg))<1<<22){
        log_error("Option -l: _memcache_maxbytes is too small %s\n",optarg);
        return 1;
      }
      break;
    case 'L':{
      static struct rlimit _rlimit={0};
      _rlimit.rlim_cur=_rlimit.rlim_max=cg_atol_kmgt(optarg);
      log_msg("Setting rlimit to %llu MB \n",(LLU)_rlimit.rlim_max>>20);
      setrlimit(RLIMIT_AS,&_rlimit);
    } break;
#if WITH_MEMCACHE
    case 'c':{
      bool ok=false;
      for(int i=0;!ok && WHEN_MEMCACHE_S[i];i++) if((ok=!strcasecmp(WHEN_MEMCACHE_S[i],optarg))) _memcache_policy=i;
      if (!ok){ log_error("Wrong option -c %s\n",optarg); usage(); return 1;}
    } break;
#endif //WITH_MEMCACHE
    }
  }
  ASSERT(MAX_PATHLEN<=PATH_MAX);
  char dot_ZIPsFS[MAX_PATHLEN+1],dirOldLogs[MAX_PATHLEN+1];
  _mnt_l=strlen(strncpy(_mnt,argv[argc-1],MAX_PATHLEN));
  { /* dot_ZIPsFS */
    {
      char *dir=dot_ZIPsFS+strlen(cg_copy_path(dot_ZIPsFS,"~/.ZIPsFS/"));
      strcat(dir,_mnt);
      for(;*dir;dir++) if (*dir=='/') *dir='_';
      snprintf(dirOldLogs,MAX_PATHLEN,"%s%s",dot_ZIPsFS,"/oldLogs");
      cg_recursive_mkdir(dirOldLogs);
    }
    FOR(i,0,2){
      snprintf(_fWarningPath[i],MAX_PATHLEN,"%s%s",dot_ZIPsFS,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
      struct stat stbuf;
      if (!lstat(_fWarningPath[i],&stbuf) && stbuf.st_size){ /* Save old logs with a mtime in file name. */
        const time_t t=stbuf.st_mtime;
        struct tm lt;
        localtime_r(&t,&lt);
        char oldLog[MAX_PATHLEN+1];
        snprintf(oldLog,MAX_PATHLEN,"%s%s",dirOldLogs,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
        strftime(strrchr(oldLog,'.'),22,"_%Y_%m_%d_%H:%M:%S",&lt);
        strcat(oldLog,".log");
        assert(!rename(_fWarningPath[i],oldLog));
        const pid_t pid=fork();
        assert(pid>=0);
        if (pid>0){
          int status;
          waitpid(pid,&status,0);
        }else{
          assert(!execlp("gzip","gzip","-f","--best",oldLog,(char*)NULL));
          assert(1);
        }
      }
      _fWarnErr[i]=fopen(_fWarningPath[i],"w");
    }
    warning(0,NULL,"");
    snprintf(_debug_ctrl,MAX_PATHLEN,"%s/%s",dot_ZIPsFS,"ZIPsFS_debug_ctrl.sh");
    {
      char mstore_base[MAX_PATHLEN+1];
      snprintf(mstore_base,MAX_PATHLEN,"%s/cachedir",dot_ZIPsFS);
      mstore_set_base_path(mstore_base);
      cg_recursive_mkdir(mstore_base);
    }
    SPECIAL_FILES[SFILE_DEBUG_CTRL]=_debug_ctrl;
    {
      struct textbuffer b={0};
      special_file_content(&b,SFILE_DEBUG_CTRL);
      const int fd=open(_debug_ctrl,O_RDONLY);
      if (fd<0 || textbuffer_differs_from_filecontent_fd(&b,fd)){
        log_msg("Going to write %s ...\n",_debug_ctrl);
        textbuffer_write_file(&b,_debug_ctrl,0770);
      }else{
        log_msg(ANSI_FG_GREEN"Up-to-date %s\n"ANSI_RESET,_debug_ctrl);
      }
      if (fd>0) close(fd);
    }
  }
  FOR(i,optind,colon){ /* Source roots are given at command line. Between optind and colon */
    if (_root_n>=ROOTS) DIE("Exceeding max number of ROOTS=%d   Increase constant ROOTS in configuration.h and recompile!\n",ROOTS);
    const char *p=argv[i];

    if (!*p){
      log_warn("Command line argument # %d is empty. %s\n",optind,i==optind?"Consequently, there will be no writable root.":"");
      continue;
    }
    struct rootdata *r=_root+_root_n++;
    if (i==optind) (_root_writable=r)->features|=ROOT_WRITABLE;
    {
      int slashes=-1;while(p[++slashes]=='/');
      if (slashes>1){ r->features|=ROOT_REMOTE; p+=(slashes-1); }
    }
    log_msg("Going to obtain realpath of  %s ...  ",p);
    r->rootpath_l=cg_strlen(realpath(p,r->rootpath));
    log_msg("Realpath is  %s\n",r->rootpath);
    if (!r->rootpath_l) r->rootpath_l=cg_strlen(realpath(p,r->rootpath));
    if (!r->rootpath_l && i!=optind){perror("");      DIE("realpath '%s':  %s  is empty\n",p,r->rootpath);}
    ht_set_mutex(mutex_dircache,ht_init(&r->dircache_ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|12));
    ht_set_mutex(mutex_dircachejobs,ht_init(&r->dircache_queue,8));
    assert(r->dircache_queue.keystore==NULL);
    assert((r->dircache_queue.flags&(HT_FLAG_KEYS_ARE_STORED_EXTERN|HT_FLAG_NUMKEY))==0);

#if WITH_DIRCACHE || WITH_STAT_CACHE

    ht_set_mutex(mutex_dircache,ht_init_interner_file(&r->dircache_ht_fname,"dircache_ht_fname",_root_n,16,DIRECTORY_CACHE_SIZE));

    ht_set_mutex(mutex_dircache,ht_init_interner_file(&r->dircache_ht_fnamearray,"dircache_ht_fnamearray",_root_n,HT_FLAG_BINARY_KEY|12, DIRECTORY_CACHE_SIZE));

    ht_set_mutex(mutex_dircache,ht_init_with_keystore_file(&r->dircache_ht_dir,"dircache_ht_dir",_root_n,HT_FLAG_KEYS_ARE_STORED_EXTERN|12,DIRECTORY_CACHE_SIZE));
#endif
  }/* Loop roots */

  {
    log_msg0("\n\nRoots:\n");
    foreach_root(i,r){
      log_msg("\t%d\t%s\t%s\n",i,rootpath(r),!cg_strlen(rootpath(r))?"":(r->features&ROOT_REMOTE)?"Remote":(r->features&ROOT_WRITABLE)?"Writable":"Local");
    }
  }
  { /* Storing information per file type for the entire run time */
    mstore_set_mutex(mutex_fhdata,mstore_init(&mstore_persistent,0x10000));
    ht_set_mutex(mutex_fhdata,ht_init_interner(&ht_intern_fileext,8,4096));
  }
  IF1(WITH_ZIPINLINE_CACHE,ht_set_mutex(mutex_dircache,ht_init(&zipinline_cache_virtualpath_to_zippath_ht,HT_FLAG_NUMKEY|16)));
  IF1(WITH_STAT_CACHE,ht_set_mutex(mutex_dircache,ht_init(&stat_ht,0*HT_FLAG_DEBUG|16)));
  log_strg("\n"ANSI_INVERSE"Roots"ANSI_RESET"\n");
  if (!_root_n){ log_error0("Missing root directories\n");return 1;}


  bool warn=false;
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
  C(DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,0);
  C(WITH_STAT_SEPARATE_THREADS,1);
  C(DEBUG_DIRCACHE_COMPARE_CACHED,0);
  C(DEBUG_TRACK_FALSE_GETATTR_ERRORS,0);
  C(WITH_AUTOGEN,1);
  C(WITH_EVICT_FROM_PAGECACHE,IS_NOT_APPLE);
  C(WITH_EXTRA_ASSERT,0);
  //if (DIRECTORY_CACHE_SIZE*DIRECTORY_CACHE_SEGMENTS<64*1024*1024){ warn=true;log_msg(RED_WARNING"Small file attribute and directory cache of only %d\n",DIRECTORY_CACHE_SEGMENTS*DIRECTORY_CACHE_SEGMENTS/1024);}
  IF1(WITH_AUTOGEN,if (!cg_is_member_of_group("docker")){ log_warn0(HINT_GRP_DOCKER); warn=true;});
#undef C
  FILE *tty=fopen("/dev/tty","r");
  if (warn && !strstr(_mnt,"/cgille/") && tty){ fprintf(stderr,"Press enter ");getc(tty);}
  if (!foreground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=true;
  foreach_root(ir,r){
    RLOOP(t,PTHREAD_LEN){
      if (ir&&t==PTHREAD_MISC0) continue; /* Only one instance */
      root_start_thread(r,t);
    }
  }
#if WITH_AUTOGEN
  char realpath_autogen_heap[MAX_PATHLEN+1];
  if (_root_writable){
    strcat(strcpy(_realpath_autogen=realpath_autogen_heap,_root_writable->rootpath),DIR_AUTOGEN);
    config_autogen_init();
  }
#endif
  static pthread_t thread_unblock;
  pthread_create(&thread_unblock,NULL,&infloop_unblock,NULL);
  _textbuffer_memusage_lock=_mutex+mutex_textbuffer_usage;
  log_msg("Running %s with PID %d. Going to fuse_main() ...\n",argv[0],getpid());


  /* { */
  /*   char *new_argv[999]; */
  /*   new_argv[0]=argv[0]; */
  /*   int new_argc=argc-colon; */
  /*   FOR(i,1,new_argc) new_argv[i]=argv[i+colon]; */
  /*   _fuse_argc=new_argc; */
  /*   _fuse_argv=new_argv; */
  /*   for(int i=0;i<new_argc;i++) printf("%d new_argv %s \n",i, new_argv[i]); */
  /* } */



  const int fuse_stat=fuse_main(_fuse_argc=argc-colon,_fuse_argv=argv+colon,&xmp_oper,NULL);
  log_msg(RED_WARNING" fuse_main returned %d\n",fuse_stat);
  const char *memleak=malloc(123);
  IF1(DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF1(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS(fuse_stat);
}

// _FILE_SYSTEM_INFO.HTML
// napkon      astra zenika Agathe  find
////////////////////////////////////////////////////////////////////////////
/* static bool find_realpath_any_rootxxx(struct zippath *zpath,const struct rootdata *onlyThisRoot){ */
/*   bool alsoCache=false; */
/*   foreach_root(ir,r){ */
/*     if (onlyThisRoot && onlyThisRoot!=r) continue; */
/*     if (find_realpath_nocache(zpath,r)) return true; */
/*   } */
/*   return false; */
/* } */
// SSMetaData zipinfo //s-mcpb-ms03/slow2/incoming/Z1/Data/30-0089/20230719_Z1_ZW_027_30-0089_Serum_EAD_14eV_3A_OxoIDA_rep_01.wiff2.Zip
// FHDATA_MAX
// 44 readlink


// FHDATA_MAX
// ls /s-mcpb-ms03/union/.mountpoints/is/2/PRO2/Data/30-0106/20240330_PRO2_AF_093_50-0139_GCKD-384_P09_H16.d
// ls: cannot access '/s-mcpb-ms03/union/.mountpoints/is/2/PRO2/Data/30-0106/20240330_PRO2_AF_093_50-0139_GCKD-384_P09_H16.d': No such file or directory


// freiwala@20240404+1109_50-0139_allFiles_PeptideAtlasWithIS_MBR.log.bat
// ~/Projects/ZIPsFS/test/diann/anja_cannot
// DIE DIE0 DEBUG_NOW fuse_session_exit
// mkdir rule proc malloc fcntl O_PATH group_member execvpe  posix_fadvise lseek fill_dir_plus  __APPLE__ __clang__ COMMA_FILL_DIR_PLUS
// log_debug_now log_debug_now0 mmap ys/mman.h  unistd _GNU_SOURCE DEBUG_NOW backtrace execinfo unistd.h
