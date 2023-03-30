#define BT_BUF_SIZE 100
#include <sys/wait.h>
#include <sys/prctl.h>
#include <execinfo.h>
#include <signal.h>
void print_backtrace(){
  int j, nptrs;
  void *buffer[BT_BUF_SIZE];
  char **strings;
  nptrs=backtrace(buffer,BT_BUF_SIZE);
  printf("backtrace() returned %d addresses\n",nptrs);
  /* The call backtrace_symbols_fd(buffer,nptrs, STDOUT_FILENO) would produce similar output to the following: */
  strings=backtrace_symbols(buffer,nptrs);
  if (strings==NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }
  for (j=0; j<nptrs; j++) printf("%s\n", strings[j]);
  free(strings);
}

void print_trace() {
  char pid_buf[30];
  sprintf(pid_buf, "%d", getpid());
  char name_buf[512];
  name_buf[readlink("/proc/self/exe",name_buf, 511)]=0;
  prctl(PR_SET_PTRACER,PR_SET_PTRACER_ANY, 0,0, 0);
  int child_pid=fork();
  if (!child_pid) {
    dup2(2,1); // redirect output to stderr - edit: unnecessary?
    execl("/usr/bin/gdb", "gdb", "--batch", "-n", "-ex", "thread", "-ex", "bt",name_buf, pid_buf,NULL);
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}
void my_backtrace(){ /*  compile with options -g -rdynamic */
  void *array[10];
  size_t size=backtrace(array,10);
  backtrace_symbols_fd(array,size,STDOUT_FILENO);
}
  //  raise(SIGSEGV);
void handler(int sig) {
  printf( "ZIPsFS Error: signal %d:\n",sig);
  //my_backtrace();
  print_trace();
  abort();
}

void init_handler() __attribute((constructor));
#include <sys/resource.h>
void init_handler(){
  log_debug_now("init_handler\n");
  signal(SIGSEGV,handler);
  struct rlimit core_limit={RLIM_INFINITY,RLIM_INFINITY};
  assert(!setrlimit(RLIMIT_CORE,&core_limit)); // enable core dumps
}
