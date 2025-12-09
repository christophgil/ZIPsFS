#ifndef _cg_log_dot_h
#define _cg_log_dot_h

static void _warning(const char *fn,int line,const uint32_t channel,const char* path,const char *format,...);
#define warning(...) _warning(__func__,__LINE__,__VA_ARGS__)
#define log_struct(st,field,format)   fprintf(stderr,"    " #field "=" #format "\n",st->field)

#define WARN_FLAG_EXIT (1<<30)
#define WARN_FLAG_MAYBE_EXIT (1<<29)
#define WARN_FLAG_ERRNO (1<<28)
#define WARN_FLAG_ONCE_PER_PATH (1<<27)
#define WARN_FLAG_ONCE (1<<26)
#define WARN_FLAG_ERROR (1<<25)
#define WARN_FLAG_FAIL (1<<24)
#define WARN_FLAG_SUCCESS (1<<23)
#define WARN_FLAG_WITHOUT_NL (1<<22)
#define WARN_SHIFT_MAX 8
#endif
