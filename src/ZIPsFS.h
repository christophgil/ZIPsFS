static int debug_statqueue_count_entries(struct rootdata *r);
static struct statqueue_entry *statqueue_add(const bool verbose,struct stat *stbuf, const char *rp, int rp_l, ht_hash_t hash, struct rootdata *r);
static int _stat_queue1(const bool verbose,const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r);
static bool stat_queue(const bool verbose,const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r);
static void *infloop_statqueue(void *arg);
static int  zpath_strlen(struct zippath *zpath,int s);
static MAYBE_INLINE struct fhdata* fhdata_at_index(int i);
static const char *rootpath(const struct rootdata *r);
static int rootindex(const struct rootdata *r);
static void destroy_thread_data(void *x);
static void init_mutex();
static const char *simplify_fname(char *s,const char *u, const char *zipfile);
static bool unsimplify_fname(char *n,const char *zipfile);
static void directory_init(struct directory *d,uint32_t flags,const char *realpath,const int realpath_l, struct rootdata *r);
static void directory_destroy(struct directory *d);
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc);
static bool stat_maybe_cache(int opt, const char *path,const int path_l,const ht_hash_t hash,struct stat *stbuf);
static bool stat_cache_or_queue(const char *rp, struct stat *stbuf,struct rootdata *r);
static int observe_thread(struct rootdata *r, int thread);
static void infloop_statqueue_start(void *arg);
static void infloop_memcache_start(void *arg);
static void root_start_thread(struct rootdata *r,int ithread);
static void *infloop_responding(void *arg);
static void *infloop_misc0(void *arg);
static void *infloop_unblock(void *arg);
static bool wait_for_root_timeout(struct rootdata *r);
static void stat_set_dir(struct stat *s);
static void stat_init(struct stat *st, int64_t size,struct stat *uid_gid);
static int zpath_newstr(struct zippath *zpath);
static bool zpath_strncat(struct zippath *zpath,const char *s,int len);
static int zpath_commit(const struct zippath *zpath);
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath);
static void zpath_init(struct zippath *zpath, const char *vp);
static void autogen_zpath_init(struct zippath *zpath,const char *path);
static bool zpath_stat(struct zippath *zpath,struct rootdata *r);
static struct zip *zip_open_ro(const char *orig);
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *shorten, char *append[]);
static bool _directory_from_dircache_zip_or_filesystem(struct directory *mydir,const struct stat *rp_stat/*current stat or cached*/);
static bool directory_from_dircache_zip_or_filesystem(struct directory *dir,const struct stat *rp_stat/*current stat or cached*/);
static void *infloop_dircache(void *arg);
static bool isSpecialFile(const char *slashFn, const char *path,const int path_l);
static bool find_realpath_special_file(struct zippath *zpath);
static int whatSpecialFile(const char *vp,const int vp_l);
static bool test_realpath(struct zippath *zpath, struct rootdata *r);
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r);
static bool find_realpath_nocache(struct zippath *zpath,struct rootdata *r);
static bool find_realpath_any_root1(const bool verbose,const bool path_starts_autogen,struct zippath *zpath,const struct rootdata *onlyThisRoot,const long which_roots);
static bool find_realpath_any_root(const int opt,struct zippath *zpath,const struct rootdata *onlyThisRoot);
static bool fhdata_can_destroy(struct fhdata *d);
static void fhdata_zip_close(bool alsoZipArchive,struct fhdata *d);
static void fhdata_destroy(struct fhdata *d);
static void fhdata_destroy_those_that_are_marked();
static zip_file_t *fhdata_zip_open(struct fhdata *d,const char *msg);
static void fhdata_init(struct fhdata *d, struct zippath *zpath);
static struct fhdata* fhdata_create(const uint64_t fh, struct zippath *zpath);
static struct fhdata* fhdata_get(const char *path,const uint64_t fh);
static void filldir(const int opt,fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups);
static ino_t next_inode();
static ino_t make_inode(const ino_t inode0,struct rootdata *r, const int entryIdx,const char *path);
static ino_t inode_from_virtualpath(const char *vp,const int vp_l);
static int filler_readdir_zip(const int opt,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups);
static int filler_readdir(const int opt,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups);
static int realpath_mk_parent(char *realpath,const char *path);
static void *xmp_init(struct fuse_conn_info *conn,struct fuse_config *cfg);
static int xmp_getattr(const char *path, struct stat *stbuf,struct fuse_file_info *fi_or_null);
static int xmp_access(const char *path, int mask);
static int xmp_readlink(const char *path, char *buf, size_t size);
static int xmp_unlink(const char *path);
static int xmp_rmdir(const char *path);
static int xmp_open(const char *path, struct fuse_file_info *fi);
static int xmp_truncate(const char *path, off_t size,struct fuse_file_info *fi);
static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi,enum fuse_readdir_flags flags);
static int xmp_mkdir(const char *path, mode_t mode);
static int create_or_open(const char *path, mode_t mode,struct fuse_file_info *fi);
static int xmp_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi);
static off_t xmp_lseek(const char *path, off_t off, int whence, struct fuse_file_info *fi);
static int fhdata_read_zip(const char *path, char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi);
static int fhdata_read_zip1(char *buf, const size_t size, const off_t offset,struct fhdata *d,struct fuse_file_info *fi);
static bool xmp_read_check_d(struct fhdata *d,const char *path);
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi);
static int xmp_release(const char *path, struct fuse_file_info *fi);
static int xmp_flush(const char *path,  struct fuse_file_info *fi);
static void _exit_ZIPsFS();
static bool stat_from_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash);
static void stat_to_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash);
static const char *zipinline_cache_virtualpath_to_zippath(const char *vp,const int vp_l);
static void dircache_clear_if_reached_limit_all(const bool always,const int mask);
static void dircache_clear_if_reached_limit(const bool always,const int mask,struct rootdata *r,size_t limit);
static void debug_assert_crc32_not_null(const struct directory *dir);
static void dircache_directory_to_cache(const struct directory *dir);
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim);
static struct ht* transient_cache_get_ht(struct fhdata *d);
static struct zippath *transient_cache_get_or_create_zpath(const char *path,const int path_l);
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l,const char *zipentryOrNull,const int zipentry_l);
static int countFhdataWithMemcache(const char *path, int len,int h);
static void _fhdataWithMemcachePrint(const char *func,int line,const char *path, int len,int h);
static bool _debugSpecificPath(int mode, const char *path, int path_l);
static void _assert_validchars(enum validchars t,const char *s,int len,const char *msg,const char *fn);
static void _assert_validchars_direntries(enum validchars t,const struct directory *dir,const char *fn);
static void _debug_directory_print(const struct directory *dir,const char *fn,const int line);
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d);
static bool debug_path(const char *vp);
static bool debug_fhdata(const struct fhdata *d);
static void debug_fhdata_listall();
static void debug_compare_directory_a_b(struct directory *A,struct directory *B);
static void debug_dircache_compare_cached(struct directory *mydir,const struct stat *rp_stat);
static void debug_track_false_getattr_errors(const char *vp,const int vp_l);
static void init_count_getattr();
static void inc_count_getattr(const char *path,enum enum_count_getattr field);
static const char *fileExtension(const char *path,const int len);
static counter_rootdata_t *filetypedata_for_ext(const char *path,struct rootdata *r);
static void _rootdata_counter_inc(counter_rootdata_t *c, enum enum_counter_rootdata f);
static void fhdata_counter_inc( struct fhdata* d, enum enum_counter_rootdata f);
static void rootdata_counter_inc(const char *path, enum enum_counter_rootdata f, struct rootdata* r);
static void log_path(const char *f,const char *path);
static void log_fh(char *title,long fh);
static int printinfo(int n, const char *format,...);
static bool isKnownExt(const char *path, int len);
static int counts_by_filetype_row0(const char *ext,int n);
static int counts_by_filetype_r(int n,struct rootdata *r, int *count_explain);
static long counter_getattr_rank(struct ht_entry *e);
static int counts_by_filetype(int n);
static int log_print_fuse_argv(int n);
static int log_print_roots(int n);
static int repeat_chars_info(int n,char c, int i);
static int print_bar(int n,float rel);
static int print_proc_status(int n,char *filter,int *val);
static void log_virtual_memory_size();
static int log_print_open_files(int n, int *fd_count);
static int print_maps(int n);
static int log_print_CPU(int n);
static int log_print_memory(int n);
static int log_print_statistics_of_read(int n);
static size_t _kilo(size_t x);
static int print_fhdata(int n,const char *title);
static int print_all_info();
static const char *zip_fdopen_err(int err);
static void fhdata_log_cache(const struct fhdata *d);
static void _log_zpath(const char *fn,const int line,const char *msg, struct zippath *zpath);
static bool fhdata_not_yet_logged(unsigned int flag,struct fhdata *d);
static void log_fuse_process();
static void usage();
#define log_zpath(...) _log_zpath(__func__,__LINE__,__VA_ARGS__) /*TO_HEADER*/
