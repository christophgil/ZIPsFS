/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#ifndef _cg_debug_dot_c
#define _cg_debug_dot_c
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <execinfo.h>
#include <signal.h>
#include <ucontext.h>
#include <stdbool.h>
#include <errno.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/time.h>
#include "cg_log.c"
#include "cg_utils.c"
#include <sys/resource.h>
static char *path_of_this_executable(){
  static char* _p;
  if (!_p){
    char p[512];
    p[readlink("/proc/self/exe",p, 511)]=0;
    _p=strdup(p);
  }
  return _p;
}
static void print_trace_using_debugger(){
  fputs(ANSI_INVERSE"print_trace_using_debugger"ANSI_RESET"\n",stderr);
  char pid_buf[30];
  sprintf(pid_buf, "%d", getpid());
  prctl(PR_SET_PTRACER,PR_SET_PTRACER_ANY, 0,0, 0);
  const int child_pid=fork();
  if (!child_pid){
#ifdef __clang__
    execl("/usr/bin/lldb", "lldb", "-p", pid_buf, "-b", "-o","bt","-o","quit" ,NULL);
#else
    execl("/usr/bin/gdb", "gdb", "--batch", "-f","-n", "-ex", "thread", "-ex", "bt",path_of_this_executable(),pid_buf,NULL);
#endif
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
static void addr2line(void *p, void *messageP){
  char cmd[999];
  sprintf(cmd,"/usr/bin/addr2line -p %p -e %s",p,path_of_this_executable());
  system(cmd);
}

static void bt_sighandler(int sig, siginfo_t *psi, void *ctxarg){
  void *trace[16];
  mcontext_t *ctxP=&((ucontext_t *) ctxarg)->uc_mcontext;
  trace[1]=(void *)ctxP->gregs[REG_RIP];
 printf(ANSI_RED"Got signal %d  pid=%d\n"ANSI_RESET,sig,getpid());
  if (sig==SIGSEGV) printf(ANSI_RED"Faulty address is %p, from %p\n"ANSI_RESET,(void*)ctxP->gregs[REG_RSP],trace[1]);
  const int trace_size=backtrace(trace,16);
  /* overwrite sigaction with caller's address */
  char **messages=backtrace_symbols(trace,trace_size);
  /* skip first stack frame (points here) */
  printf(ANSI_INVERSE"bt_sighandler  %d \n"ANSI_RESET,trace_size);
  for(int i=1; i<trace_size; ++i){
    printf(ANSI_FG_GRAY"pid=%d #%d %s"ANSI_RESET"\n",getpid(),i, messages[i]);
    usleep(1000);
    addr2line(trace[i],messages[i]);
    fputs(ANSI_RESET,stdout);
  }
  print_trace_using_debugger();
  exit(0);
}
void init_handler() __attribute((constructor));
void init_handler(){
  return; // DEBUG_NOW
  log_debug_now("init_handler\n");
  struct sigaction sa;
  sa.sa_sigaction=bt_sighandler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags=SA_RESTART|SA_SIGINFO;
  sigaction(SIGSEGV,&sa,NULL);
  sigaction(SIGUSR1,&sa,NULL);
  sigaction(SIGABRT,&sa,NULL);
  log_debug_now("init_handler pid=%d\n",getpid());
  struct rlimit core_limit={RLIM_INFINITY,RLIM_INFINITY};
  assert(!setrlimit(RLIMIT_CORE,&core_limit)); // enable core dumps
}
// sigaction signal signalfd

/*********************************************************************************/

static bool file_starts_year_ends_dot_d(const char *p){
  const int l=my_strlen(p);
  if (l<20 || p[l-1]!='d' || p[l-2]!='.') return false;
  const int slash=last_slash(p);
  if (slash<0 || strncmp(p+slash,"/202",4)) return false;
  return true;
}

static void assert_dir(const char *p, struct stat *st){
  if (!st) return;
  //  log("st=%s  %p\n",p, st);
  if(!S_ISDIR(st->st_mode)){
    log_error("assert_dir  stat S_ISDIR %s",p);
    log_file_stat("",st);
    print_trace_using_debugger();
  }
  bool r_ok=access_from_stat(st,R_OK);
  bool x_ok=access_from_stat(st,X_OK);
  if(!r_ok||!x_ok){
    log_error("access_from_stat %s r=%s x=%s ",p,yes_no(r_ok),yes_no(x_ok));
    log_file_stat("",st);
  }
}
static bool file_ends_tdf_bin(const char *p){
  const int l=my_strlen(p);
  if (l<20) return false;
  const int slash=last_slash(p);
  if (slash<0 || strncmp(p+slash,"/202",4)) return false;
  return endsWith(p,".tdf") || endsWith(p,".tdf_bin");
}
static void assert_r_ok(const char *p, struct stat *st){
  if(!access_from_stat(st,R_OK)){
    log_error("assert_r_ok  %s  ",p);
    log_file_stat("",st);
    print_trace_using_debugger();
  }
}




bool tdf_or_tdf_bin(const char *p){return endsWith(p,".tdf") || endsWith(p,".tdf_bin");}





static bool report_failure_for_tdf(const char *mthd, int res, const char *path){
  if (res && tdf_or_tdf_bin(path)){
    log_debug_abort("xmp_getattr res=%d  path=%s",res,path);
    return true;
  }
  return false;
}


enum functions{xmp_open_,xmp_access_,xmp_getattr_,xmp_read_,xmp_readdir_,mcze_,functions_l};
#if 0
static int functions_count[functions_l];
static long functions_time[functions_l];
static const char *function_name(enum functions f){
#define C(x) f==x ## _ ? #x :
  return C(xmp_open) C(xmp_access) C(xmp_getattr) C(xmp_read) C(xmp_readdir) C(mcze) "null";
#undef C
}
static void log_count_b(enum functions f){
  functions_time[f]=time_ms();
  log(" >>%s%d ",function_name(f),functions_count[f]);
  pthread_mutex_lock(mutex+mutex_log_count);
  functions_count[f]++;
  pthread_mutex_unlock(mutex+mutex_log_count);
}
static void log_count_e(enum functions f,const char *path){
  const int ms=(int)(time_ms()-functions_time[f]);
  pthread_mutex_lock(mutex+mutex_log_count);
  --functions_count[f];
  pthread_mutex_unlock(mutex+mutex_log_count);

  if (ms>1000 && (f==xmp_getattr_ || f==xmp_access_ || f==xmp_open_ || f==xmp_readdir_)){
    log(ANSI_FG_RED" %s"ANSI_FG_GRAY"%d"ANSI_FG_RED",'%s'%d<< "ANSI_RESET,function_name(f),ms,snull(path),functions_count[f]);
  }else{
    log(" %s"ANSI_FG_GRAY"%d"ANSI_RESET",%d<< ",function_name(f),ms,functions_count[f]);
  }
}
#else
#define  log_count_b(f) ;
#define  log_count_e(f,path) ;
#endif

#endif // _cg_debug_dot_c
// 1111111111111111111111111111111111111111111111111111111111111

#if __INCLUDE_LEVEL__ == 0
static void func3(){
  raise(SIGSEGV);
}
void func2(){ func3();}
void func1(){ func2();}
int main(int argc, char *argv[]){
  func1();
}
#endif
