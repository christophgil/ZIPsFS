/* This file was automatically generated.  Do not edit! */
#undef INTERFACE
void debug_read1(const char *path);
bool maybe_cache_zip_entry(enum data_op op,struct fhdata *d,bool always);
bool cache_zip_entry(enum data_op op,struct fhdata *d);
void fhdata_zip_fclose(char *msg,struct fhdata *d);
int fhdata_zip_open(struct fhdata *d,char *msg);
int test_realpath_any_root(struct zippath *zpath,int onlyThisRoot);
int read_zipdir(struct rootdata *r,struct zippath *zpath,void *buf,fuse_fill_dir_t filler_maybe_null,struct ht *no_dups);
void readdir_append(struct my_strg *s,long inode,const char *n,bool append_slash,long size);
bool illegal_char(const char *s);
int readdir_callback(void *arg1,int argc,char **argv,char **name);
int sql_exec(int flags,const char *sql,int(*callback)(void *,int,char **,char **),void *udp);
extern sqlite3 *_sqlitedb;
int find_realpath_any_root(struct zippath *zpath,int onlyThisRoot);
void log_zpath(char *msg,struct zippath *zpath);
int zpath_strcat(struct zippath *zpath,const char *s);
void stat_set_dir(struct stat *s);
void *thread_readdir_async(void *arg);
void threads_root_start();
void *thread_observe_root(void *arg);
