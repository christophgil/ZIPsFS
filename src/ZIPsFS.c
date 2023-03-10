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
#include <sys/time.h>
#include <fuse.h>
#define fill_dir_plus 0
#include <zip.h>
#include <assert.h>
#define DEBUG_NOW 1
#define SHIFT_INODE_ROOT 40
#define SHIFT_INODE_ZIPENTRY 43

#define ADD_INODE_ROOT() (((long)root+1)<<SHIFT_INODE_ROOT)
#define ADD_INODE_E(i) (((long)i+1)<<SHIFT_INODE_ZIPENTRY)

#define WITH_HASH_TABLE 1
#define WITH_ZIP 1
#include "configuration.h"
unsigned int my_strlen(const char *s){ return !s?0:strnlen(s,MAX_PATHLEN);}
int _fh_data_n=0;
#include "log.h"
#include "ht.c"
#define ST_BLKSIZE 4096
///////////////////////////////////////////////////////////
//
// The root directories are specified as program arguments
// The _root[0] is read/write, and can be empty string
// The others are read-only
//
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
int _root_n=0, _root_feature[ROOTS];
char *_root[ROOTS];
// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
///////////////////////////////////////////////////////////
//
// utils
#define MIN(X,Y) (((X)<(Y))?(X):(Y))
#define ZP_DEBUG (1<<1)
#define ZP_NOT_SET_S_IFDIR (1<<2)
#define IS_ZPATH_DEBUG() (zpath->flags&ZP_DEBUG)
struct zip_path{
  const char *virtualpath;
  char *virtualpath_without_entry;
  int realpath_capacity;
  char *realpath;
  char *entry_path;
  int len_virtual_zip_filepath;
  struct stat stbuf;
  struct zip *zarchive;
  unsigned int flags;
};

int empty_dot_dotdot(const char *s){
  return !*s || *s=='.' && (!s[1] || s[1]=='.' && !s[2]);
}
static int last_slash(const char *path){
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
  return n && p[n-1]=='/' ? n-1:n;
}
char *my_strcpy(char *dst,const char *src, size_t n){ /* Beware strncpy terminal 0 */
  *dst=0;
  if (src){
    if (n>=MAX_PATHLEN){
      n=MAX_PATHLEN-strlen(src)-1;
      log_error("my_strcpy n=%lu MAX_PATHLEN\n",n);
    }
    strncat(dst,src,n);
  }
  return dst;
}
static void recursive_mkdir(char *p){
  const int n=pathlen_ignore_trailing_slash(p);
  for(int i=0;i<n;i++){
    if (p[i]=='/') {
      p[i]=0;
      mkdir(p+i,S_IRWXU);
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
    s->st_nlink=0;
    *m=(*m&~S_IFMT)|S_IFDIR|((*m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
  }
}

#define MASK_PERMISSIONS  ((1<<12)-1)

#define ZPATH_ZIPENTRY() (zpath->virtualpath+zpath->len_virtual_zip_filepath+1)
#define ZPATH_IS_ZIP() (zpath->len_virtual_zip_filepath)
#define LOG_FILE_STAT() log_file_stat(zpath->virtualpath,&zpath->stbuf)
#define VP() zpath->virtualpath
#define RP() zpath->realpath
#define LEN_VP() zpath->len_virtual_zip_filepath
#define ZPATH_REMAINING() my_strlen(VP())-zpath->len_virtual_zip_filepath

#define NEW_ZIP_PATH(virtpath)  struct zip_path zp={0}, *zpath=&zp; VP()=virtpath
#define FIND_REAL(virtpath)  NEW_ZIP_PATH(virtpath); if (!strcmp(virtpath+strlen(virtpath)-6,"run.sh")) zpath->flags|=ZP_DEBUG;  res=realpath_or_zip_any_root(zpath)
#define FREE_THEN_SET_NULL(a) free(a);a=NULL
char *ensure_path_capacity(struct zip_path *zpath,int n){
  if (n>=zpath->realpath_capacity){
    FREE_THEN_SET_NULL(RP());
    RP()=malloc(zpath->realpath_capacity=n+10);
  }
  return RP();
}
void log_zip_path(char *msg, struct zip_path *zpath){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf(" virtualpath="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte \n",VP(),my_strlen(VP()));
  printf("    realpath="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte dir="ANSI_FG_BLUE"%s"ANSI_RESET"\n",RP(),my_strlen(RP()),yes_no(S_ISDIR(zpath->stbuf.st_mode)));
  printf("  entry_path="ANSI_FG_BLUE"%s   %u"ANSI_RESET" byte\n",zpath->entry_path,my_strlen(zpath->entry_path));
  printf(" len_virtual="ANSI_FG_BLUE"%d"ANSI_RESET"   ZIP %s"ANSI_RESET"\n",zpath->len_virtual_zip_filepath,  zpath->zarchive? ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}
void my_zip_close(struct zip_path *zpath){
  struct zip *z=zpath->zarchive;
  if (z && zip_close(z)==-1) log_zip_path(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,zpath);
  zpath->zarchive=NULL;
}
void reset_zip_path(struct zip_path *zpath){
  my_zip_close(zpath);
  FREE_THEN_SET_NULL(RP());
  FREE_THEN_SET_NULL(zpath->entry_path);
  FREE_THEN_SET_NULL(zpath->virtualpath_without_entry);
  zpath->realpath_capacity=zpath->len_virtual_zip_filepath=0;
  clear_stat(&zpath->stbuf);
}
void destroy_zip_path(struct zip_path *zpath){
  reset_zip_path(zpath);
  memset(zpath,0,sizeof(struct zip_path));
}

int stat_once_only(struct zip_path *zpath){
  if (zpath->stbuf.st_ino) return 0;
  return stat(RP(),&zpath->stbuf);
}
int my_zip_open(struct zip_path *zpath){
  //  log_entered_function("my_zip_open %s len_virtual_zip_filepath=%d %p \n",RP(),ZPATH_IS_ZIP(),zpath->zarchive);
  if (!zpath || zpath->zarchive) return 0;
  int err;
  if(!(zpath->zarchive=zip_open(RP(),0,&err))) {
    log_error("zip_open(%s)  %d\n",RP(),err);
    return -1;
  }
  return 0;
}
#define debug_is_zip  int is_zip=(strcasestr(path,".zip")>0)
int zip_contained_in_virtual_path(const char *path, char *append[]){
  const int len=my_strlen(path);
  char *e,*b=(char*)path;
  if(append) *append="";
  for(int i=4;i<=len;i++){
    e=(char*)path+i;
    if (i==len || *e=='/'){
      if (e[-4]=='.' && (e[-3]|32)=='z' && (e[-2]|32)=='i' && (e[-1]|32)=='p') { return i; }
      if (b[0]=='2' && b[1]=='0' && e[-2]=='.' && (e[-1]|32)=='d') { if (append) *append=".Zip"; return i; }
      if (*e=='/')  b=e+1;
    }
  }
  return 0;
}
//2023_my_data_zip_1.d.Zip
char *real_name_to_display_name(char *n){
  const int len=my_strlen(n);
  if(len>9 && n[0]=='2' && n[1]=='0' && !strcmp(n+len-6,".d.Zip")) { n[len-4]=0; return n;}
  return n;
}
///////////////////////////////////////////////////////////
//
// When the same file is accessed from two different programs,
// We see different fi->fh
// Wee use this as a key to obtain a data structure "fh_data"
//
// One might think that : fuse_get_context()->private_data serves the same purpose.
// However, it returns always the same pointer address
//
pthread_mutex_t _mutex_dir, _mutex;

struct fh_data{
  unsigned long fh;
  struct zip *zarchive;
  zip_file_t *zip_file;
};
const struct fh_data  FH_DATA_EMPTY={0};//.fh=0,.offset=0,.file=NULL};
//void destroy_fh_data(struct fh_data *d){

#define FH_DATA_MAX 3333

enum fh_data_op{GET,CREATE,RELEASE};
struct fh_data _fh_data[FH_DATA_MAX];
static struct fh_data* fh_data(enum fh_data_op op,struct fuse_file_info *fi){
  uint64_t fh=fi->fh;
  //log_msg(ANSI_FG_GRAY" fh_data %d  %lu\n"ANSI_RESET,op,fh);
  for(int i=_fh_data_n;--i>=0;){
    //log_msg(ANSI_FG_GRAY"fh=%lu  [%d].fh=%ld "ANSI_RESET,fh,i,_fh_data[i].fh  );
    //if (fh==_fh_data[i].fh) log_succes(" "); else log_failed(" ");
    struct fh_data *d=&_fh_data[i];
    if (fh==_fh_data[i].fh){
      if(op==RELEASE){
        //log_msg(ANSI_FG_RED"Release fh_data %lu\n"ANSI_RESET,fh);
        if (d->zip_file) zip_fclose(d->zip_file);
        if (d->zarchive) zip_close(d->zarchive);
        for(int j=i+1;j<_fh_data_n;j++) _fh_data[j-1]=_fh_data[j];
        _fh_data_n--;
      }
      return d;
    }
  }
  if (op==CREATE && _fh_data_n<FH_DATA_MAX){
    //log_msg(ANSI_FG_GREEN"New fh_data %lu\n"ANSI_RESET,fh);
    memset(&_fh_data[_fh_data_n],0,sizeof(struct fh_data));
    _fh_data[_fh_data_n].fh=fh;;
    return &_fh_data[_fh_data_n++];
  }
  return NULL;
}
#define fh_data_synchronized(op,fi) pthread_mutex_lock(&_mutex); struct fh_data* d=fh_data(op,fi);  pthread_mutex_unlock(&_mutex);
////////////////////////////////////////////////////////////
//
// Iterate over all _root to construct the real path;
// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
static int real_file(struct zip_path *zpath, int root){
  assert(_root[root]!=NULL);
  int res=ENOENT;
  if (*_root[root]){ /* The first root which is writable can be empty */
    char *vp=zpath->virtualpath_without_entry;
    if (!vp) vp=(char*)VP();
    //log_entered_function("real_file %s root=%s\n",vp,root);
    if (*vp=='/' && vp[1]==0){
      strcpy(ensure_path_capacity(zpath,my_strlen(_root[root])),_root[root]);
    }else{
      ensure_path_capacity(zpath,my_strlen(vp)+my_strlen(_root[root])+1);
      strcpy(RP(),_root[root]);
      strcat(RP(),vp);
    }
    res=stat_once_only(zpath);
    //  if (res) log_msg("_real_file %s res=%d\n",vp,res);  else log_msg("real_file %s ->%s \n",vp,RP());
    //log_exited_function("real_file\n");
  }
  return res;
}
static char *realpath_mk_parent(const char *path,int *return_res){
  //log_entered_function("realpath_mk_parent %s\n",path);
  int mem,slash=last_slash(path);
  assert(slash>=0);
  if (!*_root[0]){ *return_res=EACCES; return NULL;} /* Only first root is writable */
  //log_entered_function(" realpath_mk_parent(%s) slash=%d  \n  ",path,slash);
  if (slash<0){
    return NULL;
  }else if (slash){
    char *parent=strndup(path,slash);
    NEW_ZIP_PATH(parent);
    int res;
    if (!(res=real_file(zpath,0))){
      free(parent);
      return RP();
    }
    for(int i=1;i<_root_n;i++){
      reset_zip_path(zpath);
      if (!(res=real_file(zpath,i))) break;
    }
    if (res){
      *return_res=ENOENT;
    }else if (!S_ISDIR(zpath->stbuf.st_mode)){
      *return_res=ENOTDIR;
    }else if (!(*return_res=exceeds_max_path(mem=my_strlen(_root[0])+my_strlen(path)+2,path))){

      char *d=malloc(mem);
      strcat(strcpy(d,_root[0]),parent);
      recursive_mkdir(d);
      //if(!is_dir(d))
      free(d);
      *return_res=0;
    }
    destroy_zip_path(zpath);
    free(parent);
  }
  return *return_res?NULL: strcat(strcpy(malloc(my_strlen(_root[0])+strlen(path)+1),_root[0]),path);
}
int read_zipdir(struct zip_path *zpath, int root,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups){
  int res=0;
  const int remaining=ZPATH_REMAINING();
  if (IS_ZPATH_DEBUG())   log_entered_function("read_zipdir rp=%s filler=%p vp=%s  len_virtual_zip_filepath=%d   remaining=%d\n",RP(),filler_maybe_null,VP(),LEN_VP(),remaining);
  if (remaining<0){ /* No Zipfile */
    //log_error("realpath=%s  remaining=%d   virtualpath=%s len_virtual_zip_filepath=%d\n",RP(),remaining, VP(), ZPATH_IS_ZIP());
    res=-1;
  }else if(remaining==0 && !filler_maybe_null){ /* The virtual path is a Zip file */
    return 0; /* Just report success */
  }else{
    if (stat_once_only(zpath)) res=ENOENT;
    else{
      if (!my_zip_open(zpath)){ /* The virtual path is a Zip file with zip-entry */
        struct zip_stat sb;
        char s[MAX_PATHLEN],n[MAX_PATHLEN];
        const int len_ze=strlen(ZPATH_ZIPENTRY()), n_entries=zip_get_num_entries(zpath->zarchive,0);
        //log_debug_now(ANSI_INVERSE"read_zipdir"ANSI_RESET"  n_entries=%d\n",n_entries);
        for(int i=0; i<n_entries; i++){
          if (zip_stat_index(zpath->zarchive,i,0,&sb)) continue;
          int len=my_strlen(sb.name), is_dir=n[len-1]=='/', count=0;
          if (len>=MAX_PATHLEN) { log_warn("Exceed MAX_PATHLEN: %s\n",sb.name); continue;}
          strcpy(n,sb.name);
          if (is_dir) n[len--]=0;
          while(len){
            if (count++){ // To get all dirs, successively remove last path component.
              int slash=last_slash(n);
              if (slash<0) break;
              n[slash]=0;
              is_dir=1;
              if (!(len=my_strlen(n))) break;
            }
            //log_debug_now("n=%s filler=%s\n",n,yes_no(!!filler_maybe_null));
            if (!filler_maybe_null){
              if (len_ze==len && !strncmp(ZPATH_ZIPENTRY(),n,len_ze)){
                /* read_zipdir() has been called from realpath_or_zip()  */
                struct stat *st=&zpath->stbuf;
                st->st_size=sb.size;
                if (is_dir && !(zpath->flags&ZP_NOT_SET_S_IFDIR))  stat_set_dir(st);
                st->st_ino|=ADD_INODE_E(count)|ADD_INODE_ROOT();
                st->st_blocks=(st->st_size+511)/512;
                st->st_nlink=0;
                //log_debug_now(ANSI_GREEN" Successfully"ANSI_RESET" returning from read_zipdir %s ",VP()); log_file_stat("",st);
                //log_debug_now(ANSI_RESET"  sb.size=%lu ",sb.size);
                return 0;
              }
            }else{
              if (len<remaining || slash_not_trailing(n+remaining)>0) continue;
#if WITH_HASH_TABLE
              if (ht_get(no_dups,n)) continue;
              ht_set(no_dups,n,"");
#endif
              {
                char *q=(char*)n+remaining;
                my_strcpy(s,q,strchrnul(q,'/')-q);
              }
              if (!strlen(s)) continue;
              if (len<len_ze||strncmp(ZPATH_ZIPENTRY(),n,len_ze)) continue;
              //log_debug_now("VP=%s  n=%s  s=%s\n",VP(), n,s);
              if (slash_not_trailing(n+len_ze+1)>=0) continue;
              struct stat st=zpath->stbuf;
              st.st_size=sb.size;
              //log_debug_now("path=%s   sb.size=%lu",VP(),sb.size);
              if (is_dir) stat_set_dir(&st);
              st.st_ino=zpath->stbuf.st_ino|ADD_INODE_E(count)|ADD_INODE_ROOT();
              st.st_blksize=ST_BLKSIZE;
              st.st_blocks=(st.st_size+511)/512;
              st.st_nlink=0;
              char *color=strcmp("run.sh",s)?"":ANSI_RED;
              //log_debug_now(ANSI_GREEN"filler"ANSI_RESET" s=%s%s"ANSI_RESET" size=%lu is_dir=%d  ",color,s,sb.size,is_dir); log_file_stat("",&st);
              filler_maybe_null(buf,s,&st,0,fill_dir_plus);
            }
          }// while len
        }
      }
    }
  }
  if (!filler_maybe_null) return ENOENT;
  return res;
}
static int impl_readdir(struct zip_path *zpath,int root, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  int is_zip=strcasestr(RP(),".zip")>0;
  // if (is_zip) { log_entered_function("impl_readdir '%s\n",RP()); LOG_FILE_STAT();}

  if (!RP() || !*RP()) return 0;
  pthread_mutex_lock(&_mutex_dir);
  int res=0;
  if (ZPATH_IS_ZIP()){
    read_zipdir(zpath,root,buf,filler,no_dups);
  }else{
    if (!stat_once_only(zpath)){
      struct stat st;
      DIR *dir=opendir(RP());
      if(dir==NULL){
        perror("Unable to read directory");
        res=ENOMEM;
      }else{
        char *append="";
        struct dirent *de;
        while((de=readdir(dir))){
          char *n=de->d_name;
          if (empty_dot_dotdot(n)) continue;
#if WITH_HASH_TABLE
          if (ht_get(no_dups,n)) continue;
          ht_set(no_dups,n,"");
#endif
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
      }
    }
  }
  pthread_mutex_unlock(&_mutex_dir);
  //log_exited_function("realpath_readdir %d\n",res);
  return res;
}
// ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ
//
static int realpath_or_zip(struct zip_path *zpath, int root){
  //if (IS_ZPATH_DEBUG()) log_entered_function("realpath_or_zip r=%s vp=%s \n",_root[root],VP());
  int is_zip=strcasestr(VP(),".zip")>0;
  //log_entered_function("realpath_or_zip %s   root=%s is_zip=%d\n",vp,root,is_zip);
  int res=1;
#if WITH_ZIP
  char *append="";
  if (zpath->len_virtual_zip_filepath=zip_contained_in_virtual_path(VP(),&append)){
    const int mem=zpath->len_virtual_zip_filepath+my_strlen(append)+1;
    if (!zpath->virtualpath_without_entry){
      if (exceeds_max_path(mem,VP())) return ENAMETOOLONG;
      zpath->virtualpath_without_entry=strcat(my_strcpy(malloc(mem),VP(),zpath->len_virtual_zip_filepath),append);
      //log_debug_now("virtualpath_without_entry=%s\n",zpath->virtualpath_without_entry);
    }
    res=real_file(zpath,root);
    if (!res && ZPATH_IS_ZIP()){
      return read_zipdir(zpath,root,NULL,NULL,NULL);
    }
  }
  if (res)
#endif
    reset_zip_path(zpath);
  res=real_file(zpath,root);
  //log_exited_function("realpath_or_zip");
  return res;
}
int realpath_or_zip_any_root(struct zip_path *zpath){
  int res=-1;
  for(int i=0;i<_root_n;i++){
    assert(_root[i]!=NULL);
    if (!(res=realpath_or_zip(zpath,i))) break;
    reset_zip_path(zpath);
  }
  return res;
}
static void usage(){
  log_msg("usage:  bbfs [FUSE and mount options] mountPoint\n");
  abort();
}
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg){
  (void) conn;
  cfg->use_ino=1;
  /* Pick up changes from lower filesystem right away. This is
     also necessary for better hardlink support. When the kernel
     calls the unlink() handler, it does not know the inode of
     the to-be-removed entry and can therefore not invalidate
     the cache of the associated inode - resulting in an
     incorrect st_nlink value being reported for any remaining
     hardlinks to this inode. */
  cfg->entry_timeout=0;
  cfg->attr_timeout=0;
  cfg->negative_timeout=0;
  return NULL;
}
/////////////////////////////////////////////////
//
// Functions where Only single paths need to be  substituted

static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi){
  static int count=0;
  debug_is_zip;
  //if (is_zip)
  log_entered_function("%d xmp_getattr %s\n",count++,path);
  (void) fi;
  int res;
  FIND_REAL(path);
  //log_debug_now("xmp_getattr %d ",res);
  if (res==-1) res=ENOENT;
  if(!res){
    *stbuf=zpath->stbuf;
    if (ZPATH_REMAINING()==0) stat_set_dir(stbuf);
    log_exited_function("xmp_getattr %s res=%d is_debug=%d   ",path,res, (zpath->flags&ZP_DEBUG)); log_file_stat("",stbuf);
  }else{
    log_exited_function("xmp_getattr Error %s res=%d\n",path,res);
  }
  destroy_zip_path(zpath);
  return res==-1?-errno:-res;
}
static int xmp_access(const char *path, int mask){
  log_entered_function("xmp_access %s\n",path);
  //  if (DEBUG_NOW==DEBUG_NOW) exit(1);
  int res;
  FIND_REAL(path);
  if (!res){
    if ((mask&X_OK) && S_ISDIR(zpath->stbuf.st_mode)) mask=(mask&~X_OK)|R_OK;
    res=access(RP(),mask);
  }
  destroy_zip_path(zpath);
  return res==-1?-errno:-res;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  int res;
  FIND_REAL(path);
  if (!res && (res=readlink(RP(),buf,size-1))!=-1) buf[res]=0;
  destroy_zip_path(zpath);
  return res==-1?-errno:-res;
}
static int xmp_unlink(const char *path){
  int res;
  FIND_REAL(path);
  if (!res) res=unlink(RP());
  destroy_zip_path(zpath);
  return res==-1?-errno:-res;
}
static int xmp_rmdir(const char *path){
  int res;
  FIND_REAL(path);
  if (!res) res=rmdir(RP());
  destroy_zip_path(zpath);
  return res==-1?-errno:-res;
}

static int xmp_open(const char *path, struct fuse_file_info *fi){
  log_entered_function("xmp_open %s\n",path);
  static uint64_t _next_fh=1<<20;
  int res,handle=0;
  FIND_REAL(path);
  //log_zip_path("xmp_open",&zpath);
  if (res){
    log_warn("xmp_open(%s) FIND_REAL res=%d\n",path,res);
  }else{
    if (ZPATH_IS_ZIP()){
      if (!zpath->zarchive){
        log_warn("In xmp_open %s: zpath->zarchive==NULL\n",path);
        return -ENOENT;
      }
      handle=fi->fh=_next_fh++;
      fh_data_synchronized(CREATE,fi);
      d->zarchive=zpath->zarchive;
      zpath->zarchive=NULL;
      d->zip_file=zip_fopen(d->zarchive, ZPATH_ZIPENTRY(), 0);
      //log_debug_now("zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz fh=%lu fh_data=%p zf=%p \n",fi->fh, d, d->zip_file);
    }else{
      log_debug_now(" Going to open(%s) \n",RP());
      handle=open(RP(),fi->flags);
    }
  }
  destroy_zip_path(zpath);
  log_exited_function("xmp_open %s handle=%d res=%d\n",path,handle,res);
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
    destroy_zip_path(zpath);
  }
  return res==-1?-errno:-res;
}
static int xmp_statfs(const char *path, struct statvfs *stbuf){
  log_entered_function("xmp_statfs %s\n",path);
  int res;
  FIND_REAL(path);
  if (!res) res=statvfs(RP(),stbuf);
  destroy_zip_path(zpath);
  log_exited_function("xmp_statfs %s res=%d\n",path,res);
  return res==-1?-errno:-res;
}
/////////////////////////////////
//
// Readdir
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  debug_is_zip;
  //  log_entered_function("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr xmp_readdir %s\n",path);
  int res_all=0;
  (void) offset;
  (void) fi;
  (void) flags;
  struct ht *no_dups=ht_create(99);
  for(int i=0;i<_root_n;i++){
    int res;
    NEW_ZIP_PATH(path);
    assert(_root[i]!=NULL);
    zpath->flags|=ZP_NOT_SET_S_IFDIR;
    realpath_or_zip(zpath,i);
    impl_readdir(zpath,i,buf,filler,no_dups);
    destroy_zip_path(zpath);
  }
#if WITH_HASH_TABLE
  ht_destroy(no_dups);
#endif
  return res_all;
}
/////////////////////////////////
//
// Creating a new file object
static int xmp_mkdir(const char *create_path, mode_t mode){
  log_entered_function("xmp_mkdir %s \n",create_path);
  int res=0;
  char *cpath=realpath_mk_parent(create_path,&res);
  if (cpath){
    log_debug_now("xmp_mkdir %s res=%d\n",cpath,res);
    res=mkdir(cpath,mode);
    if (res==-1) res=errno;
    free(cpath);
  }
  return -res;
}
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function(ANSI_YELLOW"==========xmp_create"ANSI_RESET" %s\n",create_path);
  int res=0;
  char *cpath=realpath_mk_parent(create_path,&res);
  log_debug_now("xxxxxxxxxxxxxxxxxxxxxxxx cpath=%s\n",cpath);
  if (cpath){
    res=open(cpath,fi->flags,mode);
    log_exited_function("xmp_create %s res=%d\n",create_path,res);
    if (res==-1) return -errno;
    fi->fh=res;
    free(cpath);
  }

  return 0;
}
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  log_entered_function(ANSI_YELLOW"========xmp_write"ANSI_RESET" %s  fi=%p \n",create_path,fi);
  int fd;
  int res=0;
  (void) fi;
  if(fi==NULL){
    char *cpath=realpath_mk_parent(create_path,&res);
    if (!cpath) return -ENOENT;
    fd=open(cpath,O_WRONLY);
    free(cpath);
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
  int res=0;
  char *cpath=realpath_mk_parent(create_path,&res);
  if(cpath){
    res=symlink(target,cpath);
    if (res==-1) return -errno;
    free(cpath);
  }
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
static int _count_xmp_read=0;
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
  assert(fi!=NULL);
  fh_data_synchronized(GET,fi);
  //if (_count_xmp_read++%100==0)
    log_entered_function(ANSI_BLUE"%d  xmp_read"ANSI_RESET" %s size=%lu offset=%lu %p \n",_count_xmp_read,path,size ,offset, d);
  int  res=0;
  long diff;
  //log_debug_now("xmp_read  fh=%lu d=%p\n",fi->fh, d);
  if (d && d->zip_file){
    // offset>d: Need to skip data.   offset<d  means we need seek backward
    diff=offset-zip_ftell(d->zip_file);
    if (diff && zip_file_is_seekable(d->zip_file)){
      log_seek_ZIP(diff,"%s zip_file_is_seekable\n",path);
      if (zip_fseek(d->zip_file,offset,SEEK_SET)<0) return -1;
    }else if (diff<0){ // Worst case
      log_seek_ZIP(diff,"%s Going to re-open\n",path);
      zip_fclose(d->zip_file);
      d->zip_file=NULL;
      FIND_REAL(path);
      if (!ZPATH_IS_ZIP()) return -1;
      d->zip_file==zip_fopen(d->zarchive, ZPATH_ZIPENTRY(), 0); //ZIP_FL_COMPRESSED
    }
    while (diff=offset-zip_ftell(d->zip_file)){
      log_seek_ZIP(diff,"%s Going to skip\n",path);
      if (zip_fread(d->zip_file,buf,MIN(size,diff))<0) return -1;
    }
    //log_debug_now("xmp_read fh_data found");
        puts("Davor Press enter"); getchar();
    zip_int64_t n=zip_fread(d->zip_file, buf, size);
    //log_debug_now("size=%lu  read  %lu Byte\n",size,n);
    puts("Danach Press enter"); getchar();
    log_exited_function("xmp_read zip\n");
    return n;
  }else{
    int fd;
    if(fi){
      fd=fi->fh;
    }else{
      FIND_REAL(path);
      fd=open(RP(),O_RDONLY);
      destroy_zip_path(zpath);
    }
    if (fd==-1) return -errno;
    if (diff=offset-lseek(fd,0,SEEK_CUR)){
      if (offset==lseek(fd,offset,SEEK_SET)){
        log_seek(offset,"lseek Success %s\n",path);
      }else{
        //log_seek(offset,"lseek Failed %s\n",path);
        return -1;
      }
    }
    res=pread(fd,buf,size,offset);
    if (res==-1) res=-errno;
    if (fi==NULL) close(fd);
    log_exited_function("xmp_read %s res=%d\n",path,res);
  }
      log_exited_function("xmp_read regfile\n");
  return res;
}
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  log_entered_function("xmp_lseek %s  %lu %s ",path,off,whence==SEEK_SET?"SEEK_SET":whence==SEEK_CUR?"SEEK_CUR":whence==SEEK_END?"SEEK_END":"?");
  int fd;
  off_t res;
  if (fi==NULL){
    FIND_REAL(path);
    fd=open(RP(),O_RDONLY);
    destroy_zip_path(zpath);
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
  log_entered_function("xmp_release %s\n",path);
  (void) path;
  fh_data_synchronized(RELEASE,fi);
  if (!d){
    close(fi->fh);
  }
  return 0;
}
static struct fuse_operations xmp_oper={0};
int main(int argc, char *argv[]){
  assert(S_IXOTH==(S_IROTH>>2));
  assert((1<<(SHIFT_INODE_ZIPENTRY-SHIFT_INODE_ROOT))>ROOTS);


  //  printf("MIN_BUFSIZE=%d   FUSE_MAXPAGES_PATH=%s \n",MIN_BUFSIZE,FUSE_MAXPAGES_PATH);
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
  S(statfs);
  S(release);
  S(lseek);
#undef S
  if ((getuid()==0) || (geteuid()==0)){ log_msg("Running BBFS as root opens unnacceptable security holes\n");return 1;}
  char *argv_fuse[9];
  int c,argc_fuse=1;
  argv_fuse[0]=argv[0];
  while((c=getopt(argc,argv,"sfdh"))!=-1){
    switch(c){
    case 'h': usage();break;
    case 's': argv_fuse[argc_fuse++]="-s"; break;
    case 'f': argv_fuse[argc_fuse++]="-f"; break;
    case 'd': argv_fuse[argc_fuse++]="-d";break;
    }
  }
  // See which version of fuse we're running
  log_msg("FUSE_MAJOR_VERSION=%d FUSE_MAJOR_VERSION=%d \n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
  if (MAX_PATHLEN>PATH_MAX) log_abort(" MAX_PATHLEN (%d)>PATH_MAX (%d)  \n",MAX_PATHLEN,PATH_MAX);
  log_msg("MAX_PATHLEN=%d \n",MAX_PATHLEN);
  if (argc-optind<2) {usage();abort();}
  argv_fuse[argc_fuse++]=argv[argc-1]; // last is the mount point
  argv_fuse[argc_fuse]=NULL;
  _root_feature[0]=ROOT_WRITABLE;
  char *descript[ROOTS]={0};
  descript[0]=ANSI_FG_GREEN" (writable)";
  for(int i=optind;i<argc-1;i++){
    if (_root_n>=ROOTS) log_abort("Exceeding max number of ROOTS=%d\n",ROOTS);
    char *r=argv[i];
    if (i && !*r && _root_n) continue; /* Accept empty String for _root[0] */
    int slashes=-1;
    while(r[++slashes]=='/');
    if (slashes>1){
      _root_feature[_root_n]|=ROOT_REMOTE;
      descript[_root_n]=ANSI_FG_GREEN" (Remote)";
      r+=(slashes-1);
    }
    _root[_root_n++]=realpath(r,NULL);
  }
  if (!_root_n) log_abort("Missing root directories\n");
  log_msg("about to call fuse_main\n");
  log_strings("fuse argv",argv_fuse,argc_fuse,NULL);
  log_strings("root",_root,_root_n,descript);
  if (0)
    if (!fork()){
      struct stat st;
      const char *file="/home/cgille/test/fuse/mnt";
      usleep(1000*1000);
      printf(ANSI_FG_MAGENTA"Going to stat(%s)\n"ANSI_RESET,file);
      stat(file,&st);
      log_file_stat(file,&st);
      exit(0);
    }
  int fuse_stat=fuse_main(argc_fuse,argv_fuse, &xmp_oper,NULL);
  log_msg("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
