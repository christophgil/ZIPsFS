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
#define FOR0(var,to) for(int var=0;var<to;var++)
#define RLOOP(var,from) for(int var=from;--var>=0;)
#define LOG_STREAM stdout
#define A1(x) C(x,INIT)C(x,STR)C(x,INODE)C(x,THREAD)C(x,MALLOC)C(x,ROOT)C(x,OPEN)C(x,READ)C(x,ZIP_FREAD)C(x,READDIR)C(x,SEEK)C(x,ZIP)C(x,GETATTR)C(x,STAT)C(x,FHDATA)C(x,DIRCACHE)C(x,MEMCACHE)C(x,FORMAT)C(x,MISC)C(x,DEBUG)C(x,CHARS)C(x,RETRY)C(x,LEN)
#define A2(x) C(x,nil)C(x,queued)C(x,reading)C(x,done)C(x,interrupted)
#define A3(x) C(x,NEVER)C(x,SEEK)C(x,RULE)C(x,COMPRESSED)C(x,ALWAYS)
#define A4(x) C(x,nil)C(x,fhdata)C(x,dircachejobs)C(x,log_count)C(x,crc)C(x,inode)C(x,memUsage)C(x,dircache)C(x,idx)C(x,statqueue)C(x,validchars)C(x,validcharsdir)C(x,roots) //* mutex_roots must be last */
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
#include "cg_ht_v5.c"
#include "cg_utils.c"
#include "cg_log.c"
//#define PLACEHOLDER_NAME 0x1A
#define PLACEHOLDER_NAME '*'
#include "ZIPsFS_configuration.c"
#include "cg_debug.c"

#define ROOTS (1<<LOG2_ROOTS)
#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000
#define DEBUG_ABORT_MISSING_TDF 1
#define SIZE_POINTER sizeof(char *)
#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
///////////////////////////
//// Structs and enums ////
///////////////////////////

static int _fhdata_n=0, _log_count_lock=0;
enum fhdata_having{having_any,having_memcache,having_no_memcache};
static enum when_memcache_zip _memcache_policy=MEMCACHE_SEEK;
static bool _pretendSlow=false;
static int64_t _memcache_maxbytes=3L*1000*1000*1000;
static char _mkSymlinkAfterStart[MAX_PATHLEN+1]={0},_mnt[MAX_PATHLEN+1];
static const char *HOMEPAGE="https://github.com/christophgil/ZIPsFS";
///////////////
/// pthread ///
///////////////
static pthread_mutex_t _mutex[mutex_roots+ROOTS];
static pthread_key_t _pthread_key;
#define THREADS_PER_ROOT 3
/* Count recursive locks with (_mutex+mutex). Maybe inc or dec. */
static int mutex_count(int mutex,int inc){
  int8_t *locked=pthread_getspecific(_pthread_key);
  if (!locked){
#define  R (ROOTS*THREADS_PER_ROOT+99)
    static int i=0;
    static int8_t all_locked[R][mutex_roots+ROOTS];
    pthread_mutex_lock(_mutex+mutex_idx);i++;pthread_mutex_unlock(_mutex+mutex_idx);
    //log_debug_now(ANSI_RED"pthread_setspecific for %s %d"ANSI_RESET"\n",MUTEX_S[mutex],i);
    ASSERT(i<R);
    pthread_setspecific(_pthread_key,locked=all_locked[i]);
    ASSERT(locked!=NULL);
  }
#undef R
  //log_debug_now("mutex_count %s %d \n",MUTEX_S[mutex],locked[mutex]);
  if (inc>0) ASSERT(locked[mutex]++<127);
  if (inc<0) ASSERT(locked[mutex]-->=0);
  return locked[mutex];
}
#define DO_ASSERT_LOCK 1
static void lock(int mutex){ /* With DO_ASSERT_LOCK 50 nanosec. Without 25 nanosec. Not recursive 30 nanosec. */
  _log_count_lock++;
  pthread_mutex_lock(_mutex+mutex);
  if (DO_ASSERT_LOCK) mutex_count(mutex,1);
}
static void unlock(int mutex){
  if (DO_ASSERT_LOCK) mutex_count(mutex,-1);
  pthread_mutex_unlock(_mutex+mutex);
}
static int _oldstate_not_needed;
#define LOCK(mutex,code) lock(mutex);code;unlock(mutex)
#define LOCK_NCANCEL(mutex,code) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&_oldstate_not_needed);lock(mutex);code;unlock(mutex);pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&_oldstate_not_needed);

#define ASSERT_LOCKED_FHDATA() assert_locked(mutex_fhdata)
static bool _assert_locked(int mutex,bool yesno,bool log){
  const int count=mutex_count(mutex,0), ok=(yesno==(count>0));
  //if (log || !ok) log_debug_now("_assert_locked %s  %s: %d\n",yes_no(yesno),MUTEX_S[mutex],count);
  return ok;
}
#define assert_locked(mutex)       if (DO_ASSERT_LOCK) ASSERT(_assert_locked(mutex,true,false));
#define assert_not_locked(mutex)   if (DO_ASSERT_LOCK) ASSERT(_assert_locked(mutex,false,false));
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

/* The struct fhdata holds data associated with a file descriptor.
   Stored in a linear list _fhdata. The list may have holes.
   Memcache: It may also contain the cached file content of the zip entry.
   Only one of all instances with a specific virtual path should store the cached zip entry  */
struct fhdata{
  uint64_t fh; /* Serves togehter with path as key to find the instance in the linear array.*/
  char *path; /* The virtual path serves as key*/
  uint64_t path_hash; /*Serves as key*/
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
};

#define STATQUEUE_ENTRY_N 256
struct statqueue_entry{
  volatile int time;
  char rp[MAX_PATHLEN+1];
  volatile int rp_l;
  volatile uint64_t rp_hash;
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
  struct cache_by_hash dircache_byhash_name, dircache_byhash_dir;
  pthread_t pthread[PTHREAD_LEN];
  int pthread_count_started[PTHREAD_LEN];
  int pthread_when_response_deciSec; /* When last response. Overflow after after few years does not matter. */
  int pthread_when_loop_deciSec[PTHREAD_LEN]; /* Detect blocked loop. */
  struct statqueue_entry statqueue[STATQUEUE_ENTRY_N];
  bool debug_pretend_blocked[PTHREAD_LEN];
  struct fhdata *memcache_d;
};
#define foreach_root(ir,r)    int ir=0;for(struct rootdata *r=_root; ir<_root_n; r++,ir++)
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int _root_n=0;
static struct rootdata _root[ROOTS]={0}, *_writable_root=NULL;

enum memusage{memusage_mmap,memusage_malloc,memusage_n,memusage_get_curr,memusage_get_peak};
#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
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
static long  _count_stat_success=0, _count_stat_fail=0, _count_stat_from_cache=0;
static struct ht stat_ht,internalize_rp_ht,zipinline_cache_virtualpath_to_zippath_ht;
#define INTERNALIZE_RP(s,s_l,hash) ht_internalize_string(&internalize_rp_ht,s,s_l,hash)

static bool debug_path(const char *vp){
  return vp!=NULL && NULL!=strstr(vp,
                                  //"20230116_Z1_ZW_001_30-0061_poolmix_2ug_ZenoSWATH_T600_V4000_rep01"
                                  "20230116_Z1_ZW_001_30-0061_poolmix_2ug_ZenoSWATH_T600_V4000_rep02"
                                  );
}
static void _debug_nanosec(const char *msg,const int i,const char *path,struct timespec *t){
  if (!t->tv_nsec){
    log_debug_now("%s #%d path: %s\n",msg,i,path);
  }
}
#define DEBUG_NANOSEC(i,path,t) _debug_nanosec(__func__,i,path,t)
static bool stat_maybe_cache(bool alsoSyscall, const char *path,const int path_l,struct stat *stbuf){
  uint32_t valid_seconds=0;
  const int now=deciSecondsSinceStart();
  const uint64_t hash=hash64(path,path_l);
  struct cached_stat *st=NULL;
  ASSERT(path!=NULL);
  ASSERT(strlen(path)>=path_l);
  if (IS_STAT_CACHE){
    if (0<(valid_seconds=config_file_attribute_valid_seconds(true,path,path_l))){
      LOCK(mutex_dircache,st=ht_get(&stat_ht,path,path_l,hash));
      if (st && valid_seconds){
        valid_seconds=config_file_attribute_valid_seconds(!(st->st_mode&(S_IWUSR|S_IWGRP|S_IWOTH)),path,path_l);
        if (now-st->when_read_decisec<valid_seconds*10L){
#define C(f) stbuf->st_##f=st->st_##f
          C(ino);C(mtim);C(mode);C(uid);C(gid);
#undef C
          _count_stat_from_cache++;
          //DEBUG_NANOSEC(1,path,&st->st_mtim);
          return true;
        }
      }
    }
  }
  //static int count=0;  if (!(count++&1023)) log_msg("\n"ANSI_MAGENTA" ncount=%d"ANSI_RESET"\n",count);
  if (!alsoSyscall) return false;
  const bool ok=!stat(path,stbuf);
  if (!ok){
    _count_stat_fail++;
    return false;
  }
  _count_stat_success++;
#define C(f) st->st_##f=stbuf->st_##f
  if (IS_STAT_CACHE){
    LOCK(mutex_dircache,
         if (!st) st=mstore_malloc(&_root->dircache,sizeof(struct cached_stat),8);
         C(ino);C(mtim);C(mode);C(uid);C(gid);
         st->when_read_decisec=now;
         ht_set(&stat_ht,INTERNALIZE_RP(path,path_l,hash),path_l,hash,st);
         assert(st==ht_get(&stat_ht,path,path_l,hash));
         assert(st->st_mtim.tv_nsec==stbuf->st_mtim.tv_nsec));
    //    DEBUG_NANOSEC(3,path,&st->st_mtim);
  }
#undef C
  return true;
}

#define assert_validchars(t,s,len,msg) _assert_validchars(t,s,len,msg,__func__)
static void _assert_validchars(enum validchars t,const char *s,int len,const char *msg,const char *fn){
  static bool initialized;
  if (!initialized) initialized=validchars(VALIDCHARS_PATH)[PLACEHOLDER_NAME]=validchars(VALIDCHARS_FILE)[PLACEHOLDER_NAME]=true;
  const int pos=find_invalidchar(t,s,len);
  if (pos>=0){
    LOCK_NCANCEL(mutex_validchars,
                 static struct ht ht={0};
                 if (!ht.capacity) ht_init(&ht,HT_FLAG_INTKEY|12);
                 const uint64_t hash=hash64(s,len);
                 if (!ht_get_int(&ht,hash,len)){
                   ht_set_int(&ht,hash,len,(void*)1UL); warning(WARN_CHARS|WARN_FLAG_ONCE_PER_PATH,s,ANSI_FG_BLUE"%s()"ANSI_RESET" %s: position: %d",fn,msg?msg:"",pos); }
                 );
  }
}
#define  assert_validchars_direntries(t,dir) _assert_validchars_direntries(t,dir,__func__)
static void _assert_validchars_direntries(enum validchars t,const struct directory *dir,const char *fn){
  if (dir){
    RLOOP(i,dir->core.files_l){
      const char *s=dir->core.fname[i];
      _assert_validchars(VALIDCHARS_PATH,s,my_strlen(s),dir->dir_realpath,fn);
    }
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Consider ZIP file 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff2.Zip
// 20220802_Z1_AF_001_30-0001_SWATH_P01_Plasma-96PlatePosition-Rep4.wiff.scan  will be stored as *.wiff.scan in the table-of-content
// * denotes the PLACEHOLDER_NAME
//
// This will save space in cache.  Many ZIP files will have identical table-of-content and thus kept only once in the cache.
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
static const char *simplify_name(char *s,const char *u, const char *zipfile){
  ASSERT(u!=NULL);
  ASSERT(s!=NULL);
  ASSERT(s!=u);
  const int ulen=my_strlen(u);
  if (strchr(u,PLACEHOLDER_NAME)){
    static int alreadyWarned=0;
    if(!alreadyWarned++) warning(WARN_CHARS,zipfile,"Found SUBSTITUTE in file name");
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
  ASSERT(u!=NULL);  ASSERT(s!=NULL);  ASSERT(s!=u);
  char *placeholder=strchr(s,PLACEHOLDER_NAME);
  const int slen=my_strlen(s);
  if (IS_DIRCACHE_OPTIMIZE_NAMES_RESTORE && placeholder){
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
//////////////////////
// struct directory //
//////////////////////
#if 0
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d){
  if (!d->fname){ log_error("%s %s %s: d->fname is NULL\n",__func__,func,msg);exit(9);}
  const bool print=(strchr(msg,'/')!=NULL);
  if (print) printf("\n"ANSI_INVERSE"%s Directory %s   files_l=%d\n"ANSI_RESET,func,msg,d->files_l);
  FOR0(i,d->files_l){
    const char *n=d->fname[i];
    if (!n){ printf("%s %s: d->fname[%d] is NULL\n",__func__,func,i);exit(9);}
    const int len=strnlen(n,MAX_PATHLEN);
    if (len>=MAX_PATHLEN){ log_error("%s %s %s: strnlen d->fname[%d] is %d\n",__func__,func,msg,i,len);exit(9);}
    const char *s=Nth0(d->fname,i);
    if (print) printf(" (%d)\t%"PRIu64"\t%'zu\t%s\t%p\t%lu\n",i,Nth0(d->finode,i), Nth0(d->fsize,i),s,s,hash_value_strg(s));
  }
}
#endif
static void mstore_assert_lock(struct mstore *m){
  //log_entered_function("%s\n",yes_no(m->debug_mutex));
  if (m->debug_mutex){
    assert_locked(m->debug_mutex);
  }else{
    if (!m->debug_thread) m->debug_thread=(pthread_t)pthread_self();
    else ASSERT(m->debug_thread==(uint64_t)pthread_self());
  }
}
#define MSTORE_ADDSTR(m,str,len) (mstore_assert_lock(m),mstore_addstr(m,str,len))
#define MSTORE_ADD(m,src,bytes,align) (mstore_assert_lock(m),mstore_add(m,src,bytes,align))
#define MSTORE_ADD_REUSE(m,str,len,hash,byhash) (mstore_assert_lock(m),mstore_add_reuse(m,str,len,hash,byhash))
#define MSTORE_DESTROY(m) (mstore_assert_lock(m),mstore_destroy(m))
#define MSTORE_CLEAR(m) (mstore_assert_lock(m),mstore_clear(m))
#define MSTORE_USAGE(m) (mstore_assert_lock(m),mstore_usage(m))
#define MSTORE_COUNT_SEGMENTS(m) (mstore_assert_lock(m),mstore_count_segments(m))


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
  const char *s=IS_DIRCACHE_OPTIMIZE_NAMES?simplify_name(buf_for_s,n0,dir->dir_realpath):n0;
  const int len=pathlen_ignore_trailing_slash(s);
  ASSERT(len>0);
  if (d->fflags) d->fflags[L]=flags|(s[len]=='/'?DIRENT_ISDIR:0);
#define C(name) if (d->f##name) d->f##name[L]=name
  C(mtime);
  C(size);
  C(crc);
  C(inode);
#undef C
  ASSERT(d->fname!=NULL);
  assert_locked(mutex_dircache);
  const char *n=d->fname[L++]=(char*)MSTORE_ADDSTR(&dir->filenames,s,len);
  ASSERT(n!=NULL);
  ASSERT(len==strlen(n));  // DEBUG_NOW ASSERT !!!!!. Therefore added LOCK(mutex_dircache...)
#undef B
#undef L

}



/////////////////////////////////////////////////////////
// Zip inline cache                                    //
// Faster /bin/ls with many inlined ZIPs               //
// Can be deaktivated by setting  IS_ZIPINLINE_CACHE to 0 //
/////////////////////////////////////////////////////////
//   time ls -l  $TZ/Z1/Data/30-0072 |nl
//   time ls -l  $TZ/Z1/Data/30-0051 |nl
static const char *zipinline_cache_virtualpath_to_zippath(const char *vp,const int vp_l){
  const char *zip=ht_get_int(&zipinline_cache_virtualpath_to_zippath_ht,hash64(vp,vp_l),vp_l);
  if (!zip) return NULL;
  /* validation because we rely on hash only */
  const char *vp_name=vp+last_slash(vp)+1;
  const int len=config_zipentry_to_zipfile(0,vp_name,NULL);
  return strncmp(vp_name,zip+last_slash(zip)+1,len)?NULL:zip;
}

// static void zipinline_cache_set_zip(

//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
static void dircache_directory_to_cache(const struct directory *dir){
  assert_locked(mutex_dircache);
  if (!IS_DIRCACHE_PUT) return;
  struct rootdata *r=dir->root;
  dircache_clear_if_reached_limit(r,DIRECTORY_CACHE_SEGMENTS);
  assert_validchars_direntries(VALIDCHARS_PATH,dir);
  struct directory_core src=dir->core, *d=MSTORE_ADD(&r->dircache,&src,S_DIRECTORY_CORE,SIZE_T);
  RLOOP(i,src.files_l){
    const char *s=src.fname[i];
    ASSERT(s!=NULL);
    const int len=strlen(s);
    (d->fname[i]=(char*)MSTORE_ADD_REUSE(&r->dircache,s,(len+1),hash64(s,len),&r->dircache_byhash_name))[len]=0;
  }
#define C(F,type) if (src.F) d->F=MSTORE_ADD(&r->dircache,src.F,src.files_l*sizeof(type),sizeof(type))
  C_FILE_DATA_WITHOUT_NAME();
#undef C
  /* Save storage when directories look similar.   ZIP files with identical table-of-content after applying simplify_name will have same hash. */
  const uint64_t bytes=src.files_l*SIZE_POINTER,hash=hash64((char*)src.fname,bytes);
  d->fname=(char**)MSTORE_ADD_REUSE(&r->dircache,(char*)d->fname,bytes,hash, &r->dircache_byhash_dir); /* Save the array of pointers */
  const int rp_l=strlen(dir->dir_realpath);
  const char *rp=INTERNALIZE_RP(dir->dir_realpath,rp_l,0); /* The real path of the zip file */
  ht_set(&r->dircache_ht,rp,0,0,d);
  if (IS_ZIPINLINE_CACHE && dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
    static char buffer_u[PATH_MAX+1], vp_entry[PATH_MAX+1];
    const char *vp=rp+r->root_path_l;
    const int slash=last_slash(vp),vp_l=rp_l-r->root_path_l;
    strncpy(vp_entry,vp,slash+1);
    RLOOP(i,src.files_l){
      strcpy(vp_entry+slash+1,unsimplify_name(buffer_u,d->fname[i],rp));
      ht_set_int(&zipinline_cache_virtualpath_to_zippath_ht,hash_value_strg(vp_entry),strlen(vp_entry),rp);
      //const int vp_entry_l=strlen(vp_entry);
      //if (DEBUG_NOW==DEBUG_NOW) assert(rp==ht_get_int(&zipinline_cache_virtualpath_to_zippath_ht,hash_value_strg(vp_entry),vp_entry_l));
      /* We are not storing vp_entry permanently and rely on its hash value */
      //static int count; if (debug_path(rp)) log_debug_now(" # %d HHHHHHHHHHHHHHHHHHHHHHHHH %s #%d => %s %p \n",count++,vp_entry,vp_entry_l,rp,rp);
      // /Z1/Data/30-0061/20230118_Z1_ZW_002_30-0061_zSWATH_NATstd__001.wiff2  /s-mcpb-ms03.charite.de/fularchiv01.d/EquiValent/152/1/Z1/Data/30-0061/20230118_Z1_ZW_002_30-0061_zSWATH_NATstd__001.wiff2.Zip
    }
  }
}
#define timespec_equals(a,b) (a.tv_sec==b.tv_sec && a.tv_nsec==b.tv_nsec)
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim){
  assert_locked(mutex_dircache);
  if (!IS_DIRCACHE_GET) return false;
  struct directory_core *s=ht_get(&dir->root->dircache_ht,dir->dir_realpath,0,0);
  if (s){ // DEBUG_NOW
    if (!timespec_equals(s->mtim, mtim)){/* Cached data not valid any more. */
      // if (s->mtim.tv_sec!=mtim.tv_sec){
      ht_set(&dir->root->dircache_ht,dir->dir_realpath,0,0,NULL);
      if (DEBUG_NOW==DEBUG_NOW) log_debug_now("diff s-mtime %ld,%ld  %ld,%ld\n",s->mtim.tv_sec,s->mtim.tv_nsec, mtim.tv_sec,mtim.tv_nsec);
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

///////////////////////////////////////////////////////////
// The root directories are specified as program arguments
// The _root[0] may be read/write, and can be empty string
// The others are read-only
//
// #include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#define FILE_FS_INFO "/_FILE_SYSTEM_INFO.HTML"
#define FILE_LOG_WARNINGS "/_FILE_SYSTEM_WARNINGS_AND_ERRORS.log"
static char _fwarn[MAX_PATHLEN+1];
static long _count_readzip_memcache=0,_count_readzip_regular=0,_count_readzip_seekable[2]={0},_count_readzip_no_seek=0,_count_readzip_seek_fwd=0,_count_readzip_memcache_because_seek_bwd=0,
  _count_readzip_reopen_because_seek_bwd=0,_log_read_max_size=0,_count_SeqInode=0, _count_stat_cache_from_fhdata=0,_count_crc32_ok=0,_count_crc32_bad=0;
#include "ZIPsFS_debug.c"
#include "ZIPsFS_log.c"


///////////////////////////////////////////////////////////////////////
/// statqueue - running stat() in separate thread to avoid blocking ///
///////////////////////////////////////////////////////////////////////
#define foreach_statqueue_entry(i,q) struct statqueue_entry *q=r->statqueue;for(int i=0;i<STATQUEUE_ENTRY_N;q++,i++)
#define STATQUEUE_ADD_FOUND_SUCCESS (STATQUEUE_ENTRY_N+1)
#define STATQUEUE_ADD_FOUND_FAILURE (STATQUEUE_ENTRY_N+2)
#define STATQUEUE_ADD_NO_FREE_SLOT (STATQUEUE_ENTRY_N+3)
#define STATQUEUE_DONE(status) (status==STATQUEUE_FAILED||status==STATQUEUE_OK)
static int statqueue_add(struct rootdata *r, const char *rp, int rp_l, uint64_t rp_hash, struct stat *stbuf){
  //log_entered_function("path=%s\n",rp);
  assert_locked(mutex_statqueue); ASSERT(rp_l<MAX_PATHLEN);
  if (IS_STAT_CACHE){
    bool ok;
    LOCK(mutex_dircache,ok=stat_maybe_cache(false,rp,rp_l,stbuf));
    if (ok) return 0;
  }
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
  const uint64_t hash=hash64(path,len);
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
  FOR0(try,4){
    //if (DEBUG_NOW==DEBUG_NOW && ok) debug_nanosec("statqueue_stat",path,stbuf->st_tim);
    if (_statqueue_stat(path,stbuf,r)){
      if (try) warning(WARN_RETRY,path,"Succeeded on attempt %d\n",try);
      return true;
    }
  }
  return false;
}
#define PRETEND_BLOCKED(thread) while(r->debug_pretend_blocked[thread]) usleep(1000*1000);
static void infloop_statqueue_start(void *arg){ pthread_start(arg,PTHREAD_STATQUEUE);}
static void infloop_memcache_start(void *arg){ pthread_start(arg,PTHREAD_MEMCACHE);}
static void infloop_dircache_start(void *arg){ pthread_start(arg,PTHREAD_DIRCACHE);}
static void pthread_start(struct rootdata *r,int ithread){
  r->debug_pretend_blocked[ithread]=false;
  void *(*f)(void *);
  switch(ithread){
  case PTHREAD_STATQUEUE:
    if (!(r->features&ROOT_REMOTE)) return;
    f=&infloop_statqueue;
    break;
  case PTHREAD_MEMCACHE:
    f=&infloop_memcache;
    break;
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
          uint64_t hash;
          int l=*path=0;
          LOCK_NCANCEL(mutex_statqueue,if IS_Q(){ memcpy(path,q->rp,(l=q->rp_l)+1);hash=q->rp_hash;});
          path[l]=0;
          if (*path){
            const bool ok=stat_maybe_cache(true,path,l,&stbuf);
            if (ok) r->pthread_when_loop_deciSec[PTHREAD_STATQUEUE]=W=deciSecondsSinceStart();
            //if (DEBUG_NOW==DEBUG_NOW  && res && !config_not_report_stat_error(path)) log_debug_now("stat failed %s\n",path);
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
      stat(_mkSymlinkAfterStart,&stbuf);
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
static void init_mutex(){
  pthread_key_create(&_pthread_key,NULL);
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  RLOOP(i,mutex_roots+_root_n){
    pthread_mutex_init(_mutex+i,&_mutex_attr_recursive);
  }

}



static void dircache_clear_if_reached_limit(struct rootdata *r,size_t limit){
  if (!r) return;

  assert_locked(mutex_dircache);
  const size_t ss=MSTORE_COUNT_SEGMENTS(&r->dircache);
  if (ss>=limit){
    warning(WARN_DIRCACHE,r->root_path,"Clearing directory cache. Cached segments: %zu (%zu) bytes: %zu. Consider to increase DIRECTORY_CACHE_SEGMENTS",ss,limit,MSTORE_USAGE(&r->dircache));
    MSTORE_CLEAR(&r->dircache);
    ht_clear(&r->dircache_ht);
    cache_by_hash_clear(&r->dircache_byhash_name);
    cache_by_hash_clear(&r->dircache_byhash_dir);
    if (r==_root){
      if (IS_ZIPINLINE_CACHE)  ht_clear(&zipinline_cache_virtualpath_to_zippath_ht);
      if (DEBUG_NOW!=DEBUG_NOW) if (IS_STAT_CACHE) ht_clear(&stat_ht);
      ht_clear(&internalize_rp_ht);
    }
  }
}
static void dircache_clear_if_reached_limit_all(){
  LOCK(mutex_dircache,foreach_root(i,r)  dircache_clear_if_reached_limit(r,0));
}
/////////////////////////////////////////////////////////////////////////////////////////////
/// 1. Return true for local file roots.
/// 2. Return false for remote file roots (starting with double slash) that have not responded for long time
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
      if (!fdOrZero) close(fd);
    }
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
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
  C(realpath);
#undef C
}
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,NULL)?0:ENOENT;


static void zpath_reset_keep_VP(struct zippath *zpath){ /* Reset to. Later probe a different realpath */
  VP0()=EP()=RP()=NULL;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_LEN()=EP_LEN()=zpath->rp_crc=0;
  zpath->root=-1;
  zpath->strgs_l=VP_LEN();
  zpath->flags=0;
}
static void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack){
  //memset(zpath,0,sizeof(struct zippath));
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
  //ASSERT(zpath->stat_rp.st_ino!=0);
  return true;
}
//#define log_seek_ZIP(delta,...)   log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),log_msg(__VA_ARGS__)
#define log_seek(delta,...)  log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),log_msg(__VA_ARGS__)
static struct zip *zip_open_ro(const char *orig){
  struct zip *zip=NULL;
  if (orig){
    RLOOP(try,2){
      int err;
      if ((zip=zip_open(orig,ZIP_RDONLY,&err))) break;
      warning(WARN_OPEN,orig,"zip_open_ro err=%d",err);
      usleep(1000);
    }
    //log_exited_function("zip_open_ro %s\n",orig);
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
  //log_debug_now("rp=%s\n",rp);
  if (!rp) return false;
  //if (endsWithZip(rp,0)) log_entered_function("rp=%s\n",rp);
  bool debug=endsWithZip(rp,strlen(rp)) && (mydir->dir_flags&DIRECTORY_TO_QUEUE);
  LOCK_NCANCEL(mutex_dircache,int result=dircache_directory_from_cache(mydir,st->st_mtim)?1:0);

  if (!result && (mydir->dir_flags&DIRECTORY_TO_QUEUE)){ /* Read zib dir asynchronously */
    LOCK_NCANCEL(mutex_dircachejobs,ht_set(&mydir->root->dircache_queue ,rp,0,0,""));
    result=-1;
  }
  if(!result){
    result=-1;
    if (mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
      //putchar('*');
      struct zip *zip=zip_open_ro(rp);
      if (zip){
        struct zip_stat sb;
        const int n=zip_get_num_entries(zip,0);
        mydir->core.finode=NULL;
        FOR0(k,n){
          if (!zip_stat_index(zip,k,0,&sb)){
            LOCK(mutex_dircache,directory_add_file((sb.comp_method?DIRENT_IS_COMPRESSED:0),mydir,0,sb.name,sb.size,sb.mtime,sb.crc));
          }
        }
        result=1;
        zip_close(zip);
      }
    }else{
      DIR *dir=opendir(rp);
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
      if ((mydir->dir_flags&DIRECTORY_IS_ZIPARCHIVE) || config_cache_directory(rp,mydir->root->features&ROOT_REMOTE,st->st_mtim)){
        LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(mydir));
      }
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
    int iteration=*path=0;
    struct ht_entry *e;
    LOCK_NCANCEL(mutex_dircachejobs,
                 e=ht_next(&r->dircache_queue,&iteration);
                 if (e && e->key){
                   my_strncpy(path,e->key,MAX_PATHLEN);
                   FREE2(e->key);
                   e->value=NULL;
                 });
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
  if (!zpath_stat(zpath,r)) return false;
  if (ZPATH_IS_ZIP()){
    if (!endsWithZip(RP(),0)) return false;
    if (EP_LEN()) return 0==filler_readdir_zip(r,zpath,NULL,NULL,NULL); /* This sets the file attribute of zip entry  */
  }
  return true;
}
static bool find_realpath_special_file(struct zippath *zpath){
  if (!strcmp(VP(),FILE_LOG_WARNINGS)){
    zpath->realpath=zpath_newstr(zpath);
    zpath_strcat(zpath,_fwarn);
    return zpath_stat(zpath,NULL);
  }
  return false;
}
/* Uses different approaches and calls test_realpath */
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r){
  EP()=zpath_newstr(zpath);
  zpath->flags|=ZP_ZIP;
  if (!zpath_strcat(zpath,vp+last_slash(vp)+1)) return false;
  EP_LEN()=zpath_commit(zpath);
  //zpath_assert_strlen(zpath);
  return test_realpath(zpath,r);
}
static bool find_realpath_any_root(struct zippath *zpath,const struct rootdata *onlyThisRoot){
  if (!onlyThisRoot && find_realpath_special_file(zpath)) return true;
  const char *vp=VP(); /* At this point, Only zpath->virtualpath is defined. */
  const int vp_l=VP_LEN();
  char *append="";
  if (IS_ZIPINLINE_CACHE){
    const bool debug=debug_path(vp);
    //    log_debug_now("IS_ZIPINLINE_CACHE 1 ========== %s \n",vp);
    const char *cached_zip=zipinline_cache_virtualpath_to_zippath(vp,vp_l);
    //if (debug)  log_debug_now("========== %s # %d --> %s    - %p\n",vp,vp_l,cached_zip,ht_get_int(&zipinline_cache_virtualpath_to_zippath_ht,hash64(vp,vp_l),vp_l));
    if (cached_zip){
      const int l=strlen(cached_zip);
      foreach_root(ir,r){
        if (!wait_for_root_timeout(r)) continue;
        if (!strncmp(cached_zip,r->root_path,r->root_path_l) && cached_zip[r->root_path_l]=='/'){
          //if (debug) log_debug_now("fffffffffffffff cached_zip: %s r: %s\n",cached_zip,r->root_path);
          zpath_reset_keep_VP(zpath);
          VP0()=zpath_newstr(zpath);
          if (!zpath_strcat(zpath,cached_zip+r->root_path_l)) return false;
          VP0_LEN()=zpath_commit(zpath);
          if (find_realpath_try_inline(zpath,vp,r)){
            //if (debug) log_debug_now("FFFFFFFFFFFFFFF %s\n",vp);
            return true;
          }
        }
      }
    }
  }
  foreach_root(ir,r){
    if (onlyThisRoot && onlyThisRoot!=r || !wait_for_root_timeout(r)) continue;
    if (vp_l){
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
            if (pos<vp_l) zpath_strcat(zpath,vp+pos);
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
        //log_debug_now("zzzzzzzzzzzzzzzzz %s   cached_zip=%s\n",vp, cached_zip);
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
    d->memcache_took_mseconds=d->memcache_l=d->memcache_status=0;
  }
}
static void fhdata_destroy(struct fhdata *d){
  if (!d) return;
  ASSERT_LOCKED_FHDATA();
  //if (d->memcache) log_entered_function("%s fhdata_can_destroy=%s\n",d->path,yes_no(fhdata_can_destroy(d)));
  if (d->memcache_status==memcache_queued) d->memcache_status=0;
  if (!fhdata_can_destroy(d) || d->memcache_status==memcache_reading){ /* When reading to RAM cache */
    d->close_later=true;
  }else{
    memcache_free(d);
    log_msg(ANSI_FG_GREEN"Going to release fhdata d=%p path=%s fh=%lu "ANSI_RESET,d,snull(d->path),d->fh);
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
  pthread_mutex_init(&d->mutex_read,NULL);
  d->path_hash=hash_value_strg(path);
  d->fh=fh;
  d->path=strdup(path); /* Important; This must be the  last assignment */
  return d;
}
static struct fhdata* fhdata_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHDATA();
  //log_msg(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  fhdata_destroy_those_that_are_marked();
  const uint64_t h=hash_value_strg(path);
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
          having==having_memcache && !d->memcache_status ||
          having==having_no_memcache && d->memcache_status) continue; /* Otherwise two with same path, each having a cache could stick for ever */
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
  RLOOP(need_statcache,2){ /* Preferably to one with a cache */
    foreach_fhdata_path_not_null(d){
      if (!need_statcache || d->memcache){
        const int n=d->zpath.virtualpath_l;
        const char *vp=d->zpath.virtualpath;
        if (len<=n && vp && !strncmp(path,vp,n)){
          if (len==n||vp[len]=='/'){
            d2=d;
            if (d->statcache_for_subdirs_of_path) return d; /* Preferably one with statcache_for_subdirs_of_path */
          }
        }
      }
    }
  }
  return d2;
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t make_inode(const ino_t inode0,const struct rootdata *r, const int ientry_plus_1,const char *path){
  const static int SHIFT_ROOT=40, SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");;
  if (inode0<(1L<<SHIFT_ROOT) && ientry_plus_1<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((int64_t)ientry_plus_1)<<SHIFT_ENTRY)| ((r->index+1L)<<SHIFT_ROOT);
  }else{
    //log_debug_now("Making inode sequentially inode0=%lu ientry=%d\n",inode0,ientry_plus_1-1);
    static struct ht hts[ROOTS]={0};
    struct ht *ht=hts+r->index;
    ASSERT(sizeof(inode0)==8);
    const uint64_t key1=inode0+1;
    const uint64_t key2=(ientry_plus_1<<1)|1; /* Because of the implementation of hash table ht, key2 must not be zero. We multiply by 2 and add 1. */
    static uint64_t seq=1UL<<63;
    LOCK_NCANCEL(mutex_inode,
                 if (!ht->capacity) ht_init(ht,HT_FLAG_INTKEY|16);
                 uint64_t inod=(uint64_t)ht_get_int(ht,key1,key2);
                 if (!inod){ ht_set_int(ht,key1,key2,(void*)(inod=++seq));_count_SeqInode++;});
    return inod;
  }
}
/* ******************************************************************************** */
/* *** Zip *** */
#define SET_STAT() init_stat(st,isdir?-1: Nth0(d.fsize,i),&zpath->stat_rp);  st->st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(d.finode,i,i)+1,rp); zpath->rp_crc=Nth0(d.fcrc,i); \
  if (Nth0(d.fflags,i)&DIRENT_IS_COMPRESSED) zpath->flags|=ZP_IS_COMPRESSED; \
  st->st_mtime=Nth0(d.fmtime,i)
// DEBUG_NOW  ;st->st_mtim.tv_nsec=77777
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
  FOR0(i,d.files_l){
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
  //if (endsWithZip(rp,0)) log_entered_function("vp=%s rp=%s ZPATH_IS_ZIP=%d  \n",VP(),snull(rp),ZPATH_IS_ZIP());
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
      FOR0(i,d.files_l){
        const char *u=unsimplify_name(buf_for_u,d.fname[i],rp);
        if (empty_dot_dotdot(u) || ht_set(no_dups,u,0,0,"")) continue;
        bool inlined=false; /* Entries of ZIP file appear directory in the parent of the ZIP-file. */
        if (config_zipentries_instead_of_zipfile(u) && (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,u))){
          directory_init(DIRECTORY_IS_ZIPARCHIVE|DIRECTORY_TO_QUEUE,&dir2,direct_rp,r);
          if (statqueue_stat(direct_rp,&stbuf,r) && directory_from_dircache_zip_or_filesystem(&dir2,&stbuf)){
            const struct directory_core d2=dir2.core;
            FOR0(j,d2.files_l){/* Embedded zip file */
              const char *n2=d2.fname[j];
              if (n2 && !strchr(n2,'/')){
                init_stat(&st,(Nth0(d2.fflags,j)&DIRENT_ISDIR)?-1:Nth0(d2.fsize,j),&zpath->stat_rp);
                st.st_ino=make_inode(zpath->stat_rp.st_ino,r,Nth(d2.finode,j,j)+1,RP());
                const char *u2=unsimplify_name(buf_for_u2,n2,direct_rp);
                if (config_zipentry_to_zipfile(0,u2,NULL)) filler(buf,u2,&st,0,fill_dir_plus);
              }
            }
            inlined=true;
          }
        }
        if (!inlined){
          //log_debug_now("iiiiiiiiii NO INLINE %s  %d %d %d\n",u,config_zipentries_instead_of_zipfile(u),statqueue_stat(direct_rp,&stbuf,r),directory_from_dircache_zip_or_filesystem(&dir2,&stbuf));
          // log_debug_now("iiiiiiiiii NO INLINE %s  %d %d \n",u,config_zipentries_instead_of_zipfile(u),directory_from_dircache_zip_or_filesystem(&dir2,&stbuf));
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
  if (!VP_LEN()){
    RLOOP(i,2){
      const char *s=i==1?FILE_FS_INFO:FILE_LOG_WARNINGS;
      if (!ht_set(no_dups,s,0,0,"")) filler(buf,s+1,NULL,0,fill_dir_plus);
    }
  }
  //log_exited_function("impl_readdir \n");
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
#if DO_LIBFUSE_CACHE_STAT
  cfg->entry_timeout=cfg->attr_timeout=200;
  cfg->negative_timeout=20.0;
#else
  cfg->entry_timeout=cfg->attr_timeout=2;
  cfg->negative_timeout=10;
#endif
  return NULL;
}
/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null){
  {
    //    static int count=0;    if (count++%100==0){ log_msg("");log_debug_now("count=%d  path=%s\n",count,path);}
  }
  if (!strcmp(path,FILE_FS_INFO)){
    init_stat(stbuf,MAX_INFO,NULL);
    time(&stbuf->st_mtime);
    return 0;
  }
  {
    static int count=0;
    if (!(count++&0xFFFF)) log_msg("\n"ANSI_MAGENTA"xmp_getattr count=%d"ANSI_RESET"\n",count);
  }
  debug_triggerd_by_magic_files(path);
  memset(stbuf,0,sizeof(struct stat));
  //log_entered_function("%s \n",path);
  log_count_b(xmp_getattr_);
  const int slashes=count_slash(path);
  ASSERT(slashes>0); /* path starts with a slash */
  int res=-1;
#define C   (d->statcache_for_subdirs_of_path)
#define L   (d->statcache_for_subdirs_of_path_l)
#define D() struct fhdata* d=fhdata_by_subpath(path); if (d && d->path && (d->zpath.flags&ZP_ZIP))
  if (IS_STATCACHE_IN_FHDATA){ /* Temporary cache stat of subdirs in fhdata. Motivation: We are using software which is sending lots of requests to the FS */
    LOCK(mutex_fhdata,
         D(){
           if (C){
             ASSERT(slashes+1<L);
             if (C[slashes+1].st_ino){
               *stbuf=C[slashes+1];
               res=0;
               _count_stat_cache_from_fhdata++;
             }
           }else{
             const int slashes_vp=count_slash(d->zpath.virtualpath);
             ASSERT(slashes<=slashes_vp);
             C=calloc(L=slashes_vp+2,sizeof(struct stat));
           }
         });
  }
  if (res){ /* Not found in fhdata cache */
    FIND_REALPATH(path);
    if(!res){
      *stbuf=zpath->stat_vp;
      if (IS_STATCACHE_IN_FHDATA){ LOCK(mutex_fhdata, D() if (C){ ASSERT(slashes+1<L); C[slashes+1]=*stbuf;}); }
#undef D
#undef L
#undef C
    }
    if (res && !config_not_report_stat_error(path)){
      warning(WARN_GETATTR,path,"res=%d",res);
      log_zpath("!res",zpath);
    }
    zpath_destroy(zpath);
  }
  if (!res){
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
  log_cache("Going to memcache d= %p %s %'ld Bytes\n",d,d->path,st_size);
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
        FOR0(retry,RETRY_ZIP_FREAD){
          if ((n=zip_fread(zf,d->memcache+d->memcache_already_current,N()))>0){
            if (retry) warning(WARN_RETRY,path,"zip_fread succeeded retry: %d/%d  n=%ld",retry,RETRY_ZIP_FREAD-1,n);
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
            log_debug_now("fhdata_check_crc32 OK %s\n",path);
            _count_crc32_ok++;
            d->memcache_took_mseconds=currentTimeMillis()-start;
            d->memcache_status=memcache_done;
            log_succes("memcache_done  d= %p %s %p  st_size=%zu  in %'ld mseconds\n",d,path,d->memcache,d->memcache_already_current,d->memcache_took_mseconds);
          }else{
            d->memcache_status=d->memcache_already_current=d->memcache_already=0;
            _count_crc32_bad++;
          }
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
  FOR0(retry,RETRY_MEMCACHE_STORE){
    const int ret=memcache_store_try(d);
    if (retry && !ret && d->memcache_already==d->zpath.stat_vp.st_size) warning(WARN_RETRY,d->path,"memcache_store succeeded on retry %d",retry);
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
  if (IS_MEMCACHE_PUT && d && d->path){
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
          //          log_msg(" ~%zu ",d2->memcache_already);
          if (d2->memcache_status==memcache_done){
            ret=d2;
          }else if (d2->memcache_already>=offset+size){
            LOCK(mutex_fhdata, if (d2->memcache_already>=offset+size) ret=d2);
          }
          if (ret) break;
          if (d2->memcache_status==memcache_interrupted || !d2->memcache_status) return NULL;   /* If wrong crc32 then memcache_status set to 0 */
          //if (i%1000==999) log_debug_now("waiting %i  %s\n",i,d->path);
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
#define FD_ZIP_MIN (1<<20)
static int xmp_open(const char *path, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  if (fi->flags&O_WRONLY) return create_or_open(path,0775,fi);
  static uint64_t warn_lst;
#define C(x)  if (fi->flags&x){ uint64_t warn_hash=hash_value_strg(path)+x; if (warn_lst!=warn_hash) log_warn("%s %s\n",#x,path); warn_lst=warn_hash;}
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
      LOCK(mutex_fhdata,
           struct fhdata *d=fhdata_create(path,handle=fi->fh=_next_fh++);
           if (d){
             zpath_stack_to_heap(zpath);
             MAYBE_ASSERT(zpath->realpath!=NULL);
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
/** unsigned int cache_readdir:1; FOPEN_CACHE_DIR Can be filled in by opendir. It signals the kernel to  enable caching of entries returned by readdir(). */
/** unsigned int keep_cache:1;  Can be filled in by open. It signals the kernel that any currently cached file data (ie., data that the filesystem provided the last time the file was open) need not be invalidated. */

// time ls -l   ~/tmp/ZIPsFS/mnt/Z1/Data/30-0072 | nl    store
// time ls   -l   $TZ/Z2/Data/50-0079/ | nl   28976 files 3.0 sec incoming
// time ls   -l   $TZ/Z2/Data/30-0061/ | nl   28976 files 3.0 sec incoming small

/*
  static int xmp_opendir(const char *path, struct fuse_file_info *fi){
  fi->cache_readdir=1;
  return 0;
  }
*/
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  log_count_b(xmp_readdir_);
  (void)offset;(void)fi;(void)flags;
  //log_entered_function("%s \n",path);
  log_mthd_orig();
  int res=-1;
  struct ht no_dups={0};
  ht_init_with_keystore_dim(&no_dups,8,4096);
  foreach_root(ir,r){
    if (!wait_for_root_timeout(r)) continue;
    NEW_ZIPPATH(path);
    if (find_realpath_any_root(zpath,r)){
      filler_readdir(r,zpath,buf,filler,&no_dups);
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
  char real_path[MAX_PATHLEN+1];
  int res=realpath_mk_parent(real_path,create_path);
  if (!res) res=mkdir(real_path,mode);
  return minus_val_or_errno(res);
}
static int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLUE""ANSI_YELLOW"=========="ANSI_RESET" %s\n",create_path);
  char real_path[MAX_PATHLEN+1];
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
  if (!fhdata_zip_open(d,"xmp_read")){warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"xmp_read_fhdata_zip fhdata_zip_open returned -1");return -1;}
  { /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    const int64_t diff=offset-zip_ftell(d->zip_file);
    if (!diff){
      _count_readzip_no_seek++;
    }else if (zip_file_is_seekable(d->zip_file) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
      _count_readzip_seekable[diff>0]++;
    }else if (diff<0 && _memcache_policy!=MEMCACHE_NEVER){ /* Worst case = seek backward - consider using cache */
      d->memcache_is_urgent=true;
      const struct fhdata *d2=memcache_waitfor(d,size,offset);
      if (d2){
        fi->direct_io=1;
        fhdata_zip_close(false,d);
        _count_readzip_memcache_because_seek_bwd++;
        /* See ZIPsFS_notes.org  ** Freezes for one specific tdf */
        LOCK(mutex_fhdata,const int num=memcache_read(buf,d2,size,offset));
        return num;
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
  long debug_old=zip_ftell(d->zip_file);
  const int num=(int)zip_fread(d->zip_file,buf,size);
  //long debug_diff=debug_old+num-zip_ftell(d->zip_file);
  //log_debug_now("%p %s  diff=%ld num=%d  offset=%lu next=%lu size=%lu\n",d,path, debug_diff,num,offset,offset+size,size);
  if (num<0) warning(WARN_ZIP,path," zip_fread(%p,%p,%zu) returned %d\n",d->zip_file,buf,size,num);
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
  if (memcache_is_advised(d) || d->memcache_status){
    struct fhdata *d2=memcache_waitfor(d,size,offset);
    if (d2 && d2->memcache_l){
      int num=0;
      LOCK(mutex_fhdata, if (0<=(num=memcache_read(buf,d2,size,offset))) _count_readzip_memcache++);
      if(num>0 || d2->memcache_l<=offset) return num;
    }
  }
  _count_readzip_regular++;
  return fhdata_read_zip(d->path,buf,size,offset,d,fi);
}/*xmp_read_fhdata*/
static int64_t _debug_offset;
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  const uint64_t fd=fi->fh;
  //log_msg("\n");log_entered_function(ANSI_BLUE" %lx "ANSI_RESET" %s size=%zu offset=%'lu next=%'lu   fh=%lu\n",pthread_self(),path,size,offset,offset+size,fd);
  log_mthd_orig();
  if (!strcmp(path,FILE_FS_INFO)){
    const int i=min_int(MAX_INFO,print_all_info()); // length of info text
    const int n=max_int(0,min_int(i-offset,min_int(size,i-(int)offset))); // num bytes to be copied
    if (n) memcpy(buf,_info+offset,n);
    return n;
  }
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
         if (d){
           d->is_xmp_read++; /* Prevent fhdata_destroy() */
         }
       });
  int res=-1;
  if (d){
    {
      // Important for libzip. At this point we see 2 threads reading the same zip entry.
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
  ASSERT(fi!=NULL);
  log_mthd_orig();
  const uint64_t fh=fi->fh;
  if (fh>=FD_ZIP_MIN){
    LOCK(mutex_fhdata,fhdata_destroy(fhdata_get(path,fh)));
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
static int xmp_flush(const char *path,  struct fuse_file_info *fi){
  const uint64_t fh=fi->fh;
  return fh<FD_ZIP_MIN?fsync(fh):0;
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
  FOR0(i,argc){
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
    char *s=dot_ZIPsFS+sprintf(dot_ZIPsFS,"%s/.ZIPsFS/",getenv("HOME"));
    strcat(s,_mnt);
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
    if (!(r->root_path_l=my_strlen(r->root_path=realpath(p,NULL)))){ log_error("realpath %s   is empty\n",p);return 1;}

    sprintf(r->dircache_files,"%s/cachedir/%d",dot_ZIPsFS,i);
    recursive_mkdir(r->dircache_files);
    mstore_init(&r->dircache,r->dircache_files,DIRECTORY_CACHE_SIZE);
    r->dircache.debug_mutex=mutex_dircache;
    ht_init_with_keystore_dim(&r->dircache_ht,HT_FLAG_KEYS_ARE_STORED_EXTERN|12,4096);
    ht_init(&r->dircache_queue,8);
    assert(r->dircache_queue.keystore==NULL);
    assert((r->dircache_queue.flags&(HT_FLAG_KEYS_ARE_STORED_EXTERN|HT_FLAG_INTKEY))==0);

    cache_by_hash_init(&r->dircache_byhash_name,16,1);
    cache_by_hash_init(&r->dircache_byhash_dir,16,SIZE_POINTER);
  }
  if (IS_ZIPINLINE_CACHE) ht_init(&zipinline_cache_virtualpath_to_zippath_ht,HT_FLAG_INTKEY|16);
  if (IS_STAT_CACHE) ht_init(&stat_ht,0*HT_FLAG_DEBUG|16);
  ht_init_with_keystore(&internalize_rp_ht,HT_FLAG_INTERNALIZE_STRING|20,&_root->dircache);
  log_msg("\n"ANSI_INVERSE"Roots"ANSI_RESET"\n");
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  print_roots(-1); log_strg("\n");
#define C(var,x) if (var!=x) log_msg(RED_WARNING"%s is %d instead of %d\n", #var,var,x)
  C(IS_DIRCACHE_PUT,1);
  C(IS_DIRCACHE_GET,1);
  C(IS_DIRCACHE_OPTIMIZE_NAMES,1);
  C(IS_DIRCACHE_OPTIMIZE_NAMES_RESTORE,1);
  C(IS_MEMCACHE_PUT,1);
  C(IS_MEMCACHE_GET,1);
  C(IS_STATCACHE_IN_FHDATA,1);
  C(IS_ZIPINLINE_CACHE,1);
  C(IS_STAT_CACHE,1);
  C(PLACEHOLDER_NAME,7);
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
  dircache_clear_if_reached_limit_all();
  exit(fuse_stat);

}
