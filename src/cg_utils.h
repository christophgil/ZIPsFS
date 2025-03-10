// cppcheck-suppress-file unusedFunction
#ifndef _cg_utils_dot_h
#define _cg_utils_dot_h
#include <string.h>
#include <stdbool.h>
////////////////////////////////////////
#ifdef __USE_GNU
#define WITH_GNU 1
#else
#define WITH_GNU 0
#endif
////////////////////////////////////////
#if  __clang__
#  define IS_CLANG 1
#else
#  define IS_CLANG 0
#endif
////////////////////////////////////////
#ifdef __APPLE__
#  define IS_APPLE 1
#  define IS_NOT_APPLE 0
# define ST_MTIMESPEC st_mtimespec
#define HAS_FUSE_LSEEK 0
#else
#  define IS_APPLE 0
#  define IS_NOT_APPLE 1
#  define ST_MTIMESPEC st_mtim
#endif
////////////////////////////////////////
#ifndef PATH_MAX // in OpenSolaris
#define PATH_MAX 1024
#endif
////////////////////////////////////////
#ifndef WITH_DEBUG_MALLOC
#define WITH_DEBUG_MALLOC 0
#endif
////////////////////////////////////////
#ifndef WITH_ASSERT_LOCK
#define WITH_ASSERT_LOCK 0
#endif
////////////////////////////////////////
/* Could be changed to "inline" */
#define MAYBE_INLINE
////////////////////////////////////////
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
////////////////////////////////////////
////////////
/// Stat ///
////////////
/* According to POSIX2008,  st_atim, st_mtim and st_ctim of the "stat" structure must be in the sys/stat.h when the _POSIX_C_SOURCE macro is defined as 200809L. */
#if defined(HAS_ST_MTIM) && ! HAS_ST_MTIM
#define ST_MTIMESPEC st_mtimespec
#else
#define ST_MTIMESPEC st_mtim
#endif
////////////////////////////////////////
#define FOR(var,from,to) for(int var=from;var<(to);var++)
#define RLOOP(var,from) for(int var=from;--var>=0;)
#define FOREACH_CSTRING(s,ss)  for(char **s=(char**)ss; *s; s++)

#define ERROR_MSG_NO_PROC_FS "No /proc file system on this computer"


#define MAX_PATHLEN 512
#define DEBUG_NOW 1
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b


#define STRINGIZE(x) STRINGIZE_INNER(x)
#define STRINGIZE_INNER(x) #x

//#define NAME_LINE(base) CONCAT(base, __LINE__)

#define _IF1_IS_1(...) __VA_ARGS__
#define _IF1_IS_0(...)
#define _IF0_IS_0(...) __VA_ARGS__
#define _IF0_IS_1(...)
#define IF1(zeroorone,...) CONCAT(_IF1_IS_,zeroorone)(__VA_ARGS__)
#define IF0(zeroorone,...) CONCAT(_IF0_IS_,zeroorone)(__VA_ARGS__)
#define EXIT(e) fprintf(stderr,"Going to exit %s  "__FILE_NAME__":%d\n",__func__,__LINE__),exit(e)


#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B[42m"
#define ANSI_BLUE "\x1B[44m"
#define ANSI_YELLOW "\x1B[43m"
#define ANSI_CYAN "\x1B[46m"
#define ANSI_WHITE "\x1B[47m"
#define ANSI_BLACK "\x1B[40m"
#define ANSI_FG_GREEN "\x1B[32m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_MAGENTA "\x1B[35m"
#define ANSI_FG_GRAY "\x1B[30;1m"
#define ANSI_FG_BLUE "\x1B[34;1m"
#define ANSI_FG_BLACK "\x1B[100;1m"
#define ANSI_FG_YELLOW "\x1B[33m"
#define ANSI_FG_WHITE "\x1B[37m"
#define ANSI_INVERSE "\x1B[7m"
#define ANSI_BOLD "\x1B[1m"
#define ANSI_UNDERLINE "\x1B[4m"
#define ANSI_RESET "\x1B[0m"


#define GREEN_SUCCESS ANSI_GREEN" SUCCESS "ANSI_RESET
#define GREEN_DONE ANSI_GREEN" DONE "ANSI_RESET
#define RED_WARNING ANSI_RED" WARNING "ANSI_RESET

#define RED_FAIL ANSI_RED" FAIL "ANSI_RESET
#define RED_ERROR ANSI_RED" ERROR "ANSI_RESET
#define TERMINAL_CLR_LINE "\r\x1B[K"
#define SIZEOF_POINTER sizeof(void*)


#define M(op,typ) static MAYBE_INLINE typ op##_##typ(typ a,typ b){ return op(a,b);}
/* We avoid GNU statement expressions and __auto_type  */
M(MIN,int)
M(MIN,long)
M(MAX,int)
M(MAX,long)
#undef M


#define CODE_NOT_NEEDED 0
//static void cg_print_stacktrace(int calledFromSigInt);

//#define DIE(format,...)   do{ fprintf(stderr,format,__VA_ARGS__);fprintf(stderr,ANSI_RED" (in %s at %s:%i)"ANSI_RESET"\n",__func__,__FILE_NAME__,__LINE__);if (*format=='!') perror("");  exit(EXIT_FAILURE); }while(0)

//https://gustedt.wordpress.com/2023/08/08/the-new-__va_opt__-feature-in-c23/

 #ifndef __FILE_NAME__
#define __FILE_NAME__ __FILE__
#endif

#define log_strg(s)  fputs(s,stderr)
#define log_char(c)  fputc(c,stderr)
#define CG_PERROR(msg) fprintf(stderr,"%s:%d ",__FILE_NAME__,__LINE__),perror(msg)
#define cg_free_null(id,x) {cg_free(id,(void*)x),x=NULL;}


#define PRINT_PFX_FUNC_MSG(pfx1,pfx2,sfx,...)  fprintf(stderr,pfx1"%d %s():%i "pfx2,deciSecondsSinceStart()/10,__func__,__LINE__),fprintf(stderr,__VA_ARGS__),puts_stderr(sfx)

#define log_entered_function(...)     PRINT_PFX_FUNC_MSG(ANSI_INVERSE" > > > "ANSI_RESET," ","\n",__VA_ARGS__)
#define log_exited_function(...)      PRINT_PFX_FUNC_MSG(ANSI_INVERSE" < < < "ANSI_RESET," ","\n",__VA_ARGS__)
#define log_error(...)                PRINT_PFX_FUNC_MSG(RED_ERROR,"\n","\n",__VA_ARGS__)
#define log_warn(...)                 PRINT_PFX_FUNC_MSG(RED_WARNING,"\n","\n",__VA_ARGS__)
#define log_debug(...)                PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA" Debug "ANSI_RESET," ","\n",__VA_ARGS__)
#define log_debug_now(...)            PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA" Debug "ANSI_RESET," ","\n",__VA_ARGS__)
#define log_succes(...)               PRINT_PFX_FUNC_MSG(GREEN_SUCCESS," ","\n",__VA_ARGS__)
#define log_failed(...)               PRINT_PFX_FUNC_MSG(RED_FAIL," ","\n",__VA_ARGS__)
#define log_msg(...)                  PRINT_PFX_FUNC_MSG("","",NULL,__VA_ARGS__)
#define log_verbose(...)              PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA""ANSI_YELLOW" verbose "ANSI_RESET," ","\n",__VA_ARGS__)
#define DIE_WITHOUT_STACKTRACE(...)   PRINT_PFX_FUNC_MSG(ANSI_FG_RED"DIE"ANSI_RESET,"\n","\n",__VA_ARGS__),exit(EXIT_FAILURE)
#define DIE(...)                      PRINT_PFX_FUNC_MSG(ANSI_FG_RED"DIE"ANSI_RESET,"\n","\n",__VA_ARGS__),cg_print_stacktrace(0),perror("\n"),exit(EXIT_FAILURE)
#define log_errno(...)     log_error(__VA_ARGS__),perror("")



#define DIE_DEBUG_NOW(...)    DIE(__VA_ARGS__)
#define ULIMIT_S  8192  /* Stacksize [kB]  Output of  ulimit -s  The maximum stack size currently  8192 KB on Linux. Check with ulimit -s*/
#define success_or_fail(b)    ((b)?GREEN_SUCCESS:RED_FAIL)

// ---
#ifndef ASSERT
#define ASSERT(...) assert(__VA_ARGS__)
#endif
// ---
#define calloc_untracked(...) calloc(__VA_ARGS__)
#define malloc_untracked(x) malloc(x)
#define free_untracked(x) free(x)
#define strdup_untracked(x) strdup(x)

#define PRINTF_STRG_OR_STDERR(strg,n,N,...) (strg && N>n?snprintf(strg+n,N-n,__VA_ARGS__):fprintf(stderr,__VA_ARGS__))
// ---

//////////////////////////
///  Printf format     ///
//////////////////////////
#define LLU unsigned long long
#define LLD long long


enum validchars{VALIDCHARS_PATH,VALIDCHARS_FILE,VALIDCHARS_NOQUOTE,VALIDCHARS_NUM};
#endif // _cg_utils_dot_h



//////////////////////////
///  Starts            ///
//////////////////////////

#define LASTCHAR(x) x[sizeof(x)-2]
#define STRLEN(ending) ((int)sizeof(ending)-1)
#define ENDSWITH(s,slen,ending)  ((slen>=STRLEN(ending)) && s[slen-1]==LASTCHAR(ending) && (!memcmp(s+slen-STRLEN(ending),ending,STRLEN(ending))))
#define ENDSWITHI(s,slen,ending) ((slen>=STRLEN(ending)) && (s[slen-1]|32)==(32|LASTCHAR(ending)) && (!strcasecmp(s+slen-STRLEN(ending),ending)))


#define STARTSWITH(s,ending) (!strncmp(s,ending,STRLEN(ending)))
