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
#include <time.h>

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


#define GREEN_SUCCESS ANSI_GREEN" SUCCESS "ANSI_RESET
#define RED_WARNING ANSI_RED" WARNING "ANSI_RESET
#define TERMINAL_CLR_LINE "\r\x1B[K"
#ifndef LOG_STREAM
#define LOG_STREAM stdout
#endif
#define log_struct(st,field,format)   printf("    " #field "=" #format "\n",st->field)
#include "cg_ht_v7.c"

////////////
/// Time ///
////////////
static struct timeval  _startTime;
static int64_t currentTimeMillis(){
  struct timeval tv={0};
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000+tv.tv_usec/1000;
}
static int deciSecondsSinceStart(){
  if (!_startTime.tv_sec) gettimeofday(&_startTime,NULL);
  struct timeval now;
  gettimeofday(&now,NULL);
  return (int)((now.tv_sec-_startTime.tv_sec)*10+(now.tv_usec-_startTime.tv_usec)/100000);
}


static bool _killOnError=false,_logIsSilent=false, _logIsSilentFailed=false,_logIsSilentWarn=false,_logIsSilentError=false;
#define log_argptr() va_list argptr;va_start(argptr,format);vfprintf(LOG_STREAM,format,argptr);va_end(argptr)
static void log_strg(const char *s){
  if(!_logIsSilent && s) fputs(s,LOG_STREAM);
}

static void log_msg(const char *format,...){
  if(!_logIsSilent){
    log_argptr();
  }
}



enum Logs{Log_already_exists,Log_failed,Log_warn,Log_warne,Log_error,Log_succes,Log_debug_now,Log_entered_function,Log_exited_function,Log_cache};



#define log_already_exists(...)    _log_common(__func__,__LINE__,Log_already_exists,__VA_ARGS__)
#define log_failed(...)            _log_common(__func__,__LINE__,Log_failed,__VA_ARGS__)
#define log_warn(...)              _log_common(__func__,__LINE__,Log_warn,__VA_ARGS__)
#define log_warne(...)              _log_common(__func__,__LINE__,Log_warne,__VA_ARGS__)
#define log_error(...)             _log_common(__func__,__LINE__,Log_error,__VA_ARGS__)
#define log_succes(...)            _log_common(__func__,__LINE__,Log_succes,__VA_ARGS__)
#define log_debug_now(...)         _log_common(__func__,__LINE__,Log_debug_now,__VA_ARGS__)
#define log_entered_function(...)  _log_common(__func__,__LINE__,Log_entered_function,__VA_ARGS__)
#define log_exited_function(...)   _log_common(__func__,__LINE__,Log_exited_function,__VA_ARGS__)
#define log_cache(...)             _log_common(__func__,__LINE__,Log_cache,__VA_ARGS__)



static void _log_common(const char *fn,int line,enum Logs t,const char *format,...){
  if(_logIsSilent) return;
  switch(t){
  case Log_entered_function:  log_msg(ANSI_INVERSE">>>"ANSI_RESET);break;
  case Log_exited_function:   log_strg(ANSI_INVERSE"< < < <"ANSI_RESET);break;
  default:;
  }

  log_strg(fn);
  log_msg("():%d ",line);
  switch(t){
  case Log_already_exists:    log_strg("Already exists "ANSI_RESET);break;
  case Log_failed:            log_msg(ANSI_FG_RED" $$ %d Failed "ANSI_RESET,getpid());break;
  case Log_error:             log_msg(ANSI_FG_RED" Error "ANSI_RESET);break;
  case Log_succes:            log_strg(ANSI_FG_GREEN" Success "ANSI_RESET);break;
  case Log_debug_now:         log_msg("\n"ANSI_FG_MAGENTA" Debug "ANSI_RESET" ");break;
  case Log_cache:             log_msg(ANSI_MAGENTA" $$ %d CACHE"ANSI_RESET" ",getpid());break;
  case Log_warne:
  case Log_warn:              log_msg(ANSI_FG_RED" $$ %d Warn "ANSI_RESET,getpid());break;
      default:;
  }




  log_argptr();
  const int e=errno;
  if (e){
    if (t==Log_warne) log_warn("%s\n",strerror(e));
  }
}




/*
  static void _log_already_exists(const char *fn,const char *format,...){
  if(!_logIsSilent){
  log_strg(fn);
  log_strg("() Already exists "ANSI_RESET);
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
*/
static int isPowerOfTwo(unsigned int n){
  return n && (n&(n-1))==0;
}
static void _log_mthd_invoke(const char *s,int count){
  if (!_logIsSilent && isPowerOfTwo(count)) log_msg(ANSI_FG_GRAY" %s=%d "ANSI_RESET,s,count);
}
static void _log_mthd_orig(const char *s,int count){
  if (!_logIsSilent && isPowerOfTwo(count)) log_msg(ANSI_FG_BLUE" %s=%d "ANSI_RESET,s,count);
}

#define log_mthd_invoke() static int __count=0;_log_mthd_invoke(__func__,++__count)
#define log_mthd_orig() static int __count_orig=0;_log_mthd_orig(__func__,++__count_orig)
#define log_abort(...)   log_msg(ANSI_RED"\n cg_log.c Abort %s() ",__func__),log_msg(__VA_ARGS__),log_msg("ANSI_RESET\n"),abort();

/*********************************************************************************/
/* *** stat *** */




// #define log_to_file(chanel,path,format,...){ if (_log_to_file_and_stream(chanel,path,format,__VA_ARGS__)) printf(format,__VA_ARGS__);}

// (buffer-file-name)
static int _count_mmap,_count_munmap;
static void log_mem(FILE *f){
  fprintf(f,"pid=%d  uordblks=%'d  mmap/munmap=%'d/%'d\n",getpid(),mallinfo().uordblks,_count_mmap,_count_munmap);
}


///////////
/// log ///
///////////
/* *** time *** */



#define WARN_SHIFT_MAX 8
static char *_warning_channel_name[1<<WARN_SHIFT_MAX]={0},*_warning_color[1<<WARN_SHIFT_MAX]={0};
//char **_warning_channel_name=NULL;
//char *_warning_pfx[1<<WARN_SHIFT_MAX]={0};
static int _warning_count[1<<WARN_SHIFT_MAX];
#define WARN_FLAG_EXIT (1<<30)
#define WARN_FLAG_MAYBE_EXIT (1<<29)
#define WARN_FLAG_ERRNO (1<<28)
#define WARN_FLAG_ONCE_PER_PATH (1<<27)
#define WARN_FLAG_ONCE (1<<26)
#define warning(...) _warning(__func__,__LINE__,__VA_ARGS__)
static void _warning(const char *fn,int line,const uint32_t channel,const char* path,const char *format,...){
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
    if (path  && !(file=fopen(path,"w"))){
      perror("");
    }
    //#if defined _ht_dot_c_end
    ht_init(&ht,7);
    //#endif
    return;
  }
  const int i=channel&((1<<WARN_SHIFT_MAX)-1);
  if (!initialized){ log_error("Initialization  with warning(0,NULL) required.\n");exit(1);}
  if (!_warning_channel_name[i]){ log_error("_log_channels not initialized: %d.\n",i);exit(1);}
  const int mx=(channel>>WARN_SHIFT_MAX)&0xFFff;
  const char *p=path?path:"";
  bool toFile=true;
  if ((mx && _warning_count[i]>mx) ||
      written>1000*1000*1000 ||
      ((channel&WARN_FLAG_ONCE) && _warning_count[i]) ||
#if defined _ht_dot_c_end
      ((channel&WARN_FLAG_ONCE_PER_PATH) && path &&  ht_get(&ht,path,0,0)) ||
#endif
      false) toFile=false;
  _warning_count[i]++;
  written+=strlen(format)+strlen(p);
  char errx[222];*errx=0;
  pthread_mutex_lock(&mutex);
  if ((channel&WARN_FLAG_ERRNO)){
    const int errn=errno;
    if (errn) strerror_r(errn,errx,sizeof(errx));
  }
  for(int j=2;--j>=0;){
    FILE *f=j?(_logIsSilent?NULL:LOG_STREAM):(toFile?file:NULL);
    if (!f) continue;
    const char *pfx=_warning_channel_name[i], *color=_warning_color[i];
    //fprintf(f,"%s\t%s()\t%d\t%s\t",_warning_pfx[i]?_warning_pfx[i]:(ANSI_FG_RED"ERROR "ANSI_RESET),fn,_warning_count[i],p);
    fprintf(f,"\n%d\t%s%s"ANSI_RESET"\t%s():%d\t%s\t",_warning_count[i],color?color:ANSI_FG_RED,pfx?pfx:"ERROR",fn,line,p);
    va_list argptr; va_start(argptr,format);vfprintf(f,format,argptr);va_end(argptr);
    if (*errx) fprintf(f,"\t%s",errx);
    fputs("\n\n",f);
    if (f==file) fflush(f);
  }
  pthread_mutex_unlock(&mutex);
  if ((channel&WARN_FLAG_EXIT)) exit(1);
  if (_killOnError && (channel&WARN_FLAG_MAYBE_EXIT)){ log_warn("Thread %lx\nTime=%'ld\n  _killOnError\n",pthread_self(),_startTime.tv_sec+_startTime.tv_usec/1000-currentTimeMillis()); exit(9);}

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
