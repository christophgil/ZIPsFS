static int xmp_link(const char *from, const char *to){
  int res;
  res=link(from,to);
  if (res==-1) return -errno;
  return 0;
}
static int xmp_chmod(const char *path, mode_t mode,struct fuse_file_info *fi){
  (void) fi;
  int res;
  res=chmod(path,mode);
  if (res==-1) return -errno;
  return 0;
}
static int xmp_chown(const char *path, uid_t uid, gid_t gid,struct fuse_file_info *fi){
  (void) fi;
  int res;
  res=lchown(path,uid,gid);
  if (res==-1) return -errno;
  return 0;
}



#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,size_t size, int flags){
  int res=lsetxattr(path,name,value,size,flags);
  if (res==-1) return -errno;
  return 0;
}
static int xmp_getxattr(const char *path, const char *name, char *value,size_t size){
  int res=lgetxattr(path,name,value,size);
  if (res==-1) return -errno;
  return res;
}
static int xmp_listxattr(const char *path, char *list, size_t size){
  int res=llistxattr(path,list,size);
  if (res==-1) return -errno;
  return res;
}
static int xmp_removexattr(const char *path, const char *name){
  int res=lremovexattr(path,name);
  if (res==-1) return -errno;
  return 0;
}
#endif /* HAVE_SETXATTR */
#ifdef HAVE_COPY_FILE_RANGE
static ssize_t xmp_copy_file_range(const char *path_in,struct fuse_file_info *fi_in,off_t offset_in, const char *path_out,struct fuse_file_info *fi_out,off_t offset_out, size_t len, int flags){
  int fd_in, fd_out;
  ssize_t res;
  if(fi_in==NULL) fd_in=open(path_in,O_RDONLY);
  else fd_in=fi_in->fh;
  if (fd_in==-1) return -errno;
  if(fi_out==NULL) fd_out=open(path_out,O_WRONLY);
  else fd_out=fi_out->fh;
  if (fd_out==-1) {
    close(fd_in);
    return -errno;
  }
  res=copy_file_range(fd_in, &offset_in,fd_out, &offset_out,len,flags);
  if (res==-1) res=-errno;
  if (fi_out==NULL) close(fd_out);
  if (fi_in==NULL) close(fd_in);
  return res;
}
#endif


static int xmp_fsync(const char *path, int isdatasync,struct fuse_file_info *fi){
  /* Just a stub.	 This method is optional and can safely be left
     unimplemented */
  (void) path;
  (void) isdatasync;
  (void) fi;
  return 0;
}
#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,off_t offset, off_t length, struct fuse_file_info *fi){
  int fd;
  int res;
  (void) fi;
  if (mode) return -EOPNOTSUPP;
  if(fi==NULL) fd=open(path,O_WRONLY);
  else fd=fi->fh;
  if (fd==-1) return -errno;
  res=-posix_fallocate(fd,offset,length);
  if(fi==NULL) close(fd);
  return res;
}
#endif


#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],struct fuse_file_info *fi){
  (void) fi;
  int res;
  /* don't use utime/utimes since they follow symlinks */
  res=utimensat(0,path,ts,AT_SYMLINK_NOFOLLOW);
  if (res==-1) return -errno;
  return 0;
}
#endif
