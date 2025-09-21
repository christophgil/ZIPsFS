/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
// cppcheck-suppress-file unusedFunction
#ifndef _cg_debug_dot_c
#define _cg_debug_dot_c
#include "cg_utils.h"
#include "cg_stacktrace.c"
#include <sys/resource.h>


static char *path_of_this_executable(void){
  static char* _p;
  if (!_p){
    char p[512];
    *p=0;
    if (has_proc_fs()) p[readlink("/proc/self/exe",p, 511)]=0;
    _p=strdup_untracked(p);
  }
  return _p;
}


static void provoke_idx_out_of_bounds(void){
  char a[10];
  char *b=a;
  b[11]='x';

}

/////////////////////////////////
/// Stacktrace on error/ EXIT  ///
/////////////////////////////////


////////////////////////////////////////////////////////////////////////////////////////////////////
static void enable_core_dumps(void){
 struct rlimit core_limit={RLIM_INFINITY,RLIM_INFINITY};
  setrlimit(RLIMIT_CORE,&core_limit); // enable core dumps
}
/*********************************************************************************/
////////////////////////////////////
/// recognize certain file names ///
////////////////////////////////////



static bool filepath_contains_blocking(const char *p){
  return p && strstr(p,"blocking");
}

static bool tdf_or_tdf_bin(const char *p){
  if (!p) return false;
  const int p_l=cg_strlen(p);
  return cg_endsWith(0,p,p_l,".tdf",4) || cg_endsWith(0,p,p_l,".tdf_bin",8);
}
static bool filename_starts_year(const char *p,int l){
  if (l<20) return false;
  const int slash=cg_last_slash(p);
  return slash>=0 && !strncmp(p+slash,"/202",4);
}
static bool file_starts_year_ends_dot_d(const char *p){
  const int l=cg_strlen(p);
  if (l<20 || p[l-1]!='d' || p[l-2]!='.') return false;
  return filename_starts_year(p,l);
}
#define report_failure_for_tdf(...) _report_failure_for_tdf(__func__,__LINE__,__VA_ARGS__)
static bool _report_failure_for_tdf(const char *mthd, int line, const char *path){
  if (tdf_or_tdf_bin(path)){
    log_error("%s  %s:%d path=%s",__func__,mthd,line,path);
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
  if(!S_ISDIR(st->st_mode)){
    log_error("assert_dir  stat S_ISDIR %s",p);
    cg_print_stacktrace(0);
  }
  const bool r_ok=cg_access_from_stat(st,R_OK);
  const bool x_ok=cg_access_from_stat(st,X_OK);
  if(!r_ok||!x_ok){
    log_error("access_from_stat %s r=%s x=%s ",p,yes_no(r_ok),yes_no(x_ok));
    cg_log_file_stat("",st);
  }
}
static void assert_r_ok(const char *p, const struct stat *st){
  if(!cg_access_from_stat(st,R_OK)){
    log_error("assert_r_ok  %s  ",p);
    cg_log_file_stat("",st);
    cg_print_stacktrace(0);
  }
}

//////////////////////////////////
/// Count function invocations ///
//////////////////////////////////

#if 0
static int functions_count[functions_l];
static int64_t functions_time[functions_l];
static const char *function_name(enum enum_functions f){
#define C(x) f==x ## _ ? #x :
  return C(xmp_open) C(xmp_access) C(xmp_getattr) C(xmp_read) C(xmp_readdir) C(mcze) "null";
#undef C
}
static void _log_count_b(enum enum_functions f){
  functions_time[f]=currentTimeMillis();
  log(" >>%s%d ",function_name(f),functions_count[f]);
  pthread_mutex_lock(mutex+mutex_log_count);
  functions_count[f]++;
  pthread_mutex_unlock(mutex+mutex_log_count);
}
static void _log_count_e(enum enum_functions f,const char *path){
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









/*
extern char edata, etext, end;
#define IS_LITERAL_STRING(b) ((b)>=&etext && (b)<&edata)
MacOSX: get_etext());get_edata());get_end());
*/








#endif // _cg_debug_dot_c
// 1111111111111111111111111111111111111111111111111111111111111

#if __INCLUDE_LEVEL__ == 0
static void func3(void){
  raise(SIGSEGV);
}
static void func2(void){ func3();}
static void func1(void){ func2();}
int main(int argc, const char *argv[]){
  func1();
}
#endif
