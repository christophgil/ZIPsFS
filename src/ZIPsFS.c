/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS
  Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
*/
#define FUSE_USE_VERSION 31
#define _GNU_SOURCE
#ifdef linux
/* For pread()/pwrite()/utimensat() */
#define _XOPEN_SOURCE 700
#endif
#include <stdlib.h>
#include "config.h"
#include <fuse.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <pthread.h>
#include <dirent.h>
#include <errno.h>
#ifdef __FreeBSD__
#include <sys/socket.h>
#include <sys/un.h>
#endif
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <fuse.h>
#include <zip.h>

// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
///////////////////////////////////////////////////////////
//
// utils
#define DEBUG_NOW 1
#define MAX_PATHLEN 512
#define FILE_TYPE_DIR 1
enum file_type{dir,regfile,other};
struct zip_path{
  int path_capacity;
  char *path;
  enum file_type file_type;
  char *zipfile;
  struct zip_stat zstat;
  struct zip *zarchive;
};
unsigned int my_strlen(const char *s){ return !s?0:strnlen(s,MAX_PATHLEN);}
void prints(char *s){ if (s) fputs(s,stdout);}
static int last_slash(const char *path){
  int i;
  for(i=my_strlen(path);--i>=0;){
    if (path[i]=='/') return i;
  }
  return -1;
}
int pathlen_ignore_trailing_slash(const char *p){
  int n=my_strlen(p);
  if (n && p[n-1]=='/') n--;
  return n;
}
int is_regular_file(const char *path){
  struct stat path_stat;
  stat(path,&path_stat);
  return S_ISREG(path_stat.st_mode);
}
int is_dir(const char *path){
  struct stat path_stat;
  stat(path,&path_stat);
  return S_ISDIR(path_stat.st_mode);
}
static void recursive_mkdir(char *path) {
  int n=my_strlen(path);
  if (n<2) return;
  char *p=path+n-1;
  int res=0;
  if (*p=='/') *p=0;
  for (p=path+1;*p;p++){
    if (*p=='/') {
      *p=0;
      mkdir(path,S_IRWXU);
      *p='/';
    }
  }
  mkdir(path,S_IRWXU);
}
#include "log.h"
int exceeds_max_path(int need_len,const char *path){
  if (need_len>MAX_PATHLEN){
    log_warn("Path length=%d>MAX_PATHLEN=%d  %s \n"ANSI_RESET,need_len,MAX_PATHLEN,path);
    return ENAMETOOLONG;
  }
  return 0;
}

void my_zip_close(struct zip_path *zpath){
  struct zip *z=zpath->zarchive;
  if (z && zip_close(z)==-1) log_zip_path(ANSI_FG_RED"Can't close zip archive'/n"ANSI_RESET,*zpath);
  zpath->zarchive=NULL;
}
#define NEW_ZIP_PATH()  struct zip_path zpath;memset(&zpath,0,sizeof(zpath))
#define FIND_REAL()  NEW_ZIP_PATH();  res=real_file(&zpath,path)
void ensure_path_capacity(struct zip_path *zpath,int n){
  if (n>=zpath->path_capacity){
    free(zpath->path);
    zpath->path=malloc(zpath->path_capacity=n+10);
  }
}
void destroy_zip_path(struct zip_path *zpath){
  my_zip_close(zpath);
  free(zpath->zipfile);  zpath->zipfile=NULL;
  free(zpath->path);     zpath->path=NULL;
}

///////////////////////////////////////////////////////////
//
// When the same file is accessed from two different programs,
// We see different fi->fh
// Wee use this as a key to obtain a data structure "stream_data"
//
// One might think that : fuse_get_context()->private_data serves the same purpose.
// However, it returns always the same pointer address
//
pthread_mutex_t _mutex_dir, _mutex;
int _stream_data_n=0;
struct stream_data{
  long fh,offset;
  FILE *file;
};
const struct stream_data  STREAM_DATA_EMPTY={.fh=0,.offset=0,.file=NULL};
void stream_data_dispose(struct stream_data *d){
}
#define STREAM_DATA_MAX 1000
#define STREAM_DATA_GET 0
#define STREAM_DATA_CREATE 1
#define STREAM_DATA_RELEASE 2
struct stream_data _stream_data[STREAM_DATA_MAX];
static struct stream_data stream_data(int op,struct fuse_file_info *fi){
  int i,j;
  uint64_t fh=fi->fh;
  log_msg(ANSI_FG_GRAY" stream_data %d  %lu\n"ANSI_RESET,op,fh);
  for(i=_stream_data_n;--i>=0;){
    log_msg(ANSI_FG_GRAY"fh=%lu  [%d].fh=%lu "ANSI_RESET,fh,i,_stream_data[i].fh  );
    if (fh==_stream_data[i].fh) log_succes(" "); else log_failed(" ");
    if (fh==_stream_data[i].fh){
      struct stream_data d=_stream_data[i];
      if(op==STREAM_DATA_RELEASE){
        log_msg(ANSI_FG_RED"Release stream_data %lu\n"ANSI_RESET,fh);
        for(j=i+1;j<_stream_data_n;j++) _stream_data[j-1]=_stream_data[j];
        _stream_data_n--;
      }
      return d;
    }
  }
  if (op==STREAM_DATA_CREATE && _stream_data_n<STREAM_DATA_MAX){
    log_msg(ANSI_FG_GREEN"New stream_data %lu\n"ANSI_RESET,fh);
    _stream_data[_stream_data_n]=STREAM_DATA_EMPTY;
    _stream_data[_stream_data_n].fh=fh;;
    return _stream_data[_stream_data_n++];
  }
  return STREAM_DATA_EMPTY;
}
#define stream_data_synchronized(op,fi) pthread_mutex_lock(&_mutex); struct stream_data d=stream_data(op,fi);  pthread_mutex_unlock(&_mutex);
///////////////////////////////////////////////////////////
//
// The root. They are specified as program arguments
// The _root[0] is read/write, and can be empty string
// The others are read-only
//
#define ROOTS 9
#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)
int _root_n=0, _root_feature[ROOTS];
char *_root[ROOTS];

////////////////////////////////////////////////////////////
//
// Iterate over all _root to construct the real path;
// https://android.googlesource.com/kernel/lk/+/dima/for-travis/include/errno.h
static int real_file(struct zip_path *zpath, const char *path){
  //log_entered_function("real_file %s\n",path);
  int i,res=ENOENT;
  for(i=0;i<_root_n;i++){
    char *r=_root[i];
    if (*r==0) continue;
    if (*path=='/' && path[1]==0){
      ensure_path_capacity(zpath,my_strlen(r));
      strcpy(zpath->path,r);
      return 0;
    }
    ensure_path_capacity(zpath,my_strlen(path)+my_strlen(r)+1);
    strcpy(zpath->path,r);
    strcat(zpath->path,path);
    int acc=access(zpath->path,R_OK);
    struct stat st;
    stat(zpath->path,&st);
    zpath->file_type=
      S_ISDIR(st.st_mode)?dir:
      S_ISREG(st.st_mode)?regfile:
      other;
    //log_debug_now("path=%s   Access(%s)=%d\n",path,zpath->path,acc);
    if (!acc){
      res=0;
      break;
    }
  }
  if (res) log_msg("_real_file %s res=%d\n",path,res);
  else log_msg("real_file %s ->%s \n",path,zpath->path);
  //log_exited_function("real_file\n");
  return res;
}
static char *real_path_mk_parent(const char *path,int *res){
  //log_entered_function("real_path_mk_parent %s\n",path);
  int needed,slash=last_slash(path);
  log_entered_function(" real_path_mk_parent(%s) slash=%d  \n  ",path,slash);
  if (slash<0){
    log_error("Should not happen slash<0 for %s\n",path);
    return NULL;
  }else if (slash){
    char *dir=strndup(path,slash);
    NEW_ZIP_PATH();
    *res=real_file(&zpath,dir);
    log_debug_now("dir=%s  -> %s  %d \n",dir,zpath.path,*res);
    if (*res){
      *res=ENOENT;
    }else if (zpath.file_type!=FILE_TYPE_DIR){
      *res=ENOTDIR;
    }else if (!(*res=exceeds_max_path(needed=my_strlen(_root[0])+my_strlen(path)+2,path))){
      char *d=malloc(needed);
      strcpy(d,_root[0]);
      strcat(d,dir);
      recursive_mkdir(d);
      if(!is_dir(d)) *res=ENOENT;
      free(d);
    }
    destroy_zip_path(&zpath);
    free(dir);
  }
  if(!*res){
    char *fpath=malloc(my_strlen(_root[0])+strlen(path)+1);
    strcpy(fpath,_root[0]);
    strcat(fpath,path);
    return fpath;
  }
  return NULL;
}
static int real_path_readdir(int no_dots, struct zip_path zpath, void *buf, fuse_fill_dir_t filler){
  char *path=zpath.path;
  //log_entered_function("real_path_readdir '%s;\n",path);
  if (!path || !*path) return 0;
  pthread_mutex_lock(&_mutex_dir);
  int res=0;
  DIR *dir=opendir(path);
  if(dir==NULL){
    perror("Unable to read directory");
    res=ENOMEM;
  }else{
    struct dirent *de;
    while((de=readdir(dir))){
      char *fn=de->d_name;
      if (!*fn) continue;
      struct stat st;
      memset(&st,0,sizeof(st));
      st.st_ino=de->d_ino;
      st.st_mode=de->d_type <<12;
      if (no_dots && *fn=='.' && (!fn[1] || fn[1]=='.' && !fn[2])) continue;
      //log_path(" while readdir",fn);
      int fill_dir_plus=0;
      if (filler(buf,fn,&st,0,fill_dir_plus)) { res=ENOMEM; break; }
    }
    closedir(dir);
  }
  pthread_mutex_unlock(&_mutex_dir);
  //log_exited_function("real_path_readdir %d\n",res);
  return res;
}

// ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ
//

int contained_zip_file_path(const char *path, char *append[1]){
  int len=my_strlen(path);
  char *p,*b=(char*)path;
  append[0]="";
  for(int i=4;i<=len;i++){
    p=(char*)path+i;
    if (*p=='/') b=p+1;
    if (!*p || *p=='/'){
      if (*b=='2' && b[1]=='0' && p[i-4]=='.' && p[i-3]|32=='z' && p[i-2]|32=='i' && p[i-1]|32=='p') return i;
      if (p[i-2]=='.' && p[i-1]|32=='d') { *append=".Zip"; return i;}
    }
  }
  return -1;
}

static int real_file_or_zipentry(struct zip_path *zpath, const char *path){
  return real_file(zpath,path); // DEBUG_NOW
  char *append[1];
  int res=0,after_zip=contained_zip_file_path(path,append);
  if (after_zip>0){
    zpath->zipfile=malloc(after_zip+my_strlen(*append)+1);
    strcat(strncpy(zpath->zipfile,path,after_zip),*append);
    res=real_file(zpath,zpath->zipfile);
    if (!res){
      int err,found=0;
      if (!(zpath->zarchive=zip_open(zpath->zipfile,0,&err))){
        log_error("zip_open(%s)  %d\n",zpath->zipfile,err);
      }else{
        int i,len=my_strlen(path+after_zip+1);
        for (i=zip_get_num_entries(zpath->zarchive,0);--i>=0;){
          if (zip_stat_index(zpath->zarchive,i,0,&zpath->zstat)) continue;
          if (len==pathlen_ignore_trailing_slash(zpath->zstat.name) && !strncpy((char*)zpath->zstat.name,path+after_zip+1,len)){
            found=1;
            break;
          }
        }
        if (!found) {
          my_zip_close(zpath);
          zpath->zarchive=NULL;
        }
      }
      log_zip_path("real_file_or_zipentry",*zpath);
    }
  }
  return res;
}

static void usage(){
  log_msg("usage:  bbfs [FUSE and mount options] mountPoint\n");
  abort();
}
static int fill_dir_plus=0;
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
  //log_entered_function("xmp_getattr %s",path);
  (void) fi;
  int res;
  FIND_REAL();
  //log_zip_path("xmp_getattr",zpath);
  if (zpath.path){
    if(!res) res=lstat(zpath.path,stbuf);
  }
  destroy_zip_path(&zpath);
  //log_msg("xmp_getattr   lstat(%s)=%d\n",zpath.path,res);
  //log_exited_function(" xmp_getattr res=%d\n",res);
  return res==-1?-errno:-res;
}
static int xmp_access(const char *path, int mask){
  log_entered_function("xmp_access %s",path);
  int res;
  FIND_REAL();
  if (!res) res=access(zpath.path,mask);
  destroy_zip_path(&zpath);
  return res==-1?-errno:-res;
}
static int xmp_readlink(const char *path, char *buf, size_t size){
  int res;
  FIND_REAL();
  if (!res && (res=readlink(zpath.path,buf,size-1))!=-1) buf[res]=0;
  destroy_zip_path(&zpath);
  return res==-1?-errno:-res;
}
static int xmp_unlink(const char *path){
  int res;
  FIND_REAL();
  if (!res) res=unlink(zpath.path);
  destroy_zip_path(&zpath);
  return res==-1?-errno:-res;
}
static int xmp_rmdir(const char *path){
  int res;
  FIND_REAL();
  if (!res) res=rmdir(zpath.path);
  destroy_zip_path(&zpath);
  return res==-1?-errno:-res;
}
static int xmp_open(const char *path, struct fuse_file_info *fi){
  log_entered_function("xmp_open %s\n",path);
  int res;
  FIND_REAL();
  if (!res) res=open(zpath.path,fi->flags);
  destroy_zip_path(&zpath);
  log_entered_function(" open(%s)=%d\n",zpath.path,res);
  if (res==-1) return -errno;
  fi->fh=res;
  return 0;
}
static int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi){
  log_entered_function("xmp_truncate %s\n",path);
  int res;
  if (fi!=NULL) res=ftruncate(fi->fh,size);
  else{
    FIND_REAL();
    if (!res) res=truncate(zpath.path,size);
    destroy_zip_path(&zpath);
  }
  return res==-1?-errno:-res;
}
static int xmp_statfs(const char *path, struct statvfs *stbuf){
  log_entered_function("xmp_open %s\n",path);
  int res;
  FIND_REAL();
  if (!res) res=statvfs(zpath.path,stbuf);
  destroy_zip_path(&zpath);
  return res==-1?-errno:-res;
}

/////////////////////////////////
//
// Readdir
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags){
  int res=0;
  (void) offset;
  (void) fi;
  (void) flags;
  FIND_REAL();
  if(path[0]=='/' && !path[1]){
    int no_dots=0;
    for(int i=0;i<_root_n;i++){
      strcpy(zpath.path,_root[i]);
      real_path_readdir(no_dots++,zpath,buf,filler);
    }
    return 0;
  }else{
    res=real_path_readdir(0,zpath,buf,filler);
  }
  destroy_zip_path(&zpath);
  return res;
}
/////////////////////////////////
//
// Creating a new file object

static int xmp_mkdir(const char *create_path, mode_t mode){
  log_entered_function("xmp_mkdir %s \n",create_path);
  int res=0;
  char *cpath=real_path_mk_parent(create_path,&res);
  log_debug_now("xmp_mkdir %s res=%d\n",cpath,res);
  if (cpath){
    res=mkdir(cpath,mode);
    if (res==-1) res=errno;
  }
  free(cpath);
  return -res;
}
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi){
  log_entered_function("xmp_create %s\n",create_path);
  int res=0;
  char *cpath=real_path_mk_parent(create_path,&res);
  if (cpath){
    res=open(cpath,fi->flags,mode);
    if (res==-1) return -errno;
    fi->fh=res;
  }
  free(cpath);
  return 0;
}
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){
  int fd;
  int res=0;
  (void) fi;
  if(fi==NULL){
    char *cpath=real_path_mk_parent(create_path,&res);
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
  char *cpath=real_path_mk_parent(create_path,&res);
  if(cpath){
    res=symlink(target,cpath);
    if (res==-1) return -errno;
  }
  free(cpath);
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
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,struct fuse_file_info *fi){
  int fd;
  int res=0;
  if(fi==NULL){
    FIND_REAL();
    fd=open(zpath.path,O_RDONLY);
    destroy_zip_path(&zpath);
  }else fd=fi->fh;
  if (fd==-1) return -errno;
  res=pread(fd,buf,size,offset);
  if (res==-1) res=-errno;
  if(fi==NULL) close(fd);
  return res;
}
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi){
  int fd;
  off_t res;
  if (fi==NULL){
    FIND_REAL();
    fd=open(zpath.path,O_RDONLY);
    destroy_zip_path(&zpath);
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
  close(fi->fh);
  return 0;
}

static const struct fuse_operations xmp_oper={
  .init=xmp_init,
  .getattr=xmp_getattr,
  .access=xmp_access,
  .readlink=xmp_readlink,
  .readdir=xmp_readdir,
  .mknod=NULL,
  .mkdir=xmp_mkdir,
  .symlink=xmp_symlink,
  .unlink=xmp_unlink,
  .rmdir=xmp_rmdir,
  .rename=xmp_rename,
  .link=NULL, //xmp_link,
  .chmod=NULL, //xmp_chmod,
  .chown=NULL, //xmp_chown,
  .truncate=xmp_truncate,
#ifdef HAVE_UTIMENSAT
  .utimens=NULL,//xmp_utimens,
#endif
  .open=xmp_open,
  .create=xmp_create,
  .read=xmp_read,
  .write=xmp_write,
  .statfs=xmp_statfs,
  .release=xmp_release,
  .fsync=NULL,//xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
  .fallocate=NULL,//xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
  .setxattr=NULL, //xmp_setxattr,
  .getxattr=NULL, //xmp_getxattr,
  .listxattr=NULL, //xmp_listxattr,
  .removexattr=NULL,//xmp_removexattr,
#endif
#ifdef HAVE_COPY_FILE_RANGE
  .copy_file_range=NULL, //xmp_copy_file_range,
#endif
  .lseek=xmp_lseek,
};
int main(int argc, char *argv[]){
  if ((getuid()==0) || (geteuid()==0)){ log_msg("Running BBFS as root opens unnacceptable security holes\n");return 1;}

  char *argv_fuse[9];
  int c,argc_fuse=1,i;
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
  if (MAX_PATHLEN>PATH_MAX) log_abort(" MAX_PATHLEN (%d) > PATH_MAX (%d)  \n",MAX_PATHLEN,PATH_MAX);
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
    if (!*r && _root_n) continue;
    int slashes=-1;
    while(r[++slashes]=='/');
    if (slashes>1){
      _root_feature[_root_n]|=ROOT_REMOTE;
      descript[_root_n]=ANSI_FG_GREEN" (Remote)";
      r+=(slashes-1);
    }
    _root[_root_n++]=slashes?r:realpath(r,NULL);
  }
  if (!_root_n) log_abort("Missing root directories\n");
  log_msg("about to call fuse_main\n");
  log_strings("fuse argv",argv_fuse,argc_fuse,NULL);
  log_strings("root",_root,_root_n, descript);
  int fuse_stat=fuse_main(argc_fuse,argv_fuse, &xmp_oper,NULL);
  log_msg("fuse_main returned %d\n",fuse_stat);
  return fuse_stat;
}
