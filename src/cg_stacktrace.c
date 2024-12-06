/* Compare https://raw.githubusercontent.com/Dexter9313/C-stacktrace/master/c-stacktrace.h by  Florian Cabot */
/// COMPILE_MAIN=ZIPsFS                   ///

#ifndef _cg_stacktrace_dot_c
#define _cg_stacktrace_dot_c

#ifndef EXCEPTIONS
#define EXCEPTIONS
#endif //EXCEPTIONS
// ---
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#define _GNU_SOURCE
#endif // defined


#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <stdint.h>
#include <string.h>
#include <err.h>
#include <stdbool.h>
#include "cg_utils.c"
////////////////////
#ifndef HAS_BACKTRACE
#define HAS_BACKTRACE 1
#endif // HAS_BACKTRACE
#if HAS_BACKTRACE
#include <execinfo.h>
#define MAX_BACKTRACE_LINES 64
#endif // HAS_BACKTRACE
////////////////////
////////////////////
////////////////////
#if !defined(HAS_ATOS)
#ifdef __APPLE__
#define HAS_ATOS 1
#define HAS_ADDR2LINE 0
#else
#define HAS_ATOS 0
#define HAS_ADDR2LINE 1
#endif // __APPLE__
#endif // HAS_ATOS
////////////////////
static char* _thisPrg;
static struct stat _thisPrgStat;
static FILE *_stckOut=NULL;
FILE *stckOut(){ return _stckOut?_stckOut:stderr;}

////////////////////////////////////////////////////////////////////////
#if 0
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
static void cg_print_stacktrace_using_debugger(void){
  return;
  puts_stderr(ANSI_INVERSE"print_trace_using_debugger"ANSI_RESET"\n");
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
static char *this_executable(void){
  if (_thisPrg && *_thisPrg=='/') return _thisPrg;
  static bool already;
  static char thisPrgRP[PATH_MAX+1]={0}, tmp[PATH_MAX+1];
  if (!already){
    if (!has_proc_fs()){
      static int reported;
      if (!reported++) log_error("For symbolizing the stack trace, please call the program %s with absolute path.\n", snull(_thisPrg));
    }
    sprintf(tmp,"/proc/%d/exe",getpid());
    realpath(tmp,thisPrgRP);
  }
  already=true;
  return thisPrgRP;
}
////////////////////////////////////////////////////////////////////////
/*
  https://linux-audit.com/linux-aslr-and-kernelrandomize_va_space-setting/
  echo 2 > /proc/sys/kernel/randomize_va_space
  setarch `uname -m` -R    exe
*/
/*
  backtrace_symbols: The symbolic representation of each address consists of the function name (if
  this can be determined), a hexadecimal offset into the function, and the actual return address (in
  hexadecimal).

  addr_ is a virtual address of instruction (return address from stack) But addr2line can't work with
  virtual address, it accepts only offset within executable (or offset within section with the
  option).
  https://github.com/famzah/popen-noshell/blob/master/popen_noshell.c
  https://raw.githubusercontent.com/famzah/popen-noshell/refs/heads/master/popen_noshell.c
  https://raw.githubusercontent.com/famzah/popen-noshell/refs/heads/master/popen_noshell.h
  fp = popen_noshell("ls", (const char * const *)argv, "r", &pclose_arg, 0);
  while (fgets(buf, sizeof(buf)-1, fp))    printf("Got line: %s", buf);

*/
#define WITH_POPEN_NOSHELL 0
#if WITH_POPEN_NOSHELL
#include "rarely_needed/github_popen_noshell.c"
#endif


static bool addr2line_output(FILE *f,char *line,int i){
  if (*line=='?') return false;//if symbols are readable
  char *eol=strchr(line,'\r');
  if (eol || (eol=strchr(line,'\n'))) *eol=0;
  char *slash=strrchr(line,'/');
  //  fprintf(f,"[%i] %p in %s at "ANSI_FG_BLUE"%s"ANSI_RESET,i,line, slash?slash+1:line);
  fprintf(f,"[%i] %s\n",i,line);
  fflush(f);
  return true;
}

static bool addr2line_no_shell(const char *addr,const int iLine){
  if (!HAS_ATOS && !HAS_ADDR2LINE || !this_executable()) return false;
  char addr2line_cmd[512]={0},line[1035]={0};
  const char *aa[9]={0}, *a0;
  int a=1;
#define A(x) aa[a++]=x
#if HAS_ADDR2LINE
  a0="/usr/bin/addr2line";A("-e");A(this_executable());A("-f");A("-p");A((char*)addr);
#elif HAS_ATOS
  a0="atos";A("-o");A(addr);
#else
  fprintf(stckOut(),"Error: Neither HAS_ADDR2LINE nor HAS_ATOS\n");
  return false;
#endif
  aa[0]=a0;
#undef A
  char cmd_flat[99];
  *cmd_flat=0;
  FOREACH_CSTRING(s,aa){ strcat(cmd_flat,*s); strcat(cmd_flat," ");}
  IF1(WITH_POPEN_NOSHELL,struct popen_noshell_pass_to_pclose pclose_arg={0});
  FILE *fp=IF1(WITH_POPEN_NOSHELL,popen_noshell("addr2line",(const char * const*)aa,"r",&pclose_arg,0))  IF0(WITH_POPEN_NOSHELL,popen(cmd_flat,"r"));
  bool ok=false;
  if (!fp) return false;
  while(fgets(line,sizeof(line)-1,fp)){
    if (addr2line_output(stckOut(),line,iLine)) ok=true;
  }
  IF1(WITH_POPEN_NOSHELL, if (pclose_noshell(&pclose_arg)) ok=0);
  IF0(WITH_POPEN_NOSHELL,fclose(fp));
  return ok;
}



static bool addr2line(const char *addr, int lineNb){

  if (!HAS_ATOS && !HAS_ADDR2LINE || !this_executable()) return false;
  char addr2line_cmd[512]={0},line1[1035]={0}, line2[1035]={0};
  sprintf(addr2line_cmd,
#if HAS_ADDR2LINE
          /* "addr2line -f -e %.256s %p" */
          "addr2line -p -f -e %s -a %s",
#elif HAS_ATOS
          "atos -o %s  %s",
#endif
          this_executable(),addr);
  FILE *fp=popen(addr2line_cmd,"r");
  bool ok=fp!=NULL;
  while(ok && fgets(line1,sizeof(line1)-1,fp)){
    if((ok=fgets(line2,sizeof(line2)-1,fp))){ //if we have a pair of lines
      if((ok=(line2[0]!='?'))){ //if symbols are readable
        char *eol=strchr(line1,'\r');
        if (eol || (eol=strchr(line1,'\n'))) *eol=0;
        char *slash=strrchr(line2,'/');
        slash=0;
        fprintf(stckOut(),"[%i] %p in %s at "ANSI_FG_BLUE"%s"ANSI_RESET,lineNb,addr,line1, slash?slash+1:line2);
        fflush(stckOut());
      }
    }
  }
  if (fp) pclose(fp);
  return ok;
}


// https://stackoverflow.com/questions/15129089/is-there-a-way-to-dump-stack-trace-with-line-number-from-a-linux-release-binary
static void cg_print_stacktrace(int calledFromSigInt){
  log_entered_function("%d",HAS_BACKTRACE);
#if HAS_BACKTRACE
  void* buffer[MAX_BACKTRACE_LINES];
  const int nptrs=backtrace(buffer,MAX_BACKTRACE_LINES);
  char **strings=backtrace_symbols(buffer,nptrs);
  if(!strings){
    perror("backtrace_symbols");
    EXIT(EXIT_FAILURE);
  }
  for(int i=calledFromSigInt?2:1; i<(nptrs-2); ++i){
    char addr[80];
    sprintf(addr,"%p",buffer[i]);
    const char *open=strchr(strings[i],'('), *close=strchr(strings[i],')');
    if ( !(open && close && addr2line_no_shell(addr, nptrs-2-i-1))){
      fprintf(stckOut(), "! [%i] %s\n", nptrs-2-i-1, strings[i]);
    }
  }
  free_untracked(strings);
#else
  log_warn("Probably the os does not  supported function backtrace.\nYou may try to #define HAS_BACKTRACE 1\n");
#endif
}
static void _cg_print_stacktrace_test1(void){
  cg_print_stacktrace(0);
}
static void _cg_print_stacktrace_test2(void){
  _cg_print_stacktrace_test1();
}
#define CG_PRINT_STACKTRACE_TEST_MAX 2
static void cg_print_stacktrace_test(int what){
  switch(what){
  case 0:
    log_msg("Going to sleep for 10s. You can press Ctrl-C to create SIGINT");
    usleep(1000*1000*10);break;
  case 1:
    log_msg("Going to print a stack trace. Press Enter to continue "); cg_getc_tty();
    _cg_print_stacktrace_test2();
    break;
  case 2:{
#ifndef    __cppcheck__
    log_msg("Going to write to address 0. Press Enter to continue "); cg_getc_tty();

    char *s=NULL;
    strcpy(s,"Force nullPointer dereference");
#endif
  } break;
  }
}

static void my_signal_handler(int sig){
  signal(sig,SIG_DFL);
  fprintf(stckOut(),"\x1B[41mCaught signal %s\x1B[0m\n",strsignal(sig));
  cg_print_stacktrace(0);
#ifdef CLEANUP_BEFORE_EXIT
  CG_CLEANUP_BEFORE_EXIT();
#else
  fprintf(stckOut(),"Not defined: CG_CLEANUP_BEFORE_EXIT()\n");
#endif
  fflush(stderr);
  _Exit(EXIT_FAILURE);
}

/* #if ! defined(HAS_SIG_T) || HAS_SIG_T */
/* static void set_signal_handler(sig_t handler,uint64_t signals){ */
/*   // See  struct sigaction sa;       sigaction(sig,&sa,NULL); SIG_DFL */
/*   for(int sig=64,already=0;--sig>=0;){ */
/*     if ((1LU<<sig)&signals){ */
/*       fprintf(stckOut(),"%s  '%s' ",already++?"and":"Going to register signal handler for",strsignal(sig)); */
/*       signal(sig,handler); */
/*     } */
/*   } */
/*   fputc('\n',stckOut()); */
/* } */
/* #endif */




static void init_sighandler(char* main_argv_0, uint64_t signals,FILE *out){

  _stckOut=out;
  stat(_thisPrg=main_argv_0,&_thisPrgStat);

#if 0
  set_signal_handler(my_signal_handler,signals);
#else
  if (signals==0) signals=(1L<<SIGABRT)|(1L<<SIGFPE)|(1L<<SIGILL)|(1L<<SIGINT)|(1L<<SIGSEGV)|(1L<<SIGTERM);
  for(int sig=64,already=0;--sig>=0;){
    if ((1LU<<sig)&signals){
      struct sigaction act={0};
      act.sa_handler=&my_signal_handler;
      sigaction(sig,&act,NULL);
    }
  }
#endif
}








#endif //_cg_stacktrace_dot_c
//////////////////////////////////////////////////////////////////////////////////////////
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include <assert.h>
static void function_a(void){
  //cg_print_stacktrace(1);
  //DIE("Hello");
  assert(1==2);
}



int main(int argc, char *argv[]){
  assert(stckOut()!=NULL);
  _thisPrg=argv[0];
  init_sighandler(argv[0],(1L<<SIGABRT)|(1L<<SIGFPE)|(1L<<SIGILL)|(1L<<SIGINT)|(1L<<SIGSEGV)|(1L<<SIGTERM),stderr);
  cg_print_stacktrace_test(1);

}

#endif
