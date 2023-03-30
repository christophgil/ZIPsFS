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
#include "ht.c"

#define LOG_STREAM stdout
#include "cg_log.c"
#include "cg_utils.c"
#include "cg_debug.c"
#define WITH_SQL true
//////////////////////////////////////////////////////////////////
// Structs and enums
static int _fhdata_n=0,_mmap_n=0,_munmap_n=0;
enum data_op{GET,CREATE,RELEASE};
enum when_cache_zip{NEVER,SEEK,RULE,ALWAYS};
static enum when_cache_zip _when_cache=SEEK;
static char *WHEN_CACHE_S[]={"never","seek","rule","always",NULL}, _sqlitefile[MAX_PATHLEN]={0};
static bool _simulate_slow=false;
#define READDIR_SEP 0x02
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
  size_t cache_len;
  int cache_read_seconds;
  struct stat *cache_stat_subdirs_of_path;
};
struct rootdata{
  int index;
  char *path;
  int features,readdir_begin,readdir_end;
  char *readdir;
  int readdir_n,readdir_i;
  pthread_mutex_t mutex;
};
static pthread_mutexattr_t _mutex_attr_recursive;
static pthread_mutex_t _mutex_dir, _mutex_fhdata; // pthread_mutexattr_settype(&Attr,PTHREAD_MUTEX_RECURSIVE);
static void init_root(struct rootdata *rd, pthread_mutexattr_t *attr){
  pthread_mutex_init(&rd->mutex,attr);
  assert(rd->readdir_n==0);
}
static char *ensure_capacity_concat(struct rootdata *rd,int n){
  if (n<3333) n=3333;
  if (n+1>rd->readdir_n) rd->readdir=realloc(rd->readdir,rd->readdir_n=n*3/2);
  return rd->readdir;
}

#define FHDATA_MAX 3333
#define ROOTS 7
static struct fhdata _fhdata[FHDATA_MAX];
static struct rootdata _root[ROOTS]={0},_root_for_zipdir[ROOTS]={0};
static int _root_n=0;
#define ST_BLKSIZE 4096
#define FILE_FS_INFO "/_FILE_SYSTEM_INFO.HTML"
///////////////////////////////////////////////////////////
//
// The root directories are specified as program arguments
// The _root[0] is read/write, and can be empty string
// The others are read-only
//
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
static int
_count_read_zip_cached=0,
  _count_read_zip_regular=0,
  _count_read_zip_seekable=0,
  _count_read_zip_no_seek=0,
  _count_read_zip_seek_fwd=0,
  _count_read_zip_seek_bwd=0,
  _read_max_size=0;
#include "ZIPsFS.h" // (shell-command (readdir  "makeheaders "  (buffer-file-name)))


#include "configuration.h"
#include "log.h"
///////////////////////////////////////////////////////////
// Utils
int empty_dot_dotdot(const char *s){  return !*s || *s=='.' && (!s[1] || s[1]=='.'&&!s[2]); }
char *my_strncpy(char *dst,const char *src, int n){
  *dst=0;
  if (src) strncat(dst,src,n);
  return dst;
}
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


static void init_stat(struct stat *st, long size,struct stat *uid_gid){
  bool is_dir=size<0;
  clear_stat(st);
  st->st_mode=is_dir?(S_IFDIR|0777):(S_IFREG|0666);
  st->st_nlink=1;
  if (!is_dir){
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


void my_close_fh(int fh){  if (fh)  close(fh); }
int my_open_fh(const char* msg, const char *path,int flags){
  const int fh=open(path,flags);
  if (fh<=0) log_error("my_open_fh open(%s,flags) pid=%d\n",path,getpid());
  return fh;
}
#define ZP_DEBUG (1<<1)
#define ZP_ZIP (1<<2)
#define ZP_STRGS_ON_HEAP (1<<3)
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
  zpath->virtualpath=zpath->strgs=strgs_on_stack; //malloc(ZPATH_STRGS);
  zpath_strcat(zpath,virtualpath);
  zpath->virtualpath[VP_LEN()=l]=0;
}
void zpath_assert_strlen(const char *title,struct zippath *zpath){

  if (my_strlen(VP())!=VP_LEN()){
    if (title) printf("zpath_assert_strlen %s\n",title);
    log_error("VP=%s  %u!=%d\n",VP(),my_strlen(VP()),VP_LEN());
    log_zpath("Error ",zpath);
  }
  if (my_strlen(EP())!=EP_LEN()){
    if (title) printf("zpath_assert_strlen %s\n",title);
    log_error("EP=%s  %u!=%d\n",EP(),my_strlen(EP()),EP_LEN());
    log_zpath("Error ",zpath);
  }
#define LCHECK(a) assert(my_strlen(zpath->a)==zpath->a ## _l);
  LCHECK(virtualpath);
  LCHECK(virtualpath_without_entry);
  LCHECK(entry_path);
#undef LCHECK
}
int zpath_strncat(struct zippath *zpath,const char *s,int len){
  const int l=min_int(my_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      log_error("zpath_strncat %s %d ZPATH_STRGS\n",s,len);
      abort();
      return -1;
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
  char *heap=zpath->strgs=(char*)malloc(zpath->strgs_l+1);
  assert(heap!=NULL);
  memcpy(heap,stack,zpath->strgs_l+1);
  const long d=(long)(heap-stack);
#define ZPSHIFT(a) if (zpath->a) zpath->a+=d
  ZPSHIFT(virtualpath);
  ZPSHIFT(virtualpath_without_entry_l);
  ZPSHIFT(entry_path);
  ZPSHIFT(realpath);
#undef ZPSHIFT
}
#define NEW_ZIPPATH(virtpath)  char __zpath_strgs[ZPATH_STRGS];struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath,__zpath_strgs)
#define FIND_REAL(virtpath)    NEW_ZIPPATH(virtpath),res=find_realpath_any_root(zpath,-1)
const char *zpath_zipentry(struct zippath *zpath){
  if (!zpath) return NULL;
  return EP();
}
void log_zpath(char *msg, struct zippath *zpath){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf("    %p strgs="ANSI_FG_BLUE"%s"ANSI_RESET"  "ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, (zpath->flags&ZP_STRGS_ON_HEAP)?"Heap":"Stack", zpath->strgs_l);
  printf("    %p    VP="ANSI_FG_BLUE"'%s'"ANSI_RESET,VP(),snull(VP())); log_file_stat("",&zpath->stat_vp);
  printf("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  printf("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  printf("    %p    RP="ANSI_FG_BLUE"'%s'"ANSI_RESET,RP(), snull(RP())); log_file_stat("",&zpath->stat_rp);
  printf("    zip=%s  ZIP %s"ANSI_RESET"\n",yes_no(ZPATH_IS_ZIP()),  zpath->zarchive?ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}
void zpath_reset_realpath(struct zippath *zpath){ /* keep VP(), VP0() and EP() */
  if (zpath){
    struct zip *z=zpath->zarchive;
    if (z){
      zpath->zarchive=NULL;
      //log_debug_now("zpath_reset_realpath zip_close %p \n",zpath);
      if (zip_close(z)==-1) log_zpath(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,zpath);
    }
    const int fd=zpath->zarchive_fd;
    if(fd){
      zpath->zarchive_fd=0;
      my_close_fh(fd);
    }
    zpath->strgs_l=zpath->realpath_pos;
    clear_stat(&zpath->stat_rp);
    clear_stat(&zpath->stat_vp);
  }
}
void zpath_reset_keep_VP(struct zippath *zpath){
  VP0()=EP()=NULL;
  VP0_LEN()=EP_LEN()=0;
  zpath->strgs_l=(int)(VP()-zpath->strgs)+VP_LEN(); /* strgs is behind VP() */
}
void zpath_destroy(struct zippath *zpath){
  if (zpath){
    zpath_reset_realpath(zpath);
    if (zpath->flags&ZP_STRGS_ON_HEAP){FREE_NULL(zpath->strgs);}
    memset(zpath,0,sizeof(struct zippath));
  }
}
int zpath_stat(struct zippath *zpath){
  if (!zpath) return -1;
  int ret=0;
  if (!zpath->stat_rp.st_ino){
    ret=stat(RP(),&zpath->stat_rp);
    zpath->stat_vp=zpath->stat_rp;
  }
  return ret;
}
struct zip *my_zip_fdopen_ro(int *fd,const char *path){
  if (!path || *fd<=0 && (*fd=my_open_fh("my_zip_fdopen_ro",path,O_RDONLY))<=0) return NULL;
  int err;


  //zip_error_t err;
  //  ,zip_error_strerror(&err));
  struct zip *zip=zip_fdopen(*fd,0,&err);
  if (!zip) log_error("my_zip_fdopen_ro(%d)   %s  \n",*fd,path);
  return zip;
}
struct zip  *zpath_zip_open(struct zippath *zpath){
  if (!zpath) return NULL;
  if (!zpath->zarchive) zpath->zarchive=my_zip_fdopen_ro(&zpath->zarchive_fd,RP());
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
  if (SQLITE_OK==sqlite3_exec(_sqlitedb,sql,callback,udp,&errmsg)){
    //log_succes("%s\n",sql);
  }else{
    if (flags&SQL_SUCCESS) log_error("%s\n%s\n",sql,sqlite3_errmsg(_sqlitedb));
    if (flags&SQL_ABORT) abort();
    FREE_NULL(_sqlitedb);
  }
}
struct sqlresult{  long mtime; struct rootdata *rd; bool ok; };
int readdir_callback(void *arg1, int argc, char **argv,char **name) {
  struct sqlresult *r=(struct sqlresult*)arg1;
  for (int i=0;i<argc;i++){
    char *n=name[i],*a=argv[i];
    if (*n=='m' && !strcmp(n,"mtime")) r->mtime=atol(a);
    else if (*n=='r' && !strcmp(n,"readdir")){
      strcpy(ensure_capacity_concat(r->rd,r->rd->readdir_end=strlen(a)),a);
      r->rd->readdir_begin=0;
      r->ok=true;

    }else{
      log_warn("readdir_callback  %s=%s\n",n,snull(a));
    }
  }
  return 0;
}

bool illegal_char(const char *s){ return (s && (strchr(s,'\t')||strchr(s,READDIR_SEP))); }
void readdir_append(int *i, struct rootdata *rd, long inode, const char *n,bool append_slash,long size){
  if (!illegal_char(n)){
    *i+=sprintf(*i+ensure_capacity_concat(rd,*i+strlen(n)+55),"%s%s\t%lx\t%lx%c",n,append_slash?"/":"",inode,size,READDIR_SEP); //
  }
}
struct name_ino_size{char *name; long inode; long size; bool is_dir; int b,e,name_n;char *txt;};
static char *my_memchr(const char *b,const char *e, char c){
  return e>b?memchr(b,c,e-b):NULL;
}
static bool readdir_iterate(struct  name_ino_size*nis, struct rootdata *rd){ /* Parses the text written with readdir_append and readdir_concat */
  char *b=rd->readdir+rd->readdir_begin;
  char *e=rd->readdir+rd->readdir_end;
  if (nis->txt!=b){ nis->txt=b; nis->b=0; *e=0;}
  nis->e=(int)(strchrnul(b+nis->b+1,READDIR_SEP)-b);
  if (b+nis->e>e) return false;
  char *sep1,*sep2;
  if (!(sep1=my_memchr(nis->name=b+nis->b,e,'\t'))) return false;
  if (!(sep2=my_memchr(sep1+1,e,'\t'))) return false;
  nis->name[nis->name_n=sep1-nis->name-(nis->is_dir=(sep1[-1]=='/'))]=0;
  nis->inode=strtol(sep1+1,NULL,16);
  nis->size=strtol(sep2+1,NULL,16);
  nis->b=nis->e+1;
  return true;
}
#define READDIR_ZIP (1<<1)
static int readdir_concat(int opt,struct rootdata *rd,long mtime,const char *rp,struct zip *zip){
  if (WITH_SQL){
    char sql[999];
    struct sqlresult sqlresult={0};
    sqlresult.rd=rd;
    sprintf(sql,"SELECT mtime FROM readdir WHERE path='%s';",snull(rp));
    sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
    if (sqlresult.mtime==mtime){
      sprintf(sql,"SELECT readdir FROM readdir WHERE path='%s';",snull(rp));
      sql_exec(SQL_SUCCESS,sql,readdir_callback,&sqlresult);
      if (sqlresult.ok){
        //log_debug_now("from sql %s\n",rd->readdir);
        return 0;
      }
    }
  }
  int i=rd->readdir_begin=sprintf(ensure_capacity_concat(rd,0),"INSERT OR REPLACE INTO readdir VALUES('%s','%ld','",snull(rp),mtime);
  if(opt&READDIR_ZIP){
    int fd=0;
    if (zip || (zip=my_zip_fdopen_ro(&fd,rp))){
      struct zip_stat sb;
      const int n_entries=zip_get_num_entries(zip,0);
      for(int k=0; k<n_entries; k++){
        if (!zip_stat_index(zip,k,0,&sb)){
          readdir_append(&i,rd,k+1,sb.name,false,(long)sb.size);
        }
      }
      if (fd>0){
        zip_close(zip);
        close(fd);
      }
    }
  }else if (rp){
    DIR *dir=opendir(rp);
    if(dir==NULL){perror("Unable to read directory");return ENOMEM;}
    struct dirent *de;
    while((de=readdir(dir))){
      const char *n=de->d_name;
      if (!empty_dot_dotdot(n)) readdir_append(&i,rd,de->d_ino,n,(bool)(de->d_type==(S_IFDIR>>12)),0);
    }
    closedir(dir);
  }
  if (rd->readdir[i-1]=='|') --i;
  rd->readdir_end=i;
  sprintf(rd->readdir+i,"');");
  sql_exec(SQL_SUCCESS,rd->readdir,readdir_callback,NULL);
  return 0;
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
  if (zpath_strcat(zpath,_root[root].path) || !(*vp0=='/' && vp0[1]==0) && zpath_strcat(zpath,vp0)) return ENAMETOOLONG;
  int res=zpath_stat(zpath);
  if (!res && ZPATH_IS_ZIP()){
    if (EP_LEN()) return read_zipdir(_root+root,zpath,NULL,NULL,NULL);
    stat_set_dir(&zpath->stat_vp);
  }
  return res;
}
int test_realpath_any_root(struct zippath *zpath,int force_root){
  zpath->realpath_pos=zpath->strgs_l;
  zpath_assert_strlen("test_realpath_any_root ",zpath);
  for(int i=0;i<_root_n;i++){
    assert(_root[i].path!=NULL);
    if (force_root!=-1 && i!=force_root) continue;
    if (!test_realpath(zpath,i)) return 0;
    zpath_reset_realpath(zpath);
  }
  return -1;
}
int find_realpath_any_root(struct zippath *zpath,int force_root){
  const char *vp=VP(); /* Only zpath->virtualpath is defined */
  //  int strgs_l_save=zpath->strgs_l;
  zpath_reset_keep_VP(zpath);
  char *append="";
  const int vp_l=VP_LEN(), zip_l=zip_contained_in_virtual_path(vp,&append);
  int res=-1;
  if (zip_l){
    zpath->flags|=ZP_ZIP;
    VP0()=zpath_newstr(zpath);
    if (zpath_strncat(zpath,vp,zip_l) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
    VP0_LEN()=zpath_strlen(zpath);
    EP()=zpath_newstr(zpath);
    if (zip_l+1<vp_l) zpath_strcat(zpath,vp+zip_l+1);
    EP_LEN()=zpath_strlen(zpath);
    zpath_assert_strlen("find_realpath_any_root 1 ",zpath);
    res=test_realpath_any_root(zpath,force_root);
    if (!*EP()) stat_set_dir(&zpath->stat_vp);
  }
  if (res){
    int approach=0;
    for(int preventRunAway=3;--preventRunAway>=0 && approach>=0;){
      const int len=zipentry_to_zipfile(&approach,vp,&append);
      if (len){
        //    zpath->strgs_l=strgs_l_save;
        zpath_reset_keep_VP(zpath);
        VP0()=zpath_newstr(zpath);
        if (zpath_strncat(zpath,vp,len) || zpath_strcat(zpath,append)) return ENAMETOOLONG;
        VP0_LEN()=zpath_strlen(zpath);
        EP()=zpath_newstr(zpath);
        if (zpath_strcat(zpath,vp+last_slash(vp)+1)) return ENAMETOOLONG;
        EP_LEN()=zpath_strlen(zpath);
        zpath_assert_strlen("find_realpath_any_root 2 ",zpath);
        if (!(res=test_realpath_any_root(zpath,force_root))) break;
      }
    }
  }
  if (res){
    //    zpath->strgs_l=strgs_l_save;
    zpath_reset_keep_VP(zpath);
    zpath_assert_strlen("find_realpath_any_root 3 ",zpath);
    res=test_realpath_any_root(zpath,force_root);
  }
  return res;
}
static void usage(){
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
static const struct fhdata  FHDATA_EMPTY={0};//.fh=0,.offset=0,.file=NULL};
int fhdata_zip_open(struct fhdata *d,char *msg){
  log_mthd_invoke(read_zipdir);
  zip_file_t *zf=d->zip_file;
  if (zf && !zip_ftell(zf)) return 0;
  log_mthd_orig(read_zipdir);
  fhdata_zip_fclose(d,"fhdata_zip_open");
  struct zip *za=d->zpath.zarchive;
  //log("fhdata_zip_open %s\n",msg);
  if(za &&  (d->zip_file=zip_fopen(za,zpath_zipentry(&d->zpath),ZIP_RDONLY))!=0) return 0;
  return -1;
}
void fhdata_zip_fclose(struct fhdata *d,char *msg){
  if (d) {
    if (d->zip_file){
      zip_fclose(d->zip_file);
      //log_debug_now("Going to zip_fclose %s %p  \n",msg,d->zip_file);
    }
    d->zip_file=NULL;
  }
}
static struct fhdata* fhdata(enum data_op op,const char *path,struct fuse_file_info *fi){
  if (!fi) return NULL;
  uint64_t fh=fi->fh;
  //log(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  uint64_t hash=hash_key(path);
  struct fhdata *d;
  for(int i=_fhdata_n;--i>=0;){
    d=&_fhdata[i];
    if (fh==d->fh && d->path_hash==hash && !strcmp(path,d->path)){
      if(op==RELEASE){
        log(ANSI_FG_RED"Release fhdata %lu\n"ANSI_RESET,fh);
        fhdata_zip_fclose(d,"RELEASE");
        zpath_destroy(&d->zpath);
        cache_zip_entry(RELEASE,d);
        FREE_NULL(d->path);
        FREE_NULL(d->cache_stat_subdirs_of_path);
        for(int j=i+1;j<_fhdata_n;j++) _fhdata[j-1]=_fhdata[j];
        _fhdata_n--;
      }
      return d;
    }
  }
  if (op==CREATE && _fhdata_n<FHDATA_MAX){
    //log(ANSI_FG_GREEN"New fhdata %lu\n"ANSI_RESET,fh);
    d=_fhdata+_fhdata_n;
    memset(d,0,sizeof(struct fhdata));
    d->fh=fh;
    d->path=strdup(path);
    d->path_hash=hash;
    return _fhdata+_fhdata_n++;
  }
  return NULL;
}
static struct fhdata *fhdata_by_vpath(const char *path,struct fhdata *not_this){
  const uint64_t h=hash_key(path);
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (d!=not_this && d->path_hash==h && !strcmp(path,_fhdata[i].path)) return d;
  }
  return NULL;
}

#define fhdata_synchronized(op,path,fi) pthread_mutex_lock(&_mutex_fhdata); struct fhdata* d=fhdata(op,path,fi);  pthread_mutex_unlock(&_mutex_fhdata);
/* static struct fhdata* fhdata_synchronized(data_op op,const char *path,struct fuse_file_info *fi){ */
/*   pthread_mutex_lock(&_mutex_fhdata); */
/*   struct fhdata* d=fhdata(op,path,fi); */
/*   pthread_mutex_unlock(&_mutex_fhdata); */
/* } */


/* *************************************************************/
/* There are many xmp_getattr calls on /d folders during reads */
/* This is a cache */
static struct fhdata *fhdata_by_virtualpath(const char *path){
  const int len=my_strlen(path);
  if (!len) return NULL;
  //const uint64_t h=hash_key(path);
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (!d) continue;
    const int n=d->zpath.virtualpath_l;
    if (len<=n){
      const char *vp=d->zpath.virtualpath;
      if (vp && !strncmp(path,vp,n) && len==n || vp[len]=='/') return d;
    }
  }
  return NULL;
}
static struct stat *get_stat_from_fhdata(const char *path){
  struct fhdata *d=fhdata_by_virtualpath(path);
  if (!d) return NULL;
  if (!d->cache_stat_subdirs_of_path) d->cache_stat_subdirs_of_path=calloc(count_slash(d->path)+1,sizeof(struct stat));
  struct stat *s=d->cache_stat_subdirs_of_path+count_slash(path);
  if (s->st_ino==0){
    int res;
    FIND_REAL(path);
    char *rp=zpath->realpath;
    log_debug_now("get_stat_from_fhdata rp=%s\n",rp);
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


static int realpath_mk_parent(char *realpath,const char *path){
  //log_entered_function("realpath_mk_parent %s\n",path);
  const char *p0=_root[0].path;
  if (!*p0) return EACCES;/* Only first root is writable */
  const int slash=last_slash(path);
  //log_entered_function(" realpath_mk_parent(%s) slash=%d  \n  ",path,slash);
  if (slash>0){
    int res=0;
    char *parent=strndup(path,slash);
    FIND_REAL(parent);
    if (!res){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,p0),parent,MAX_PATHLEN);
      recursive_mkdir(realpath);
    }
    free(parent);
    zpath_destroy(zpath);
    if (res) return ENOENT;
  }
  strncat(strcpy(realpath,p0),path,MAX_PATHLEN);
  return 0;
}

int read_zipdir(struct rootdata *rd, struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  log_mthd_orig(read_zipdir);
  int res=0;
  const int zentry_l=EP_LEN();
  log_entered_function("read_zipdir rp=%s filler=%p  vp=%s  entry_path=%s   zentry_l=%d\n",RP(),filler_maybe_null,VP(),EP(),zentry_l);
  if(!zentry_l && !filler_maybe_null){ /* The virtual path is a Zip file */
    return 0; /* Just report success */
  }else{
    if (zpath_stat(zpath)) res=ENOENT;
    else if (!readdir_concat(READDIR_ZIP,rd,zpath->stat_rp.st_mtime,RP(),zpath_zip_open(zpath))){ /* The virtual path is a Zip file with zip-entry */
      char s[MAX_PATHLEN], *zentry=EP();// n[MAX_PATHLEN];
      const int len_ze=strlen(zentry);
      //log_debug_now(ANSI_INVERSE"read_zipdir"ANSI_RESET"  n_entries=%d\n",n_entries);
      struct name_ino_size nis={0};
      while(readdir_iterate(&nis, rd)){
        char *n=nis.name;
        int len=my_strlen(n),is_dir=nis.is_dir, not_at_the_first_pass=0;
        if (len>=MAX_PATHLEN) { log_warn("Exceed MAX_PATHLEN: %s\n",n); continue;}
        while(len){
          if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
            const int slash=last_slash(n);
            if (slash<0) break;
            n[slash]=0;
            is_dir=1;
          }
          if (!(len=my_strlen(n))) break;
          if (!filler_maybe_null){  /* ---  read_zipdir() has been called from test_realpath() --- */
            if (len_ze==len && !strncmp(zentry,n,len)){
              struct stat *st=&zpath->stat_vp;
#define SET_STAT()                                                      \
              init_stat(st,is_dir?-1:nis.size,&zpath->stat_rp);         \
              st->st_ino^=((nis.inode<<SHIFT_INODE_ZIPENTRY)|ADD_INODE_ROOT(rd->index));
              SET_STAT();
              return 0;
            }
          }else{
            if (len<zentry_l || slash_not_trailing(n+zentry_l)>0) continue;
            { const char *q=(char*)n+zentry_l; my_strncpy(s,q,(int)(strchrnul(q,'/')-q)); }
            if (!*s || len<len_ze||strncmp(zpath_zipentry(zpath),n,len_ze)) continue;
            if (slash_not_trailing(n+len_ze+1)>=0) continue;
            if (ht_set(no_dups,s,"")) continue;
            struct stat stbuf, *st=&stbuf;
            SET_STAT();
#undef SET_STAT
            //log_debug_now(ANSI_GREEN"zip filler"ANSI_RESET" s=%s  n=%s "ANSI_RESET,s,n); log_file_stat(" ",st);
            filler_maybe_null(buf,s,st,0,fill_dir_plus);
          }
        }// while len
      }
    }
  }
  return filler_maybe_null? res:ENOENT;
}

static int impl_readdir(struct rootdata *rd,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  log_mthd_invoke(impl_readdir);
  log_entered_function("impl_readdir vp=%s rp=%s ZPATH_IS_ZIP=%d  \n",VP(),snull(rp),ZPATH_IS_ZIP());
  if (!rp || !*rp) return 0;
  pthread_mutex_lock(&_mutex_dir);
  if (ZPATH_IS_ZIP()){
    read_zipdir(rd,zpath,buf,filler,no_dups);
  }else{
    assert(file_mtime(rp)==zpath->stat_rp.st_mtime);
    const long mtime=zpath->stat_rp.st_mtime;
    if (mtime){
      log_mthd_orig(impl_readdir);
      struct stat st;
      char direct_rp[MAX_PATHLEN], *append="", display_name[MAX_PATHLEN];
      struct name_ino_size nis={0},nis2;
      readdir_concat(0,rd,mtime,rp,NULL);
      memset(&nis,0,sizeof(nis));
      //log_debug_now("\nrd->readdir\n");
      print_substring(2,rd->readdir,rd->readdir_begin,rd->readdir_end); puts("\n\n");
      while(readdir_iterate(&nis,rd)){
        char *n=nis.name;
        //log_debug_now("n=%s\t",n);
        //log_debug_now("name=%s  inode=%ld size=%ld b=%d e=%d \n",n,nis.inode,nis.size,nis.b,nis.e);
        if (empty_dot_dotdot(n)) continue;
        if (ht_set(no_dups,n,"")) continue;
        struct rootdata *rd2=_root_for_zipdir+rd->index; /* Prevent collision with field rd->readdir */
        if (directly_replace_zip_by_contained_files(n) &&
            (MAX_PATHLEN!=snprintf(direct_rp,MAX_PATHLEN,"%s/%s",rp,n)) &&
            !readdir_concat(READDIR_ZIP,rd2,file_mtime(direct_rp),direct_rp,NULL)){
          memset(&nis2,0,sizeof(nis2));
          for(int j=0;readdir_iterate(&nis2,rd2);j++){
            if (strchr(nis2.name,'/') || ht_set(no_dups,nis2.name,"")) continue;
            init_stat(&st,nis2.is_dir?-1:nis.size,&zpath->stat_rp);
            st.st_ino=nis.inode^((nis2.inode<<SHIFT_INODE_ZIPENTRY)|ADD_INODE_ROOT(rd->index));
            //log_debug_now(" filler nis2.name=%s ",nis2.name); log_file_stat(" ",&st);
            filler(buf,nis2.name,&st,0,fill_dir_plus);
          }
        }else{
          init_stat(&st,(nis.is_dir||zip_contained_in_virtual_path(n,&append))?-1:nis.size,NULL);
          st.st_ino=nis.inode^ADD_INODE_ROOT(rd->index);
          //if(ZPATH_IS_ZIP())  st.st_mode|=(S_IXUSR|S_IXGRP);
          //log_debug_now("filler %s\n",real_name_to_display_name(display_name,n));
          filler(buf,real_name_to_display_name(display_name,n),&st,0,fill_dir_plus);
        }
      }
    }
    if (!rd->index && !strcmp(VP(),"/")) filler(buf,FILE_FS_INFO+1,NULL,0,fill_dir_plus);
  }
  pthread_mutex_unlock(&_mutex_dir);
  //log_exited_function("realpath_readdir \n");
  return 0;
}
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  (void) conn;
  cfg->use_ino=1;
  cfg->entry_timeout=cfg->attr_timeout=cfg->negative_timeout=0;
  return NULL;
}
/////////////////////////////////////////////////
//
// Functions where Only single paths need to be  substituted

static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi){
  if (!strcmp(path,FILE_FS_INFO)){
    init_stat(stbuf,MAX_INFO,NULL);
    time(&stbuf->st_mtime);
    return 0;
  }
  //log_entered_function("xmp_getattr %s fh=%lu \n",path,fi!=NULL?fi->fh:0);
  log_mthd_invoke(xmp_getattr);
  pthread_mutex_lock(&_mutex_fhdata);
  struct fhdata* d=fhdata(GET,path,fi);
  if (!d) d=fhdata_by_vpath(path,NULL);
  struct stat *stat_cached=d?NULL:get_stat_from_fhdata(path);
  if (stat_cached){
    //log_debug_now("xmp_getattr d FOUND  %s ",path); log_file_stat(" ",stat_cached); //exit(9);
      *stbuf=*stat_cached;
      return stat_cached->st_ino==-1?-1:0;
    }
  pthread_mutex_unlock(&_mutex_fhdata);
  //  if (d && d->stat.st_ino){ *stbuf=d->stat; return 0;}
  if (d && d->zpath.stat_vp.st_ino){
    *stbuf=d->zpath.stat_vp;
    return 0;
  }
  log_mthd_orig(xmp_getattr);
  static int count=0;
  int res;
  FIND_REAL(path);
  if(!res){
    *stbuf=zpath->stat_vp;
  }
  zpath_destroy(zpath);
  if (endsWith(path,"20230126_PRO1_KTT_002_30-0046_LisaKahl_P01_VNATSerAuxpM1evoM2Glucose10mMGlycine3mM_dia_BC7_1_12094.d")){
    log("*");
  }
  return res==-1?-ENOENT:-res;
}
/* static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi){ */
/*   int res=_xmp_getattr(path,stbuf,fi); */
/*   log_entered_function("_xmp_getattr %s fh=%lu    returns %d \n",path,fi!=NULL?fi->fh:0,res); */
/*   return res; */
/* } */
static int xmp_access(const char *path, int mask){
  if (!strcmp(path,FILE_FS_INFO)) return 0;
  //log_entered_function("xmp_access %s\n",path);
  int res;
  log_mthd_orig(xmp_access);
  FIND_REAL(path);
  if (res==-1) res=ENOENT;
  if (!res){
    if ((mask&X_OK) && S_ISDIR(zpath->stat_vp.st_mode)) mask=(mask&~X_OK)|R_OK;
    res=access(RP(),mask);
  }
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  log_mthd_orig(xmp_readlink);
  int res;
  FIND_REAL(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_unlink(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;
  FIND_REAL(path);
  if (!res) res=unlink(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_rmdir(const char *path){
  log_mthd_orig(xmp_unlink);
  int res;
  FIND_REAL(path);
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
      d->cache_len=d2->cache_len;
    }
    //uint64_t h=d->path_hash;
    /*for(int i=_fhdata_n; --i>=0;){
      if(d!=_fhdata+i && _fhdata[i].path_hash==h  && !strcmp(d->path,_fhdata[i].path)){
      c=d->cache=_fhdata[i].cache;
      d->cache_len=_fhdata[i].cache_len;
      break;
      }
      }*/
    if (c){log_cache(ANSI_FG_GREEN"Found cache in other record %p\n"ANSI_RESET,d->cache);
    }
  }
  switch(op){
  case CREATE:
    if (!c){
      long  len=d->zpath.stat_vp.st_size; //bsize=zpath.stbuf.st_blksize;
      //len+=(bsize-(len%bsize));
      log_cache(ANSI_RED"Going to cache %s %'ld Bytes"ANSI_RESET"\n",d->path,len);
      char *bb=mmap(NULL,len,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
      if (bb!=MAP_FAILED) _mmap_n++;
      else{
        log_warn("mmap returned MAP_FAILED\n");
        return false;
      }
      d->cache=bb;
      fhdata_zip_open(d,"CREATE");
      {
        long start=time_ms();
        long already=0;
        int count=0;
        while(already<len){
          const long n=zip_fread(d->zip_file,bb+already,len-already);
          log_cache("Read %'ld\n",n);
          if (n<0){
            log_error("cache_zip_entry_in_RAM %s\n  read %'ld/%'ld",d->path,already,len);
            break;
          }
          already+=n;
          count++;
        }
        d->cache_len=already;
        //log_succes("Bulk read zip entry %s in %'lu seconds in %d shunks\n",d->path,time_ms()-start,count);
        d->cache_read_seconds=time_ms()-start;
      }
      fhdata_zip_fclose(d,"CREATED");
      log_cached(-1,"CREATE");
      return true;
    }
    break;
  case RELEASE:
    if (c){
      bool hasref=false;
      for(int i=_fhdata_n; --i>=0;){
        if (_fhdata+i!=d && (hasref=(_fhdata[i].cache==c))){
          //log_debug_now("Not munmap %p for i=%ld since ref i=%d ",c, (long)(d-_fhdata),i);
          break;
        }
      }
      if(!hasref){
        log_cache("Going to release %p\n",c);
        if (munmap(c,d->cache_len)){
          perror("munmap");
        }else{
          log_succes("munmap\n");
          _munmap_n++;
        }
        d->cache=NULL;
        d->cache_len=0;
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
    case RULE:
      if (!need_cache_zip_entry(d->zpath.stat_vp.st_size,d->zpath.virtualpath)) return false;
      break;
    }
  }
  pthread_mutex_lock(&_mutex_fhdata);
  const bool success=cache_zip_entry(op,d);
  pthread_mutex_unlock(&_mutex_fhdata);
  return success;
}
static int xmp_open(const char *path, struct fuse_file_info *fi){
  if (!strcmp(path,FILE_FS_INFO)){
    fi->direct_io=1;
    return 0;
  }
  log_mthd_orig(xmp_open);
  log_entered_function("xmp_open %s\n",path);
  static uint64_t _next_fh=FH_ZIP_MIN;
  int res,handle=0;
  FIND_REAL(path);
  //log_zpath("xmp_open",&zpath);
  if (res){
    log_warn("xmp_open(%s) FIND_REAL res=%d\n",path,res);
  }else{
    if (ZPATH_IS_ZIP()){
      if (!zpath->zarchive){
        log_warn("In xmp_open %s: zpath->zarchive==NULL\n",path);
        return -ENOENT;
      }
      handle=fi->fh=_next_fh++;
      fhdata_synchronized(CREATE,path,fi);
      zpath_stack_to_heap(zpath); d->zpath=*zpath;zpath=NULL;
      if (!maybe_cache_zip_entry(CREATE,d,false)) fhdata_zip_open(d,"xmp_open");
    }else{
      handle=my_open_fh("xmp_open reg file",RP(),fi->flags);
    }
  }
  zpath_destroy(zpath);
  //log_exited_function("xmp_open %s handle=%d res=%d\n",path,handle,res);
  if (res) return -ENOENT;
  if (handle==-1) return -errno;
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
  log_entered_function("xmp_readdir %s\n",path);
  log_mthd_orig(xmp_readdir);
  int res_all=0;
  (void) offset;
  (void) fi;
  (void) flags;
  struct ht *no_dups=ht_create(7);
  for(int i=0;i<_root_n;i++){
    NEW_ZIPPATH(path);
    assert(_root[i].path!=NULL);

    if (!find_realpath_any_root(zpath,i)){
      impl_readdir(_root+i,zpath,buf,filler,no_dups);
    }else{
      //log_zpath("xmp_readdir ",zpath);
    }
    zpath_destroy(zpath);
  }
  ht_destroy(no_dups);
  return res_all;
}
/////////////////////////////////
//
// Creating a new file object
static int xmp_mkdir(const char *create_path, mode_t mode){
  log_entered_function("xmp_mkdir %s \n",create_path);
  char realpath[MAX_PATHLEN];
  int res=realpath_mk_parent(realpath,create_path);
  if (res) return -res;
  res=mkdir(realpath,mode);
  if (res==-1) res=errno;
  return -res;
}
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_YELLOW"==========xmp_create"ANSI_RESET" %s\n",create_path);
  char realpath[MAX_PATHLEN];
  int res=realpath_mk_parent(realpath,create_path);
  if (res) return -res;
  res=open(realpath,fi->flags,mode);
  log_exited_function("xmp_create %s res=%d\n",create_path,res);
  if (res==-1) return -errno;
  fi->fh=res;
  return 0;
}
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  log_entered_function(ANSI_YELLOW"========xmp_write"ANSI_RESET" %s  fi=%p \n",create_path,fi);
  int fd;
  int res=0;
  (void) fi;
  if(fi==NULL){
    char realpath[MAX_PATHLEN];
    if (res=realpath_mk_parent(realpath,create_path)) return -res;
    fd=open(realpath,O_WRONLY);
  }else fd=fi->fh;
  if (fd==-1) return -errno;
  res=pwrite(fd,buf,size,offset);
  if (res==-1) res=-errno;
  if(fi==NULL) close(fd);
  return res;
}
////////////////////////////////////////////////////////
//
// Two paths
static int xmp_symlink(const char *target, const char *create_path){ // target,link
  log_entered_function("xmp_symlink %s %s\n",target,create_path);
  char realpath[MAX_PATHLEN];
  int res=realpath_mk_parent(realpath,create_path);
  if (res) return -res;
  res=symlink(target,realpath);
  if (res==-1) return -errno;
  return -res;
}
static int xmp_rename(const char *old_path, const char *neu_path, unsigned int flags){ // from,to
  log_entered_function(" xmp_rename from=%s to=%s \n",old_path,neu_path);
  if (flags) return -EINVAL;
  char old[MAX_PATHLEN],neu[MAX_PATHLEN], *p0=_root[0].path;
  strcpy(old,p0);
  strcat(old,old_path);
  strcpy(neu,p0);
  strcat(neu,neu_path);
  int res=rename(old,neu);
  if (res==-1) return -errno;
  return -res;
}
static int maybe_read_from_cache(struct fhdata *d, char *buf, size_t size, off_t offset,bool always){
  int res=-1;
  if (d && maybe_cache_zip_entry(GET,d,always)){
    //log_cache("xmp_read %s  cache=%p size=%'zu\n",path,cache,size);
    res=0;
    if (offset<d->cache_len) memcpy(buf,d->cache+offset,res=MIN(size,d->cache_len-offset));
  }
  return res;
}
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
  log_mthd_orig(xmp_read);
  if (size>(int)_read_max_size) _read_max_size=(int)size;
  static int _count_xmp_read=0;
  assert(fi!=NULL);
  if (!strcmp(path,FILE_FS_INFO)){
    pthread_mutex_lock(&_mutex_fhdata);
    long n=get_info()-(long)offset;
    if (n>(long)size) n=size; /* This cast to (long) is important */
    //    log_debug_now("get_info=%d  size=%ld offset=%ld n=%ld   \n",i,size,offset,n);
    if (n<=0) return 0;
    memcpy(buf,_info+offset,n);
    pthread_mutex_unlock(&_mutex_fhdata);
    return n;
  }
  fhdata_synchronized(CREATE,path,fi);
  d->access=time(NULL);
  //if (_count_xmp_read++%1000==0) log_entered_function(ANSI_BLUE"%d  xmp_read"ANSI_RESET" %s size=%zu offset=%'lu %p  fh=%lu\n",_count_xmp_read,path,size ,offset,d,fi==NULL?0:fi->fh);
  int res=0;
  long diff;
  //log_debug_now("xmp_read  fh=%lu d=%p\n",fi->fh,d);
  if((res=maybe_read_from_cache(d,buf,size,offset,false))>=0){
    _count_read_zip_cached++;
    return res;
    //log_cache("xmp_read %s  cache=%p size=%'zu\n",path,cache,size);
  }else if (d->zip_file){
    log_debug_now("xmp_read zip_file\n");
    _count_read_zip_regular++;
    // offset>d: Need to skip data.   offset<d  means we need seek backward
    diff=offset-zip_ftell(d->zip_file);
    //log_debug_now(" o=%zu d=%ld ",offset,diff);
    if (!diff) _count_read_zip_no_seek++;
    if (diff && zip_file_is_seekable(d->zip_file)){
      //log_seek_ZIP(diff,"%s zip_file_is_seekable\n",path);
      if (zip_fseek(d->zip_file,offset,SEEK_SET)<0) return -1;
      _count_read_zip_seekable++;
    }else if (diff<0){ // Worst case
      log_debug_now(" "ANSI_RED"seek-bwd"ANSI_RESET" ");
      if (_when_cache>=SEEK && (res=maybe_read_from_cache(d,buf,size,offset,false))>=0){
        return res;
      }
      _count_read_zip_seek_bwd++;
      fhdata_zip_open(d,"SEEK");
    }
    {
      while (diff=offset-zip_ftell(d->zip_file)){
        _count_read_zip_seek_fwd++;
        if (zip_fread(d->zip_file,buf,MIN(size,diff))<0) return -1;
      }
    }
    zip_int64_t n=zip_fread(d->zip_file,buf, size);
    return n;
  }else{
    log_debug_now("xmp_read file\n");
    int fd;
    if(fi){
      fd=fi->fh;
    }else{
      FIND_REAL(path);
      if (res) return -ENOENT;
      fd=open(RP(),O_RDONLY);
      zpath_destroy(zpath);
    }
    if (fd==-1) return -errno;
    if (offset-lseek(fd,0,SEEK_CUR)){
      if (offset==lseek(fd,offset,SEEK_SET)){
        //log_seek(offset,"lseek Success %s\n",path);
      }else{
        //log_seek(offset,"lseek Failed %s\n",path);
        return -1;
      }
    }
    res=pread(fd,buf,size,offset);
    if (res==-1) res=-errno;
    if (fi==NULL) close(fd);
    //log_exited_function("xmp_read %s res=%d\n",path,res);
  }
  //log_exited_function("xmp_read regfile\n");
  return res;
}
/////////////////////////////////////////////////
//
// These are kept unchanged from passthrough example
static int xmp_release(const char *path, struct fuse_file_info *fi){
  log_mthd_orig(xmp_release);
  (void) path;
  fhdata_synchronized(RELEASE,path,fi);
  log_entered_function(ANSI_FG_GREEN"xmp_release %s d=%p zip_file=%p\n"ANSI_RESET,path,d,!d?NULL:d->zip_file);
  if (fi->fh <FH_ZIP_MIN) close(fi->fh);
  //  get_info();puts(_info);
  return 0;
}
static struct fuse_operations xmp_oper={0};
struct rlimit _rlimit={0};
int main(int argc, char *argv[]){

  setlocale(LC_NUMERIC, "");
  assert(S_IXOTH==(S_IROTH>>2));
  assert((1<<(SHIFT_INODE_ZIPENTRY-SHIFT_INODE_ROOT))>ROOTS);
  /* Debugging */
  {
    pthread_mutexattr_init(&_mutex_attr_recursive);
    pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex_fhdata,&_mutex_attr_recursive);
    pthread_mutex_init(&_mutex_dir,NULL);
  }
  zipentry_to_zipfile_test();
  if (false){
    struct zippath zp,*zpath=&zp;
    char strgs[999];
    zpath_init(zpath,"12345",strgs);
    char *s3=zpath_newstr(zpath);
    int s3_len=zpath_strlen(zpath);
    char *s1=zpath_newstr(zpath);
    zpath_strcat(zpath,"a");
    zpath_strcat(zpath,"b");
    zpath_strcat(zpath,"");
    zpath_strcat(zpath,"c");
    int s1_len=zpath_strlen(zpath);
    char *s2=zpath_newstr(zpath);
    zpath_strcat(zpath,"A");
    zpath_strcat(zpath,"B");
    zpath_strcat(zpath,"");
    zpath_strcat(zpath,"C");
    int s2_len=zpath_strlen(zpath);
    printf("s=%s %d \n ",s1,s1_len);
    printf("s=%s %d \n ",s2,s2_len);
    printf("s=%s %d \n ",s3,s3_len);
    exit(9);
  }
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr);
  S(access);
  S(readlink);
  S(readdir);
  S(mkdir);
  S(symlink);
  S(unlink);
  S(rmdir);
  S(rename);
  S(truncate);
  S(open);
  S(create);
  S(read);
  S(write);
  //S(statfs);
  S(release);
  //S(lseek);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  char *argv_fuse[9]={0};
  int c,argc_fuse=1;
  argv_fuse[0]=argv[0];
  while((c=getopt(argc,argv,"o:sfdhC:L:D:"))!=-1){
    switch(c){
    case 'S': _simulate_slow=true; break;
    case 'D': my_strncpy(_sqlitefile,optarg,MAX_PATHLEN); break;
    case 'h': usage();break;
    case 'L': {
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
    case 's': argv_fuse[argc_fuse++]="-s"; break;
    case 'f': argv_fuse[argc_fuse++]="-f"; break;
    case 'd': argv_fuse[argc_fuse++]="-d";break;
    case 'o': argv_fuse[argc_fuse++]="-o"; argv_fuse[argc_fuse++]=strdup(optarg);break;
    }
  }
  argv_fuse[argc_fuse++]="-s";
  // See which version of fuse we're running
  log("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  /* *************************************************************** */
  /* *** SQLITE3 *** */
  if (!*_sqlitefile){
    sprintf(_sqlitefile,"%s/tmp/ZIPsFS", getenv("HOME"));
    recursive_mkdir(_sqlitefile);
    strcat(_sqlitefile,"/sqlite.db");
  }
  assert(SQLITE_OK==sqlite3_config(SQLITE_CONFIG_SERIALIZED));   // See https://www.sqlite.org/c3ref/config.html
  log("_sqlitefile='%s' sqlite3_threadsafe=%d\n",_sqlitefile,sqlite3_threadsafe());
  if (SQLITE_OK==sqlite3_open(_sqlitefile,&_sqlitedb)) log_succes("Opened %s\n",_sqlitefile);
  else { log_error("Open database: %s\n%s\n",_sqlitefile, sqlite3_errmsg(_sqlitedb)); exit(1);}
  char *sql="CREATE TABLE IF NOT EXISTS readdir (path TEXT PRIMARY KEY,mtime INT8,readdir TEXT);";
  if (sql_exec(SQL_ABORT|SQL_SUCCESS,sql,0,0));
  /* ************************************************************** */
  /* *** argv. argv_fuse  *** */
  assert(MAX_PATHLEN<=PATH_MAX);
  if (argc-optind<2) {usage();abort();}
  argv_fuse[argc_fuse++]=argv[argc-1]; // last is the mount point
  for(int i=optind;i<argc-1;i++){
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    char *r=argv[i];
    if (i && !*r && _root_n) continue; /* Accept empty String for _root[0] */
    int slashes=-1;
    while(r[++slashes]=='/');
    struct rootdata *rd=&_root[_root_n];
    rd->index=_root_n++;
    init_root(rd,&_mutex_attr_recursive);
    if (slashes>1){
      rd->features|=ROOT_REMOTE;
      r+=(slashes-1);
    }
    rd->path=realpath(r,NULL);
  }
  _root[0].features=ROOT_WRITABLE;
  if (!_root_n) log_abort("Missing root directories\n");
  log("about to call fuse_main\n");
  log_strings("fuse argv",argv_fuse,argc_fuse);
  //log_strings("root",_root,_root_n);
  int fuse_stat=fuse_main(argc_fuse,argv_fuse, &xmp_oper,NULL);
  log("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
