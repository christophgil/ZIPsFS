static int mutex_count(int mutex,int inc);
static void unlock(int mutex);
static bool _assert_locked(int mutex,bool yesno,bool log);
static void _assert_validchars(enum validchars t,const char *s,int len,const char *msg,const char *fn);
static void _assert_validchars_direntries(enum validchars t,const struct directory *dir,const char *fn);
static const char *simplify_name(char *s,const char *u, const char *zipfile);
static const char *unsimplify_name(char *u,const char *s,const char *zipfile);
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d);
static int my_stat(const char *path,struct stat *statbuf);
static void mstore_assert_lock(struct mstore *m);
static void directory_init(struct directory *d, const char *realpath, struct rootdata *r);
static void directory_destroy(struct directory *d);
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, zip_uint32_t crc);
static void dircache_directory_to_cache(const struct directory *dir);
static bool dircache_directory_from_cache(struct directory *dir, const int64_t mtime);
static int statqueue_add(struct rootdata *r, const char *rp, int rp_l, uint64_t rp_hash, struct stat *stbuf);
static int _statqueue_stat(const char *path, struct stat *statbuf,struct rootdata *r);
static bool statqueue_stat(const char *path, struct stat *statbuf,struct rootdata *r);
static void rthread_start(struct rootdata *r,int ithread);
static void *infloop_statqueue(void *arg);
static void *infloop_unblock(void *arg);
static void start_threads();
static void dircache_clear_if_reached_limit(struct rootdata *r,size_t limit);
static void dircache_clear_if_reached_limit_all();
static bool wait_for_root_timeout(struct rootdata *r);
static void stat_set_dir(struct stat *s);
static void init_stat(struct stat *st, int64_t size,struct stat *uid_gid);
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const char *zipentryOrNull);
static char *zpath_newstr(struct zippath *zpath);
static bool zpath_strncat(struct zippath *zpath,const char *s,int len);
static int zpath_commit(const struct zippath *zpath);
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath);
static void zpath_init(struct zippath *zpath,const char *virtualpath,char *strgs_on_stack);
static void zpath_destroy(struct zippath *zpath);
static bool zpath_stat(struct zippath *zpath,struct rootdata *r);
static struct zip *zip_open_ro(const char *orig);
static int zip_contained_in_virtual_path(const char *path, int *shorten, char *append[]);
static bool directory_from_dircache_zip_or_filesystem(const int opt, struct directory *mydir,const int64_t mtime);
static void *infloop_dircache(void *arg);
static bool test_realpath(struct zippath *zpath, struct rootdata *r);
static bool find_realpath_special_file(struct zippath *zpath);
static bool find_realpath_any_root(struct zippath *zpath,const struct rootdata *onlyThisRoot);
static bool fhdata_can_destroy(struct fhdata *d);
static void fhdata_zip_close(bool alsoZipArchive,struct fhdata *d);
static void memcache_free(struct fhdata *d);
static void fhdata_destroy(struct fhdata *d);
static void fhdata_destroy_those_that_are_marked();
static zip_file_t *fhdata_zip_open(struct fhdata *d,const char *msg);
static struct fhdata* fhdata_create(const char *path,const uint64_t fh);
static struct fhdata* fhdata_get(const char *path,const uint64_t fh);
static struct fhdata *fhdata_by_virtualpath(const char *path,uint64_t path_hash, const struct fhdata *not_this,const enum fhdata_having having);
static struct fhdata *fhdata_by_subpath(const char *path);
static ino_t make_inode(const ino_t inode0,const int  iroot, const int ientry_plus_1,const char *path);
static int filler_readdir_zip(struct rootdata *r,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups);
static int filler_readdir(struct rootdata *r,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups);
static int realpath_mk_parent(char *realpath,const char *path);
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg);
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null);
static int xmp_access(const char *path, int mask);
static int xmp_readlink(const char *path, char *buf, size_t size);
static int xmp_unlink(const char *path);
static int xmp_rmdir(const char *path);
static int64_t trackMemUsage(const enum memusage t,int64_t a);
static size_t memcache_read(char *buf,const struct fhdata *d, const size_t size, const off_t offset);
static void memcache_store(struct fhdata *d);
static void *infloop_memcache(void *arg);
static bool memcache_is_advised(const struct fhdata *d);
static struct fhdata *memcache_waitfor(struct fhdata *d,size_t size,size_t offset);
static int xmp_open(const char *path, struct fuse_file_info *fi);
static int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi);
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags);
static int xmp_mkdir(const char *create_path, mode_t mode);
static int create_or_open(const char *create_path, mode_t mode,struct fuse_file_info *fi);
static int xmp_create(const char *create_path, mode_t mode,struct fuse_file_info *fi);
static int xmp_write(const char *create_path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi);
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi);
static int fhdata_read_zip(const char *path, char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi);
static int fhdata_read(char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi);
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi);
static int xmp_release(const char *path, struct fuse_file_info *fi);
static int xmp_flush(const char *path,  struct fuse_file_info *fi);
