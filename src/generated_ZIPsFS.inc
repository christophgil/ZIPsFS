static void destroy_thread_data(void *x);
static int cg_mutex_count(int mutex,int inc);
static MAYBE_INLINE void lock(int mutex);
static MAYBE_INLINE void unlock(int mutex);
static void cg_mutex_test_1(void);
static void cg_mutex_test_2(void);
static char *path_of_this_executable(void);
static void provoke_idx_out_of_bounds(void);
static void enable_core_dumps(void);
static bool filepath_contains_blocking(const char *p);
static bool tdf_or_tdf_bin(const char *p);
static bool filename_starts_year(const char *p,int l);
static bool file_starts_year_ends_dot_d(const char *p);
static bool _report_failure_for_tdf(const char *mthd, int line, const char *path);
static void assert_dir(const char *p, const struct stat *st);
static void assert_r_ok(const char *p, const struct stat *st);
static const char *function_name(enum enum_functions f);
static void _log_count_b(enum enum_functions f);
static void _log_count_e(enum enum_functions f,const char *path);
static void func3(void);
static void *malloc_untracked(const size_t size);
static void *calloc_untracked(const size_t nmemb,const size_t size);
static char *strdup_untracked(const char *s);
static bool has_proc_fs(void);
static void puts_stderr(const char *s);
static const char *error_symbol(const int x);
static const char *error_symbol_zip(const int x);
static void fprint_strerror(FILE *f,int err);
static void *cg_malloc(const int id, const size_t size);
static void *cg_calloc(const int id,size_t nmemb, size_t size);
static char *cg_strdup(const int id,const char *s);
static void cg_free(const int id,const void *ptr);
static void *_cg_mmap(const int id, const size_t length, const int fd_or_zero);
static int _cg_munmap(const int id,const void *ptr,size_t length);
static char *cg_stpncpy0(char *dst,const char *src,const int n);
static const char *cg_str_lremove(const char *s, const char *pfx,const int  pfx_l);
static int cg_empty_dot_dotdot(const char *s);
static char *cg_strncpy(char *dst,const char *src, int n);
static uint32_t cg_strlen(const char *s);
static int cg_sum_strlen(const char **ss, const int n);
static int cg_idx_of_pointer(void **aa, const int n, const void *a);
static bool cg_endsWithIC(const bool ic,const char* s,int s_l,const char* e,const int e_l_or_zero);
static bool cg_startsWith(const char* s,int s_l,const char* e,int e_l);
static bool cg_endsWithZip(const char *s, int len);
static bool cg_endsWithDotD(const char *s, int len);
static int cg_find_suffix(const int opt,const char *s, const int s_l,const char **xx,const int *xx_l);
static int cg_last_slash(const char *path);
static int cg_str_replace(const int opt,char *haystack, const int h_l_or_zero, const char *needle,  const int n_l_or_zero, const char *replacement,  const int r_l_or_zero);
static int cg_strsplit(int opt_and_sep, const char *s, const int s_l, const char *tokens[], int *tokens_l);
static const char *rm_pfx_us(const char *s);
static int64_t currentTimeMillis(void);
static int deciSecondsSinceStart(void);
static void cg_sleep_ms(const int millisec, const char *msg);
static int cg_pathlen_ignore_trailing_slash(const char *p);
static bool cg_path_equals_or_is_parent(const char *subpath,const int subpath_l,const char *path,const int path_l);
static bool *cg_validchars(enum enum_validchars type);
static int cg_find_invalidchar(enum enum_validchars type,const char *s,const int len);
static void cg_path_for_fd(const char *title, char *path, int fd);
static int cg_count_fd_this_prg(void);
static bool cg_check_path_for_fd(const char *title, const char *path, int fd);
static void cg_print_path_for_fd(int fd);
static int cg_array_length(const char **xx);
static int isPowerOfTwo(unsigned int n);
static unsigned int isqrt(unsigned int y);
static bool is_square_number(unsigned int y);
static MAYBE_INLINE int64_t cg_atol_kmgt(const char *s);
static void cg_log_file_mode(mode_t m);
static long cg_file_size(const char *path);
static bool cg_pid_exists(const pid_t pid);
static void _cg_log_file_stat(const char *fn,const char * name,const struct stat *s);
static void cg_log_open_flags(int flags);
static bool cg_stat_differ(const char *title,const struct stat *s1,const struct stat *s2);
static bool cg_is_stat_mode(const mode_t mode,const char *f);
static bool cg_file_set_atime(const char *path, const struct stat *stbuf,long secondsFuture);
static bool cg_set_st_mode_flag(const char *path, mode_t mode);
static int cg_getc_tty(void);
static bool cg_fd_write(const int fd,const char *t,const off_t size0);
static bool cg_fd_write_str(const int fd,const char *t);
static int cg_symlink_overwrite_atomically(const char *src,const char *lnk);
static bool cg_mkdir(const char *path,const mode_t mode);
static bool _cg_recursive_mkdir(const bool parentOnly,const char *path);
static void log_list_filedescriptors(const int fd);
static char* cg_path_expand_tilde(char *dst, const int dst_max, const char *path);
static double cg_timespec_diff(const struct timespec a, const struct timespec b);
static bool cg_timespec_eq(struct timespec a, struct timespec b);
static struct timespec cg_file_last_modified(const char *path);
static bool cg_file_is_newer_than(const char *path1,const char *path2);
static bool cg_is_member_of_group(char *group);
static bool cg_is_member_of_group_docker(void);
static bool cg_log_exec_fd(const int fd, char const * const * const env, char const * const * const cmd);
static bool cg_log_waitpid_status(FILE *f,const unsigned int status,const char *msg);
static int cg_waitpid_logtofile_return_exitcode(int pid,const char *err);
static void cg_exec(char const * const * const env,char const * const * const cmd,const int fd_out,const int fd_err);
static int differs_filecontent_from_string(const int opt,const char* path, const long seek,const char* text,const long text_l);
static bool cg_pid_exists_proc(const pid_t pid);
static void ht_set_mutex(const int mutex,struct ht *ht);
static struct ht *_ht_init_with_keystore(struct ht *ht,const char *name,uint32_t flags_log2initalCapacity, struct mstore *m, uint32_t mstore_dim);
static void _ht_free_entries(struct ht *ht);
static void ht_destroy(struct ht *ht);
static void ht_clear(struct ht *ht);
static MAYBE_INLINE int debug_count_empty(const struct ht_entry *ee, const uint32_t capacity);
static struct ht_entry* _ht_get_entry_ee(struct ht_entry *ee, const uint32_t capacity, const bool intkey,const char* key, const uint64_t keylen_hash);
static int _ht_expand(struct ht *ht);
static const char* _newKey(struct ht *ht,const char *key,uint64_t keylen_hash);
static struct ht_entry *PROFILED(ht_get_entry)(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash,const bool create);
static void ht_clear_entry(const struct ht *ht,struct ht_entry *e);
static struct ht_entry* ht_remove(struct ht *ht,const char* key,const ht_keylen_t key_l, ht_hash_t hash );
static void *PROFILED(ht_set)(struct ht *ht,const char* key,const ht_keylen_t key_l,ht_hash_t hash, const void* value);
static void* ht_get(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash);
static const void *ht_intern(struct ht *ht,const void *bytes,const off_t bytes_l,ht_hash_t hash,const int memoryalign);
static bool PROFILED(ht_only_once)(struct ht *ht,const char *s,const int s_l_or_zero);
static void* ht_sget(struct ht *ht, const char* key);
static struct ht_entry* ht_sget_entry(struct ht *ht, const char* key,const bool create);
static void *ht_sset(struct ht *ht,const char* key, const void* value);
static const void *ht_sinternalize(struct ht *ht,const char *key);
static struct ht_entry *ht_numkey_get_entry(struct ht *ht, uint64_t key_high_variability, uint64_t const key2,bool create);
static void *ht_numkey_set(struct ht *ht, uint64_t key_high_variability, const uint64_t key2, const void *value);
static int ht_report_memusage_to_strg(char *strg,int max_bytes,struct ht *ht,const bool html);
static void debug_print_keys(const struct ht *ht);
static void test_ht_1(int argc, const char *argv[]);
static void test_num_keys(int argc, const char *argv[]);
static void test_internalize(int argc, const char *argv[]);
static void test_intern_num(int argc, const char *argv[]);
static void test_use_as_set(int argc, const char *argv[]);
static void test_unique(int argc, const char *argv[]);
static  void test_no_dups(int argc,const char *argv[]);
static void test_mstore2(int argc, const char *argv[]);
static void test_intern_substring(int argc, const char *argv[]);
static void _warning(const char *fn,int line,const uint32_t channel,const char* path,const char *format,...);
static int cpuusage_read_proc(struct pstat* r,const pid_t pid);
static void cpuusage_calc_pct(const struct pstat* cur_usage,const struct pstat* last_usage,float* ucpu_usage, float* scpu_usage);
static void cpuusage_calc(const struct pstat* cur_usage,const struct pstat* last_usage,long unsigned int* ucpu_usage,long unsigned int* scpu_usage);
static off_t textbuffer_memusage(const int flags,const off_t delta);
static void textbuffer_memusage_change(const uint8_t flags,const struct textbuffer *b,const off_t delta);
static off_t textbuffer_length(const struct textbuffer *b);
static int textbuffer_char_at(const struct textbuffer *b, const off_t pos);
static off_t _textbuffer_copy_to_or_compare(const bool isCopy,const struct textbuffer *b,const off_t from,const off_t to, char *dst);
static int textbuffer_add_segment(const uint8_t flags,struct textbuffer *b, const char *bytes, const off_t size_or_zero);
static char *textbuffer_malloc(const int flags,struct textbuffer *b, off_t size);
static off_t textbuffer_read(const uint8_t flags,struct textbuffer *b,const int fd);
static int textbuffer_from_exec_output(const uint8_t flags,struct textbuffer *b,char const * const cmd[],char const * const env[], const char *path_stderr);
static void textbuffer_reset(struct textbuffer *b);
static char *textbuffer_first_segment_with_min_capacity(const int flags,struct textbuffer *b, off_t min_size);
static void textbuffer_destroy(struct textbuffer *b);
static bool textbuffer_write_fd(struct textbuffer *b,const int fd);
static bool textbuffer_write_file(struct textbuffer *b,const char *path,const int mode);
static int textbuffer_differs_from_filecontent_fd(const struct textbuffer *b,const int fd);
static void test_ps_pid(const int pid);
static void test_write_byte_by_byte(const struct textbuffer *b);
static void test_write_bytes_block(const int fd,const struct textbuffer *b);
static void test_exec(void);
static bool _is_zip(const char *s);
static bool _is_Zip(const char *s);
static bool _is_tdf_or_tdf_bin(const char *path);
static const int config_virtual_dirpath_to_zipfile(const char *b, const char *e,char *append[]);
static int config_containing_zipfile_of_virtual_file(const int approach,const char *path,const int path_l,char *suffix[]);
static void config_containing_zipfile_of_virtual_file_test(void);
static uint64_t config_file_attribute_valid_mseconds(const int opt, const char *path,const int path_l);
static char *config_zipfilename_to_virtual_dirname(char *dirname,const char *zipfile,const int zipfile_l);
static bool config_skip_zipfile_show_zipentries_instead(const char *zipfile,const int zipfile_l);
static bool config_not_report_stat_error(const char *path,const int path_l);
static bool config_advise_cache_zipentry_in_ram(const char *virtualpath,const char *realpath,const off_t filesize, const int flags);
static bool config_advise_cache_zipentry_in_ram_never(const char *virtualpath,const char *realpath,const off_t filesize, const int flags);
static bool config_advise_evict_from_filecache(const char *realpath,const int realpath_l, const char *zipentryOrNull, const off_t filesize);
static bool config_advise_transient_cache_for_zipentries(const char *path, const int path_l);
static bool config_do_not_list_file(const char *parent, const char *filename,const int filename_l);
static bool config_not_overwrite(const char *path,const int path_l);
static bool config_advise_cache_directory_listing(const char *path,const int path_l,const bool isRemote,const bool isZipArchive, const struct timespec mtime);
static char *config_some_file_path_extensions(const char *path, int path_l, int *return_index);
static long config_search_file_which_roots(const char *virtualpath,const int virtualpath_l);
static bool config_readir_no_other_roots(const char *realpath,const int realpath_l);
static long config_num_retries_getattr(const char *path, const int path_l, int *sleep_milliseconds);
static bool config_file_is_readonly(const char *path, const int path_l);
static void config_exclude_files(const char *path, const int path_l, const int num_files, char **files, const off_t *fsize);
static bool config_has_sufficient_storage_space(const char *realpath, const long availableBytes, const long totalBytes);
static int countFhandleWithMemcache(const char *path, int len,int h);
static void _fhandleWithMemcachePrint(const char *func,int line,const char *path, int len,int h);
static bool _debugSpecificPath(int mode, const char *path, int path_l);
static void _assert_validchars(enum enum_validchars t,const char *s,int len,const char *msg,const char *fn);
static void _assert_validchars_direntries(enum enum_validchars t,const struct directory *dir,const char *fn);
static void _debug_directory_print(const struct directory *dir,const char *fn,const int line);
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d);
static bool debug_path(const char *vp);
static bool debug_fhandle(const struct fHandle *d);
static void debug_fhandle_listall(void);
static void debug_compare_directory_a_b(struct directory *A,struct directory *B);
static void debug_dircache_compare_cached(struct directory *mydir,const struct stat *rp_stat);
static void debug_track_false_getattr_errors(const char *vp,const int vp_l);
static bool debug_trigger_vp(const char *vp,const int vp_l);
static bool stat_from_cache(const int opt,struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash);
static void stat_to_cache(const struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash);
static const char *zinline_cache_vpath_to_zippath(const char *vp,const int vp_l);
static void dircache_clear_if_reached_limit_all(const bool always,const int mask);
static bool _clear_ht(struct ht *ht);
static void dircache_clear_if_reached_limit(const bool always,const int mask,struct rootdata *r,const off_t limit);
static void debug_assert_crc32_not_null(const struct directory *dir);
static void dircache_directory_to_cache(const struct directory *dir);
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim);
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l);
static struct ht* transient_cache_get_ht(struct fHandle *d);
static struct zippath *transient_cache_get_or_create_zpath(const bool create,const char *virtualpath,const int virtualpath_l);
static int transient_cache_find_realpath(const int opt, struct zippath *zpath, const char *vp,const int vp_l);
static void transient_cache_store(const bool onlyThisRoot,const struct zippath *zpath, const char *vp,const int vp_l);
static void transient_cache_destroy(struct fHandle *d);
static void transient_cache_activate(struct fHandle *d);
static int get_log_flags();
static void init_count_getattr(void);
static void inc_count_getattr(const char *path,enum enum_count_getattr field);
static const char *fileExtension(const char *path,const int len);
static counter_rootdata_t *filetypedata_for_ext(const char *path,struct rootdata *r);
static void _rootdata_counter_inc(counter_rootdata_t *c, enum enum_counter_rootdata f);
static void fhandle_counter_inc( struct fHandle* d, enum enum_counter_rootdata f);
static void rootdata_counter_inc(const char *path, enum enum_counter_rootdata f, struct rootdata* r);
static void log_path(const char *f,const char *path);
static void log_fh(const char *title,const long fh);
static bool isKnownExt(const char *path, int len);
static int counts_by_filetype_row0(const char *ext,int n);
static int counts_by_filetype_r(int n,struct rootdata *r, int *count_explain);
static long counter_getattr_rank(const struct ht_entry *e);
static int counts_by_filetype(int n);
static int log_print_fuse_argv(int n);
static int log_print_roots(int n);
static int repeat_chars_info(int n,char c, int i);
static int print_bar(int n,float rel);
static int print_proc_status(int n,char *filter,int *val);
static void log_virtual_memory_size(void);
static int log_memusage_ht(int n,const bool html);
static int log_mutex_locks(int n,const bool html);
static int log_malloc(int n,const bool html);
static int log_print_open_files(int n, int *fd_count);
static int print_maps(int n);
static int log_print_CPU(int n);
static int log_print_memory(int n);
static int log_print_statistics_of_read(int n);
static size_t _kilo(size_t x);
static int print_fhandle(int n,const char *title);
static int print_all_info(const int flags);
static const char *zip_fdopen_err(int err);
static void fhandle_log_cache(const struct fHandle *d);
static void _log_zpath(const char *fn,const int line,const char *msg, struct zippath *zpath);
static bool fhandle_not_yet_logged(unsigned int flag,struct fHandle *d);
static void log_fuse_process(void);
static void suggest_help(void);
static void ZIPsFS_usage(void);
static struct memcache *memcache_new(struct fHandle *d);
static off_t memcache_read(char *buf,const struct fHandle *d,const off_t from,off_t to);
static off_t memcache_read_fhandle(char *buf, const off_t size, const off_t offset,struct fHandle *d,struct fuse_file_info *fi);
static bool memcache_fhandle_to_queue(struct fHandle *d);
static bool fhandle_check_crc32(struct fHandle *d);
static int memcache_store_try(struct fHandle *d);
static void memcache_store(struct fHandle *d);
static void *infloop_memcache(void *arg);
static off_t ramUsageForFilecontent(void);
static off_t ramUsageForFilecontentMax(void);
static bool virtualpathStat(struct stat *st, const char *vp);
static void fhandle_set_text(struct fHandle *d, struct textbuffer *b);
static uint64_t fhandle_set_filesource_info(struct zippath *zpath);
static bool _memcache_is_advised(const struct fHandle *d);
static bool memcache_is_advised(const struct fHandle *d);
static off_t memcache_waitfor(struct fHandle *d, off_t min_fill);
static void memcache_free(struct fHandle *d);
static void memcache_infer_from_other_handle(struct fHandle *d);
static bool memcache_set_maxbytes(const char *s);
static bool memcache_set_policy(const char *a);
static bool memcache_can_destroy(struct fHandle *d);
static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r);
static yes_zero_no_t find_realpath_try_inline_rules(struct zippath *zpath,char *append, struct rootdata *r);
static yes_zero_no_t zipinline_find_realpath_any_root(struct zippath *zpath,const long which_roots);
static const char *zipentry_placeholder_insert(char *s,const char *u, const char *zipfile);
static bool zipentry_placeholder_expand(char *n,const char *zipfile);
static void autogen_run(struct fHandle *d);
static bool virtualpath_startswith_autogen(const char *vp, const int vp_l_or_zero);
static const char *vp_without_pfx_autogen(const char *vp,const int vp_l_or_zero);
static bool autogen_not_up_to_date(struct timespec st_mtim,const char *vp,const int vp_l);
static long autogen_estimate_filesize(const char *vp,const int vp_l);
static bool autogen_remove_if_not_up_to_date(struct zippath *zpath);
static void autogen_filldir(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups);
static void *_autogen_cleanup_runnable(void *arg);
static void autogen_cleanup(void);
static bool _autogen_is_placeholder(const char *c);
static void autogen_free_argv(char const * const * cmd,const char * const * cmd_orig);
static void autogen_zpath_init(struct zippath *zpath,const char *path);
static bool autogen_zpath_set_stat(struct stat *stbuf,const struct zippath *zpath,const char *path,const int path_l);
static struct autogen_rule **config_autogen_rules(void);
static bool config_autogen_file_is_invalid(const char *path,const int path_l, const struct stat *st, const char *rootpath);
static long config_autogen_estimate_filesize(const struct autogen_files *ff, bool *rememberFileSize);
static int config_autogen_add_virtual_infiles(struct autogen_files *ff);
static enum enum_autogen_run_res config_autogen_run(struct autogen_files *ff);
static void aimpl_wait_concurrent(const struct autogen_rule *ac, const int inc);
static void aimpl_init(void);
static const char *aimpl_fileext(const char *vp, const int vp_l,const struct autogen_rule *ac);
static bool _aimpl_patterns_match(const char *vp, const int vp_l,const char ** const patterns[], int * const patterns_l[], int *count);
static bool _aimpl_matches(const char *vp, const int vp_l,const struct autogen_rule *ac);
static void aimpl_vgenerated_from_vinfile(char *generated,const char *vp,const int vp_l, const struct autogen_rule *ac);
static bool _autogen_rule_matches(struct autogen_files *ff);
static int _autogen_realinfiles(struct autogen_files *ff,const struct autogen_rule *ac);
static int autogen_realinfiles(struct autogen_files *ff);
static void aimpl_maybe_reset_atime_in_future(const struct fHandle *d);
static int _aimpl_fd_open(const char *path, int *fd);
static int aimpl_run(struct autogen_files *ff);
static void aimpl_cleanup(const char *root);
static int _autogen_filecontent_append(const int flags, struct autogen_files *ff, const char *s,const long s_l);
static char *ctrl_file_end();
static void special_file_content(struct textbuffer *b,const enum enum_special_files i);
static bool trigger_files(const bool isGenerated,const char *path,const int path_l);
static int read_special_file(const int i, char *buf, const off_t size, const off_t offset);
static void make_info(const int flags);
static int debug_statqueue_count_entries(struct rootdata *r);
static struct statqueue_entry *statqueue_add(struct stat *stbuf, const char *rp, int rp_l, ht_hash_t hash, struct rootdata *r);
static yes_zero_no_t _stat_queue_and_wait(const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r);
static bool stat_queue_and_wait(const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r);
static void *infloop_statqueue(void *arg);
static struct ht_entry *ht_get_entry(struct ht *ht, const char* key,const ht_keylen_t key_l,ht_hash_t hash,const bool create);
static void *ht_set(struct ht *ht,const char* key,const ht_keylen_t key_l,ht_hash_t hash, const void* value);
static bool ht_only_once(struct ht *ht,const char *s,const int s_l_or_zero);
static int _xmp_getattr(const char *path, struct stat *stbuf);
static int xmp_access(const char *path, int mask);
static int xmp_open(const char *path, struct fuse_file_info *fi);
static int _xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi);
static int xmp_release(const char *path, struct fuse_file_info *fi);
static struct ht *ht_set_id(const int id,struct ht *ht);
static int zpath_strlen(const struct zippath *zpath,int s);
static bool zpath_exists(struct zippath *zpath);
static MAYBE_INLINE struct fHandle* fhandle_at_index(int i);
static const char* rootpath(const struct rootdata *r);
static const char* report_rootpath(const struct rootdata *r);
static int rootindex(const struct rootdata *r);
static void init_mutex(void);
static void directory_init(struct directory *d,uint32_t flags,const char *realpath,const int realpath_l, struct rootdata *r);
static void directory_destroy(struct directory *d);
static void directory_add_file(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc);
static int stat_cache_opts_for_root(const struct rootdata *r);
static bool stat_from_cache_or_syscall(const int opt, const char *path,const int path_l,const ht_hash_t hash,struct stat *stbuf);
static bool stat_from_cache_or_syscall_or_async(const char *rp, struct stat *stbuf,struct rootdata *r);
static int observe_thread(struct rootdata *r, const int thread);
static void root_start_thread(struct rootdata *r,int ithread);
static void *infloop_responding(void *arg);
static void *infloop_misc0(void *arg);
static void init_infloop(struct rootdata *r, const int ithread);
static void *infloop_unblock(void *arg);
static void log_root_blocked(struct rootdata *r,const bool blocked);
static bool wait_for_root_timeout(struct rootdata *r);
static void stat_set_dir(struct stat *s);
static void stat_init(struct stat *st, int64_t size,const struct stat *uid_gid);
static int zpath_newstr(struct zippath *zpath);
static bool zpath_strncat(struct zippath *zpath,const char *s,int len);
static int zpath_commit_hash(const struct zippath *zpath, ht_hash_t *hash);
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath);
static void zpath_reset_keep_VP(struct zippath *zpath);
static void zpath_init(struct zippath *zpath, const char *vp);
static bool zpath_stat(struct zippath *zpath,struct rootdata *r);
static struct zip *zip_open_ro(const char *orig);
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *shorten, char *append[]);
static bool directory_from_dircache_zip_or_filesystem(struct directory *mydir,const struct stat *rp_stat/*current stat or cached*/);
static void *infloop_dircache(void *arg);
static bool isSpecialFile(const char *slashFn, const char *path,const int path_l);
static bool find_realpath_special_file(struct zippath *zpath);
static int whatSpecialFile(const char *vp,const int vp_l);
static bool _test_realpath(struct zippath *zpath, struct rootdata *r);
static bool test_realpath(struct zippath *zpath, struct rootdata *r);
static bool find_realpath_nocache(struct zippath *zpath,struct rootdata *r);
static long search_file_which_roots(const char *vp,const int vp_l,const bool path_starts_with_autogen);
static bool find_realpath_any_root1(struct zippath *zpath,const long roots);
static bool find_realpath_any_root(const int opt,struct zippath *zpath,const struct rootdata *onlyThisRoot);
static bool fhandle_can_destroy(struct fHandle *d);
static void _fhandle_destroy(struct fHandle *d,const int fd_evict,const char *path_evict,const int path_evict_l);
static void fhandle_destroy(struct fHandle *d);
static void fhandle_destroy_those_that_are_marked(void);
static void fhandle_init(struct fHandle *d, struct zippath *zpath);
static struct fHandle* fhandle_create(const uint64_t fh, struct zippath *zpath);
static struct fHandle* fhandle_get(const char *path,const uint64_t fh);
static void filldir(const int opt,fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups);
static ino_t next_inode(void);
static ino_t make_inode(const ino_t inode0,struct rootdata *r, const int entryIdx,const char *path);
static ino_t inode_from_virtualpath(const char *vp,const int vp_l);
static int filler_readdir_zip(const int opt,struct zippath *zpath,void *buf, fuse_fill_dir_t filler_maybe_null,struct ht *no_dups);
static int filler_readdir(const int opt,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups);
static int has_sufficient_storage_space(const char *path);
static int realpath_mk_parent(char *realpath,const char *path);
static int PROFILED(_xmp_getattr)(const char *path, struct stat *stbuf);
static int PROFILED(xmp_access)(const char *path, int mask);
static int xmp_readlink(const char *path, char *buf, size_t size);
static int xmp_unlink(const char *path);
static int xmp_rmdir(const char *path);
static uint64_t next_fh();
static int xmp_open(const char *path, struct fuse_file_info *fi);
static int xmp_truncate(const char *path, off_t size IF1(WITH_FUSE_3,,struct fuse_file_info *fi));
static int PROFILED(_xmp_readdir)(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi);
static int xmp_mkdir(const char *path, mode_t mode);
static int create_or_open(const char *path, mode_t mode, struct fuse_file_info *fi);
static int xmp_write(const char *path, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi);
static int xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags));
static int _xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags));
static off_t xmp_lseek(const char *path, const off_t off, const int whence, struct fuse_file_info *fi);
static off_t fhandle_zip_ftell(const struct fHandle *d);
static off_t fhandle_zip_fread(struct fHandle *d, void *buf,  zip_uint64_t nbytes, const char *errmsg);
static bool fhandle_zip_fseek(struct fHandle *d, const off_t offset, const char *errmsg);
static zip_file_t *fhandle_zip_open(struct fHandle *d,const char *msg);
static void fhandle_zip_close(struct fHandle *d);
static off_t fhandle_read_zip(const char *path, char *buf, const off_t size, const off_t offset,struct fHandle *d,struct fuse_file_info *fi);
static uint64_t fd_for_fhandle(long *fd, struct fHandle *d,const int open_flags);
static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi);
static int _xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi);
static int xmp_release(const char *path, struct fuse_file_info *fi);
static int xmp_flush(const char *path, struct fuse_file_info *fi);
static void exit_ZIPsFS(void);
