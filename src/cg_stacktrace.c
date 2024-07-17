/* Compare https://raw.githubusercontent.com/Dexter9313/C-stacktrace/master/c-stacktrace.h by  Florian Cabot */
#ifndef EXCEPTIONS
#define EXCEPTIONS

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>
#include "cg_utils.h"
//#if IS_NETBSD || IS_FREEBSD // backtrace(), backtrace_symbols()
#include <execinfo.h>
//#endif

#define MAX_BACKTRACE_LINES 64
static char* _thisPrg;
static FILE *_stckOut;

////////////////////////////////////////////////////////////////////////
#if 0
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
static void cg_print_stacktrace_using_debugger(){
  return;
  fputs(ANSI_INVERSE"print_trace_using_debugger"ANSI_RESET"\n",stderr);
  char pid_buf[30];
  sprintf(pid_buf, "%d", getpid());
  prctl(PR_SET_PTRACER,PR_SET_PTRACER_ANY, 0,0, 0);
  const int child_pid=fork();
  if (!child_pid){
#ifdef __clang__
    execl("/usr/bin/lldb", "lldb", "-p", pid_buf, "-b", "-o","bt","-o","quit" ,NULL);
#else
    if (*path_of_this_executable()) execl("/usr/bin/gdb", "gdb", "--batch", "-f","-n", "-ex", "thread", "-ex", "bt",path_of_this_executable(),pid_buf,NULL);
#endif
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}
#endif
////////////////////////////////////////////////////////////////////////
  /*  See  addr2line_cmd=%s\n",addr2line_cmd);*/
static bool addr2line(const char *program_name, const void *addr, int lineNb){
  char addr2line_cmd[512]={0},line1[1035]={0}, line2[1035]={0};
#if IS_APPLE
  sprintf(addr2line_cmd,"atos -o %.256s %p",program_name,addr);
#else
  sprintf(addr2line_cmd,"addr2line -f -e %.256s %p",program_name,addr);
#endif
  FILE *fp=popen(addr2line_cmd, "r");
  bool ok=fp!=NULL;
  while(ok && fgets(line1,sizeof(line1)-1,fp)){
    if((ok=fgets(line2,sizeof(line2)-1,fp))){ //if we have a pair of lines
      if((ok=(line2[0]!='?'))){ //if symbols are readable
        char *eol=strchr(line1,'\r');
        if (eol || (eol=strchr(line1,'\n'))) *eol=0;
        char *slash=strrchr(line2,'/');
        slash=0;
        fprintf(_stckOut,"[%i] %p in %s at "ANSI_FG_BLUE"%s"ANSI_RESET,lineNb,addr,line1, slash?slash+1:line2);
        fflush(_stckOut);
      }
    }
  }
  if (fp) pclose(fp);
  return ok;
}
static void cg_print_stacktrace(int calledFromSigInt){
#if HAS_BACKTRACE
  void* buffer[MAX_BACKTRACE_LINES];
  const int nptrs=backtrace(buffer,MAX_BACKTRACE_LINES);
   char **strings=backtrace_symbols(buffer,nptrs);
  if(!strings){
    perror("backtrace_symbols");
    EXIT(EXIT_FAILURE);
  }
  for(int i=calledFromSigInt?2:1; i<(nptrs-2); ++i){
    if (!addr2line(_thisPrg, buffer[i], nptrs-2-i-1)){
      fprintf(_stckOut?_stckOut:stderr, " [%i] %s\n", nptrs-2-i-1, strings[i]);
    }
  }
  free(strings);
  #else
  log_warn0("Probably the os does not  supported function backtrace.\nYou may try to #define HAS_BACKTRACE 1\n");
  #endif
}
static void my_signal_handler(int sig){
  signal(sig,SIG_DFL);
  fprintf(_stckOut,"\x1B[41mCaught signal %s\x1B[0m\n",strsignal(sig));
  cg_print_stacktrace(0);
  _Exit(EXIT_FAILURE);
}
static void set_signal_handler(sig_t handler,uint64_t signals){
  // See  struct sigaction sa;       sigaction(sig,&sa,NULL); SIG_DFL
  for(int sig=64,already=0;--sig>=0;){
    if ((1LU<<sig)&signals){
      fprintf(_stckOut,"%s  '%s' ",already++?"and":"Going to register signal handler for",strsignal(sig));
      signal(sig,handler);
    }
  }
  fputc('\n',_stckOut);
}
static void init_sighandler(char* main_argv_0, uint64_t signals,FILE *out){
  _stckOut=out?out:stderr;
  _thisPrg=main_argv_0;
  set_signal_handler(my_signal_handler,signals);
}
#endif
//////////////////////////////////////////////////////////////////////////////////////////
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__==0
static void function_a(){
  //cg_print_stacktrace(1);
  //DIE0("Hello");
  #include <assert.h>
  assert(1==2);
}
static void function_b(){
  function_a();
}
int main(int argc, char *argv[]){
  init_sighandler(argv[0],(1L<<SIGABRT)|(1L<<SIGFPE)|(1L<<SIGILL)|(1L<<SIGINT)|(1L<<SIGSEGV)|(1L<<SIGTERM),_stckOut);



  //  int a=0;
  //a=a/0;
  function_b();
}
#endif
