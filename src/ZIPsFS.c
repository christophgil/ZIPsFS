/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS
  Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  (global-set-key (kbd "<f1>") '(lambda() (interactive) (switch-to-buffer "ZIPsFS.c")))
  gcc --shared  -fPIC shell.c sqlite3.c  -o libsqlite3.so
  sudo apt-get install libsqlite3-dev
*/
#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include <stdlib.h>
#include "config.h"
//#include "math.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#ifdef __FreeBSD__
#include <sys/un.h>
#endif
#include <locale.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <stdbool.h>
#define FH_ZIP_MIN (1<<20)
#include <fuse.h>
#define fill_dir_plus 0
#include <zip.h>
#include <sqlite3.h>
#include <assert.h>
#define LOG_STREAM stdout
#include "cg_log.c"
#include "cg_utils.c"
#include "configuration.h"
#include "ht4.c"
#define WITH_SQL true
#define FIND_ZIPFILE 1
// DEBUG_NOW==DEBUG_NOW
#define DEBUG_ABORT_MISSING_TDF 1
//////////////////////////////////////////////////////////////////
// Structs and enums
static int _fhdata_n=0,_mmap_n=0,_munmap_n=0;
enum data_op{GET,CREATE,RELEASE};
enum when_cache_zip{NEVER,SEEK,RULE,ALWAYS};
static enum when_cache_zip _when_cache=SEEK;
static char *WHEN_CACHE_S[]={"never","seek","rule","always",NULL}, _sqlitefile[MAX_PATHLEN]={0};
static bool _simulate_slow=false;
#define EQUIVALENT 5
#define READDIR_SEP 0x02
struct name_ino_size{char *name; long inode; long size; bool is_dir; int b,e,name_n;char *t;};
struct my_strg{
  char *t;
  int
  b, /* begin of text */
    e, /* end of text */
    t_l, /* Capacity */
    index;  /* Index of root needed to get the right _ht_job_readdir  */

};
struct zippath{
  char *strgs; /* Contains several strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  int strgs_l, realpath_pos,current_string;
  char *virtualpath;
  int virtualpath_l;
  char *virtualpath_without_entry;
  int virtualpath_without_entry_l;
  char *entry_path;
  int entry_path_l;
  char *realpath;
  int root;
  struct stat stat_rp,stat_vp;
  struct zip *zarchive;
  unsigned int flags;
  int zarchive_fd;
};
struct fhdata{
  unsigned long fh; /*Serves as key*/
  char *path; /*Serves as key*/
  uint64_t path_hash; /*Serves as key*/
  zip_file_t *zip_file;
  struct zippath zpath;
  time_t access;
  char *cache;
  size_t cache_l;
  int cache_read_seconds;
  struct stat *cache_stat_subdirs_of_path;
  bool cache_try;
  long offset;
};
struct rootdata{
  int index;
  char *path;
  int features;
  pthread_mutex_t mutex;
  struct statfs statfs;
  long statfs_when;
  int statfs_mseconds,delayed;
};

#define ROOTS 7
static pthread_mutexattr_t _mutex_attr_recursive;
enum mutex{mutex_fhdata,mutex_jobs,mutex_debug_read1,mutex_debug,mutex_log_count,mutex_roots};
static pthread_mutex_t mutex[mutex_roots+ROOTS];
#include "cg_debug.c"
#include "ZIPsFS.h" // (shell-command (concat "makeheaders "  (buffer-file-name)))

#define FILE_FS_INFO "/_FILE_SYSTEM_INFO.HTML"
///////////////////////////////////////////////////////////
// The root directories are specified as program arguments
// The _root[0] is read/write, and can be empty string
// The others are read-only
//

#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int _root_n=0;
static struct rootdata _root[ROOTS]={0};
static ht _ht_job_readdir[ROOTS]={0}, ht_debug_read1={0};
char *ensure_capacity(struct my_strg *s,int n){
  if (n<32) n=32;
  const int n2=2*n+1;
  if (!s->t){
    s->t=calloc(s->t_l=n2,1);    /* with malloc, valgrind reports  uninitialised value ???? */
  }else if (n+1>s->t_l){
    s->t=realloc(s->t,s->t_l=n2);
  }
  return s->t;
}
void *thread_observe_root(void *arg){
  struct rootdata *r=arg;
  while(1){
    long before=currentTimeMillis();
    statfs(r->path,&r->statfs);
    if ((r->statfs_mseconds=(int)((r->statfs_when=currentTimeMillis())-before))>ROOT_OBSERVE_EVERY_MSECONDS*2){
      log_warn("\nstatfs %s took %'ld usec\n",r->path,before-r->statfs_when);
    }
    usleep(1000*ROOT_OBSERVE_EVERY_MSECONDS);
  }
}
void threads_root_start(){
  pthread_t thread[ROOTS];
  for(int i=_root_n;--i>=0;){
    struct rootdata *r=_root+i;
    if (!my_strlen(r->path)) continue;
    if (r->features&ROOT_REMOTE && pthread_create(thread+i,NULL,&thread_observe_root, (void*)r)){
      log_error("Creating thread_observe_root %d %s \n",i,r->path);
      perror("");
    }
    if (pthread_create(thread+i,NULL,&thread_readdir_async, (void*)(_ht_job_readdir+i))){
      log_error("Creating thread_readdir_async %d %s \n",i,r->path);
      perror("");
    }
  }
}
//////////////////////
/////    Utils   /////
//////////////////////
/********************************************************************************/
/* *** Stat **** */
#define ST_BLKSIZE 4096
void stat_set_dir(struct stat *s){
  if(s){
    mode_t *m= &(s->st_mode);
    assert(S_IROTH>>2==S_IXOTH);
    if(!(*m&S_IFDIR)){
      s->st_size=ST_BLKSIZE;
      s->st_nlink=1;
      *m=(*m&~S_IFMT)|S_IFDIR|((*m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
    }
  }
}
void init_stat(struct stat *st, long size,struct stat *uid_gid){
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

void my_close_fh(int fh){
  if (fh>2 && fh<FH_ZIP_MIN && close(fh)){
    /*
      char path[MAX_PATHLEN];
      path_for_fd("my_close_fh", path,fh);
      log_error("my_close_fh %d  %s ",fh,path);
      perror("");
    */
  }
}
int my_open_fh(const char* msg, const char *path,int flags){
  const int fh=open(path,flags);
  if (fh<=0){
    log_error("my_open_fh open(%s,flags) pid=%d\n",path,getpid());
    perror("");
  }
  return fh;
}
#define ZP_DEBUG (1<<1)
#define ZP_ZIP (1<<2)
#define ZP_STRGS_ON_HEAP (1<<3)
#define ZP_PATH_IS_ONLY_SLASH (1<<4)
#define IS_ZPATH_DEBUG() (zpath->flags&ZP_DEBUG)
//////////////////////////////////////////////////////////////////////////
// The struct zippath is used to identify the real path
// from the virtual path
// All strings are stacked in the field ->strgs.
// strgs may start as a stack variable and continue to be a heap.
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
void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack){
  memset(zpath,0,sizeof(struct zippath));
  assert(virtualpath!=NULL);
  const int l=pathlen_ignore_trailing_slash(virtualpath);
  if (!l) {
    assert(*virtualpath=='/' && !virtualpath[1]);
    zpath->flags|=ZP_PATH_IS_ONLY_SLASH;
  }
  zpath->virtualpath=zpath->strgs=strgs_on_stack;
  zpath_strcat(zpath,virtualpath);
  zpath->virtualpath[VP_LEN()=l]=0;
}
void zpath_assert_strlen(const char *title,struct zippath *zpath){
#define C(X)    if (my_strlen(X())!=X ## _LEN()){                       \
    if (title) log("zpath_assert_strlen %s\n",title);                   \
    log_error(#X "=%s  %u!=%d\n",X(),my_strlen(X()),X ## _LEN()); log_zpath("Error ",zpath);}
  C(VP);
  C(EP);
#undef C
#define C(a) assert(my_strlen(zpath->a)==zpath->a ## _l);
  C(virtualpath);
  C(virtualpath_without_entry);
  C(entry_path);
#undef C
  if (!(zpath->flags&ZP_PATH_IS_ONLY_SLASH) && !VP_LEN()){
    if (title) log("zpath_assert_strlen %s\n",title);
    assert(VP_LEN()>0);
  }
}
int zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=min_int(my_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      log_abort("zpath_strncat %s %d ZPATH_STRGS\n",s,len);
    }
    my_strncpy(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return 0;
}
int zpath_strcat(struct zippath *zpath,const char *s){ return zpath_strncat(zpath,s,9999); }
int zpath_strlen(struct zippath *zpath){ return zpath->strgs_l-zpath->current_string;}
char *zpath_newstr(struct zippath *zpath){
  char *s=zpath->strgs+(zpath->current_string=++zpath->strgs_l);
  *s=0;
  return s;
}
void zpath_stack_to_heap(struct zippath *zpath){
  if (!zpath || (zpath->flags&ZP_STRGS_ON_HEAP)) return;
  zpath->flags|=ZP_STRGS_ON_HEAP;
  const char *stack=zpath->strgs;
  if (!(zpath->strgs=(char*)malloc(zpath->strgs_l+1))) log_abort("zpath_stack_to_heap malloc");
  memcpy(zpath->strgs,stack,zpath->strgs_l+1);
  const long d=(long)(zpath->strgs-stack);
#define C(a) if (zpath->a) zpath->a+=d
  C(virtualpath);
  C(virtualpath_without_entry_l);
  C(entry_path);
  C(realpath);
#undef C
}
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REAL(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,-1)
void log_zpath(char *msg, struct zippath *zpath){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf("    this=%p   only slash=%d\n",zpath,0!=(zpath->flags&ZP_PATH_IS_ONLY_SLASH));
  printf("    %p strgs="ANSI_FG_BLUE"%s"ANSI_RESET"  "ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, (zpath->flags&ZP_STRGS_ON_HEAP)?"Heap":"Stack", zpath->strgs_l);
  printf("    %p    VP="ANSI_FG_BLUE"'%s' VP_LEN=%d"ANSI_RESET,VP(),snull(VP()),VP_LEN()); log_file_stat("",&zpath->stat_vp);
  printf("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  printf("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  printf("    %p    RP="ANSI_FG_BLUE"'%s'"ANSI_RESET,RP(), snull(RP())); log_file_stat("",&zpath->stat_rp);
  printf("    zip=%s  ZIP %s"ANSI_RESET"\n",yes_no(ZPATH_IS_ZIP()),  zpath->zarchive?ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}


void zpath_close_zip(struct zippath *zpath){
  if (!zpath) return;
  struct zip *z=zpath->zarchive;
  if (z){
    zpath->zarchive=NULL;
    if (zip_close(z)==-1) log_zpath(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,zpath);
  }
  const int fd=zpath->zarchive_fd;
  if(fd){
    zpath->zarchive_fd=0;
    my_close_fh(fd);
  }
}
void zpath_reset_realpath(struct zippath *zpath){ /* keep VP(), VP0() and EP() */
  if (!zpath) return;
  zpath_close_zip(zpath);
  zpath->strgs_l=zpath->realpath_pos;
  clear_stat(&zpath->stat_rp);
  clear_stat(&zpath->stat_vp);
}
void zpath_reset_keep_VP(struct zippath *zpath){
  //log_debug_now("zpath_reset_keep_VP %p",zpath);
  VP0()=EP()=RP()=NULL;
  VP0_LEN()=EP_LEN()=0;
  zpath->flags&=ZP_PATH_IS_ONLY_SLASH;
  zpath->strgs_l=(int)(VP()-zpath->strgs)+VP_LEN(); /* strgs is behind VP() */
}
void zpath_destroy(struct zippath *zpath){
  if (zpath){
    zpath_reset_realpath(zpath);
    if (zpath->flags&ZP_STRGS_ON_HEAP){FREE(zpath->strgs);}
    memset(zpath,0,sizeof(struct zippath));
  }
}
int zpath_stat(struct zippath *zpath,struct rootdata *r){
  if (!zpath) return -1;
  int res=0;
  char path[MAX_PATHLEN];
  if (!zpath->stat_rp.st_ino){
    for(int tries=(r && r->features&ROOT_REMOTE)?3:1,try=tries;--try>=0;){
      for(int equiv=0;equiv<EQUIVALENT;equiv++){
        if (!equivalent_path(path,RP(),(equiv)%EQUIVALENT)) equiv=999;
        if (!(res=stat(path,&zpath->stat_rp)) || !tdf_or_tdf_bin(VP())) goto ok;
      }
      usleep(1000*100);
    }
  ok:
    if (!res) zpath->stat_vp=zpath->stat_rp;
  }
  return res;
}
#define log_seek_ZIP(delta,...)   log(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),log(__VA_ARGS__)
#define log_seek(delta,...)  log(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),log(__VA_ARGS__)

struct zip *my_zip_fdopen_ro(int *fd,const char *orig,int equivalent){
  struct zip *zip=NULL;
  char path[MAX_PATHLEN];
  log_entered_function("my_zip_fdopen_ro %s\n",orig);
  for(int try=2;--try>=0;){
    for(int equiv=1;equiv<EQUIVALENT;equiv++){
      if (!equivalent_path(path,orig,(equivalent+equiv)%EQUIVALENT)) equiv=999;
      if ((*fd=my_open_fh("my_zip_fdopen_ro",path,O_RDONLY))<0){
        log_warn("my_zip_fdopen_ro failed to get fd for %s\n",path);
      }else{
        int err;
        if ((zip=zip_fdopen(*fd,0,&err))) goto ok;
        log_error("my_zip_fdopen_ro(%d)   %s  \n",*fd,path);
        close(*fd);
      }
    }
    usleep(1000);
  }
 ok:
  if (!zip && DEBUG_ABORT_MISSING_TDF && endsWith(orig,".d") && !strstr("202",orig+1+last_slash(orig))) log_debug_abort("Abort\n");
  return zip;
}
struct zip *zpath_zip_open(struct zippath *zpath,int equivalent){
  if (!zpath) return NULL;
  if (!zpath->zarchive) zpath->zarchive=my_zip_fdopen_ro(&zpath->zarchive_fd,RP(),equivalent);
  return zpath->zarchive;
}
//////////////////////////////////////////////////////////////////////
// Is the virtualpath a zip entry?
#define debug_is_zip  int is_zip=(strcasestr(path,".zip")>0)
int zip_contained_in_virtual_path(const char *path, char *append[]){
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
int sql_exec(int flags,const char* sql, int (*callback)(void*,int,char**,char**), void* udp ){
  char *errmsg=0;
  if (_sqlitedb){
    int res=0;
    if (SQLITE_OK!=(res=sqlite3_exec(_sqlitedb,sql,callback,udp,&errmsg))){
      if (flags&SQL_SUCCESS) log_error("%s\n sqlite3_errmsg=%s  %d \n",sql,sqlite3_errmsg(_sqlitedb),res);
      if (flags&SQL_ABORT) abort();
      return 1;
    }
  }
  return 0;
}


int zipfile_callback(void *arg1, int argc, char **argv,char **name){
  char *zipfile=arg1;
  for (int i=0;i<argc;i++){
    const char *n=name[i];
    if (*n=='z' && !strcmp(n,"zipfile") && strlen(n)<MAX_PATHLEN) strcpy(zipfile,argv[i]);
  }
  return 0;
}




struct readdir_sqlresult{ long mtime; struct my_strg *s; bool ok; };
int readdir_callback(void *arg1, int argc, char **argv,char **name){
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
      log_warn("readdir_callback  %s=%s\n",n,snull(a));
    }
  }
  return 0;
}
bool illegal_char(const char *s){ return (s && (strchr(s,'\t')||strchr(s,READDIR_SEP))); }
void readdir_append(struct my_strg *s, long inode, const char *n,bool append_slash,long size){
  if (!illegal_char(n) && !empty_dot_dotdot(n)){
    s->e+=sprintf(s->e+ensure_capacity(s,max_int(3333,s->e+strlen(n)+55)),"%s%s\t%lx\t%lx%c",n,append_slash?"/":"",inode,size,READDIR_SEP); //
  }
}

char *my_memchr(const char *b,const char *e, char c){  return e>b?memchr(b,c,e-b):NULL; }
bool readdir_iterate(struct  name_ino_size *nis, struct my_strg *s){ /* Parses the text written with readdir_append and readdir_concat */
  char *t=s->t;
  if (!t) return false;
  char *b=t+s->b,*e=t+s->e;
  if (nis->t!=b){ nis->t=b; nis->b=0; *e=0;}
  nis->e=(int)(strchrnul(b+nis->b+1,READDIR_SEP)-b); // valgrind: uninitialized value
  char *sep1=0,*sep2=0;
  if (b+nis->e>e ||
      !(sep1=my_memchr(nis->name=b+nis->b,e,'\t')) ||
      !(sep2=my_memchr(sep1+1,e,'\t'))) return false;
  nis->name[nis->name_n=sep1-nis->name-(nis->is_dir=(sep1[-1]=='/'))]=0;
  nis->inode=strtol(sep1+1,NULL,16);
  nis->size=strtol(sep2+1,NULL,16);
  nis->b=nis->e+1;
  return true;
}
#define READDIR_ZIP (1<<1)
#define READDIR_ONLY_SQL (1<<2)


bool readdir_concat_unsynchronized(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip){
  if (WITH_SQL){
    char sql[999];
    struct readdir_sqlresult sqlresult={0};
    sqlresult.s=s;
    if (SNPRINTF(sql,sizeof(sql),"SELECT mtime FROM readdir WHERE path='%s';",snull(rp))) return false;
    sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
    if (sqlresult.mtime==mtime){
      if (SNPRINTF(sql,sizeof(sql),"SELECT readdir FROM readdir WHERE path='%s';",snull(rp))) return false;
      sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
      if (sqlresult.ok){
        return true;

      }
    }
  }
  if (opt&READDIR_ONLY_SQL){ /* Read zib dir asynchronously */
    pthread_mutex_lock(mutex+mutex_jobs);
    ht_set(_ht_job_readdir+s->index,rp,"");
    pthread_mutex_unlock(mutex+mutex_jobs);
    return false;
  }
  s->e=s->b=sprintf(ensure_capacity(s,3333),"INSERT OR REPLACE INTO readdir VALUES('%s','%ld','",snull(rp),mtime);
  if(opt&READDIR_ZIP){
    int fd=0;
    if (zip || (zip=my_zip_fdopen_ro(&fd,rp,0))){
      struct zip_stat sb;
      const int n_entries=zip_get_num_entries(zip,0);
      for(int k=0; k<n_entries; k++){
        if (!zip_stat_index(zip,k,0,&sb))  readdir_append(s,k+1,sb.name,false,(long)sb.size);
      }
      if (fd>0){zip_close(zip);close(fd);}
    }
  }else if (rp){
    //log_debug_now("Going to opendir(%s)\n",rp);
    DIR *dir=opendir(rp);
    if(dir==NULL){perror("Unable to read directory");return false;}
    struct dirent *de;
    while((de=readdir(dir))) readdir_append(s,de->d_ino,de->d_name,(bool)(de->d_type==(S_IFDIR>>12)),0);
    closedir(dir);
  }
  if (s->t[s->e-1]=='|') --s->e;
  sprintf(s->t+s->e,"');");
  sql_exec(SQL_SUCCESS,s->t,readdir_callback,NULL);
  return true;
}
bool readdir_concat(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip){
  const int m=+mutex_roots+s->index;
  pthread_mutex_lock(mutex+m);
  bool success=readdir_concat_unsynchronized(opt,s,mtime,rp,zip);
  pthread_mutex_unlock(mutex+m);
  return success;
}

/* Reading zip dirs asynchroneously */
void *thread_readdir_async(void *arg){
  struct ht *queue=arg;
  char path[MAX_PATHLEN];
  while(1){
    usleep(1000*10);
    int iteration=0;
    ht_entry *e;
    *path=0;
    pthread_mutex_lock(mutex+mutex_jobs);
    e=ht_next(queue,&iteration);
    if (e && e->key){
      my_strncpy(path,e->key,MAX_PATHLEN);
      FREE(e->key);
      e->value=NULL;
    }
    pthread_mutex_unlock(mutex+mutex_jobs);
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
int test_realpath(struct zippath *zpath, int root){
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
int test_realpath_any_root(struct zippath *zpath,int onlyThisRoot){
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
      log_warn("Remote root %s not responding.\n",r->path);
      r->delayed++;
    }else{

      //log_debug_now("Going to test_realpath i=%d \n",i); log_zpath("", zpath);
      if (!(zpath->flags&ZP_PATH_IS_ONLY_SLASH)) assert(VP_LEN()>0);
      if (!test_realpath(zpath,i)) {
        //log_debug_now("test_realpath failed\n");
        return 0;
      }
      zpath_reset_realpath(zpath);
    }
  }
  return -1;
}
int find_realpath_any_root(struct zippath *zpath,int onlyThisRoot){
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
    //log_debug_now("zip_l=%d zpath->strgs_l=%d  VP0=%s VP0_LEN=%d\n",zip_l,zpath->strgs_l,VP0(),VP0_LEN());
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
        log_debug_now("lLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLLL\n");
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
void usage(){
  log("\
Usage:  ZIPsFS -s -f root-dir1 root-dir2 ... root-dir-n  mountPoint\n\n\
The first root-dir1 is writable, the others read-only.\n\n");
  log("Caching zip-entries in RAM is controled by the option -c [");
  for(char **s=WHEN_CACHE_S; *s; s++){
    if (s!=WHEN_CACHE_S) putchar('|');
    prints(*s);
  }
  log("]\n");
  exit(0);
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
#define FHDATA_MAX 3333
static struct fhdata _fhdata[FHDATA_MAX];
static const struct fhdata  FHDATA_EMPTY={0};//.fh=0,.offset=0,.file=NULL};
int fhdata_zip_open(struct fhdata *d,char *msg){
  zip_file_t *zf=d->zip_file;
  if (zf && !zip_ftell(zf)) return 0;
  fhdata_zip_fclose("fhdata_zip_open",d);
  struct zippath *zpath=&d->zpath;
  for(int try=3;--try>=0;){
    if (zpath_zip_open(zpath,try)){ // DEBUG_NOW
      if((d->zip_file=zip_fopen(zpath->zarchive,EP(),ZIP_RDONLY))) return 0;
      log_warn("Failed zip_fopen %s\n",RP());
    }
    usleep(100*1000*try*try);
    zpath_close_zip(zpath);
  }
  log_error("Failed zip_fopen %s  second time\n",RP());

  return -1;
}
void fhdata_zip_fclose(char *msg,struct fhdata *d){
  zip_file_t *z;
  pthread_mutex_lock(mutex+mutex_fhdata);
  z=d?d->zip_file:NULL;
  if (z) d->zip_file=NULL;
  pthread_mutex_unlock(mutex+mutex_fhdata);
  d->zip_file=NULL;
  if (z) zip_fclose(z);
}
struct fhdata* fhdata(enum data_op op,const char *path,uint64_t fh){
  //log(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  uint64_t hash=hash_key(path);
  struct fhdata *d;
  for(int i=_fhdata_n;--i>=0;){
    d=&_fhdata[i];
    if (fh==d->fh && d->path_hash==hash && !strcmp(path,d->path)){
      if(op==RELEASE){
        log(ANSI_FG_RED"Release fhdata %lu\n"ANSI_RESET,fh);
        fhdata_zip_fclose("RELEASE",d);
        zpath_destroy(&d->zpath);
        cache_zip_entry(RELEASE,d);
        FREE(d->path);
        FREE(d->cache_stat_subdirs_of_path);
        for(int j=i+1;j<_fhdata_n;j++) _fhdata[j-1]=_fhdata[j];
        _fhdata_n--;
      }
      return d;
    }
  }
  if (op==CREATE && _fhdata_n<FHDATA_MAX){
    //log(ANSI_FG_GREEN"New fhdata %lu\n"ANSI_RESET,fh);
    memset(d=_fhdata+_fhdata_n,0,sizeof(struct fhdata));
    d->fh=fh;
    d->path=strdup(path);
    d->path_hash=hash;
    return _fhdata+_fhdata_n++;
  }
  return NULL;
}
struct fhdata *fhdata_by_vpath(const char *path,struct fhdata *not_this){
  const uint64_t h=hash_key(path);
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (d!=not_this && d->path_hash==h && !strcmp(path,_fhdata[i].path)) return d;
  }
  return NULL;
}
#define fhdata_synchronized(op,path,fi) pthread_mutex_lock(mutex+mutex_fhdata); d=fhdata(op,path,fi->fh);  pthread_mutex_unlock(mutex+mutex_fhdata);
/* *************************************************************/
/* There are many xmp_getattr calls on /d folders during reads */
/* This is a cache */
struct fhdata *fhdata_by_virtualpath(const char *path){
  const int len=my_strlen(path);
  if (!len) return NULL;
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    const int n=d->zpath.virtualpath_l;
    if (len<=n){
      const char *vp=d->zpath.virtualpath;
      if (vp && !strncmp(path,vp,n) && len==n || vp[len]=='/') return d;
    }
  }
  return NULL;
}
struct stat *get_stat_from_fhdata(const char *path){
  struct fhdata *d=fhdata_by_virtualpath(path);
  if (!d) return NULL;
  if (!d->cache_stat_subdirs_of_path) d->cache_stat_subdirs_of_path=calloc(count_slash(d->path)+1,sizeof(struct stat));
  struct stat *s=d->cache_stat_subdirs_of_path+count_slash(path);
  if (!s->st_ino){
    int res;
    FIND_REAL(path);
    const char *rp=zpath->realpath;
    if (!rp || res || stat(rp,s)){
      log_error("get_stat_from_fhdata stat(%s,...)\n",rp);
      perror(" ");
      s->st_ino=-1;
    }
    if (ZPATH_IS_ZIP()) stat_set_dir(s);
    zpath_destroy(zpath);
  }
  return s;
}

/* ******************************************************************************** */
/* *** Zip *** */
static int _count_read_zip_cached=0,_count_read_zip_regular=0,_count_read_zip_seekable=0,_count_read_zip_no_seek=0,_count_read_zip_seek_fwd=0,_count_read_zip_seek_bwd=0,_read_max_size=0;
#include "log.h"
int read_zipdir(struct rootdata *r, struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  log_mthd_orig(read_zipdir);
  int res=0;
  bool found=false;
  //log_entered_function("read_zipdir rp=%s filler=%p  vp=%s  entry_path=%s   EP_LEN()=%d\n",RP(),filler_maybe_null,VP(),EP(),EP_LEN());
  if(!EP_LEN() && !filler_maybe_null){ /* The virtual path is a Zip file */
    return 0; /* Just report success */
  }else{
    if (zpath_stat(zpath,r)) res=ENOENT;
    else{
      struct my_strg s={0};
      s.index=r->index;
      if (readdir_concat(READDIR_ZIP|READDIR_ONLY_SQL,&s,zpath->stat_rp.st_mtime,RP(),NULL) ||
          readdir_concat(READDIR_ZIP,&s,zpath->stat_rp.st_mtime,RP(),zpath_zip_open(zpath,0))
          ){ /* The virtual path is a Zip file with zip-entry */
        char str[MAX_PATHLEN];
        const int len_ze=EP_LEN();
        //log_debug_now(ANSI_INVERSE"read_zipdir"ANSI_RESET"  n_entries=%d\n",n_entries);
        struct name_ino_size nis={0};
        while(!found && readdir_iterate(&nis,&s)){
          char *n=nis.name;
          int len=my_strlen(n),isdir=nis.is_dir, not_at_the_first_pass=0;
          if (len>=MAX_PATHLEN) { log_warn("Exceed MAX_PATHLEN: %s\n",n); continue;}
          while(!found && len){
            if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
              const int slash=last_slash(n);
              if (slash<0) break;
              n[slash]=0;
              isdir=1;
            }
            if (!(len=my_strlen(n))) break;
            if (!filler_maybe_null){  /* ---  read_zipdir() has been called from test_realpath() --- */
              if (len_ze==len && !strncmp(EP(),n,len)){
                struct stat *st=&zpath->stat_vp;
#define SET_STAT()                                                      \
                init_stat(st,isdir?-1:nis.size,&zpath->stat_rp);        \
                st->st_ino^=((nis.inode<<SHIFT_INODE_ZIPENTRY)|ADD_INODE_ROOT(r->index));
                SET_STAT();
                found=true;
                break;
              }
            }else{
              if (len<EP_LEN() || len<len_ze) continue;
              {
                const char *q=n+EP_LEN();
                if  (slash_not_trailing(q)>0) continue;
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
              // if (tdf_or_tdf_bin(str)) { log_debug_now(ANSI_GREEN"zip filler"ANSI_RESET" %s "ANSI_RESET,str); log_file_stat(" ",str); }//DEBUG_NOW
              filler_maybe_null(buf,str,st,0,fill_dir_plus);
            }
          }// while len
        }
      }/*if*/
      FREE(s.t);
    }
  }
  return filler_maybe_null?res:found?0:ENOENT;
}
int impl_readdir(struct rootdata *r,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  log_mthd_invoke(impl_readdir);
  log_entered_function("impl_readdir vp=%s rp=%s ZPATH_IS_ZIP=%d  \n",VP(),snull(rp),ZPATH_IS_ZIP());
  if (!rp || !*rp) return 0;
  if (ZPATH_IS_ZIP()){
    read_zipdir(r,zpath,buf,filler,no_dups);
  }else{
    assert(file_mtime(rp)==zpath->stat_rp.st_mtime);
    const long mtime=zpath->stat_rp.st_mtime;
    if (mtime){
      log_mthd_orig(impl_readdir);
      struct stat st;
      struct my_strg sql={0},rd1={0}; rd1.index=r->index;
      char direct_rp[MAX_PATHLEN], *append="", display_name[MAX_PATHLEN];
      struct name_ino_size nis={0};
      readdir_concat(0,&rd1,mtime,rp,NULL);
      memset(&nis,0,sizeof(nis));

      while(readdir_iterate(&nis,&rd1)){
        char *n=nis.name;
        if (empty_dot_dotdot(n) || ht_set(no_dups,n,"")) continue;
        struct my_strg rd2={0}; rd2.index=r->index;
        if (FIND_ZIPFILE && list_contained_zipentries_instead_of_zipfile(n) &&
            (MAX_PATHLEN>=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,n)) &&
            readdir_concat(READDIR_ZIP|READDIR_ONLY_SQL,&rd2,file_mtime(direct_rp),direct_rp,NULL)){
          struct name_ino_size nis2={0};
          for(int j=0;readdir_iterate(&nis2,&rd2);j++){
            if (strchr(nis2.name,'/') || ht_set(no_dups,nis2.name,"")) continue;
            init_stat(&st,nis2.is_dir?-1:nis.size,&zpath->stat_rp);
            st.st_ino=nis.inode^((nis2.inode<<SHIFT_INODE_ZIPENTRY)|ADD_INODE_ROOT(r->index));
            filler(buf,nis2.name,&st,0,fill_dir_plus);
            bool tosql=true;
            for(int k=0;k<=ZIPENTRY_TO_ZIPFILE_MAX && tosql && PATH_MAX!=zipentry_to_zipfile(k,nis2.name,&append);k++) if (append) tosql=false;
            if (tosql){ //s-mcpb-ms03/slow2/incoming/Z1/Data/30-0072/20220815_Z1_ZWLRS_0036_30-0072_Q1_10_100ng_Zeno_rep01.wiff2.Zip
              sprintf(ensure_capacity(&sql,222+VP_LEN()+strlen(nis2.name)+strlen(direct_rp)),"INSERT OR REPLACE INTO zipfile VALUES('%s/%s','%s');",VP(),nis2.name,direct_rp);
              if (sql_exec(SQL_SUCCESS,sql.t,0,0)) log_error("Error %s\n",sql.t);
            }
          }
        }else{
          init_stat(&st,(nis.is_dir||zip_contained_in_virtual_path(n,&append))?-1:nis.size,NULL);
          st.st_ino=nis.inode^ADD_INODE_ROOT(r->index);
          filler(buf,zipfile_name_to_display_name(display_name,n),&st,0,fill_dir_plus);
        }
        FREE(rd2.t);
      }
      FREE(rd1.t);
      FREE(sql.t);
    }
    if (!VP_LEN() && !ht_set(no_dups,FILE_FS_INFO,"")) filler(buf,FILE_FS_INFO+1,NULL,0,fill_dir_plus);
  }
  //log_exited_function("realpath_readdir \n");
  return 0;
}
/************************************************************************************************/
/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
int realpath_mk_parent(char *realpath,const char *path){
  const char *p0=_root[0].path;
  if (!*p0) return EACCES;/* Only first root is writable */
  const int slash=last_slash(path);
  log_entered_function(" realpath_mk_parent(%s) slash=%d  \n  ",path,slash);
  int res=0;
  {
    FIND_REAL(path);
    bool exist=!res && zpath->root>0;
    zpath_destroy(zpath);
    if (exist){
      log_warn("Only  allowed to overwrite files in root 0 %s\n",RP());
      return EACCES;
    }
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
void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  (void) conn;
  cfg->use_ino=1;
  cfg->entry_timeout=cfg->attr_timeout=cfg->negative_timeout=2.0;
  return NULL;
}
/////////////////////////////////////////////////
// Functions where Only single paths need to be  substituted
int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null){
  if (!strcmp(path,FILE_FS_INFO)){
    init_stat(stbuf,MAX_INFO,NULL);
    time(&stbuf->st_mtime);
    return 0;
  }
  log_entered_function("xmp_getattr %s fh=%lu \n",path,fi_or_null!=NULL?fi_or_null->fh:0);
  //log_mthd_invoke(xmp_getattr);
  log_count_b(xmp_getattr_);
  struct fhdata* d;
  pthread_mutex_lock(mutex+mutex_fhdata);
  if (!fi_or_null || !(d=fhdata(GET,path,fi_or_null->fh))) d=fhdata_by_vpath(path,NULL);
  struct stat *stat_cached=d?NULL:get_stat_from_fhdata(path);
  pthread_mutex_unlock(mutex+mutex_fhdata);
  int res=0;
  if (stat_cached){
    *stbuf=*stat_cached;
    res=stat_cached->st_ino==-1?-1:0;
  }else if (d && d->zpath.stat_vp.st_ino){
    *stbuf=d->zpath.stat_vp;
    res=0;
  }else{
    log_mthd_orig(xmp_getattr);
    FIND_REAL(path);
    if(!res) *stbuf=zpath->stat_vp;
    zpath_destroy(zpath);
  }
  if (res){
    debug_my_file_checks(path,stbuf);
    report_failure("xmp_getattr",res,path);
  }
  //log_exited_function("xmp_getattr %s  res=%d ",path,res); log_file_stat(" ",stbuf);
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
int xmp_access(const char *path, int mask){
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
  report_failure("xmp_access",res,path);
  log_count_e(xmp_access_,path);
  return res==-1?-errno:-res;
}
int xmp_readlink(const char *path, char *buf, size_t size){
  log_mthd_orig(xmp_readlink);
  int res;FIND_REAL(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
int xmp_unlink(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;FIND_REAL(path);
  if (!res) res=unlink(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
int xmp_rmdir(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;FIND_REAL(path);
  if (!res) res=rmdir(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}

bool cache_zip_entry(enum data_op op,struct fhdata *d){
  char *c=d->cache;
  if (op==RELEASE) log_entered_function("cache_zip_entry RELEASE %p\n",c);
  if (!c){
    if (op==RELEASE) return false;
    struct fhdata *d2=fhdata_by_vpath(d->path,d);
    if (d2){
      c=d->cache=d2->cache;
      d->cache_l=d2->cache_l;
    }
    if (c) log_cache(ANSI_FG_GREEN"Found cache in other record %p\n"ANSI_RESET,d->cache);
  }
  switch(op){
  case CREATE:
    if (!c){
      const long len=d->zpath.stat_vp.st_size;
      log_cache(ANSI_RED"Going to cache %s %'ld Bytes"ANSI_RESET"\n",d->path,len);
      char *bb=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
      if (bb==MAP_FAILED) { log_warn("mmap returned MAP_FAILED\n"); return false; }
      _mmap_n++;
      fhdata_zip_open(d,"CREATE");
      bool ok=true;
      {
        const long start=time_ms();
        long already=0;
        int count=0;
        while(already<len){
          //log_debug_now("d->zip_file=%p\n",d->zip_file);
          assert(d->zip_file!=NULL);
          const long n=zip_fread(d->zip_file,bb+already,len-already);
          if (n==0) break;
          log_cache("Read %'ld\n",n);
          if (n<0){
            log_error("cache_zip_entry %s read=%'ld len=%'ld",d->path,already,len);
            ok=false;
            break;
          }
          already+=n;
          count++;
        }
        if(ok){
          d->cache=bb;
          d->cache_l=already;
          //log_succes("Bulk read zip entry %s in %'lu seconds in %d shunks\n",d->path,time_ms()-start,count);
          d->cache_read_seconds=time_ms()-start;
        }
      }
      fhdata_zip_fclose("CREATED",d);
      log_cached(-1,"CREATE");
      return ok;
    }
    break;
  case RELEASE:
    if (c){
      bool hasref=false;
      for(int i=_fhdata_n; --i>=0;){
        if (_fhdata+i!=d && (hasref=(_fhdata[i].cache==c))) break;
      }
      if(!hasref){
        log_cache("Going to release %p\n",c);
        if (munmap(c,d->cache_l)){
          perror("munmap");
        }else{
          log_succes("munmap\n");
          _munmap_n++;
        }
        d->cache=NULL;
        d->cache_l=0;
        return true;
      }
    }
    return false;
  }
  return c!=NULL;
}
bool maybe_cache_zip_entry(enum data_op op,struct fhdata *d,bool always){
  if (!always && !d->cache){
    switch(_when_cache){
    case NEVER: return false;break;
    case RULE: if (!keep_zipentry_in_cache(d->zpath.stat_vp.st_size,d->zpath.virtualpath)) return false;break;
    }
  }
  if (op==CREATE) log_count_b(mcze_);
  pthread_mutex_lock(mutex+mutex_fhdata);
  const bool success=d->cache?true:cache_zip_entry(op,d);
  pthread_mutex_unlock(mutex+mutex_fhdata);
  if (op==CREATE) log_count_e(mcze_,d->zpath.virtualpath);
  return success;
}
static uint64_t _next_fh=FH_ZIP_MIN;
int xmp_open(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  if (fi->flags&(O_WRONLY)) return create_or_open(path,0775,fi);

#define C(x) if (fi->flags&x) log_warn("%s %s\n",#x,path)
  //  C(O_CREAT); C(O_RDONLY);
  C(O_RDWR);  C(O_CREAT);
#undef C
  if (!strcmp(path,FILE_FS_INFO)){fi->direct_io=1;return 0;}
  log_mthd_orig(xmp_open);
  log_entered_function("xmp_open %s  %x \n",path,fi->flags);
  log_count_b(xmp_open_);
  int res,handle=0;
  if (keep_file_attribute_in_cache(path)) fi->keep_cache=1;
  FIND_REAL(path);
  if (res){
    if (report_failure("xmp_open",res,path)) log_zpath("ffffffffffffffff Failed ",zpath);
  }else{
    if (ZPATH_IS_ZIP()){
      if (!zpath_zip_open(zpath,0)){
        log_warn("In xmp_open %s: zpath->zarchive==NULL\n",path);
        report_failure("xmp_open",res,path);
        res=ENOENT;
      }else{
        pthread_mutex_lock(mutex+mutex_fhdata);
        struct fhdata* d=fhdata(CREATE,path,handle=fi->fh=_next_fh++);
        zpath_stack_to_heap(zpath);
        d->zpath=*zpath;zpath=NULL;
        d->cache_try=true;
        pthread_mutex_unlock(mutex+mutex_fhdata);
      }
    }else{
      handle=my_open_fh("xmp_open reg file",RP(),fi->flags);
    }
  }
  zpath_destroy(zpath);
  log_count_e(xmp_open_,path);
  if (res || handle==-1){
    report_failure("xmp_open",handle==-1?-1:res,path);
    return res=-1?-ENOENT:-errno;
  }
  DEBUG_TDF_MAYBE_SLEEP(handle)=debug_tdf_maybe_sleep(path,10000);
  fi->fh=handle;
  return 0;
}
int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi){
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
int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  log_count_b(xmp_readdir_);
  (void)offset;(void)fi;(void)flags;
  log_entered_function("xmp_readdir %s\n",path);
  log_mthd_orig(xmp_readdir);
  int res=-1;
  struct ht no_dups={0};
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
int xmp_mkdir(const char *create_path, mode_t mode){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"xmp_mkdir %s \n",create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if (!(res=realpath_mk_parent(realpath,create_path)) &&
      (res=mkdir(realpath,mode))==-1) res=errno;
  return -res;
}


int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"==========xmp_create"ANSI_RESET" %s\n",create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if ((res=realpath_mk_parent(realpath,create_path))) return -res;
  if ((res=open(realpath,fi->flags,mode))==-1) return -errno;
  fi->fh=res;
  return 0;
}
int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  create_or_open(create_path,mode,fi);
}

int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  log_entered_function(ANSI_FG_BLACK""ANSI_YELLOW"========xmp_write"ANSI_RESET" %s  fi=%p \n",create_path,fi);
  int fd,res=0;
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
// Two paths
int xmp_symlink(const char *target, const char *create_path){ // target,link
  log_entered_function("xmp_symlink %s %s\n",target,create_path);
  char realpath[MAX_PATHLEN];
  int res;
  if (!(realpath_mk_parent(realpath,create_path)) &&
      (res=symlink(target,realpath))==-1) return -errno;
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
int maybe_read_from_cache(struct fhdata *d, char *buf, size_t size, off_t offset,enum data_op op,bool always){
  int n=-1;
  if (d && maybe_cache_zip_entry(op,d,always)){
    //log_cache("xmp_read %s  cache=%p size=%'zu\n",path,cache,size);
    n=0;
    if (offset<d->cache_l) memcpy(buf,d->cache+offset,n=MIN(size,d->cache_l-offset));
  }
  return n;
}
void debug_read1(const char *path){
  if (!tdf_or_tdf_bin(path)) return;
  pthread_mutex_lock(mutex+mutex_debug_read1);
  bool first=!ht_set(&ht_debug_read1,path,"");
  pthread_mutex_unlock(mutex+mutex_debug_read1);

  if (!first) return;
  //  log_fh("",fd);
  usleep(5*60*1000*1000);
}

int read_info(const char *path,char *buf,size_t size, off_t offset){
  if (!strcmp(path,FILE_FS_INFO)){
    pthread_mutex_lock(mutex+mutex_fhdata);
    long n=get_info()-(long)offset;
    pthread_mutex_unlock(mutex+mutex_fhdata);
    if (n>(long)size) n=size; // This cast to (long) is important
    //log_debug_now("get_info  size=%ld offset=%ld n=%ld   \n",size,offset,n);
    if (n<=0) return 0;
    memcpy(buf,_info+offset,n);
    return n;
  }
  return -1;
}

static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
    assert(fi!=NULL);
    log_entered_function("xmp_lseek %s %lu",path,off);
    if (whence==SEEK_HOLE){ log_warn("SEEK_HOLE lseek %s\n",path);   return -ENOSYS;}
    const int fd=fi->fh;
    struct fhdata* d;
    fhdata_synchronized(CREATE,path,fi);
    if (!d) return -1;
    switch(whence){
    case SEEK_DATA: /* do nothing */; break;
    case SEEK_SET: d->offset=off; break;
    case SEEK_CUR: d->offset+=off; break;
    case SEEK_END: d->offset+=d->zpath.stat_vp.st_size+off; break;
    case SEEK_HOLE: d->offset=d->zpath.stat_vp.st_size; break;
    }
    return d->offset;
}




int _xmp_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi,struct fhdata *d){
  //log_entered_function(ANSI_BLUE"xmp_read"ANSI_RESET" %s size=%zu offset=%'lu   fh=%lu\n",path,size ,offset,fi==NULL?0:fi->fh);
  log_mthd_orig(xmp_read);
  if (size>(int)_read_max_size) _read_max_size=(int)size;
  assert(fi!=NULL);
  const int fd=fi->fh;
  int res=read_info(path,buf,size,offset);
  if (res>=0) return res;
  fhdata_synchronized(CREATE,path,fi);
  d->access=time(NULL);
  //static int _countr=0; if (_countr++%1000==0) log_entered_function(ANSI_BLUE"%d  xmp_read"ANSI_RESET" %s size=%zu offset=%'lu %p  fh=%lu\n",_countr,path,size ,offset,d,fi==NULL?0:fi->fh);
  if (d->cache_try){
    d->cache_try=false;
    if (!maybe_cache_zip_entry(CREATE,d,false)) fhdata_zip_open(d,"xmp_read");
  }
  if((res=maybe_read_from_cache(d,buf,size,offset,GET,false))>=0){
    usleep(DEBUG_TDF_MAYBE_SLEEP(fd));DEBUG_TDF_MAYBE_SLEEP(fd)=0;
    _count_read_zip_cached++;
    return res;
  }else if (d->zip_file){
    _count_read_zip_regular++;
    /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    long diff=offset-zip_ftell(d->zip_file);
    if (!diff) _count_read_zip_no_seek++;
    if (diff && zip_file_is_seekable(d->zip_file)){
      if (zip_fseek(d->zip_file,offset,SEEK_SET)<0) return -1;
      _count_read_zip_seekable++;
    }else if (diff<0){ // Worst case
      if ((res=maybe_read_from_cache(d,buf,size,offset,CREATE,true))>=0) return res;
      _count_read_zip_seek_bwd++;
      fhdata_zip_open(d,"SEEK");
    }
    while (diff=offset-zip_ftell(d->zip_file)){
      _count_read_zip_seek_fwd++;
      if (zip_fread(d->zip_file,buf,MIN(size,diff))<0) return -1;
    }
    return zip_fread(d->zip_file,buf,size);
  }else{
    //log_debug_now("xmp_read file\n");
    if (fd==-1) return -errno;
    if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){log_seek(offset,"lseek Failed %s\n",path);return -1; }
    if ((res=pread(fd,buf,size,offset))==-1) res=-errno;
  }
  //log_exited_function("xmp_read regfile\n");
  return res;
}

int xmp_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
  //log_count_b(xmp_read_);
        struct fhdata* d;
        const int res=_xmp_read(path,buf,size,offset,fi,d);
  //log_count_e(xmp_read_,path);
        if (res>0) d->offset+=res;
  return res;
}



/////////////////////////////////////////////////
//
// These are kept unchanged from passthrough example
int xmp_release(const char *path, struct fuse_file_info *fi){
  assert(fi!=NULL);
  log_mthd_orig(xmp_release);
        struct fhdata* d;
  fhdata_synchronized(RELEASE,path,fi);
  log_entered_function(ANSI_FG_GREEN"xmp_release %s d=%p zip_file=%p\n"ANSI_RESET,path,d,!d?NULL:d->zip_file);
  my_close_fh(fi->fh);
  return 0;
}

int xmp_flush(const char *path, struct fuse_file_info *fi){
  return 0;
}


int main(int argc, char *argv[]){
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  assert(S_IXOTH==(S_IROTH>>2));
  assert((1<<(SHIFT_INODE_ZIPENTRY-SHIFT_INODE_ROOT))>ROOTS);
  zipentry_to_zipfile_test();
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr);   S(access); //S(getxattr);
  S(readlink);  S(readdir);   S(mkdir);
  S(symlink);   S(unlink);
  S(rmdir);     S(rename);    S(truncate);
  S(open);      S(create);    S(read);  S(write);   S(release);
  S(flush);
  S(lseek);
  //S(statfs);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  char *argv_fuse[9]={0};
  int c,argc_fuse=1;
  argv_fuse[0]=argv[0];
  bool clearDB=false;
  while((c=getopt(argc,argv,"+No:sfdhC:L:D:"))!=-1){
    switch(c){
    case 'N': clearDB=true; break;
    case 'S': _simulate_slow=true; break;
    case 'D': my_strncpy(_sqlitefile,optarg,MAX_PATHLEN); break;
    case 'H': case 'h': usage();break;
    case 'L':{
      static struct rlimit _rlimit={0};
      rlim_t megab=atol(optarg);
      _rlimit.rlim_cur=megab<<20;
      _rlimit.rlim_max=megab<<20;
      setrlimit(RLIMIT_AS,&_rlimit);
    } break;
    case 'C': {
      int ok=0;
      for(int i=0;WHEN_CACHE_S[i];i++){
        if (ok=!strcmp(WHEN_CACHE_S[i],optarg)){
          _when_cache=i;
          break;
        }
      }
      if (!ok){
        log_error("Wrong option -c %s\n",optarg);
        usage();
      }
    } break;
    case 's':case 'f':case 'd': argv_fuse[argc_fuse++]=argv[optind-1];break;
    case 'o': argv_fuse[argc_fuse++]="-o"; argv_fuse[argc_fuse++]=strdup(optarg);break;
    }
  }
  //argv_fuse[argc_fuse++]="-s";
  //argv_fuse[argc_fuse++]="-d";
  // See which version of fuse we're running
  log("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  /* *************************************************************** */
  /* *** SQLITE3 *** */
  //if (SQLITE_OK==sqlite3_open(_sqlitefile,&_sqlitedb)) log_succes("Opened %s\n",_sqlitefile);
  /* ************************************************************** */
  /* *** argv. argv_fuse  *** */
  assert(MAX_PATHLEN<=PATH_MAX);
  if (argc-optind<2) {usage();abort();}
  char *mnt=argv_fuse[argc_fuse++]=argv[argc-1]; // last is the mount point
  if (!*_sqlitefile){
    const int n=sprintf(_sqlitefile,"%s/tmp/ZIPsFS/", getenv("HOME"));
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
    char *sql="CREATE TABLE IF NOT EXISTS readdir (path TEXT PRIMARY KEY,mtime INT8,readdir TEXT);";
    if (sql_exec(SQL_ABORT|SQL_SUCCESS,sql,0,0)) log_abort("Error %s\n",sql);
    sql="CREATE TABLE IF NOT EXISTS zipfile (path TEXT PRIMARY KEY,zipfile TEXT);";
    if (sql_exec(SQL_ABORT|SQL_SUCCESS,sql,0,0)) log_abort("Error %s\n",sql);
  }
  for(int i=optind;i<argc-1;i++){
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    char *ri=argv[i];
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
  _root[0].features=ROOT_WRITABLE;
  if (!_root_n) log_abort("Missing root directories\n");
  {
    pthread_mutexattr_init(&_mutex_attr_recursive);
    pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
    for(int i=mutex_roots+_root_n;--i>=0;) pthread_mutex_init(mutex+i,&_mutex_attr_recursive);
  }

  log("about to call fuse_main\n");
  log_strings("fuse argv",argv_fuse,argc_fuse);
  //log_strings("root",_root,_root_n);
  threads_root_start();
  const int fuse_stat=fuse_main(argc_fuse,argv_fuse,&xmp_oper,NULL);
  log("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
