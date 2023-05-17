/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#ifndef _cg_log_dot_c
#define _cg_log_dot_c
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdarg.h>
#include <sys/time.h>
//#include <unistd.h>
//#include <error.h>
#include <errno.h>
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

static bool _logIsSilent=false, _logIsSilentFailed=false,_logIsSilentWarn=false,_logIsSilentError=false;
#define log_argptr() va_list argptr;va_start(argptr,format);vfprintf(LOG_STREAM,format,argptr);va_end(argptr)
static void log_strg(const char *s){
  if(!_logIsSilent && s) fputs(s,LOG_STREAM);
}
static void log_already_exists(const char *format,...){
  if(!_logIsSilent){
    log_strg(" Already exists "ANSI_RESET);
    log_argptr();
  }
}

static void log_msg(const char *format,...){
  if(!_logIsSilent){
    log_argptr();
  }
}

static void log_failed(const char *format,...){
  if(!_logIsSilentFailed){
    log_msg(ANSI_FG_RED" $$ %d Failed "ANSI_RESET,getpid());
    log_argptr();
  }
}
static void log_warn(const char *format,...){
  if(!_logIsSilentWarn){
    log_msg(ANSI_FG_RED" $$ %d Warn "ANSI_RESET,getpid());
    log_argptr();
  }
}
static void log_error(const char *format,...){
  if(!_logIsSilentError){
    log_msg(ANSI_FG_RED" $$ %d Error "ANSI_RESET,getpid());
    log_argptr();
  }
}
static void log_succes(const char *format,...){
  if(!_logIsSilent){
    log_strg(ANSI_FG_GREEN" Success "ANSI_RESET);
    log_argptr();
  }
}
static void log_debug_now(const char *format,...){
  if(!_logIsSilent){
    //log_msg(ANSI_FG_MAGENTA" Debug "ANSI_RESET" %lx ",pthread_self());
    log_msg(ANSI_FG_MAGENTA" Debug "ANSI_RESET" ");
    log_argptr();
  }
}
static void log_entered_function(const char *format,...){
  if(!_logIsSilent){
    log_msg(ANSI_INVERSE">>>"ANSI_RESET);
    log_argptr();
  }
}
static void log_exited_function(const char *format,...){
  if(!_logIsSilent){
    log_strg(ANSI_INVERSE"< < < <"ANSI_RESET);
    log_argptr();
  }
}
static void log_cache(const char *format,...){
  if(!_logIsSilent){
    log_msg(ANSI_MAGENTA" $$ %d CACHE"ANSI_RESET" ",getpid());
    log_argptr();
  }
}
static int isPowerOfTwo(unsigned int n){
  return n && (n&(n-1))==0;
}
static void _log_mthd(char *s,int count){
  if (!_logIsSilent && isPowerOfTwo(count)) log_msg(" %s=%d ",s,count);
}
#define log_mthd_invoke(s) static int __count=0;_log_mthd(ANSI_FG_GRAY #s ANSI_RESET,++__count)
#define log_mthd_orig(s) static int __count_orig=0;_log_mthd(ANSI_FG_BLUE #s ANSI_RESET,++__count_orig)
#define log_abort(...)   fputs(ANSI_RED"\n AAAAAAAAAAAAAAAAAAA cg_log.c Abort",LOG_STREAM),log_msg(__VA_ARGS__),log_msg("ANSI_RESET\n"),abort();
#if 0
#define log_debug_abort(...)   log_abort(__VA_ARGS__)
#else
#define log_debug_abort(...)   log_error(__VA_ARGS__)
#endif


/*********************************************************************************/
/* *** stat *** */




// #define log_to_file(chanel,path,format,...) { if (_log_to_file_and_stream(chanel,path,format,__VA_ARGS__)) printf(format,__VA_ARGS__);}

// (buffer-file-name)
static int _count_mmap,_count_munmap;
static void log_mem(FILE *f){
  fprintf(f,"pid=%d  uordblks=%'d  mmap/munmap=%'d/%'d\n",getpid(),mallinfo().uordblks,_count_mmap,_count_munmap);
}

///////////
/// log ///
///////////
/* *** time *** */
static long _startTimeMillis=0;
static long currentTimeMillis(){
  struct timeval tv={0};
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000+tv.tv_usec/1000;
}

static bool  _killOnError=false;
static void killOnError_exit(){
  if (_killOnError){
    log_msg("Thread %lx\nTime=%'ld\n  killOnError_exit\n",pthread_self(),_startTimeMillis-currentTimeMillis());
    exit(9);
  }
}
#define WARN_SHIFT_MAX 8
char *_warning_channel_name[1<<WARN_SHIFT_MAX];
char *_warning_pfx[1<<WARN_SHIFT_MAX]={0};
static int _warning_count[1<<WARN_SHIFT_MAX];
#define WARN_FLAG_EXIT (1<<30)
#define WARN_FLAG_MAYBE_EXIT (1<<29)
#define WARN_FLAG_ERRNO (1<<28)
#define WARN_FLAG_ONCE_PER_PATH (1<<27)
#define WARN_FLAG_ONCE (1<<26)
static void warning(const unsigned int channel,const char* path,const char *format,...){
  static pthread_mutex_t mutex;
  static bool initialized;
  static FILE *file;
#if defined _ht_dot_c_end
  static struct ht ht;
#endif
  static int written;
  if (!channel){
    initialized=true;
    pthread_mutex_init(&mutex,NULL);
    if (path  && !(file=fopen(path,"w"))) log_debug_now(" Error fopen  %s 'n",path), perror("");
#if defined _ht_dot_c_end
    ht_init(&ht,7);
#endif
    return;
  }
  const int i=channel&((1<<WARN_SHIFT_MAX)-1);
  if (!initialized){ log_error("Initialization  with warning(0,NULL) required.\n");exit(1);}
  if (!_warning_channel_name[i]){ log_error("_log_channels not initialized.\n");exit(1);}
  const int mx=(channel>>WARN_SHIFT_MAX)&0xFFff;
  const char *p=path?path:"";
  bool toFile=true;
  if ((mx && _warning_count[i]>mx) ||
      written>1000*1000*1000 ||
      ((channel&WARN_FLAG_ONCE) && _warning_count[i]) ||
#if defined _ht_dot_c_end
      (channel&WARN_FLAG_ONCE_PER_PATH) && path &&  ht_get(NULL,path) ||
#endif
      false) toFile=false;
  log_debug_now("warning toFile=%d\n",toFile);
  _warning_count[i]++;
  written+=strlen(format)+strlen(p);
  char errx[222];*errx=0;
  if ((channel&WARN_FLAG_ERRNO)){
    const int errn=errno;
    pthread_mutex_lock(&mutex);
    if (errn) strerror_r(errn,errx,sizeof(errx));
  }
  for(int j=2;--j>=0;){
    FILE *f=j?(_logIsSilent?NULL:LOG_STREAM):(toFile?file:NULL);
    if (!f) continue;
    fprintf(f,"%s\t%d\t%s\t",_warning_pfx[i]?_warning_pfx[i]:(ANSI_FG_RED"ERROR "ANSI_RESET),_warning_count[i],p);
    va_list argptr; va_start(argptr,format);vfprintf(f,format,argptr);va_end(argptr);
    if (*errx) fprintf(f,"\t%s",errx);
    fputs("\n\n",f);
    if (f==file) fflush(f);
  }
  pthread_mutex_unlock(&mutex);
  if ((channel&WARN_FLAG_EXIT)) exit(1);
  if ((channel&WARN_FLAG_MAYBE_EXIT)) killOnError_exit();
}

/* *** Enable format string warnings for cppcheck *** */
#ifdef __cppcheck__
#define warning(channel,path,...) printf(__VA_ARGS__)
#define log_warn(...) printf(__VA_ARGS__)
#define log_error(...) printf(__VA_ARGS__)
#define log_cache(...) printf(__VA_ARGS__)
#define log_succes(...) printf(__VA_ARGS__)
#define log_failed(...) printf(__VA_ARGS__)
#define log_debug_now(...) printf(__VA_ARGS__)
#define log_already_exists(...) printf(__VA_ARGS__)
#define log_exited_function(...) printf(__VA_ARGS__)
#define log_entered_function(...) printf(__VA_ARGS__)
#endif
////////////////////////////////////////////////////////////////////////////////
#endif //_cg_log_dot_c
// 1111111111111111111111111111111111111111111111111111111111111
#if __INCLUDE_LEVEL__ == 0
int main(int argc, char *argv[]){
  log_debug_now("argc= %d \n",argc);
  if (0){
  warning(0,argv[1],"%s","");
  open("afafsdfasd",O_WRONLY);
  printf("aaaaaaaaaaa \n");
  _warning_channel_name[1]="Hello";
  warning(1|WARN_FLAG_ERRNO,"path","error ");
  }
}
#endif
