int stat_PROFILED(const char *path,struct stat *statbuf);
int lstat_PROFILED(const char *path,struct stat *statbuf);
DIR *opendir_PROFILED(const char *name);
zip_int64_t zip_get_num_entries_PROFILED(zip_t *archive, zip_flags_t flags);

