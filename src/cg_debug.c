#define BT_BUF_SIZE 100
#include <sys/wait.h>
#include <sys/prctl.h>
#include <execinfo.h>
#include <signal.h>
void print_backtrace(){
  void *buffer[BT_BUF_SIZE];
  const int nptrs=backtrace(buffer,BT_BUF_SIZE);
  printf(ANSI_INVERSE"backtrace() returned %d addresses\n"ANSI_RESET,nptrs);
  /* The call backtrace_symbols_fd(buffer,nptrs, STDOUT_FILENO) would produce similar output to the following: */
  char **strings=backtrace_symbols(buffer,nptrs);
  if (strings==NULL) {
    perror("backtrace_symbols");
  }else{
    for (int j=0; j<nptrs; j++) printf("%s\n", strings[j]);
    free(strings);
  }
}

void print_trace() {
  fputs(ANSI_INVERSE"print_trace"ANSI_RESET"\n",stderr);
  char pid_buf[30],name_buf[512];
  sprintf(pid_buf, "%d", getpid());
  name_buf[readlink("/proc/self/exe",name_buf, 511)]=0;
  prctl(PR_SET_PTRACER,PR_SET_PTRACER_ANY, 0,0, 0);
  const int child_pid=fork();
  if (!child_pid) {
    execl("/usr/bin/gdb", "gdb", "--batch", "-f","-n", "-ex", "thread", "-ex", "bt",name_buf, pid_buf,NULL);
    abort(); /* If gdb failed to start */
  } else {
    waitpid(child_pid,NULL,0);
  }
}
void my_backtrace(){ /*  compile with options -g -rdynamic */
  fputs(ANSI_INVERSE"my_backtrace"ANSI_RESET"\n",stderr);
  void *array[10];
  size_t size=backtrace(array,10);
  backtrace_symbols_fd(array,size,STDOUT_FILENO);
}
//  raise(SIGSEGV);
void _handler(int sig) {
  printf( "ZIPsFS Error: signal %d:\n",sig);
  my_backtrace();
  print_trace();
  print_backtrace();
  if (sig==SIGABRT){
    printf("\n_handler calling exit();\n");
    exit(1);
  }
  else abort();
}

void init_handler() __attribute((constructor));
#include <sys/resource.h>
void init_handler(){
  log_debug_now("init_handler\n");
  signal(SIGSEGV,_handler);
  signal(SIGABRT,_handler);

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
    log_error("assert_dir  stat S_ISDIR %s",p);
    log_file_stat("",st);
    print_trace();
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
    log_error("assert_r_ok  %s  ",p);
    log_file_stat("",st);
    print_trace();
  }
}
void debug_my_file_checks(const char *p, struct stat *s){
  if(file_starts_year_ends_dot_d(p)) assert_dir(p,s);
  if (file_ends_tdf_bin(p)) assert_r_ok(p,s);
}
bool tdf_or_tdf_bin(const char *p) {return endsWith(p,".tdf") || endsWith(p,".tdf_bin");}


/* static int _debug_tdf_maybe_sleep[0xffff+1]; */
/* #define DEBUG_TDF_MAYBE_SLEEP(fd) _debug_tdf_maybe_sleep[(fd-FH_ZIP_MIN)&0xffff] */
/* int debug_tdf_maybe_sleep(const char *path, int factor){ */
/*   if (tdf_or_tdf_bin(path)){ */
/*     int us=((random())&0x3f)*factor; */
/*     //log_debug_now(" Begin sleep %d  ",us); */
/*     usleep(us); */
/*     //log_debug_now(" Done sleep "); */
/*     return us; */
/*   } */
/*   return 0; */
/* } */



bool report_failure(const char *mthd, int res, const char *path){
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
const char *function_name(enum functions f){
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
