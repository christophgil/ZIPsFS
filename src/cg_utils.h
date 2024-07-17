#ifndef _cg_utils_dot_h
#define _cg_utils_dot_h
#include "cg_utils_early.h"



#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

#include <dirent.h>

#include <stdio.h>
#include <assert.h>
#include <sys/types.h>
#include <unistd.h>
// #if __linux__
// #include <linux/limits.h>
// #elif __FreeBSD__
// #include <sys/limits.h>
// #endif

#include <stdint.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
// #include <sys/vfs.h>
#if IS_LINUX
#include <sys/statvfs.h>
#elif IS_FREEBSD
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#endif
#include <sys/param.h>
#include <locale.h>



#define MAYBE_INLINE
#define FOR(var,from,to) for(int var=from;var<(to);var++)
#define RLOOP(var,from) for(int var=from;--var>=0;)

#define ERROR_MSG_NO_PROC_FS "No /proc file system on this computer"
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif
#define MAX_PATHLEN 512
#define DEBUG_NOW 1
#define FREE(s) free((void*)s),s=NULL
#define CONCAT(a, b) CONCAT_INNER(a, b)
#define CONCAT_INNER(a, b) a ## b
#define NAME_LINE(base) CONCAT(base, __LINE__)

#define STRINGIZE(x) STRINGIZE_INNER(x)
#define STRINGIZE_INNER(x) #x



#define FREE2(a) { void *NAME_LINE(tmp)=(void*)a; a=NULL;free(NAME_LINE(tmp));}

#define _IF1_IS_1(...) __VA_ARGS__
#define _IF1_IS_0(...)
#define _IF0_IS_0(...) __VA_ARGS__
#define _IF0_IS_1(...)
#define IF1(zeroorone,...) CONCAT(_IF1_IS_,zeroorone)(__VA_ARGS__)
#define IF0(zeroorone,...) CONCAT(_IF0_IS_,zeroorone)(__VA_ARGS__)
#define EXIT(e) fprintf(stderr,"Going to exit %s  "__FILE__":%d\n",__func__,__LINE__),exit(e)


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

//#define DIE(format,...)   do{ fprintf(stderr,format,__VA_ARGS__);fprintf(stderr,ANSI_RED" (in %s at %s:%i)"ANSI_RESET"\n",__func__,__FILE__,__LINE__);if (*format=='!') perror("");  exit(EXIT_FAILURE); }while(0)

//https://gustedt.wordpress.com/2023/08/08/the-new-__va_opt__-feature-in-c23/
#define log_strg(s)  fputs(s,stderr)
#define log_char(c)  fputc(c,stderr)
#define CG_PERROR(msg) fprintf(stderr,"%s:%d ",__FILE__,__LINE__),perror(msg)

#define PRINT_PFX_FUNC_MSG(pfx1,pfx2,sfx,format,...)   fprintf(stderr,pfx1"%d s  %s():%i)"pfx2 format sfx,deciSecondsSinceStart()/10,__func__,__LINE__,__VA_ARGS__)

#define log_entered_function(...)     PRINT_PFX_FUNC_MSG(ANSI_INVERSE" > > > "ANSI_RESET,"\n","\n",__VA_ARGS__)
#define log_exited_function(...)     PRINT_PFX_FUNC_MSG(ANSI_INVERSE" < < < "ANSI_RESET,"\n","\n",__VA_ARGS__)

#define log_error(...)     PRINT_PFX_FUNC_MSG(RED_ERROR,"\n","\n",__VA_ARGS__)
#define log_warn(...)      PRINT_PFX_FUNC_MSG(RED_WARNING,"\n","\n",__VA_ARGS__)
#define log_debug(...)     PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA" Debug "ANSI_RESET," ","\n",__VA_ARGS__)
#define log_debug_now(...) PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA" Debug "ANSI_RESET," ","\n",__VA_ARGS__)

#define log_succes(...)    PRINT_PFX_FUNC_MSG(GREEN_SUCCESS," ","\n",__VA_ARGS__)
#define log_failed(...)    PRINT_PFX_FUNC_MSG(RED_FAILED," ","\n",__VA_ARGS__)
#define log_msg(...)       PRINT_PFX_FUNC_MSG("","","",__VA_ARGS__)
#define log_verbose(...)   PRINT_PFX_FUNC_MSG(ANSI_FG_MAGENTA""ANSI_YELLOW" verbose "ANSI_RESET," ","\n",__VA_ARGS__)
#define DIE_WITHOUT_STACKTRACE(...)           PRINT_PFX_FUNC_MSG(ANSI_FG_RED"DIE"ANSI_RESET,"\n","\n",__VA_ARGS__),exit(EXIT_FAILURE)
#define DIE(...)     PRINT_PFX_FUNC_MSG(ANSI_FG_RED"DIE"ANSI_RESET,"\n","\n",__VA_ARGS__),cg_print_stacktrace(0),perror("\n"),exit(EXIT_FAILURE)
#define log_errno(...)     log_error(__VA_ARGS__),perror("")

#define log_entered_function0(msg) log_entered_function("%s",msg)
#define log_exited_function0(msg) log_exited_function("%s",msg)
#define log_error0(msg) log_error("%s",msg)
#define log_msg0(msg) log_msg("%s",msg)
#define log_debug0(msg) log_debug("%s",msg)
#define log_debug_now0(msg) log_debug_now("%s",msg)
#define log_warn0(msg) log_warn("%s",msg)
#define log_verbose0(msg) log_verbose("%s",msg)
#define log_failed0(msg) log_failed("%s",msg)
#define log_succes0(msg) log_succes("%s",msg)
#define log_errno0(msg) log_errno("%s",msg)

#define DIE0(msg) DIE("%s",msg)
#define DIE_DEBUG_NOW(...) DIE(__VA_ARGS__)
#define DIE_DEBUG_NOW0(...) DIE0(__VA_ARGS__)

#define ULIMIT_S  8192  // Stacksize [kB]  Output of  ulimit -s  The maximum stack size currently  8192 KB on Linux. Check with ulimit -s*/


#define success_or_fail(b)  ((b)?GREEN_SUCCESS:RED_FAIL)


#ifndef ASSERT
#define ASSERT(...) assert(__VA_ARGS__)
#endif
#else
#endif // _cg_utils_dot_h

#define LLU unsigned long long
#define LLD long long

/* According to POSIX2008, the "st_atim", "st_mtim" and "st_ctim" members of the "stat" structure must be available in the <sys/stat.h> header when the _POSIX_C_SOURCE macro is defined as 200809L. */
