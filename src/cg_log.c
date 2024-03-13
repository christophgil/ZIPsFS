/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#define _GNU_SOURCE
#ifndef _cg_log_dot_c
#define _cg_log_dot_c
#include <pthread.h>
#include <stdarg.h>  // provides va_start
#define log_struct(st,field,format)   fprintf(stderr,"    " #field "=" #format "\n",st->field)
#include "cg_ht_v7.c"
#include "cg_utils.c"

////////////
/// Time ///
////////////
//
/* errno -l | sed  's|^\([^ ]*\)\ .*|C(\1);|1' | paste -d " "  - - |paste -d " "  - - |paste -d " "  - - */
static bool _killOnError=false, _logIsSilent=false, _logIsSilentFailed=false,_logIsSilentWarn=false,_logIsSilentError=false;

/*********************************************************************************/
/* *** RAM *** */
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
#define WARN_FLAG_ERROR (1<<25)
static FILE *_fWarnErr[2];
#define warning(...) _warning(__func__,__LINE__,__VA_ARGS__)
static void _warning(const char *fn,int line,const uint32_t channel,const char* path,const char *format,...){
  const int e=errno;
  static pthread_mutex_t mutex;
  static bool initialized;
  static struct ht ht;
  static int written;
  const bool iserror=(channel&(WARN_FLAG_ERROR|WARN_FLAG_MAYBE_EXIT));
  if (!channel){
    assert(_fWarnErr[0]!=NULL);
    assert(_fWarnErr[1]!=NULL);
    initialized=true;
    pthread_mutex_init(&mutex,NULL);
    ht_init(&ht,7);
    return;
  }
  const int i=channel&((1<<WARN_SHIFT_MAX)-1);
  if (!initialized){ log_error0("Initialization  with warning(0,NULL) required.\n");EXIT(1);}
  if (!_warning_channel_name[i]){ log_error0("_log_channels not initialized: %d.\n");EXIT(1);}
  const int mx=(channel>>WARN_SHIFT_MAX)&0xFFff;
  const char *p=path?path:"";
  bool toFile=true;
  if ((mx && _warning_count[i]>mx) ||
      written>1000*1000*1000 ||
      ((channel&WARN_FLAG_ONCE) && _warning_count[i]) ||

      ((channel&WARN_FLAG_ONCE_PER_PATH) && path &&  ht_only_once(&ht,path,0))) toFile=false;
  _warning_count[i]++;
  written+=strlen(format)+strlen(p);

  pthread_mutex_lock(&mutex);
  for(int j=2;--j>=0;){
    FILE *f=j?(_logIsSilent?NULL:stderr):!toFile?NULL:_fWarnErr[iserror];
    if (!f) continue;
    const char *pfx=_warning_channel_name[i], *color=_warning_color[i];
    //fprintf(f,"%s\t%s()\t%d\t%s\t",_warning_pfx[i]?_warning_pfx[i]:(ANSI_FG_RED"ERROR "ANSI_RESET),fn,_warning_count[i],p);
    fprintf(f,"\n%d\t%s%s"ANSI_RESET"\t%s():%d\t%s\t",_warning_count[i],color?color:ANSI_FG_RED,pfx?pfx:"ERROR",fn,line,p);
    va_list argptr; va_start(argptr,format);vfprintf(f,format,argptr);va_end(argptr);
    if ((channel&WARN_FLAG_ERRNO) && e){
      fputc('\t',f);
      fprint_strerror(f,e);
    }
    fputs("\n\n",f);
    if (f==_fWarnErr[0]||f==_fWarnErr[1]) fflush(f);
  }
  pthread_mutex_unlock(&mutex);
  if ((channel&WARN_FLAG_EXIT)) EXIT(1);
  if (_killOnError && (channel&WARN_FLAG_MAYBE_EXIT)) DIE("Thread %lx\nTime=%'ld\n  _killOnError\n",pthread_self(),_startTime.tv_sec+_startTime.tv_usec/1000-currentTimeMillis());

}
#endif //_cg_log_dot_c


#if __INCLUDE_LEVEL__ == 0
int main(int argc, char *argv[]){
  log_debug_now("argc= %d \n",argc);
  if (0){
    warning(0,argv[1],"%s","");
    open("afafsdfasd",O_WRONLY);
    fprintf(stderr,"aaaaaaaaaaa \n");
    _warning_channel_name[1]="Hello";
    warning(1|WARN_FLAG_ERRNO,"path","error ");
  }
}
#endif
