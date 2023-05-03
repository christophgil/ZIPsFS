/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
int xmp_flush(const char *path,struct fuse_file_info *fi);
int xmp_release(const char *path,struct fuse_file_info *fi);
int _xmp_read(const char *path,char *buf,const size_t size,const off_t offset,const uint64_t fd,struct fhdata *d);
int _xmp_read_via_zip(const char *path,char *buf,const size_t size,const off_t offset,const uint64_t fd,struct fhdata *d);
off_t xmp_lseek(const char *path,off_t off,int whence,struct fuse_file_info *fi);
int read_from_cache(char *buf,char *cache,long cache_l,size_t size,off_t offset);
int xmp_rename(const char *old_path,const char *neu_path,unsigned int flags);
int xmp_symlink(const char *target,const char *create_path);
int xmp_write(const char *create_path,const char *buf,size_t size,off_t offset,struct fuse_file_info *fi);
int xmp_create(const char *create_path,mode_t mode,struct fuse_file_info *fi);
int xmp_mkdir(const char *create_path,mode_t mode);
int xmp_readdir(const char *path,void *buf,fuse_fill_dir_t filler,off_t offset,struct fuse_file_info *fi,enum fuse_readdir_flags flags);
int xmp_truncate(const char *path,off_t size,struct fuse_file_info *fi);
int create_or_open(const char *create_path,mode_t mode,struct fuse_file_info *fi);
int xmp_open(const char *path,struct fuse_file_info *fi);
struct fhdata *maybe_cache_zip_entry(enum data_op op,struct fhdata *d,bool always);
int xmp_rmdir(const char *path);
int xmp_unlink(const char *path);
int xmp_readlink(const char *path,char *buf,size_t size);
int xmp_access(const char *path,int mask);
int xmp_getattr(const char *path,struct stat *stbuf,struct fuse_file_info *fi_or_null);
void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg);
int realpath_mk_parent(char *realpath,const char *path);
int xmp_statfs(const char *path,struct statvfs *stbuf);
int xmp_releasedir(const char *path,struct fuse_file_info *fi);
int impl_readdir(struct rootdata *r,struct zippath *zpath,void *buf,fuse_fill_dir_t filler,struct ht *no_dups);
bool readdir_concat_z(struct my_strg *s,long mtime,const char *rp);
struct fhdata *fhdata_by_subpath(const char *path);
struct fhdata *fhdata_get(const enum data_op op,const char *path,const uint64_t fh);
struct fhdata *fhdata_create(const char *path,uint64_t fh);
zip_file_t *fhdata_zip_open(struct fhdata *d,char *msg);
struct fhdata *cache_zip_entry(enum data_op op,struct fhdata *d);
void fhdata_destroy(struct fhdata *d,int i);
struct fhdata *fhdata_by_virtualpath(const char *path,const struct fhdata *not_this,const enum fhdata_having having);
bool fhdata_can_destroy(struct fhdata *d);
void usage();
int test_realpath_any_root(struct zippath *zpath,int onlyThisRoot);
int read_zipdir(struct rootdata *r,struct zippath *zpath,void *buf,fuse_fill_dir_t filler_maybe_null,struct ht *no_dups);
int test_realpath(struct zippath *zpath,int root);
bool readdir_concat(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip);
bool readdir_concat_unsynchronized(int opt,struct my_strg *s,long mtime,const char *rp,struct zip *zip);
bool readdir_iterate(struct name_ino_size *nis,struct my_strg *s);
char *my_memchr(const char *b,const char *e,char c);
void readdir_append(struct my_strg *s,long inode,const char *n,bool append_slash,long size);
bool illegal_char(const char *s);
int readdir_callback(void *arg1,int argc,char **argv,char **name);
int zipfile_callback(void *arg1,int argc,char **argv,char **name);
int sql_exec(int flags,const char *sql,int(*callback)(void *,int,char **,char **),void *udp);
extern sqlite3 *_sqlitedb;
int zip_contained_in_virtual_path(const char *path,char *append[]);
struct zip *zpath_zip_open(struct zippath *zpath,int equivalent);
struct zip *my_zip_open_ro(const char *orig,int equivalent);
int zpath_stat(struct zippath *zpath,struct rootdata *r);
void zpath_destroy(struct zippath *zpath);
void zpath_reset_keep_VP(struct zippath *zpath);
void zpath_reset_realpath(struct zippath *zpath);
void zpath_close_zip(struct zippath *zpath);
int find_realpath_any_root(struct zippath *zpath,int onlyThisRoot);
void zpath_stack_to_heap(struct zippath *zpath);
char *zpath_newstr(struct zippath *zpath);
int zpath_strlen(struct zippath *zpath);
int zpath_strncat(struct zippath *zpath,const char *s,int len);
void log_zpath(char *msg,struct zippath *zpath);
void zpath_assert_strlen(const char *title,struct zippath *zpath);
int zpath_strcat(struct zippath *zpath,const char *s);
void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack);
int my_open_fh(const char *msg,const char *path,int flags);
void my_close_fh(int fh);
void init_stat(struct stat *st,long size,struct stat *uid_gid);
void stat_set_dir(struct stat *s);
void *thread_readdir_async(void *arg);
void threads_root_start();
void *thread_observe_root(void *arg);
char *ensure_capacity(struct my_strg *s,int n);
int xmp_read(const char *path,char *buf,const size_t size,const off_t offset,struct fuse_file_info *fi);
extern char *CACHE_FILLING;
