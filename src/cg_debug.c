/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
#ifndef _cg_debug_dot_c
#define _cg_debug_dot_c
#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/time.h>
#include "cg_log.c"
#include "cg_utils.c"
#include "cg_stacktrace.c"
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


static void provoke_idx_out_of_bounds(){
  char a[10];
  char *b=a;
  b[11]='x';

}

/////////////////////////////////
/// Stacktrace on error/exit  ///
/////////////////////////////////
#define ASSERT(...) (assert(__VA_ARGS__))

////////////////////////////////////////////////////////////////////////////////////////////////////
static void enable_core_dumps(){
 struct rlimit core_limit={RLIM_INFINITY,RLIM_INFINITY};
  setrlimit(RLIMIT_CORE,&core_limit); // enable core dumps
}
/*********************************************************************************/
////////////////////////////////////
/// recognize certain file names ///
////////////////////////////////////
static bool tdf_or_tdf_bin(const char *p){
  return endsWith(p,".tdf") || endsWith(p,".tdf_bin");
}
static bool filename_starts_year(const char *p,int l){
  if (l<20) return false;
  const int slash=last_slash(p);
  return slash>=0 && !strncmp(p+slash,"/202",4);
}
static bool file_starts_year_ends_dot_d(const char *p){
  const int l=my_strlen(p);
  if (l<20 || p[l-1]!='d' || p[l-2]!='.') return false;
  return filename_starts_year(p,l);
}
#define report_failure_for_tdf(...) _report_failure_for_tdf(__func__,__VA_ARGS__)
static bool _report_failure_for_tdf(const char *mthd, int res, const char *path){
  if (res && tdf_or_tdf_bin(path)){
    log_debug_abort("xmp_getattr res=%d  path=%s",res,path);
    return true;
  }
  return false;
}
/*********************************************************************************/
//////////////////////////////////
///  File ///
//////////////////////////////////

static void assert_dir(const char *p, const struct stat *st){
  if (!st) return;
  //  log("st=%s  %p\n",p, st);
  if(!S_ISDIR(st->st_mode)){
    log_error("assert_dir  stat S_ISDIR %s",p);
    log_file_stat("",st);
    print_stacktrace(0);
  }
  bool r_ok=access_from_stat(st,R_OK);
  bool x_ok=access_from_stat(st,X_OK);
  if(!r_ok||!x_ok){
    log_error("access_from_stat %s r=%s x=%s ",p,yes_no(r_ok),yes_no(x_ok));
    log_file_stat("",st);
  }
}
static void assert_r_ok(const char *p, const struct stat *st){
  if(!access_from_stat(st,R_OK)){
    log_error("assert_r_ok  %s  ",p);
    log_file_stat("",st);
    print_stacktrace(0);
  }
}

//////////////////////////////////
/// Count function invocations ///
//////////////////////////////////
enum functions{xmp_open_,xmp_access_,xmp_getattr_,xmp_read_,xmp_readdir_,mcze_,functions_l};
#if 0
static int functions_count[functions_l];
static int64_t functions_time[functions_l];
static const char *function_name(enum functions f){
#define C(x) f==x ## _ ? #x :
  return C(xmp_open) C(xmp_access) C(xmp_getattr) C(xmp_read) C(xmp_readdir) C(mcze) "null";
#undef C
}
static void _log_count_b(enum functions f){
  functions_time[f]=currentTimeMillis();
  log(" >>%s%d ",function_name(f),functions_count[f]);
  pthread_mutex_lock(mutex+mutex_log_count);
  functions_count[f]++;
  pthread_mutex_unlock(mutex+mutex_log_count);
}
static void _log_count_e(enum functions f,const char *path){
  const int64_t ms=currentTimeMillis()-functions_time[f];
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
static void func2(){ func3();}
static void func1(){ func2();}
int main(int argc, char *argv[]){
  func1();
}
#endif
