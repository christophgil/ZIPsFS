/*
  ZIPsFS   Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  It has been developed starting with
  fuse-3.14: Filesystem in Userspace
  passthrough.c
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  (global-set-key (kbd "<f1>") '(lambda() (interactive) (switch-to-buffer "ZIPsFS.c")))
*/
#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include "config.h"
//#include "math.h"
#include <fuse.h>
#include <pthread.h>
#include <dirent.h>
#ifdef __FreeBSD__
#include <sys/un.h>
#endif
#include <sys/mman.h>
#include <fuse.h>
#define fill_dir_plus 0
#include <zip.h>
#include <sqlite3.h>
#define LOG_STREAM stdout
#include "cg_log.c"
#include "cg_utils.c"
#include "configuration.h"
#include "ht4.c"
#include "cg_debug.c"
#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000
#define FIND_ZIPFILE 1
// DEBUG_NOW==DEBUG_NOW
#define DEBUG_ABORT_MISSING_TDF 1
//////////////////////////////////////////////////////////////////
// Structs and enums
enum warnings{WARN_INIT,WARN_STR,WARN_INODE,WARN_THREAD,WARN_MALLOC,WARN_ROOT,WARN_OPEN,WARN_READ,WARN_SEEK,WARN_GETATTR, WARN_SQL,WARN_FHDATA,WARN_CACHE,WARN_N};
static int _fhdata_n=0;
enum fhdata_having{having_any,having_cache,having_no_cache};
enum data_op{GET,CREATE,RELEASE};
enum when_cache_zip{NEVER,SEEK,RULE,ALWAYS};
static enum when_cache_zip _when_cache=SEEK;
static char *WHEN_CACHE_S[]={"never","seek","rule","always",NULL}, _sqlitefile[MAX_PATHLEN]={0};
static bool _simulate_slow=false;
static long _maxMemForCache=0;
// #define EQUIVALENT 5
#define READDIR_SEP 0x02
#define LOCK_FHDATA()  pthread_mutex_lock(_mutex+mutex_fhdata)
#define UNLOCK_FHDATA() pthread_mutex_unlock(_mutex+mutex_fhdata)
#define LOCK_FHDATA_REDUNDANT()
//LOCK_FHDATA()
#define UNLOCK_FHDATA_REDUNDANT()
// UNLOCK_FHDATA()
#define LOCK_D() pthread_mutex_lock(&d->mutex)
#define UNLOCK_D() pthread_mutex_unlock(&d->mutex)
struct name_ino_size{char *name; long inode; long size; zip_uint32_t crc; bool is_dir; int b,e,name_n;char *t;};
struct my_strg{
  char *t;
  int b,e, /* begin/end of text */
    capacity,
    root;  /* Index of root. Needed to get the right _ht_job_readdir  */
};
struct zippath{
  char *strgs; /* Contains several strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  int strgs_l,   /* Current Lenght of strgs. */
    realpath_pos, /* Position of RP in strgs */
    current_string; /* The String that is currently build within strgs.*/
  char *virtualpath;
  int virtualpath_l;
  char *virtualpath_without_entry;  /*  Let Virtualpath be "/foo.zip/bar". virtualpath_without_entry will be "/foo.zip". */
  int virtualpath_without_entry_l;
  char *entry_path;  /*  Let Virtualpath be "/foo.zip/bar". entry_path will be "bar". */
  int entry_path_l;
  char *realpath;
  zip_uint32_t rp_crc;
  int root;         /* index of root */
  struct stat stat_rp,stat_vp;
  struct zip *zarchive;
  unsigned int flags;
  //  pthread_mutex_t mutex;
};
struct fhdata{
  uint64_t fh; /* Serves togehter with path as key to find the instance in the linear array.*/
  int fh_of_file;
  char *path; /* The virtual path serves as key*/
  uint64_t path_hash; /*Serves as key*/
  zip_file_t *zip_file;
  struct zippath zpath;
  time_t access;
  char *cache;  /* Zip entries are loaded into RAM */
  size_t cache_l;
  int cache_read_seconds;
  struct stat *cache_stat_subdirs_of_path;
  bool cache_try,close_later;
  long offset;
  int xmp_read; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex;
  uint64_t n_read;
  unsigned int already_logged;
};
struct rootdata{
  int index;
  char *path;
  int features;
  struct statfs statfs;
  long statfs_when;
  int statfs_mseconds,delayed;
};
enum memusage{memusage_mmap,memusage_malloc,memusage_n};
static int _memusage_count[2*memusage_n];
//////////////////////////////////////////////////////////////////////////
// The struct zippath is used to identify the real path
// from the virtual path
// All strings are stacked in the field ->strgs.
// strgs may start as a stack variable and continue to be a heap.
#define ZP_DEBUG (1<<1)
#define ZP_ZIP (1<<2)
#define ZP_STRGS_ON_HEAP (1<<3)
#define ZP_PATH_IS_ONLY_SLASH (1<<4)
#define IS_ZPATH_DEBUG() (zpath->flags&ZP_DEBUG)
#define MASK_PERMISSIONS  ((1<<12)-1)
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
static struct fhdata _fhdata[FHDATA_MAX];
#define LOG2_ROOTS 5
#define ROOTS (1<<LOG2_ROOTS)
static char *CACHE_FILLING="CACHE_FILLING", *CACHE_FAILED="CACHE_FAILED";
///////////////////////////////////////////////////////////
// The root directories are specified as program arguments
// The _root[0] is read/write, and can be empty string
// The others are read-only
//
static pthread_mutexattr_t _mutex_attr_recursive;
enum mutex{mutex_fhdata,mutex_jobs,mutex_debug1,mutex_debug,mutex_log_count,mutex_crc,mutex_inode,mutex_memUsage,mutex_roots}; //* mutex_roots must be last */
static pthread_mutex_t _mutex[mutex_roots+ROOTS];
#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))
#define FILE_FS_INFO "/_FILE_SYSTEM_INFO.HTML"
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int _root_n=0;
static struct rootdata _root[ROOTS]={0};
static struct ht _ht_job_readdir[ROOTS]={0};
static int _count_read_zip_cached=0,_count_read_zip_regular=0,_count_read_zip_seekable=0,_count_read_zip_no_seek=0,_count_read_zip_seek_fwd=0,_count_read_zip_seek_bwd=0,_read_max_size=0, _count_close_later=0,_countSeqInode=0;
static bool fhdata_has_cache(struct fhdata *d){ return d && d->cache && d->cache_l;}
#include "log.c"
static char *ensure_capacity(struct my_strg *s,int n){
  if (n<32) n=32;
  if (!s->t){
    s->t=calloc(s->capacity=n<<1,1);    /* with malloc, valgrind reports  uninitialised value ???? */
  }else if (n+1>s->capacity){
    s->t=realloc(s->t,s->capacity=n<<1);
  }
  return s->t;
}
static void *thread_observe_root(void *arg){
  struct rootdata *r=arg;
  while(1){
    const long before=currentTimeMillis();
    statfs(r->path,&r->statfs);
    if ((r->statfs_mseconds=(int)((r->statfs_when=currentTimeMillis())-before))>ROOT_OBSERVE_EVERY_MSECONDS*2){
      log_warn("\nstatfs %s took %'ld usec\n",r->path,before-r->statfs_when);
    }
    usleep(1000*ROOT_OBSERVE_EVERY_MSECONDS);
  }
}
static void start_threads(){
  pthread_t thread[ROOTS];
  for(int i=_root_n;--i>=0;){
    struct rootdata *r=_root+i;
    if (!my_strlen(r->path)) continue;
    if (r->features&ROOT_REMOTE && pthread_create(thread+i,NULL,&thread_observe_root, (void*)r)) warning(WARN_THREAD|WARN_FLAG_EXIT,r->path,"Creating thread_observe_root %d",i);
    if (pthread_create(thread+i,NULL,&thread_readdir_async, (void*)(_ht_job_readdir+i))) warning(WARN_THREAD|WARN_FLAG_EXIT,r->path,"Creating thread_readdir_async %d",i);
  }
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
static void init_stat(struct stat *st, long size,struct stat *uid_gid){
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
static int my_open_fh(const char* msg, const char *path,int flags){
  int fh=open(path,flags);
  if (fh<=0) warning(WARN_OPEN,path,"my_open_fh:  fh=%d",fh);
  if (!check_path_for_fd("my_open_fh",path,fh)) fh=-1;
  if (_killOnError && fh<=0 && tdf_or_tdf_bin(path)) killOnError_exit();
  return fh;
}
static void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack){
  memset(zpath,0,sizeof(struct zippath));
  assert(virtualpath!=NULL);
  const int l=pathlen_ignore_trailing_slash(virtualpath);
  if (!l){
    assert(*virtualpath=='/' && !virtualpath[1]);
    zpath->flags|=ZP_PATH_IS_ONLY_SLASH;
  }
  zpath->virtualpath=zpath->strgs=strgs_on_stack;
  zpath_strcat(zpath,virtualpath);
  zpath->virtualpath[VP_LEN()=l]=0;
}
static void zpath_assert_strlen(const char *title,struct zippath *zpath){
#define C(X)    if (my_strlen(X())!=X ## _LEN()){  if (title)log("zpath_assert_strlen %s\n",title);  log_error(#X "=%s  %u!=%d\n",X(),my_strlen(X()),X ## _LEN()); log_zpath("Error ",zpath);}
  C(VP);
  C(EP);
#undef C
#define C(a) assert(my_strlen(zpath->a)==zpath->a ## _l);
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
#undef C
  if (!(zpath->flags&ZP_PATH_IS_ONLY_SLASH)){
    //if (title) log("zpath_assert_strlen %s\n",title);
    assert(VP_LEN()>0);
  }
}
static int zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=min_int(my_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      log_abort("zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len);
      return 1;
    }
    my_strncpy(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return 0;
}
static int zpath_strcat(struct zippath *zpath,const char *s){
  return zpath_strncat(zpath,s,9999);
}
static int zpath_strlen(struct zippath *zpath){
  return zpath->strgs_l-zpath->current_string;
}
static char *zpath_newstr(struct zippath *zpath){
  char *s=zpath->strgs+(zpath->current_string=++zpath->strgs_l);
  *s=0;
  return s;
}
static void zpath_stack_to_heap(struct zippath *zpath){
  if (!zpath || (zpath->flags&ZP_STRGS_ON_HEAP)) return;
  zpath->flags|=ZP_STRGS_ON_HEAP;
  const char *stack=zpath->strgs;
  if (!(zpath->strgs=(char*)malloc(zpath->strgs_l+1))) log_abort("zpath_stack_to_heap malloc");
  memcpy(zpath->strgs,stack,zpath->strgs_l+1);
  const long d=(long)(zpath->strgs-stack);
#define C(a) if (zpath->a) zpath->a+=d
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
  C(realpath);
#undef C
}
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REAL(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,-1)
void zpath_reset_realpath(struct zippath *zpath){ /* keep VP(), VP0() and EP() */
  if (!zpath) return;
  {
    struct zip *z=zpath->zarchive;
    if (z){ /* This can raise Signal 14 in futex-internal.h:320:13 */
      zpath->zarchive=NULL;
      if (zip_close(z)==-1) log_zpath(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,zpath);
    }
  }
  zpath->strgs_l=zpath->realpath_pos;
  clear_stat(&zpath->stat_rp);
  clear_stat(&zpath->stat_vp);
}
void zpath_reset_keep_VP(struct zippath *zpath){ /* Reset to. Later probe a different realpath */
  VP0()=EP()=RP()=NULL;
  VP0_LEN()=EP_LEN()=zpath->rp_crc=0;
  zpath->flags&=ZP_PATH_IS_ONLY_SLASH;
  zpath->strgs_l=(int)(VP()-zpath->strgs)+VP_LEN(); /* strgs is behind VP() */
}
static void zpath_destroy(struct zippath *zpath){
  if (zpath){
    LOCK_FHDATA_REDUNDANT();
    zpath_reset_realpath(zpath);
    if (zpath->flags&ZP_STRGS_ON_HEAP){
      assert((long)_fhdata<(long)zpath && (long)zpath<(long)(_fhdata+_fhdata_n));
      FREE(zpath->strgs);
    }
    memset(zpath,0,sizeof(struct zippath));
    UNLOCK_FHDATA_REDUNDANT();
  }
}
static int zpath_stat(struct zippath *zpath,struct rootdata *r){
  if (!zpath) return -1;
  int res=0;
  //char path[MAX_PATHLEN];
  if (!zpath->stat_rp.st_ino){
    for(int tries=(r && r->features&ROOT_REMOTE)?3:1,try=tries;--try>=0;){
      //      for(int equiv=0;equiv<EQUIVALENT;equiv++){        if (!equivalent_path(path,RP(),equiv)) equiv=EQUIVALENT;
      if (!(res=stat(RP(),&zpath->stat_rp)) || !tdf_or_tdf_bin(VP())) goto ok;
      //}
      usleep(1000*100);
    }
  ok:
    if (!res) zpath->stat_vp=zpath->stat_rp;
  }
  return res;
}
//#define log_seek_ZIP(delta,...)   log(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),log(__VA_ARGS__)
#define log_seek(delta,...)  log(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),log(__VA_ARGS__)
static struct zip *zip_open_ro(const char *orig,int equivalent){
  if (!orig) return NULL;
  struct zip *zip=NULL;
  //char path[MAX_PATHLEN];
  log_entered_function("zip_open_ro %s\n",orig);
  for(int try=2;--try>=0;){
    //    for(int equiv=1;equiv<EQUIVALENT;equiv++){
    //if (!equivalent_path(path,orig,(equivalent+equiv)%EQUIVALENT)) equiv=EQUIVALENT;
    int err;
    if ((zip=zip_open(orig,ZIP_RDONLY,&err))) goto ok;
    warning(WARN_OPEN,orig,"zip_open_ro err=%d",err);
    //}
    usleep(1000);
  }
 ok:
  log_exited_function("zip_open_ro %s\n",orig);
  return zip;
}
static struct zip *zpath_zip_open(struct zippath *zpath,int equivalent){
  if (!zpath) return NULL;
  if (!zpath->zarchive) zpath->zarchive=zip_open_ro(RP(),equivalent);
  return zpath->zarchive;
}
///////////////////////////////////
/// Is the virtualpath a zip entry?
///////////////////////////////////
static int zip_contained_in_virtual_path(const char *path, char *append[]){
  const int len=my_strlen(path);
  char *e,*b=(char*)path;
  if(append) *append="";
  for(int i=4;i<=len;i++){
    e=(char*)path+i;
    if (i==len || *e=='/'){
      RECOGNIZE_ZIP_FILES_1();
      RECOGNIZE_ZIP_FILES_2();
      RECOGNIZE_ZIP_FILES_3();
      RECOGNIZE_ZIP_FILES_4();
      if (*e=='/')  b=e+1;
    }
  }
  return 0;
}
/////////////////////////////////////////////////////////////////////
//
// sqlite3
//
sqlite3 *_sqlitedb;
#define SQL_ABORT (1<<1)
#define SQL_SUCCESS (1<<2)
static int sql_exec(int flags,const char* sql, int (*callback)(void*,int,char**,char**), void* udp ){
  char *errmsg=0;
  if (_sqlitedb && sql){
    int res=0;
    if (SQLITE_OK!=(res=sqlite3_exec(_sqlitedb,sql,callback,udp,&errmsg))){
      if (flags&SQL_SUCCESS) warning(WARN_SQL,sql,"sqlite3_errmsg=%s  %d",sqlite3_errmsg(_sqlitedb),res);
      if (flags&SQL_ABORT) abort();
      return 1;
    }
  }
  return 0;
}
int zipfile_callback(void *arg1, int argc, char **argv,char **name){ /* dot-zip-file for given virtualpath */
  char *zipfile=arg1;
  for (int i=0;i<argc;i++){
    const char *n=name[i];
    if (*n=='z' && !strcmp(n,"zipfile") && strlen(n)<MAX_PATHLEN) strcpy(zipfile,argv[i]);
  }
  return 0;
}
struct readdir_sqlresult{ long mtime; struct my_strg *s; bool ok;};
static int readdir_callback(void *arg1, int argc, char **argv,char **name){
  struct readdir_sqlresult *r=arg1;
  struct my_strg *s=r->s;
  for (int i=0;i<argc;i++){
    const char *n=name[i],*a=argv[i];
    if (*n=='m' && !strcmp(n,"mtime")) r->mtime=atol(a);
    else if (*n=='r' && !strcmp(n,"readdir")){
      strcpy(ensure_capacity(s,max_int(3333,s->e=strlen(a))),a);
      s->b=0;
      r->ok=true;
    }else{
      warning(WARN_READ,a,"readdir_callback  %s",n);
    }
  }
  return 0;
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static bool illegal_char(const char *s){ return (s && (strchr(s,'\t')||strchr(s,READDIR_SEP))); }
static void readdir_append(struct my_strg *s, long inode, const char *n,bool append_slash,long size, zip_uint32_t crc){
  if (!illegal_char(n) && !empty_dot_dotdot(n)){
    s->e+=sprintf(s->e+ensure_capacity(s,max_int(3333,s->e+strlen(n)+55)),"%s%s\t%lx\t%lx\t%x%c",n,append_slash?"/":"",inode,size,crc,READDIR_SEP); //
  }
}
static char *my_memchr(const char *b,const char *e,char c){ return e>b?memchr(b,c,e-b):NULL; }
static bool readdir_iterate(struct name_ino_size *nis, struct my_strg *s){ /* Parses the text written with readdir_append and readdir_concat */
  char *t=s->t;
  if (!t) return false;
  char *b=t+s->b,*e=t+s->e;
  if (nis->t!=b){ nis->t=b; nis->b=0; *e=0;}
  nis->e=(int)(strchrnul(b+nis->b+1,READDIR_SEP)-b); // valgrind: uninitialized value
  char *sep1=0,*sep2=0,*sep3=0;
  if (b+nis->e>e ||
      !(sep1=my_memchr(nis->name=b+nis->b,e,'\t')) ||
      !(sep2=my_memchr(sep1+1,e,'\t')) ||
      !(sep3=my_memchr(sep2+1,e,'\t'))
      ) return false;
  nis->name[nis->name_n=sep1-nis->name-(nis->is_dir=(sep1[-1]=='/'))]=0;
  nis->inode=strtol(sep1+1,NULL,16);
  nis->size=strtol(sep2+1,NULL,16);
  nis->crc=strtol(sep3+1,NULL,16);
  nis->b=nis->e+1;
  return true;
}
#define READDIR_ZIP (1<<1)
#define READDIR_ONLY_SQL (1<<2)
#define READDIR_NO_SQL (1<<3)
static bool readdir_concat_unsynchronized(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip){
  if ((opt&READDIR_NO_SQL)==0){ /* Read zib dir asynchronously */
    char sql[999];
    struct readdir_sqlresult sqlresult={0};
    sqlresult.s=s;
    if (SNPRINTF(sql,sizeof(sql),"SELECT mtime FROM readdir WHERE path='%s';",snull(rp))) return false;
    sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
    if (sqlresult.mtime==mtime){
      if (SNPRINTF(sql,sizeof(sql),"SELECT readdir FROM readdir WHERE path='%s';",snull(rp))) return false;
      sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
      if (sqlresult.ok) return true;
    }
  }
  if (opt&READDIR_ONLY_SQL){ /* Read zib dir asynchronously */
    pthread_mutex_lock(_mutex+mutex_jobs);
    ht_set(_ht_job_readdir+s->root,rp,"");
    pthread_mutex_unlock(_mutex+mutex_jobs);
    return false;
  }
  s->e=s->b=sprintf(ensure_capacity(s,3333),"INSERT OR REPLACE INTO readdir VALUES('%s','%ld','",snull(rp),mtime);
  if(opt&READDIR_ZIP){
    if (zip || (zip=zip_open_ro(rp,0))){
      struct zip_stat sb;
      const int n_entries=zip_get_num_entries(zip,0);
      for(int k=0; k<n_entries; k++){
        if (!zip_stat_index(zip,k,0,&sb))  readdir_append(s,k+1,sb.name,false,(long)sb.size, sb.crc);
      }
    }
  }else if(rp){
    DIR *dir=opendir(rp);
    if(dir==NULL){perror("Unable to read directory");return false;}
    struct dirent *de;
    while((de=readdir(dir))) readdir_append(s,de->d_ino,de->d_name,(bool)(de->d_type==(S_IFDIR>>12)),0,0);
    closedir(dir);
  }
  if (s->t[s->e-1]=='|') --s->e;
  sprintf(s->t+s->e,"');");
  sql_exec(SQL_SUCCESS,s->t,readdir_callback,NULL);
  return true;
}
static bool readdir_concat(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip){
  pthread_mutex_t *m=_mutex+mutex_roots+s->root;
  pthread_mutex_lock(m);
  bool success=readdir_concat_unsynchronized(opt,s,mtime,rp,zip);
  pthread_mutex_unlock(m);
  return success;
}
/* Reading zip dirs asynchroneously */
static void *thread_readdir_async(void *arg){
  struct ht *queue=arg;
  char path[MAX_PATHLEN];
  while(1){
    usleep(1000*10);
    int iteration=0;
    struct ht_entry *e;
    *path=0;
    pthread_mutex_lock(_mutex+mutex_jobs);
    e=ht_next(queue,&iteration);
    if (e && e->key){
      my_strncpy(path,e->key,MAX_PATHLEN);
      FREE(e->key);
      e->value=NULL;
    }
    pthread_mutex_unlock(_mutex+mutex_jobs);
    if (*path){
      struct my_strg s={0};
      readdir_concat(READDIR_ZIP,&s,file_mtime(path),path,NULL);
    }
  }
}
/////////////////////////////////////////////////////////////////////
//
// Given virtual path, search for real path
//
////////////////////////////////////////////////////////////
//
// Iterate over all _root to construct the real path;
// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
static int test_realpath(struct zippath *zpath, int root){
  if (*_root[root].path==0) return ENOENT; /* The first root which is writable can be empty */
  char *vp0=VP0_LEN()?VP0():VP();
  zpath_assert_strlen("test_realpath ",zpath);
  zpath->strgs_l=zpath->realpath_pos;
  zpath->realpath=zpath_newstr(zpath);
  zpath->root=root;
  if (zpath_strcat(zpath,_root[root].path) || !(*vp0=='/' && vp0[1]==0) && zpath_strcat(zpath,vp0)) return ENAMETOOLONG;
  const int res=zpath_stat(zpath,_root+root);
  if (!res && ZPATH_IS_ZIP()){
    if (EP_LEN()) return read_zipdir(_root+root,zpath,NULL,NULL,NULL);
    stat_set_dir(&zpath->stat_vp);
  }
  return res;
}
static int test_realpath_any_root(struct zippath *zpath,int onlyThisRoot){
  zpath->realpath_pos=zpath->strgs_l;
  zpath_assert_strlen("test_realpath_any_root ",zpath);
  for(int i=0;i<_root_n;i++){
    struct rootdata *r=_root+i;
    assert(r->path!=NULL);
    if (onlyThisRoot!=-1 && i!=onlyThisRoot) continue;
    bool ok=!((r->features&ROOT_REMOTE) && r->statfs_when);
    for(int try=100;!ok && --try>=0;){
      if (r->statfs_when ||  (currentTimeMillis()-r->statfs_when)<ROOT_OBSERVE_EVERY_MSECONDS*2) ok=true;
      else usleep(200*ROOT_OBSERVE_EVERY_MSECONDS);
    }
    if (!ok){
      warning(WARN_ROOT|WARN_FLAG_ONCE_PER_PATH,r->path,"Remote root not responding.");
      r->delayed++;
    }else{
      if (!(zpath->flags&ZP_PATH_IS_ONLY_SLASH)) assert(VP_LEN()>0);
      if (!test_realpath(zpath,i)){
        return 0;
      }
      zpath_reset_realpath(zpath);
    }
  }
  return -1;
}
static int find_realpath_any_root(struct zippath *zpath,int onlyThisRoot){
  const char *vp=VP(); /* Only zpath->virtualpath is defined */
  //  int strgs_l_save=zpath->strgs_l;
  zpath_reset_keep_VP(zpath);
  char *append="";
  const int vp_l=VP_LEN(), zip_l=zip_contained_in_virtual_path(vp,&append);
  int res=-1;
  if (zip_l){ /* Bruker MS files. The zip file name without the zip-suffix is the  folder name.  */
    VP0()=zpath_newstr(zpath);
    if (zpath_strncat(zpath,vp,zip_l) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
    VP0_LEN()=zpath_strlen(zpath);
    EP()=zpath_newstr(zpath);
    zpath->flags|=ZP_ZIP;
    if (zip_l+1<vp_l) zpath_strcat(zpath,vp+zip_l+1);
    EP_LEN()=zpath_strlen(zpath);
    zpath_assert_strlen("find_realpath_any_root 1 ",zpath);
    res=test_realpath_any_root(zpath,onlyThisRoot);
    if (!*EP()) stat_set_dir(&zpath->stat_vp);
  }
  if (res){ /* Sciex MS files */
    for(int k=0;k<=ZIPENTRY_TO_ZIPFILE_MAX;k++){ /* Zip entry is directly shown in directory listing. The zipfile is not shown as a folder. */
      const int len=zipentry_to_zipfile(k,vp,&append);
      if (len==MAX_PATHLEN) break;
      if (len){
        zpath_reset_keep_VP(zpath);
        VP0()=zpath_newstr(zpath);
        if (zpath_strncat(zpath,vp,len) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
        VP0_LEN()=zpath_strlen(zpath);
        EP()=zpath_newstr(zpath);
        zpath->flags|=ZP_ZIP;
        if (zpath_strcat(zpath,vp+last_slash(vp)+1)) return ENAMETOOLONG;
        EP_LEN()=zpath_strlen(zpath);
        zpath_assert_strlen("find_realpath_any_root 2 ",zpath);
        if (!(res=test_realpath_any_root(zpath,onlyThisRoot))) break;
      }
    }
  }
  if (FIND_ZIPFILE && res){ /* Sciex MS files */
    struct my_strg sql={0}; /* Zip entry is directly shown in directory listing. The zipfile is not shown as a folder.   Previosly been stored in SQL */
    sprintf(ensure_capacity(&sql,222+VP_LEN()),"SELECT zipfile FROM zipfile WHERE path='%s';",VP());  ///%s','%s');",VP0(),n,rp);
    char zipfile[MAX_PATHLEN];
    *zipfile=0;
    if (!sql_exec(SQL_SUCCESS,sql.t,zipfile_callback,zipfile) && *zipfile){
      res=0;
      zpath_reset_keep_VP(zpath);
      EP()=zpath_newstr(zpath);
      zpath->flags|=ZP_ZIP;
      if (zpath_strcat(zpath,VP()+last_slash(VP())+1)) return ENAMETOOLONG;
      EP_LEN()=zpath_strlen(zpath);
      RP()=zpath_newstr(zpath);
      if (zpath_strncat(zpath,zipfile,MAX_PATHLEN)) return ENAMETOOLONG;
      zpath_strlen(zpath);
      zpath->flags|=ZP_ZIP;
      zpath_stat(zpath,NULL);
    }
    FREE(sql.t);
  }
  if (res){ /* Just a file */
    zpath_reset_keep_VP(zpath);
    zpath_assert_strlen("find_realpath_any_root 4 ",zpath);
    res=test_realpath_any_root(zpath,onlyThisRoot);
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
static bool debug_fhdata(struct fhdata *d){ return d && !d->n_read && tdf_or_tdf_bin(d->path);}
static const struct fhdata FHDATA_EMPTY={0};
static bool fhdata_can_destroy(struct fhdata *d){
  return d->xmp_read<=0 && !(fhdata_has_cache(d) && fhdata_by_virtualpath(d->path,d,having_no_cache));
}
static void fhdata_destroy(struct fhdata *d){
  if (d){
    LOCK_FHDATA_REDUNDANT();
    if (!fhdata_can_destroy(d)){
      d->close_later=true;
    }else{
      char *path=d->path;
      d->path=NULL;/* Set path to null before anything else. NULL signals invalid instance */
      log(ANSI_FG_GREEN"Release fhdata d= %p %s fh= %lu "ANSI_RESET,d,path,d->fh); fhdata_log_cache(d);
      free(path);
      zip_file_t *z=d->zip_file; d->zip_file=NULL; if (z) zip_fclose(z);
      zpath_destroy(&d->zpath);
      cache_zip_entry(RELEASE,d);
      FREE(d->cache_stat_subdirs_of_path);
      pthread_mutex_destroy(&d->mutex);
      memset(d,0,sizeof(struct fhdata));
    }
    UNLOCK_FHDATA_REDUNDANT();
  }
}
static zip_file_t *fhdata_zip_open(struct fhdata *d,char *msg){
  assert(d->path!=NULL);
  zip_file_t *zf=d->zip_file;
  if (zf) return zf;
  struct zippath *zpath=&d->zpath;
  // Hier SIGSEGV
  struct zip *z=zpath_zip_open(zpath,0);
  if (z){
    d->zip_file=zip_fopen(z,EP(),ZIP_RDONLY);
    if (d->zip_file) return d->zip_file;
    warning(WARN_OPEN,RP(),"Failed zip_fopen");
  }
  warning(WARN_OPEN,RP(),"fhdata_zip_open second time");
  return NULL;
}
static struct fhdata* fhdata_create(const char *path,uint64_t fh){
  struct fhdata *d=NULL;
  for(int i=_fhdata_n;--i>=0;) if (!_fhdata[i].path) d=_fhdata+i; /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhdata_n>=FHDATA_MAX){
      warning(WARN_STR,path,"Excceeding FHDATA_MAX");
      return NULL;
    }
    d=_fhdata+_fhdata_n++;
  }
  memset(d,0,sizeof(struct fhdata));
  d->path_hash=hash_value(path);
  pthread_mutex_init(&d->mutex,NULL);
  d->fh=fh;
  d->path=strdup(path); /* This must be the  last assignment */
  return d;
}
static struct fhdata* fhdata_get(const enum data_op op,const char *path,const uint64_t fh){
  //log(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  const uint64_t h=hash_value(path);
  for(int i=_fhdata_n;--i>=0;){
    struct fhdata *d=_fhdata+i;
    if (!d->path) continue;
    if (fh==d->fh && d->path_hash==h && !strcmp(path,d->path)){
      if(op==RELEASE){ fhdata_destroy(d);return NULL;}
      d->close_later=false;
      return d;
    }else if (d->close_later){
      fhdata_destroy(d);
    }
  }
  return op==CREATE?fhdata_create(path,fh):NULL;
}
static struct fhdata *fhdata_by_virtualpath(const char *path,const struct fhdata *not_this,const enum fhdata_having having){
  if(path!=NULL){
    const uint64_t h=hash_value(path);
    for(int i=_fhdata_n; --i>=0;){
      struct fhdata *d=_fhdata+i;
      if (!d->path ||
          d==not_this ||
          having==having_cache && !d->cache ||
          having==having_no_cache && d->cache) continue; /* Otherwise two with same path, each having a cache could stick for ever */
      if (d->path_hash==h && !strcmp(path,d->path)) return d;
    }
  }
  return NULL;
}
/* *************************************************************/
/* There are many xmp_getattr calls on /d folders during reads */
/* This is a cache */
/* Looking into the currently open fhdata.  */
static struct fhdata *fhdata_by_subpath(const char *path){
  const int len=my_strlen(path);
  if (!len) return NULL;
  struct fhdata *d2=NULL;
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (!d->path) continue;
    const int n=d->zpath.virtualpath_l;
    if (len<=n ){
      const char *vp=d->zpath.virtualpath;
      if (vp && !strncmp(path,vp,n) && (len==n||vp[len]=='/')){
        d2=d;
        if (d->cache_stat_subdirs_of_path) return d;
      }
    }
  }
  return d2;
}
/* ******************************************************************************** */
/* *** Inode *** */
/* #define SHIFT_INODE_ROOT 40 */
/* #define SHIFT_INODE_ZIPENTRY 45 */
/* #define ROOTS ((1<<(SHIFT_INODE_ZIPENTRY-SHIFT_INODE_ROOT))-1) */
/*   // #define SET_STAT() init_stat(st,isdir?-1:nis.size,&zpath->stat_rp); st->st_ino^=((nis.inode<<SHIFT_INODE_ZIPENTRY)|ADD_INODE_ROOT(r->index)); zpath->rp_crc=nis.crc */
ino_t make_inode(const ino_t inode0,const int  iroot, const int ientry_plus_1,const char *path){
  const static int SHIFT_ROOT=40, SHIFT_ENTRY=(SHIFT_ROOT+LOG2_ROOTS);
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");;
  if (DEBUG_NOW!=DEBUG_NOW && inode0<(1L<<SHIFT_ROOT) && ientry_plus_1<(1<<(63-SHIFT_ENTRY))){  // (- 63 45)
    return inode0| (((long)ientry_plus_1)<<SHIFT_ENTRY)| ((iroot+1L)<<SHIFT_ROOT);
  }else{
    log_debug_now("Making inode sequentially inode0=%lu ientry=%d\n",inode0,ientry_plus_1-1);
    static struct ht hts[ROOTS];
    struct ht *ht=hts+iroot;
    assert(sizeof(inode0)==8);
    pthread_mutex_lock(_mutex+mutex_inode);
    if (!ht->capacity) ht_init(ht,16);
    uint64_t
      key1=inode0+1,
      key2=(ientry_plus_1<<1)|1, /* Because of the implementation of hash table ht, key2 must not be zero */
      inod=ht_get_int(ht,key1,key2);
    if (!inod){
      static uint64_t seq=1UL<<63;
      ht_set_int(ht,key1,key2,inod=++seq);
    }
    _countSeqInode++;
    pthread_mutex_unlock(_mutex+mutex_inode);
    return inod;
  }
}
/* ******************************************************************************** */
/* *** Zip *** */
static bool readdir_concat_z(struct my_strg *s,long mtime,const char *rp){
  bool success=false;
  struct zip *z=rp?zip_open_ro(rp,0):NULL;
  if (z){
    success=readdir_concat(READDIR_ZIP|READDIR_NO_SQL,s,mtime,rp,z);
    zip_close(z);
  }else{
    perror("readdir_concat_z");
  }
  return success;
}
static int read_zipdir(struct rootdata *r, struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  log_mthd_orig(read_zipdir);
  int res=filler_maybe_null?0:ENOENT;
  //log_entered_function("read_zipdir rp=%s filler= %p  vp=%s  entry_path=%s   EP_LEN()=%d\n",RP(),filler_maybe_null,VP(),EP(),EP_LEN());
  if(!EP_LEN() && !filler_maybe_null){ /* The virtual path is a Zip file */
    return 0; /* Just report success */
  }else{
    if (zpath_stat(zpath,r)) res=ENOENT;
    else{
      struct my_strg s={0};
      s.root=r->index;
      if (readdir_concat(READDIR_ZIP|READDIR_ONLY_SQL,&s,zpath->stat_rp.st_mtime,RP(),NULL) || readdir_concat_z(&s,zpath->stat_rp.st_mtime,RP())){
        char str[MAX_PATHLEN];
        const int len_ze=EP_LEN();
        struct name_ino_size nis={0};
        while(readdir_iterate(&nis,&s)){
          char *n=nis.name;
          int len=my_strlen(n),isdir=nis.is_dir, not_at_the_first_pass=0;
          if (len>=MAX_PATHLEN){ warning(WARN_STR,n,"Exceed MAX_PATHLEN"); continue;}
          while(len){
            if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
              const int slash=last_slash(n);
              if (slash<0) break;
              n[slash]=0;
              isdir=1;
            }
            if (!(len=my_strlen(n))) break;
            if (!filler_maybe_null){  /* ---  read_zipdir() has been called from test_realpath(). The goal is to set zpath->stat_vp --- */
              if (len_ze==len && !strncmp(EP(),n,len)){
                struct stat *st=&zpath->stat_vp;
#define SET_STAT() init_stat(st,isdir?-1:nis.size,&zpath->stat_rp); st->st_ino=make_inode(zpath->stat_rp.st_ino,r->index,nis.inode+1,RP()); zpath->rp_crc=nis.crc
                SET_STAT();
                res=0;
                goto behind_loops;
              }
            }else{
              if (len<EP_LEN() || len<len_ze) continue;
              {
                const char *q=n+EP_LEN();
                if (slash_not_trailing(q)>0) continue;
                my_strncpy(str,q,(int)(strchrnul(q,'/')-q));
              }
              if (!*str ||
                  !zipentry_filter(VP0(),str) ||
                  strncmp(EP(),n,len_ze) ||
                  slash_not_trailing(n+len_ze+1)>=0 ||
                  ht_set(no_dups,str,"")) continue;
              struct stat stbuf, *st=&stbuf;
              SET_STAT();
#undef SET_STAT
              filler_maybe_null(buf,str,st,0,fill_dir_plus);
            }
          }// while len
        }
      }/*if*/
    behind_loops:
      FREE(s.t);
    }
  }
  return res;
}
static int impl_readdir(struct rootdata *r,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  log_mthd_invoke(impl_readdir);
  log_entered_function("impl_readdir vp=%s rp=%s ZPATH_IS_ZIP=%d  \n",VP(),snull(rp),ZPATH_IS_ZIP());
  if (!rp || !*rp) return 0;
  if (ZPATH_IS_ZIP()){
    //LOCK_ZPATH(zpath);
    read_zipdir(r,zpath,buf,filler,no_dups);
    //UNLOCK_ZPATH(zpath);
  }else{
    assert(file_mtime(rp)==zpath->stat_rp.st_mtime);
    const long mtime=zpath->stat_rp.st_mtime;
    if (mtime){
      log_mthd_orig(impl_readdir);
      struct stat st;
      struct my_strg sql={0},rd1={0}; rd1.root=r->index;
      char direct_rp[MAX_PATHLEN], *append="", display_name[MAX_PATHLEN];
      struct name_ino_size nis={0};
      readdir_concat(0,&rd1,mtime,rp,NULL);
      memset(&nis,0,sizeof(nis));
      while(readdir_iterate(&nis,&rd1)){
        char *n=nis.name;
        if (empty_dot_dotdot(n) || ht_set(no_dups,n,"")) continue;
        struct my_strg rd2={0}; rd2.root=r->index;
        if (FIND_ZIPFILE && list_contained_zipentries_instead_of_zipfile(n) &&
            (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,n)) &&
            readdir_concat(READDIR_ZIP|READDIR_ONLY_SQL,&rd2,file_mtime(direct_rp),direct_rp,NULL)){
          struct name_ino_size nis2={0};
          for(int j=0;readdir_iterate(&nis2,&rd2);j++){
            if (strchr(nis2.name,'/') || ht_set(no_dups,nis2.name,"")) continue;
            init_stat(&st,nis2.is_dir?-1:nis.size,&zpath->stat_rp);
            st.st_ino=make_inode(zpath->stat_rp.st_ino,r->index,nis.inode+1,RP());
            filler(buf,nis2.name,&st,0,fill_dir_plus);
            bool tosql=true;
            for(int k=0;k<=ZIPENTRY_TO_ZIPFILE_MAX && tosql && PATH_MAX!=zipentry_to_zipfile(k,nis2.name,&append);k++) if (append) tosql=false;
            if (tosql){ //s-mcpb-ms03/slow2/incoming/Z1/Data/30-0072/20220815_Z1_ZWLRS_0036_30-0072_Q1_10_100ng_Zeno_rep01.wiff2.Zip
              sprintf(ensure_capacity(&sql,222+VP_LEN()+strlen(nis2.name)+strlen(direct_rp)),"INSERT OR REPLACE INTO zipfile VALUES('%s/%s','%s');",VP(),nis2.name,direct_rp);
              if (sql_exec(SQL_SUCCESS,sql.t,0,0)) warning(WARN_SQL,sql.t,"Error");
            }
          }
        }else{
          init_stat(&st,(nis.is_dir||zip_contained_in_virtual_path(n,&append))?-1:nis.size,NULL);
          st.st_ino=make_inode(zpath->stat_rp.st_ino,r->index,0,RP());
          filler(buf,zipfile_name_to_display_name(display_name,n),&st,0,fill_dir_plus);
        }
        FREE(rd2.t);
      }
      FREE(rd1.t);
      FREE(sql.t);
    }
    if (!VP_LEN() && !ht_set(no_dups,FILE_FS_INFO,"")) filler(buf,(FILE_FS_INFO)+1,NULL,0,fill_dir_plus);
  }
  //log_exited_function("realpath_readdir \n");
  return 0;
}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;}
static int xmp_statfs(const char *path, struct statvfs *stbuf){
  const char *p=_root[0].path;
  if (!*p && _root_n>1) p=_root[1].path;
  if (!*p) return -ENOSYS;
  const int res=statvfs(p,stbuf);
  return res==-1?-errno:res;
}
/************************************************************************************************/
/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
static int realpath_mk_parent(char *realpath,const char *path){
  const char *p0=_root[0].path;
  if (!*p0) return EACCES;/* Only first root is writable */
  const int slash=last_slash(path);
  log_entered_function(" realpath_mk_parent(%s) slash=%d  \n  ",path,slash);
  int res=0;
  {
    FIND_REAL(path);
    const bool exist=!res && zpath->root>0;
    zpath_destroy(zpath);
    if (exist){ warning(WARN_OPEN|WARN_FLAG_ONCE,RP(),"It is only allowed to overwrite files in root 0");return EACCES;}
  }
  if (slash>0){
    char parent[MAX_PATHLEN];
    my_strncpy(parent,path,slash);
    FIND_REAL(parent);
    if (!res){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,p0),parent,MAX_PATHLEN);
      recursive_mkdir(realpath);
    }
    zpath_destroy(zpath);
    if (res) return ENOENT;
  }
  strncat(strcpy(realpath,p0),path,MAX_PATHLEN);
  return 0;
}
/********************************************************************************/
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  //  void *x=fuse_apply_conn_info_opts;
  //conn->async_read=1;
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
  memset(stbuf,0,sizeof(struct stat));
  //log_entered_function("xmp_getattr %s fh=%d \n",path,fh);
  //log_mthd_invoke(xmp_getattr);
  log_count_b(xmp_getattr_);
  struct fhdata* d=NULL;
  struct stat *ss=NULL;
  const int slashes=count_slash(path);
  int res=-1;
  if (DO_CACHE_GETATTR){
    LOCK_FHDATA();
    if ((d=fhdata_by_subpath(path)) &&  (d->zpath.flags&ZP_ZIP)){
      if ((ss=d->cache_stat_subdirs_of_path)){
#define S() ss[slashes+1]
        if (S().st_ino){
          *stbuf=S();
          res=0;
          log_succes("xmp_getattr from cache \n");
        }
      }else{
        const int slashes_vp=count_slash(d->zpath.virtualpath);
        assert(slashes<=slashes_vp);
        ss=d->cache_stat_subdirs_of_path=calloc(slashes_vp+2,sizeof(struct stat));
      }
    }
    UNLOCK_FHDATA();
  }
  if (res){
    FIND_REAL(path);
    if(!res){
      *stbuf=zpath->stat_vp;
      if (ss){
        LOCK_FHDATA();
        if ((ss=d->cache_stat_subdirs_of_path))   S()=*stbuf;
#undef S
        UNLOCK_FHDATA();
      }
    }
    zpath_destroy(zpath);
  }
  log_exited_function("xmp_getattr %s  res=%d ",path,res); log_file_stat(" ",stbuf);
  if (res){
    debug_my_file_checks(path,stbuf);
    warning(WARN_GETATTR,path,"xmp_getattr  res=%d",res);
    if (_killOnError && tdf_or_tdf_bin(path)) killOnError_exit();
  }else if (!stbuf->st_size && tdf_or_tdf_bin(path)){
    warning(WARN_GETATTR|WARN_FLAG_MAYBE_EXIT,path,"xmp_getattr stbuf->st_size is zero ");
  }
  log_count_e(xmp_getattr_,path);
  return res==-1?-ENOENT:-res;
}
/* static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi){ */
/* int res=_xmp_getattr(path,stbuf,fi); */
/* log_entered_function("_xmp_getattr %s fh=%lu    returns %d \n",path,fi!=NULL?fi->fh:0,res); */
/* return res; */
/* } */
/* static int xmp_getxattr(const char *path, const char *name, char *value, size_t size){ */
/*   log_entered_function("xmp_getxattr path=%s, name=%s, value=%s\n", path, name, value); */
/*   return -1; */
/* } */
static int xmp_access(const char *path, int mask){
  if (!strcmp(path,FILE_FS_INFO)) return 0;
  log_count_b(xmp_access_);
  log_mthd_orig(xmp_access);
  int res;FIND_REAL(path);
  if (res==-1) res=ENOENT;
  if (!res){
    if ((mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) mask=(mask&~X_OK)|R_OK;
    res=access(RP(),mask);
  }
  zpath_destroy(zpath);
  report_failure_for_tdf("xmp_access",res,path);
  log_count_e(xmp_access_,path);
  return res==-1?-errno:-res;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  log_mthd_orig(xmp_readlink);
  int res;FIND_REAL(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_unlink(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;FIND_REAL(path);
  if (!res) res=unlink(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_rmdir(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;FIND_REAL(path);
  if (!res) res=rmdir(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static long trackMemUsage(enum memusage t,long a){
  static long memUsage=0;
  if (!a) return memUsage;
  pthread_mutex_lock(_mutex+mutex_memUsage);
  memUsage+=a;
  _memusage_count[2*t+(a<0)]++;
  pthread_mutex_unlock(_mutex+mutex_memUsage);
  return a;
}
static struct fhdata *cache_zip_entry(enum data_op op,struct fhdata *d){
  if (!d || d->cache && d->cache_l) return d;
  char *c=d->cache;
  if (op==RELEASE) log_entered_function("cache_zip_entry RELEASE %p\n",c);
  if (!c && op!=RELEASE){
    LOCK_FHDATA();
    struct fhdata *d2=fhdata_by_virtualpath(d->path,NULL,having_cache);
    UNLOCK_FHDATA();
    if (d2 && d2->cache){
      //log_cache(ANSI_FG_GREEN"Found cache in other record\n"ANSI_RESET);
      return d2;
    }
  }
  if (op==CREATE){
    if (!c || c==CACHE_FILLING){
      const long len=d->zpath.stat_vp.st_size;
      log_cache(ANSI_RED"Going to cache d= %p %s %'ld Bytes"ANSI_RESET"\n",d,d->path,len);
      if (!len && _killOnError) killOnError_exit();
      const long currentMem=trackMemUsage(0,0);
      if (currentMem+len>_maxMemForCache){
        log_warn("_maxMemForCache reached: %'ld\n",currentMem);
        d->cache=CACHE_FAILED;
        return d;
      }
      char *bb;
      if(len<SIZE_CUTOFF_MMAP_vs_MALLOC){
        if ((bb=malloc(len))) trackMemUsage(memusage_malloc,len);
        else warning(WARN_MALLOC|WARN_FLAG_ERRNO,d->path,"cache_zip_entry malloc returns NULL");
      }else{
        if (MAP_FAILED==(bb=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0))) bb=NULL;
        if (bb) trackMemUsage(memusage_mmap,len);
        else warning(WARN_MALLOC|WARN_FLAG_ERRNO,d->path,"cache_zip_entry mmap fails");
      }
      if (!bb){
        warning(WARN_MALLOC|WARN_FLAG_ONCE,"MAP_FAILED %p",(void*)mmap);
        log_mem(LOG_STREAM);
        d->cache=CACHE_FAILED;
        return d;
      }else{
        struct zip *za=zip_open_ro(d->zpath.realpath,0);
        zip_file_t *zf=za?zip_fopen(za,d->zpath.entry_path,ZIP_RDONLY):NULL;
        if (!zf) warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,d->zpath.realpath,"cache_zip_entry-CREATE: d= %p Failed zip_open za= %p",d,za);
        bool ok=(zf!=NULL);
        if (zf){
          const long start=time_ms();
          long already=0;
          int count=0;
          LOCK_FHDATA();
          while(already<len){
            long n=zip_fread(zf,bb+already,len-already);
            if (n==0) { usleep(10000); n=zip_fread(zf,bb+already,len-already);assert(n<=0);}
            if (n==0) break;
            if (n<0){ ok=false; warning(WARN_CACHE,d->path,"cache_zip_entry-CREATE: d= %p  read=%'ld len=%'ld",d,already,len); break; }
            already+=n;
            count++;
          }
          assert(already==len);
          //          usleep(2000);
          if (already){
            pthread_mutex_lock(_mutex+mutex_crc);
            const uint32_t crc=cg_crc32(bb,len,0);
            pthread_mutex_unlock(_mutex+mutex_crc);
            if (d->zpath.rp_crc!=crc)  killOnError_exit();
          }
          if(ok){
            d->cache=bb;
            d->cache_l=already;
            log_succes("mmap d= %p %s %p  len=%ld  in %'ld mseconds in %d shunks\n",d,d->path,bb,already,   time_ms()-start,count);
            d->cache_read_seconds=time_ms()-start;
          }
          UNLOCK_FHDATA();
        }
        //log_cached(-1,"CREATE");
        zip_fclose(zf);
        zip_close(za);
        return d;
      }
    }
  }
  if (op==RELEASE){
    const size_t len=d->cache_l;
    if (c){
      bool hasref=false;
      for(int i=_fhdata_n; --i>=0;){
        struct fhdata *d2=_fhdata+i;
        if (d2->path && d2!=d && (hasref=(d2->cache==c))) break;
      }
      if(!hasref){
        log_cache("Going to release %p %zu\n",c,len);
        if(len<SIZE_CUTOFF_MMAP_vs_MALLOC){
          FREE(d->cache);
          trackMemUsage(memusage_malloc,-len);
        }else{
          if (munmap(c,len)){
            perror("munmap");
          }else{
            log_succes("munmap\n");
            trackMemUsage(memusage_mmap,-len);
          }
        }
        d->cache=NULL;
        d->cache_l=0;
      }
    }
    return NULL;
  }
  return c?d:NULL;
}
static bool fhdata_should_cache(struct fhdata *d){
  return d &&
    _when_cache!=NEVER &&
    (_when_cache==ALWAYS || store_zipentry_in_cache(d->zpath.stat_vp.st_size,d->zpath.virtualpath));
}
static struct fhdata *fhdata_wait_for_zip_entry(struct fhdata *d){
  if (!d || fhdata_has_cache(d)) return d;
  {
    //if (debug_fhdata(d)) { log_debug_now("wait_for_zip_entry  d= %p Entering while loop ... \n",d); }
    char *cache=NULL;
    struct fhdata *d2=NULL;
    while(1){
      LOCK_FHDATA();
      d2=cache_zip_entry(GET,d);
      cache=d2?d2->cache:NULL;
      UNLOCK_FHDATA();
      if (cache!=CACHE_FILLING){
        //log_debug_now("wait_for_zip_entry d= %p Going to break loop  d2= %p  ",d,d2);fhdata_log_cache(d2);
        break;
      }
      //usleep((d2->zpath.stat_rp.st_size>>17)*(random()&255));
      usleep(10*1000);
    }
    //if (debug_fhdata(d)) { log_debug_now("wait_for_zip_entry  d= %p After-while  cache= %p  ",d,cache);  fhdata_log_cache(d2); }
    if (cache){
      if (cache==CACHE_FAILED) warning(WARN_CACHE,"CACHE_FAILED","wait_for_zip_entry  d= %p returning d2 =%p",d,d2);
      return d2;
    }
  }
  d->cache=CACHE_FILLING;
  cache_zip_entry(CREATE,d);
  if (debug_fhdata(d)){
    fhdata_log_cache(d);
    // DEBUG_NOW LOG_TO_SQL
    //if (_killOnError && !fhdata_has_cache(d)) {log_debug_now("!fhdata_has_cache(d)  "); killOnError_exit();}
  }
  if (d->cache==CACHE_FILLING) d->cache=CACHE_FAILED;
  return d;
}
#define FH_ZIP_MIN (1<<20)
static uint64_t _next_fh=FH_ZIP_MIN;
static int xmp_open(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  if (fi->flags&(O_WRONLY)) return create_or_open(path,0775,fi);
#define C(x) if (fi->flags&x) log_warn("%s %s\n",#x,path)
  C(O_RDWR);  C(O_CREAT);
#undef C
  if (!strcmp(path,FILE_FS_INFO)) return 0;
  log_mthd_orig(xmp_open);
  log_count_b(xmp_open_);
  int res;
  uint64_t handle=0;
  if (keep_file_attribute_in_cache(path)) fi->keep_cache=1;
  FIND_REAL(path);
  if (res){
    if (report_failure_for_tdf("xmp_open",res,path)) log_zpath("xmp_open Failed ",zpath);
  }else{
    if (ZPATH_IS_ZIP()){
      LOCK_FHDATA();
      struct fhdata* d=fhdata_create(path,handle=fi->fh=_next_fh++);
      zpath_stack_to_heap(zpath);
      UNLOCK_FHDATA();
      assert(zpath->realpath!=NULL);
      d->zpath=*zpath;
      zpath=NULL;
      d->cache_try=true;
    }else{
      handle=my_open_fh("xmp_open reg file",RP(),fi->flags);
    }
  }
  zpath_destroy(zpath);
  log_count_e(xmp_open_,path);
  if (res || handle==-1){
    report_failure_for_tdf("xmp_open",handle==-1?-1:res,path);
    return res=-1?-ENOENT:-errno;
  }
  fi->fh=handle;
  return 0;
}
static int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi){
  log_entered_function("xmp_truncate %s\n",path);
  int res;
  if (fi!=NULL) res=ftruncate(fi->fh,size);
  else{
    FIND_REAL(path);
    if (!res) res=truncate(RP(),size);
    zpath_destroy(zpath);
  }
  return res==-1?-errno:-res;
}
/////////////////////////////////
//
// Readdir
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  log_count_b(xmp_readdir_);
  (void)offset;(void)fi;(void)flags;
  log_entered_function("xmp_readdir %s \n",path);
  log_mthd_orig(xmp_readdir);
  int res=-1;
  struct ht no_dups={0};
  ht_init_with_keystore(&no_dups,8,12);
  for(int i=0;i<_root_n;i++){
    NEW_ZIPPATH(path);
    assert(_root[i].path!=NULL);
    const int r=find_realpath_any_root(zpath,i);
    if (!r) { impl_readdir(_root+i,zpath,buf,filler,&no_dups); res=0;}
    zpath_destroy(zpath);
  }
  ht_destroy(&no_dups);
  log_count_e(xmp_readdir_,path);
  log_exited_function("xmp_readdir %s %d\n",path,res);
  return res;
}
/////////////////////////////////
//
// Creating a new file object
static int xmp_mkdir(const char *create_path, mode_t mode){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"xmp_mkdir %s \n",create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if (!(res=realpath_mk_parent(realpath,create_path)) &&
      (res=mkdir(realpath,mode))==-1) res=errno;
  return -res;
}
static int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"==========xmp_create"ANSI_RESET" %s\n",create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if ((res=realpath_mk_parent(realpath,create_path))) return -res;
  if ((res=open(realpath,fi->flags,mode))==-1) return -errno;
  fi->fh=res;
  return 0;
}
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  return create_or_open(create_path,mode,fi);
}
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"========xmp_write"ANSI_RESET" %s  fi= %p \n",create_path,fi);
  int res=0;
  uint64_t fd;
  if(!fi){
    char realpath[MAX_PATHLEN];
    if ((res=realpath_mk_parent(realpath,create_path))) return -res;
    fd=open(realpath,O_WRONLY);
  }else fd=fi->fh;
  if (fd==-1) return -errno;
  if ((res=pwrite(fd,buf,size,offset))==-1) res=-errno;
  if(!fi) close(fd);
  return res;
}
////////////////////////////////////////////////////////
//
// Functions with two paths
int xmp_symlink(const char *target, const char *create_path){ // target,link
  log_entered_function("xmp_symlink %s %s\n",target,create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if (!(realpath_mk_parent(realpath,create_path)) && (res=symlink(target,realpath))==-1) return -errno;
  return -res;
}
int xmp_rename(const char *old_path, const char *neu_path, unsigned int flags){ // from,to
  log_entered_function(" xmp_rename from=%s to=%s \n",old_path,neu_path);
  if (flags) return -EINVAL;
  char old[MAX_PATHLEN],neu[MAX_PATHLEN], *p0=_root[0].path;
  strcpy(old,p0);
  strcat(old,old_path);
  strcpy(neu,p0);
  strcat(neu,neu_path);
  const int res=rename(old,neu);
  return res==-1?-errno:-res;
}
static int read_from_cache(char *buf, const char *cache, long cache_l,  size_t size, off_t offset){
  int n=0;
  if (offset<cache_l && cache) memcpy(buf,cache+offset,n=MIN(size,cache_l-offset));
  return n;
}
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  assert(fi!=NULL);
  log_entered_function("xmp_lseek %s %lu",path,off);
  int ret=off;
  LOCK_FHDATA();
  struct fhdata* d=fhdata_get(GET,path,fi->fh);
  if (d){
    switch(whence){
    case SEEK_DATA:
    case SEEK_SET: ret=d->offset=off;break;
    case SEEK_CUR: ret=(d->offset+=off);break;
    case SEEK_END: ret=(d->offset=d->zpath.stat_vp.st_size+off);break;
    case SEEK_HOLE:ret=(d->offset=d->zpath.stat_vp.st_size);break;
    }
  } else warning(WARN_SEEK|WARN_FLAG_ONCE,path,"xmp_lseek d==NULL");
  UNLOCK_FHDATA();
  return ret;
}
static int _xmp_read_via_zip(const char *path, char *buf, const size_t size, const off_t offset,const uint64_t fd,struct fhdata *d){
  if (!fhdata_zip_open(d,"xmp_read")){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,path,"_xmp_read_via_zip fhdata_zip_open return -1");
    return -1;
  }
  _count_read_zip_regular++;
  { /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    const long diff=offset-zip_ftell(d->zip_file);
    if (!diff){
      _count_read_zip_no_seek++;
    }else if (zip_file_is_seekable(d->zip_file) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
      _count_read_zip_seekable++;
    }else if (diff<0 && _when_cache!=NEVER){ // Worst case seek backward
      struct fhdata *d2=fhdata_wait_for_zip_entry(d);
      if (fhdata_has_cache(d2)){
        const int res=read_from_cache(buf,d2->cache,d2->cache_l,size,offset);
        if (res>0){
          _count_read_zip_seek_bwd++;
          return res;
        }
        if (d->zip_file){zip_fclose(d->zip_file);d->zip_file=NULL;}
        fhdata_zip_open(d,"SEEK");
      }
    }
  }
  long diff;
  //  common variant immunodifficiency
  while((diff=offset-zip_ftell(d->zip_file))){
    _count_read_zip_seek_fwd++;
    if (zip_fread(d->zip_file,buf,MIN(size,diff))<0){
      if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,path,"zip_file skipping diff=%ld size=%ld",diff,MIN(size,diff));
      return -1;
    }
  }
  if (fhdata_not_yet_logged(FHDATA_ALREADY_LOGGED_VIA_ZIP,d)) log("_xmp_read_via_zip %s size=%zu offset=%lu fd=%lu \n",path,size,offset,fd);
  return zip_fread(d->zip_file,buf,size);
}
static int _xmp_read(const char *path, char *buf, const size_t size, const off_t offset,const uint64_t fd,struct fhdata *d){
  if (size>(int)_read_max_size) _read_max_size=(int)size;
  //log_debug_now(" xmp_read d= %p %s offset=%'ld # %d fh=%d fi= %p\n",d,path,offset, d->xmp_read,fd,fi);
  //usleep(1000*100);
  int res=-1;
  struct zippath *zpath=&d->zpath;
  if (ZPATH_IS_ZIP()){
    if (fhdata_should_cache(d)){
      struct fhdata *d2=fhdata_wait_for_zip_entry(d);
      if (!fhdata_has_cache(d2)) warning(WARN_CACHE|WARN_FLAG_MAYBE_EXIT,path,"_xmp_read: No cache for d= %p  cache_try=%d",d,d->cache_try);
      d->cache_try=false;
      const char *cache=d2->cache;
      size_t cache_l=d2->cache_l;
      res=read_from_cache(buf,cache, cache_l,size,offset);
    }
    // if (debug_fhdata(d))      log_debug_now("_xmp_read d2 %s d= %p  size=%zu off=%ld   res=%d   cache=%p  cache_l=%zu \n",path, d,size,offset,res,cache,cache_l);
    if(res>=0){
      _count_read_zip_cached++;
      return res;
    }
    LOCK_D();
    res=_xmp_read_via_zip(path,buf,size,offset,fd,d);
    //buf[100]=0;log_debug_now("Did _xmp_read_via_zip %s  %d ... \n",buf,res); exit(9);
    UNLOCK_D();
    if (res>0) return res;
  }else{
    if (fd==-1) return -errno;
    if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){log_seek(offset,"lseek Failed %s\n",path);return -1; }
    if ((res=pread(fd,buf,size,offset))==-1) res=-errno;
  }
  return res;
}
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  assert(fi!=NULL);
  const uint64_t  fd=fi->fh;
  //if (tdf_or_tdf_bin(path)) log_entered_function(ANSI_BLUE" %lx _xmp_read"ANSI_RESET" %s size=%zu offset=%'lu   fh=%lu\n",pthread_self(),path,size,offset,fd);
  log_mthd_orig(xmp_read);
  if (!strcmp(path,FILE_FS_INFO)){
    LOCK_FHDATA(); const long n=min_long(get_info()-(long)offset,size); UNLOCK_FHDATA();
    return n<=0?0:memcpy(buf,_info+offset,n),n;
  }
  bool go=true;
  struct fhdata *d;
  { LOCK_FHDATA();
    // if (d && tdf_or_tdf_bin(path)) log_debug_now("Begin LOCK_FHDATA d= %p zpath=%p\n",d,&d->zpath);
    if (!(d=fhdata_get(GET,path,fd))){
      go=false;
    }else if (!d->path){
      go=false;
      warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"xmp_read d= %p is_closed",d);
    }else if (!d->zpath.realpath){
      go=false;
      assert(fd==d->fh);
      log_zpath("_killOnError ",&d->zpath);
      warning(WARN_FHDATA|WARN_FLAG_MAYBE_EXIT,path,"%lx !d->zpath.realpath d= %p zpath= %p",pthread_self(),d,&d->zpath);
    }else if (go){
      d->xmp_read++;
      d->access=time(NULL);
    }
    //if (d && tdf_or_tdf_bin(path)) log_debug_now("End LOCK_FHDATA d= %p zpath=%p\n",d,&d->zpath);
    UNLOCK_FHDATA();
  }
  int res=-1;
  if (go) res=_xmp_read(path,buf,size,offset,fd,d);
  if (d){
  LOCK_FHDATA();
  if (res>0) d->n_read+=res;
  d->xmp_read--;
  //  Got signal 11, faulty address is 0x7ff0f77fd970, from 0x408874
  if (res<=0 && tdf_or_tdf_bin(path)){
    struct fhdata *d2=cache_zip_entry(GET,d);
    warning(WARN_CACHE|WARN_FLAG_ONCE_PER_PATH,path," _xmp_read res<0  d= %p  off=%ld size=%zu     res=%d  n_read=%lu go=%d  d2= %p"ANSI_RESET,d,offset,size, res,d->n_read,go,d2);
    fhdata_log_cache(d2);
    if (_killOnError && _when_cache==ALWAYS && (d2==NULL || d2->cache_l && offset<d2->cache_l)){ log_warn("d2=NULL\n");killOnError_exit();}
  }
  UNLOCK_FHDATA();
  }
  return res;
}
static int xmp_release(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  log_mthd_orig(xmp_release);
  const uint64_t fh=fi->fh;
  if (fh>=FH_ZIP_MIN){
  LOCK_FHDATA();
  fhdata_get(RELEASE,path,fh);
  UNLOCK_FHDATA();
  }else if (fh>2 && close(fh)){
    warning(WARN_OPEN,path,"my_close_fh %lu",fh);
    perror("");
    print_path_for_fd(fh);
  }
  log_exited_function(ANSI_FG_GREEN"xmp_release %s fh=%lu"ANSI_RESET"\n",path,fh);
  return 0;
}
static int xmp_flush(const char *path, struct fuse_file_info *fi){
  return 0;
}
static void usage(){
  log("\
Usage:  ZIPsFS options  root-dir1 root-dir2 ... root-dir-n : libfuse-options  mountPoint\n\n\
The first root-dir1 is writable, the others read-only.\n\n\
Lower case options are passed to the fuse_main of libfuse:\n\n\
\n\
ZIPsFS options:\n\n\
      -q quiet, no debug messages to stdout\n\
      -c never|seek|rule|always]    When to read zip entries into RAM\n\
         seek: When the data is not read continuously\n\
         rule: According to rules based on the file name encoded in configuration.h\n\
         The default is when backward seeking would be requires otherwise\n\
      -d <sqlite database file>    The database stores directory information.\n\
      -C Clear sqlite  database file\n\
      -h Print usage.\n\
      -k Kill on error\n\
      -l <megabytes> Limit memory usage\n\
\n\n\
fuse options:These options are given after a colon.\n\n\
 -d Debug information\n\
 -f File system should not detach from the controlling terminal and run in the foreground.\n\
 -s Single threaded mode.\n\
 -h Print usage information.\n\
 -V Version\n\\n");
  log("Caching zip-entries in RAM is controled by the option -c [");
  for(char **s=WHEN_CACHE_S; *s; s++){
    if (s!=WHEN_CACHE_S) putchar('|');
    prints(*s);
  }
  log("]\n");
}
int main( int argc,  char *argv[]){
  //  struct stat st;  printf("ino_t=%zu\n",sizeof(st.st_ino));exit(1);
  _startTimeMillis=currentTimeMillis();
  log(ANSI_INVERSE""ANSI_UNDERLINE"This is %s  main(...)"ANSI_RESET"\n",path_of_this_executable());
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  assert(S_IXOTH==(S_IROTH>>2));
  zipentry_to_zipfile_test();
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr);   S(access); //S(getxattr);
  S(readlink);  S(readdir);   S(mkdir);
  S(symlink);   S(unlink);
  S(rmdir);     S(rename);    S(truncate);
  S(open);      S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  S(lseek);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  bool clearDB=false;
  //    argv_fuse[argc_fuse++]="--fuse-flag";    argv_fuse[argc_fuse++]="sync_read";
  for(int c;(c=getopt(argc,argv,"+Cdqnksc:l:L:"  ))!=-1;){  // :o:sfdh
    switch(c){
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 'C': clearDB=true; break;
    case 's': _simulate_slow=true; break;
    case 'd': my_strncpy(_sqlitefile,optarg,MAX_PATHLEN); break;
    case 'h': usage();break;
    case 'l':_maxMemForCache=atol_kmgt(optarg);break;
    case 'L':{
      static struct rlimit _rlimit={0};
      _rlimit.rlim_cur=_rlimit.rlim_max=atol_kmgt(optarg);
      setrlimit(RLIMIT_AS,&_rlimit);
    } break;
    case 'c':
      for(char **s=WHEN_CACHE_S;;s++){
        if (!*s){ log_error("Wrong option -c %s\n",optarg); usage(); break;}
        if ((!strcmp(*s,optarg))){ _when_cache=s-WHEN_CACHE_S; break;}
      }
      break;
    }
  }
  // See which version of fuse we're running
  log("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  /* *************************************************************** */
  /* *** SQLITE3 *** */
  //if (SQLITE_OK==sqlite3_open(_sqlitefile,&_sqlitedb)) log_succes("Opened %s\n",_sqlitefile);
  /* ************************************************************** */
  /* *** argv. argv_fuse  *** */
  assert(MAX_PATHLEN<=PATH_MAX);
  if (argc-optind<2) {usage();abort();}
  const char *mnt=argv[argc-1]; // last is the mount point
  if (!*_sqlitefile){
    const int n=sprintf(_sqlitefile,"%s/tmp/ZIPsFS/",getenv("HOME"));
    recursive_mkdir(_sqlitefile);
    sprintf(_sqlitefile+n,"%s_sqlite.db",mnt+last_slash(mnt)+1);
  }
  if (clearDB && remove(_sqlitefile) && errno!=ENOENT) { log_error("remove %s\n",_sqlitefile); perror(""); }
  if (SQLITE_OK!=sqlite3_open_v2(_sqlitefile,&_sqlitedb,SQLITE_OPEN_READWRITE|SQLITE_OPEN_CREATE|SQLITE_OPEN_FULLMUTEX,NULL)){
    log_abort("Open database: %s\n%s\n",_sqlitefile, sqlite3_errmsg(_sqlitedb));
  }else{
    log_succes("Opened '%s' \n",_sqlitefile);
  }
  {
    const  char *sql="CREATE TABLE IF NOT EXISTS readdir (path TEXT PRIMARY KEY,mtime INT8,readdir TEXT);";
    if (sql_exec(SQL_ABORT|SQL_SUCCESS,sql,0,0)) log_abort("Error %s\n",sql);
    sql="CREATE TABLE IF NOT EXISTS zipfile (path TEXT PRIMARY KEY,zipfile TEXT);";
    if (sql_exec(SQL_ABORT|SQL_SUCCESS,sql,0,0)) log_abort("Error %s\n",sql);
  }
  int colon=0;
  for(int i=optind;i<argc-1;i++){
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    const char *ri=argv[i];
    if (*ri==':' && !ri[1]) { colon=i; break;}
    //if (i && !*ri && _root_n) continue; /* Accept empty String for _root[0] */
    struct rootdata *r=&_root[_root_n];
    r->index=_root_n++;
    {
      int slashes=-1;while(ri[++slashes]=='/');
      if (slashes>1){
        r->features|=ROOT_REMOTE;
        ri+=(slashes-1);
      }
    }
    r->path=!*ri?"":realpath(ri,NULL);
  }
  if (!colon || colon==argc-1){
    log_error("There must be a colon in the argument list. The colon must be followed by the  libfuse options and arguments\n");
    return 1;
  }
  _root[0].features=ROOT_WRITABLE;
  if (_root[0].path){
    char file_warnings[MAX_PATHLEN];
    sprintf(file_warnings,"%s/%s",_root[0].path,"_FS_WARNINGS_AND_ERRORS.log");
    warning(0,file_warnings,"");
  }else{
    warning(0,NULL,"");
  }
#define W(a) _warning_channel_name[a]=# a
  W(WARN_STR);
  W(WARN_THREAD);
  W(WARN_MALLOC);
  W(WARN_ROOT);
  W(WARN_OPEN);
  W(WARN_READ);
  W(WARN_SEEK);
  W(WARN_INODE);
  W(WARN_GETATTR);
  W(WARN_SQL);
  W(WARN_FHDATA);
  W(WARN_CACHE);
#undef W
  if (!_root_n) log_abort("Missing root directories\n");
  {
    pthread_mutexattr_init(&_mutex_attr_recursive);
    pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
    for(int i=mutex_roots+_root_n;--i>=0;) pthread_mutex_init(_mutex+i, &_mutex_attr_recursive); // DEBUG_NOW
  }
  /*
    char *argvfuse[22]={0};
    argvfuse[0]=argv[0];
    int argcfuse=1;
    for(int i=colon+1;i<argc && argcfuse<22;) argvfuse[argcfuse++]=argv[i];
    log_debug_now("FFFFFFFFFFFFFFFFFFF %d %lu\n",getpid(),pthread_self());
  */
  log_strings(" \n",argv+colon,argc-colon);
  //log_strings("root",_root,_root_n);
  start_threads();
  const int fuse_stat=fuse_main(argc-colon,argv+colon,&xmp_oper,NULL);
  log("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
