/*
  Copyright (C) 2023
  christoph Gille
  This program can be distributed under the terms of the GNU GPLv3.
*/
#ifndef _cg_log_dot_c
#define _cg_log_dot_c
#include <pthread.h>
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
#define TERMINAL_CLR_LINE "\r\x1B[K"

#ifndef LOG_STREAM
#define LOG_STREAM stdout
#endif

#define log_struct(st,field,format)   printf("    " #field "=" #format "\n",st->field)
void prints(char *s){ if (s) fputs(s,LOG_STREAM);}
#define log(...)  printf(__VA_ARGS__)
#define log_already_exists(...) (prints(" Already exists "ANSI_RESET),log(__VA_ARGS__)
#define log_failed(...)  log(ANSI_FG_RED" $$ %d Failed "ANSI_RESET,getpid()),log(__VA_ARGS__)
#define log_warn(...)  log(ANSI_FG_RED" $$ %d Warn "ANSI_RESET,getpid()),log(__VA_ARGS__)
#define log_error(...)  log(ANSI_FG_RED" $$ %d Error "ANSI_RESET,getpid()),log(__VA_ARGS__)
#define log_succes(...)  prints(ANSI_FG_GREEN" Success "ANSI_RESET),log(__VA_ARGS__)
#define log_debug_now(...)   log(ANSI_FG_MAGENTA" Debug "ANSI_RESET" %lx ",pthread_self()),log(__VA_ARGS__)
#define log_entered_function(...)   log(ANSI_INVERSE">>>"ANSI_RESET),log(__VA_ARGS__)
#define log_exited_function(...)   prints(ANSI_INVERSE"< < < <"ANSI_RESET),log(__VA_ARGS__)
#define log_cache(...)  log(ANSI_MAGENTA" $$ %d CACHE"ANSI_RESET" ",getpid()),log(__VA_ARGS__)
int isPowerOfTwo(unsigned int n){ return n && (n&(n-1))==0;}
void _log_mthd(char *s,int count){ if (isPowerOfTwo(count)) log(" %s=%d ",s,count);}
#define log_mthd_invoke(s) static int __count=0;_log_mthd(ANSI_FG_GRAY #s ANSI_RESET,++__count)
#define log_mthd_orig(s) static int __count_orig=0;_log_mthd(ANSI_FG_BLUE #s ANSI_RESET,++__count_orig)
#define log_abort(...)   prints(ANSI_RED"\n AAAAAAAAAAAAAAAAAAA cg_log.c Abort"),log(__VA_ARGS__),log("ANSI_RESET\n"),abort();

#if 0
#define log_debug_abort(...)   log_abort(__VA_ARGS__)
#else
#define log_debug_abort(...)   log_error(__VA_ARGS__)
#endif

#endif
