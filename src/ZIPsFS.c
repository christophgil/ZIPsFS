/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS
  Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  (global-set-key (kbd "<f1>") '(lambda() (interactive) (switch-to-buffer "ZIPsFS.c")))
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
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#include <malloc.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <execinfo.h>
#include <signal.h>
#include <locale.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdbool.h>
#define FH_ZIP_MIN (1<<20)
#include <fuse.h>
#define fill_dir_plus 0
#include <zip.h>
#include <assert.h>
#include "ht.c"
#include <sys/mman.h>
#define DEBUG_NOW 1
#define SHIFT_INODE_ROOT 40
#define SHIFT_INODE_ZIPENTRY 43
#define ADD_INODE_ROOT() (((long)root+1)<<SHIFT_INODE_ROOT)
#define ADD_INODE_E(i) (((long)i+1)<<SHIFT_INODE_ZIPENTRY)
//////////////////////////////////////////////////////////////////
// Structs and enums
static int _fhdata_n=0,_mmap_n=0,_munmap_n=0;
enum data_op{GET,CREATE,RELEASE};
enum when_cache_zip{NEVER,SEEK,RULE,ALWAYS};
static enum when_cache_zip _when_cache=SEEK;
static char *WHEN_CACHE_S[]={"never","seek","rule","always",NULL};
struct zippath{
  const char *virtualpath;
  char *virtualpath_without_entry; /*free*/
  int realpath_max;
  char *realpath;  /*free*/
  char *entry_path; /*free*/
  int len_virtual_zipfile;
  struct stat stbuf;
  struct zip *zarchive;
  unsigned int flags;
  int zarchive_fd;
};
struct fhdata{
  unsigned long fh; /*Serves as key*/
  char *path; /*Serves as key*/
  uint64_t path_hash; /*Serves as key*/
  zip_file_t *zip_file;
  struct stat stat;
  //  struct statvfs statvfs;  bool statvfs_set;
  struct zippath zpath;
  time_t access;
  char *cache;
  size_t cache_len;
  int cache_read_sec;

};


#define FHDATA_MAX 3333
#define ROOTS 7
static struct fhdata _fhdata[FHDATA_MAX];

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
static int _root_n=0, _root_feature[ROOTS]={0};
static char *_root[ROOTS]={0}, *_root_descript[ROOTS]={0};

static int
_count_read_zip_cached=0,
  _count_read_zip_regular=0,
      _count_read_zip_seekable=0,
    _count_read_zip_seek_fwd=0,
  _count_read_zip_seek_bwd=0,
  _read_max_size=0;

#include "ZIPsFS.h" // (shell-command (concat  "makeheaders "  (buffer-file-name)))
#include "configuration.h"
#include "log.h"

// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
///////////////////////////////////////////////////////////
// Utils
long time_ms(){
struct timeval tp;
gettimeofday(&tp,NULL);
return tp.tv_sec*1000+tp.tv_usec/1000;
}

unsigned int my_strlen(const char *s){ return !s?0:strnlen(s,MAX_PATHLEN);}
void my_close_fh(int fh){
  if (fh){
    //log_debug_now("my_close_fh %d\n",fh);
    close(fh);
  }
}
int my_open_fh(const char* msg, const char *path,int flags){
  int fh=open(path,flags);
  //log_debug_now("my_open_fh %s %s fh=%d\n",msg,path,fh);
  return fh;
}
#define MIN(X,Y) (((X)<(Y))?(X):(Y))
#define ZP_DEBUG (1<<1)
#define ZP_NOT_SET_S_IFDIR (1<<2)
#define IS_ZPATH_DEBUG() (zpath->flags&ZP_DEBUG)
int empty_dot_dotdot(const char *s){
  return !*s || *s=='.' && (!s[1] || s[1]=='.'&&!s[2]);
}
int last_slash(const char *path){
  for(int i=my_strlen(path);--i>=0;){
    if (path[i]=='/') return i;
  }
  return -1;
}
static int slash_not_trailing(const char *path){
  char *p=strchr(path,'/');
  return p && p[1]?(int)(p-path):-1;
}
int pathlen_ignore_trailing_slash(const char *p){
  const int n=my_strlen(p);
  return n && p[n-1]=='/'?n-1:n;
}
char *my_strcpy(char *dst,const char *src, size_t n){ /* Beware strncpy terminal 0 */
  *dst=0;
  if (src){
    if (n>=MAX_PATHLEN){
      n=MAX_PATHLEN-1;
      log_error("my_strcpy n=%zu MAX_PATHLEN\n",n);
    }
    strncat(dst,src,n);
  }
  return dst;
}
static void recursive_mkdir(char *p){
  const int n=pathlen_ignore_trailing_slash(p);
  for(int i=2;i<n;i++){
    if (p[i]=='/') {
      p[i]=0;
      mkdir(p,S_IRWXU);
      p[i]='/';
    }
  }
  mkdir(p,S_IRWXU);
}
int exceeds_max_path(int need_len,const char *path){
  if (need_len>MAX_PATHLEN){
    log_warn("Path length=%d>MAX_PATHLEN=%d  %s \n"ANSI_RESET,need_len,MAX_PATHLEN,path);
    return ENAMETOOLONG;
  }
  return 0;
}
void clear_stat(struct stat *st){ if(st) memset(st,0,sizeof(struct stat));}
void stat_set_dir(struct stat *s){
  if(s){
    mode_t *m= &(s->st_mode);
    assert(S_IROTH>>2==S_IXOTH);
    s->st_size=ST_BLKSIZE;
    s->st_nlink=1;
    *m=(*m&~S_IFMT)|S_IFDIR|((*m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
  }
}
//////////////////////////////////////////////////////////////////////////
// The struct zippath is used to identify the real path
// from the virtual path
#define MASK_PERMISSIONS  ((1<<12)-1)
const char *zpath_zipentry(struct zippath *zpath){
  if (!zpath) return NULL;
  const char *v=zpath->virtualpath;
  size_t l=zpath->len_virtual_zipfile;
  return strlen(v)>l?v+l+1:"";
}
#define ZPATH_IS_ZIP() (zpath->len_virtual_zipfile)
#define LOG_FILE_STAT() log_file_stat(zpath->virtualpath,&zpath->stbuf)
#define VP() zpath->virtualpath
#define RP() zpath->realpath
#define LEN_VP() zpath->len_virtual_zipfile
#define ZPATH_REMAINING() my_strlen(VP())-zpath->len_virtual_zipfile
#define NEW_ZIPPATH(virtpath)  struct zippath zp={0}, *zpath=&zp; VP()=virtpath
#define FIND_REAL(virtpath)  NEW_ZIPPATH(virtpath); if (!strcmp(virtpath+strlen(virtpath)-6,"run.sh")) zpath->flags|=ZP_DEBUG;  res=realpath_or_zip_any_root(zpath)
#define FREE_THEN_SET_NULL(a) free(a),a=NULL
char *zpath_ensure_path_capacity(struct zippath *zpath,int n){
  if (n>=zpath->realpath_max){
    FREE_THEN_SET_NULL(RP());
    RP()=malloc(zpath->realpath_max=n+10);
  }
  return RP();
}
void zpath_log(char *msg, struct zippath *zpath){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf(" virtualpath="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte \n",VP(),my_strlen(VP()));
  printf("    realpath="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte dir="ANSI_FG_BLUE"%s"ANSI_RESET"\n",RP(),my_strlen(RP()),yes_no(S_ISDIR(zpath->stbuf.st_mode)));
  printf("  entry_path="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte\n",zpath->entry_path,my_strlen(zpath->entry_path));
  printf(" len_virtual="ANSI_FG_BLUE"%d"ANSI_RESET"   ZIP %s"ANSI_RESET"\n",zpath->len_virtual_zipfile,  zpath->zarchive? ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}
void zpath_reset(struct zippath *zpath){
  if (zpath){
    struct zip *z=zpath->zarchive;
    if (z && zip_close(z)==-1) zpath_log(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,zpath);
    zpath->zarchive=NULL;
    if (zpath->zarchive_fd) my_close_fh(zpath->zarchive_fd);
    zpath->zarchive_fd=0;
    FREE_THEN_SET_NULL(RP());
    FREE_THEN_SET_NULL(zpath->entry_path);
    FREE_THEN_SET_NULL(zpath->virtualpath_without_entry);
    zpath->realpath_max=LEN_VP()=0;
    clear_stat(&zpath->stbuf);
  }
}
void zpath_destroy(struct zippath *zpath){
  if (zpath){
    zpath_reset(zpath);
    memset(zpath,0,sizeof(struct zippath));
  }
}
int zpath_stat(struct zippath *zpath){
  if (zpath->stbuf.st_ino) return 0;
  return stat(RP(),&zpath->stbuf);
}
int zpath_zip_open(struct zippath *zpath){
  //  log_entered_function("zpath_zip_open %s len_virtual_zipfile=%d %p \n",RP(),ZPATH_IS_ZIP(),zpath->zarchive);
  if (zpath && !zpath->zarchive){
    //log_msg("zpath_zip_open zpath=%p ",zpath);
    int err, fd=zpath->zarchive_fd=my_open_fh("zpath_zip_open ",RP(),O_RDONLY);
    /* O_DIRECT results in Error "No such file or directory" */
    if(fd<0 || !(zpath->zarchive=zip_fdopen(fd,0,&err))) {
      log_error("zip_open(%s)  %d\n",RP(),err);
      return -1;
    }
  }
  return 0;
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
// Given virtual path, search for real path
//
////////////////////////////////////////////////////////////
//
// Iterate over all _root to construct the real path;
// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
static int real_file(struct zippath *zpath, int root){
  assert(_root[root]!=NULL);
  int res=ENOENT;
  if (*_root[root]){ /* The first root which is writable can be empty */
    char *vp=zpath->virtualpath_without_entry;
    if (!vp) vp=(char*)VP();
    //log_entered_function("real_file %s root=%s\n",vp,root);
    if (*vp=='/' && vp[1]==0){
      strcpy(zpath_ensure_path_capacity(zpath,my_strlen(_root[root])),_root[root]);
    }else{
      zpath_ensure_path_capacity(zpath,my_strlen(vp)+my_strlen(_root[root])+1);
      strcpy(RP(),_root[root]);
      strcat(RP(),vp);
    }
    res=zpath_stat(zpath);
    //  if (res) log_msg("_real_file %s res=%d\n",vp,res);  else log_msg("real_file %s ->%s \n",vp,RP());
    //log_exited_function("real_file\n");
  }
  return res;
}
static int realpath_or_zip(struct zippath *zpath, int root){
  int res=1;
  char *append="";
  if (LEN_VP()=zip_contained_in_virtual_path(VP(),&append)){
    if (!zpath->virtualpath_without_entry){
      const int mem=LEN_VP()+my_strlen(append)+1;
      if (exceeds_max_path(mem,VP())) return ENAMETOOLONG;
      zpath->virtualpath_without_entry=strcat(my_strcpy(malloc(mem),VP(),LEN_VP()),append);
    }
    res=real_file(zpath,root);
    if (!res && ZPATH_IS_ZIP()) return read_zipdir(zpath,root,NULL,NULL,NULL);
  }
  if (res)
    zpath_reset(zpath);
  res=real_file(zpath,root);
  //log_exited_function("realpath_or_zip");
  return res;
}
int realpath_or_zip_any_root(struct zippath *zpath){
  int res=-1;
  for(int i=0;i<_root_n;i++){
    assert(_root[i]!=NULL);
    if (!(res=realpath_or_zip(zpath,i))) break;
    zpath_reset(zpath);
  }
  return res;
}
static void usage(){
  log_msg("\
Usage:  ZIPsFS -s -f root-dir1 root-dir2 ... root-dir-n  mountPoint\n\n\
The first root-dir1 is writable, the others read-only.\n\n");
  log_msg("Caching zip-entries in RAM is controled by the option -c [");

  for(char **s=WHEN_CACHE_S; *s; s++){
    if (s!=WHEN_CACHE_S) putchar('|');
    prints(*s);
  }
  log_msg("]\n");

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
static pthread_mutex_t _mutex_dir, _mutex_fhdata; // pthread_mutexattr_settype(&Attr, PTHREAD_MUTEX_RECURSIVE);
static const struct fhdata  FHDATA_EMPTY={0};//.fh=0,.offset=0,.file=NULL};
int fhdata_zip_open(struct fhdata *d,char *msg){
  zip_file_t *zf=d->zip_file;
  if (zf && !zip_ftell(zf)) return 0;
  fhdata_zip_fclose(d,"fhdata_zip_open");
  struct zip *za=d->zpath.zarchive;
  //log_msg("fhdata_zip_open %s\n",msg);
  if(za &&  (d->zip_file=zip_fopen(za,zpath_zipentry(&d->zpath),0))!=0) return 0;
  return -1;
}

void fhdata_zip_fclose(struct fhdata *d,char *msg){
  if (d) {
    if (d->zip_file){
      zip_fclose(d->zip_file);
      log_debug_now("Going to zip_fclose %s %p  \n",msg,d->zip_file);
    }
    d->zip_file=NULL;
  }
}
static struct fhdata* fhdata(enum data_op op,const char *path,struct fuse_file_info *fi){
  if (!fi) return NULL;
  uint64_t fh=fi->fh;
  //log_msg(ANSI_FG_GRAY" fhdata %d  %lu\n"ANSI_RESET,op,fh);
  uint64_t hash=hash_key(path);
  struct fhdata *d;
  for(int i=_fhdata_n;--i>=0;){
    d=&_fhdata[i];
    if (fh==d->fh && strcmp(path,d->path)) log_debug_now("fh reicht nicht\n");
    if (fh==d->fh && d->path_hash==hash && !strcmp(path,d->path)){
      if(op==RELEASE){
        log_msg(ANSI_FG_RED"Release fhdata %lu\n"ANSI_RESET,fh);
        fhdata_zip_fclose(d,"RELEASE");
        zpath_destroy(&d->zpath);
        cache_zip_entry(RELEASE,d);
        FREE_THEN_SET_NULL(d->path);
        for(int j=i+1;j<_fhdata_n;j++) _fhdata[j-1]=_fhdata[j];
        _fhdata_n--;
      }
      return d;
    }
  }
  if (op==CREATE && _fhdata_n<FHDATA_MAX){
    //log_msg(ANSI_FG_GREEN"New fhdata %lu\n"ANSI_RESET,fh);
    d=_fhdata+_fhdata_n;
    memset(d,0,sizeof(struct fhdata));
    d->fh=fh;
    d->path=strdup(path);
    d->path_hash=hash;;
    return _fhdata+_fhdata_n++;
  }
  return NULL;
}
#define fhdata_synchronized(op,path,fi) pthread_mutex_lock(&_mutex_fhdata); struct fhdata* d=fhdata(op,path,fi);  pthread_mutex_unlock(&_mutex_fhdata);

/* static struct fhdata* fhdata_synchronized(data_op op,const char *path,struct fuse_file_info *fi){ */
/*   pthread_mutex_lock(&_mutex_fhdata); */
/*   struct fhdata* d=fhdata(op,path,fi); */
/*   pthread_mutex_unlock(&_mutex_fhdata); */
/* } */
static int realpath_mk_parent(char *realpath,const char *path){
  //log_entered_function("realpath_mk_parent %s\n",path);
  if (!*_root[0]) return EACCES;/* Only first root is writable */
  const int slash=last_slash(path);
  //log_entered_function(" realpath_mk_parent(%s) slash=%d  \n  ",path,slash);
  if (slash>0){
    int res=0;
    char *parent=strndup(path,slash);
    FIND_REAL(parent);
    if (!res){
      strcpy(realpath,RP());
      strncat(strcpy(realpath,_root[0]),parent,MAX_PATHLEN);
      recursive_mkdir(realpath);
    }
    free(parent);
    zpath_destroy(zpath);
    if (res) return ENOENT;
  }
  strncat(strcpy(realpath,_root[0]),path,MAX_PATHLEN);
  return 0;
  //log_debug_now("realpath_mk_parent %s %s \n",_root[0],path);

  //return res?errno:0;
}
int read_zipdir(struct zippath *zpath, int root,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  int res=0;
  const int remaining=ZPATH_REMAINING();
  if (IS_ZPATH_DEBUG())   log_entered_function("read_zipdir rp=%s filler=%p vp=%s  len_virtual_zipfile=%d   remaining=%d\n",RP(),filler_maybe_null,VP(),LEN_VP(),remaining);
  if (remaining<0){ /* No Zipfile */
    //log_error("realpath=%s  remaining=%d   virtualpath=%s len_virtual_zipfile=%d\n",RP(),remaining, VP(), ZPATH_IS_ZIP());
    res=-1;
  }else if(remaining==0 && !filler_maybe_null){ /* The virtual path is a Zip file */
    return 0; /* Just report success */
  }else{
    if (zpath_stat(zpath)) res=ENOENT;
    else if (!zpath_zip_open(zpath)){ /* The virtual path is a Zip file with zip-entry */
      struct zip_stat sb;
      char s[MAX_PATHLEN],n[MAX_PATHLEN];
      const int len_ze=strlen(zpath_zipentry(zpath)), n_entries=zip_get_num_entries(zpath->zarchive,0);
      //log_debug_now(ANSI_INVERSE"read_zipdir"ANSI_RESET"  n_entries=%d\n",n_entries);
      for(int i=0,count=0; i<n_entries; i++){
        if (zip_stat_index(zpath->zarchive,i,0,&sb)) continue;
        int len=my_strlen(sb.name), is_dir=n[len-1]=='/',not_at_the_first_pass=0;
        if (len>=MAX_PATHLEN) { log_warn("Exceed MAX_PATHLEN: %s\n",sb.name); continue;}
        strcpy(n,sb.name);
        if (is_dir) n[len--]=0;
        while(len){
          if (not_at_the_first_pass++){ /* To get all dirs, and parent dirs successively remove last path component. */
            const int slash=last_slash(n);
            if (slash<0) break;
            n[slash]=0;
            is_dir=1;
          }
          if (!(len=my_strlen(n))) break;
          count++;
          if (!filler_maybe_null){
            /* read_zipdir() has been called from realpath_or_zip()  */
            if (len_ze==len && !strncmp(zpath_zipentry(zpath),n,len)){
              struct stat *st=&zpath->stbuf;
              st->st_size=sb.size;
              if (is_dir && !(zpath->flags&ZP_NOT_SET_S_IFDIR))  stat_set_dir(st);
              st->st_ino^=(ADD_INODE_E(count)|ADD_INODE_ROOT());
              st->st_blocks=(st->st_size+511)/512;
              st->st_nlink=1;
              //log_debug_now(ANSI_RESET"  sb.size=%lu ",sb.size);
              return 0;
            }
          }else{
            if (len<remaining || slash_not_trailing(n+remaining)>0) continue;
            {
              const char *q=(char*)n+remaining;
              my_strcpy(s,q,strchrnul(q,'/')-q);
            }
            if (ht_set(no_dups,s,"")) continue;
            //log_debug_now("Vor strncmp(%s,%s,%d) \n",zpath_zipentry(zpath),n,len_ze  );
            if (!*s || len<len_ze||strncmp(zpath_zipentry(zpath),n,len_ze)) continue;
            //log_debug_now("VP=%s  n=%s  s=%s\n",VP(), n,s);
            if (slash_not_trailing(n+len_ze+1)>=0) continue;
            struct stat st=zpath->stbuf;
            st.st_size=sb.size;
            //log_debug_now("path=%s   sb.size=%lu",VP(),sb.size);
            if (is_dir) stat_set_dir(&st);
            st.st_ino=zpath->stbuf.st_ino^(ADD_INODE_E(count)|ADD_INODE_ROOT());
            st.st_blksize=ST_BLKSIZE;
            st.st_blocks=(st.st_size+511)/512;
            st.st_nlink=1;
            //log_debug_now(ANSI_GREEN"filler"ANSI_RESET" s=%s%s"ANSI_RESET" size=%lu is_dir=%d  ",strcmp("run.sh",s)?"":ANSI_RED,s,sb.size,is_dir); log_file_stat("",&st);
            filler_maybe_null(buf,s,&st,0,fill_dir_plus);
          }
        }// while len
      }
    }
  }
  return filler_maybe_null? res:ENOENT;
}

static int impl_readdir(struct zippath *zpath,int root, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  const int is_zip=strcasestr(rp,".zip")>0;
  //log_entered_function("impl_readdir '%s\n",rp);
  //LOG_FILE_STAT();
  if (!rp || !*rp) return 0;
  pthread_mutex_lock(&_mutex_dir);
  int res=0;
  if (ZPATH_IS_ZIP()){
    read_zipdir(zpath,root,buf,filler,no_dups);
  }else{
    if (!zpath_stat(zpath)){
      struct stat st;
      DIR *dir=opendir(rp);
      if(dir==NULL){
        perror("Unable to read directory");
        res=ENOMEM;
      }else{
        char *append="";
        struct dirent *de;
        while((de=readdir(dir))){
          char *n=de->d_name;
          if (empty_dot_dotdot(n) || ht_set(no_dups,n,"")) continue;
          clear_stat(&st);
          st.st_ino=de->d_ino|ADD_INODE_ROOT();
          int t=de->d_type<<12;
          if (zip_contained_in_virtual_path(n,&append)) t=S_IFDIR;
          st.st_mode=t;
          n=real_name_to_display_name(n);
          if(ZPATH_IS_ZIP())  st.st_mode|=(S_IXUSR|S_IXGRP);
          filler(buf,n,&st,0,fill_dir_plus);
        }
        closedir(dir);
        if (!root && !strcmp(VP(),"/")){
          //simple_stat(&st);
          filler(buf,FILE_FS_INFO+1,NULL,0,fill_dir_plus);
        }
      }
    }
  }

  pthread_mutex_unlock(&_mutex_dir);
  //log_exited_function("realpath_readdir %d\n",res);
  return res;
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
  if (is_skip_file_name(path)) return -ENOENT;
  if (!strcmp(path,FILE_FS_INFO)){
    clear_stat(stbuf);
    stbuf->st_mode=S_IFREG|0444;
    stbuf->st_nlink=1;
    time(&stbuf->st_mtime);
    stbuf->st_size=MAX_INFO;
    return 0;
  }
  fhdata_synchronized(GET,path,fi);
  if (d && d->stat.st_ino){
    *stbuf=d->stat;
    return 0;
  }
  static int count=0;
  int res;
  FIND_REAL(path);
  //log_entered_function("%d xmp_getattr %s fh=%lu FIND_REAL=%d\n",count++,path, fi!=NULL?fi->fh:0,res);
  if(!res){
    *stbuf=zpath->stbuf;
    if (ZPATH_REMAINING()==0) stat_set_dir(stbuf);
    //log_exited_function("xmp_getattr %s res=%d is_debug=%d   ",path,res, (zpath->flags&ZP_DEBUG)); log_file_stat("",stbuf);
    if (d) d->stat=*stbuf;
  }
  zpath_destroy(zpath);
  return res==-1?-ENOENT:-res;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  int res;
  FIND_REAL(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_unlink(const char *path){
  int res;
  FIND_REAL(path);
  if (!res) res=unlink(RP());
  zpath_destroy(zpath);
  return res==-1?-errno:-res;
}
static int xmp_rmdir(const char *path){
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
    uint64_t h=d->path_hash;
    for(int i=_fhdata_n; --i>=0;){
      if(d!=_fhdata+i && _fhdata[i].path_hash==h  && !strcmp(d->path,_fhdata[i].path)){
        c=d->cache=_fhdata[i].cache;
        d->cache_len=_fhdata[i].cache_len;
        break;
      }
    }
    if (c){log_cache(ANSI_FG_GREEN"Found cache in other record %p\n"ANSI_RESET,d->cache);
    }
  }
  switch(op){
  case CREATE:
    if (!c){
      const size_t len=d->zpath.stbuf.st_size;
      log_cache(ANSI_RED"Going to cache %s %'zu Bytes"ANSI_RESET"\n",d->path,len);
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
        zip_int64_t already=0, n;
        int count=0;
        while(already<len){
          n=zip_fread(d->zip_file,bb+already,len-already);
          log_cache("Read %'zu\n",n);
          if (n<0){
            log_error("cache_zip_entry_in_RAM %s\n  read %'zu/%'zu",d->path,already,len);
            break;
          }
          already+=n;
          count++;
        }
        d->cache_len=already;
        //log_succes("Bulk read zip entry %s in %'lu seconds in %d shunks\n",d->path,time_ms()-start, count);
        d->cache_read_sec=time_ms()-start;
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
      if (!need_cache_zip_entry(d->zpath.virtualpath)); return false;
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
  //log_entered_function("xmp_open %s\n",path);
  static uint64_t _next_fh=FH_ZIP_MIN;
  int res,handle=0;
  FIND_REAL(path);
  //zpath_log("xmp_open",&zpath);
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
      d->zpath=*zpath;zpath=NULL;
      if (!maybe_cache_zip_entry(CREATE,d,false)) fhdata_zip_open(d,"xmp_open");
    }else{
      //log_debug_now(" Going to open(%s) \n",RP());
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
  int res_all=0;
  (void) offset;
  (void) fi;
  (void) flags;
  struct ht *no_dups=ht_create(7);
  for(int i=0;i<_root_n;i++){
    //int res;
    NEW_ZIPPATH(path);
    assert(_root[i]!=NULL);
    zpath->flags|=ZP_NOT_SET_S_IFDIR;
    realpath_or_zip(zpath,i);
    impl_readdir(zpath,i,buf,filler,no_dups);
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
  //log_debug_now("xmp_mkdir %s res=%d\n",realpath,res);
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
  char old[MAX_PATHLEN],neu[MAX_PATHLEN];
  strcpy(old,_root[0]);
  strcat(old,old_path);
  strcpy(neu,_root[0]);
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
  //if (_count_xmp_read++%1000==0) log_entered_function(ANSI_BLUE"%d  xmp_read"ANSI_RESET" %s size=%zu offset=%'lu %p  fh=%lu\n",_count_xmp_read,path,size ,offset, d,fi==NULL?0:fi->fh);
  int res=0;
  long diff;
  //log_debug_now("xmp_read  fh=%lu d=%p\n",fi->fh, d);
  if((res=maybe_read_from_cache(d,buf,size,offset,false))>=0){
    _count_read_zip_cached++;
    return res;
    //log_cache("xmp_read %s  cache=%p size=%'zu\n",path,cache,size);
  }else if (d && d->zip_file){
    _count_read_zip_regular++;
    // offset>d: Need to skip data.   offset<d  means we need seek backward
    diff=offset-zip_ftell(d->zip_file);

    if (diff && zip_file_is_seekable(d->zip_file)){
      //log_seek_ZIP(diff,"%s zip_file_is_seekable\n",path);
      if (zip_fseek(d->zip_file,offset,SEEK_SET)<0) return -1;
      _count_read_zip_seekable++;
    }else if (diff<0){ // Worst case
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
    //log_debug_now("xmp_read fhdata found");
    zip_int64_t n=zip_fread(d->zip_file, buf, size);

    return n;
  }else{
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
    if (diff=offset-lseek(fd,0,SEEK_CUR)){
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
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  log_entered_function("xmp_lseek %s  %lu %s ",path,off,whence==SEEK_SET?"SEEK_SET":whence==SEEK_CUR?"SEEK_CUR":whence==SEEK_END?"SEEK_END":"?");
  int fd;
  off_t res;
  assert(fi!=NULL);
  if (fi==NULL){
    FIND_REAL(path);
    if (res) return -1;
    fd=open(RP(),O_RDONLY);
    zpath_destroy(zpath);
  }else fd=fi->fh;
  if (fd==-1) return -errno;
  res=lseek(fd,off,whence);
  if (res==-1) res=-errno;
  if (fi==NULL) close(fd);
  return res;
}
/////////////////////////////////////////////////
//
// These are kept unchanged from passthrough example
static int xmp_release(const char *path, struct fuse_file_info *fi){
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
  signal(SIGSEGV,handler);
  {
    pthread_mutexattr_t Attr;
    pthread_mutexattr_init(&Attr);
    pthread_mutexattr_settype(&Attr,PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_mutex_fhdata,&Attr);
    pthread_mutex_init(&_mutex_dir,NULL);
  }

  //  raise(SIGSEGV);
  //  printf("EOF=%d  \n",EOF);exit(9);
  /* { */
  /* char *n=strdup("2023_my_data_zip_1.d.Zip"); */
  /* real_name_to_display_name(n); */
  /* puts(n); */
  /* exit(9); */
  /* } */
  /*{
    struct stat st={0};
    char *f="/home/cgille/tmp/txxxxxxxxxxx.txt";
    stat(f,&st);
    printf("xxxxxxxxxxx ");log_file_stat(f,&st);
    //log_statvfs("/");
    exit(9);
    }*/
  //  printf("'%s'",real_name_to_virtual_name(strdup("2023_hello.d.Zip")));exit(9);
  //char *uninitialized;  printf("%p",uninitialized);
  // printf("sizeof=%lu\n",sizeof( ino_t));    exit(9);
  /*
    {
    char *append[1];
    int res=0,after_zip=zip_contained_in_virtual_path(argv[1],append);
    log_debug_now(" after_zip=%d   append=%s \n",after_zip,*append);
    return 0;
    }
  */
  /*
    struct stat stbuf;
    int res= xmp_getattr("/test_zip/20230202_my_data_.d.Zip/test.bib", &stbuf,NULL);
    log_debug_now("res=%d\n",res);
    mode_t m=stbuf.st_mode;
    log_debug_now("S_ISDIR=%d\n",S_ISDIR(m));
    log_debug_now("S_ISREG=%d\n",S_ISREG(m));
    log_debug_now("m=%d\n",m);
    exit(9);
  */
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr);
  //S(access);
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
  if ((getuid()==0) || (geteuid()==0)){ log_msg("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  char *argv_fuse[9]={0};
  int c,argc_fuse=1;
  argv_fuse[0]=argv[0];
  while((c=getopt(argc,argv,"o:sfdhc:l:"))!=-1){
    switch(c){
    case 'h': usage();break;
    case 'l': {
      rlim_t  megab=atol(optarg);
      _rlimit.rlim_cur=megab<<20;
      _rlimit.rlim_max=megab<<20;
      setrlimit(RLIMIT_AS,&_rlimit);
      // getrlimit
    } break;
    case 'c': {
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
  log_msg("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  if (MAX_PATHLEN>PATH_MAX) log_abort(" MAX_PATHLEN (%d)>PATH_MAX (%d)  \n",MAX_PATHLEN,PATH_MAX);
  log_msg("MAX_PATHLEN=%d \n",MAX_PATHLEN);
  if (argc-optind<2) {usage();abort();}
  argv_fuse[argc_fuse++]=argv[argc-1]; // last is the mount point
  _root_feature[0]=ROOT_WRITABLE;
  _root_descript[0]=" (writable)";
  for(int i=optind;i<argc-1;i++){
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    char *r=argv[i];
    if (i && !*r && _root_n) continue; /* Accept empty String for _root[0] */
    int slashes=-1;
    while(r[++slashes]=='/');
    if (slashes>1){
      _root_feature[_root_n]|=ROOT_REMOTE;
      _root_descript[_root_n]=" (Remote)";
      r+=(slashes-1);
    }
    _root[_root_n++]=realpath(r,NULL);
  }
  if (!_root_n) log_abort("Missing root directories\n");
  log_msg("about to call fuse_main\n");
  log_strings("fuse argv",argv_fuse,argc_fuse,NULL);
  log_strings("root",_root,_root_n,_root_descript);
  int fuse_stat=fuse_main(argc_fuse,argv_fuse, &xmp_oper,NULL);
  log_msg("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
