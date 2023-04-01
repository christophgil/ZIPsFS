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
  char pid_buf[30],name_buf[512];
  sprintf(pid_buf, "%d", getpid());
  name_buf[readlink("/proc/self/exe",name_buf, 511)]=0;
  prctl(PR_SET_PTRACER,PR_SET_PTRACER_ANY, 0,0, 0);
  const int child_pid=fork();
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
void _handler(int sig) {
  printf( "ZIPsFS Error: signal %d:\n",sig);
  my_backtrace();
  //print_trace();
  abort();
}

void init_handler() __attribute((constructor));
#include <sys/resource.h>
void init_handler(){
  log_debug_now("init_handler\n");
  signal(SIGSEGV,_handler);
  struct rlimit core_limit={RLIM_INFINITY,RLIM_INFINITY};
  assert(!setrlimit(RLIMIT_CORE,&core_limit)); // enable core dumps
}
// sigaction signal signalfd

/*********************************************************************************/

bool file_starts_year_ends_dot_d(const char *p){
  const int l=my_strlen(p);
  if (l<20 || p[l-1]!='d' || p[l-2]!='.') return false;
    const int slash=last_slash(p);
    if (slash<0 || strncmp(p+slash,"/202",4)) return false;
  return true;
}

void assert_dir(const char *p, struct stat *st){
  if (!st) return;
  //  log("st=%s  %p\n",p, st);
  if(!S_ISDIR(st->st_mode)){
    log_error("stat S_ISDIR %s",p);
    log_file_stat("",st);
    exit(9);
  }
  bool r_ok=access_from_stat(st,R_OK);
    bool x_ok=access_from_stat(st,X_OK);
   if(!r_ok||!x_ok){
     log_error("access_from_stat %s r=%s x=%s ",p,yes_no(r_ok),yes_no(x_ok));
     log_file_stat("",st);
   }
}
bool file_ends_tdf_bin(const char *p){
  const int l=my_strlen(p);
  if (l<20) return false;
    const int slash=last_slash(p);
    if (slash<0 || strncmp(p+slash,"/202",4)) return false;
    return endsWith(p,".tdf") || endsWith(p,".tdf_bin");
}
void assert_r_ok(const char *p, struct stat *st){
  if(!access_from_stat(st,R_OK)){
    log_error("access_from_stat r_ok %s  ",p);
    log_file_stat("",st);
  }
}


void my_file_checks(const char *p, struct stat *s){
  if(file_starts_year_ends_dot_d(p)) assert_dir(p,s);
  if (file_ends_tdf_bin(p)) assert_r_ok(p,s);
}
