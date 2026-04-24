#ifndef _cg_log_dot_h
#define _cg_log_dot_h

static void _viamacro_warning(const char *fn,int line,const uint32_t channel,const char* path,const char *format,...);
#define warning(...) _viamacro_warning(__func__,__LINE__,__VA_ARGS__)
#define log_struct(st,field,format)   fprintf(stderr,"    " #field "=" #format "\n",st->field)

enum {WARN_SHIFT_MAX=8};
enum {WARN_FLAG_EXIT=1<<30,WARN_FLAG_MAYBE_EXIT=1<<29,WARN_FLAG_ERRNO=1<<28,WARN_FLAG_ONCE_PER_PATH=1<<27,WARN_FLAG_ONCE=1<<26,WARN_FLAG_ERROR=1<<25,WARN_FLAG_FAIL=1<<24,WARN_FLAG_SUCCESS=1<<23,WARN_FLAG_WITHOUT_NL=1<<22};


#endif
