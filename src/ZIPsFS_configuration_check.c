/////////////////////////////////////////////////////////////////
/// COMPILE _MAIN=ZIPsFS                                      ///
/// Exit if incorrect settings for specific mountpoint        ///
/// Maybe I have forgotten to restore after debugging ...     ///
/////////////////////////////////////////////////////////////////


#define T(var,x)  if (!(var x)) warn=true, printf("%-30s   ( "ANSI_FG_BLUE"Value"ANSI_RESET" "ANSI_ITALIC"OPERATOR"ANSI_RESET" Recommended )   ( "ANSI_FG_BLUE"%d"ANSI_RESET" %s )   "ANSI_FG_RED"false"ANSI_RESET"\n",#var,var,#x)
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "ZIPsFS_configuration.h"
#include "ZIPsFS_early.h"
#include "cg_utils.h"
#include <stdio.h>
#else // __INCLUDE_LEVEL__ IIIIIIIIIIIIIIIIIIIIIIIIIIIIII
#endif //__INCLUDE_LEVEL__

#define WITH_PTHREAD_LOCK 1 /*  Should be true. Only for testing if suspect deadlock set to 0 */

#define CHECK_DEBUG_OFF()\
  T(DEBUG_DIRCACHE_COMPARE_CACHED,== 0);\
  T(DEBUG_TRACK_FALSE_GETATTR_ERRORS,== 0);\
  T(WITH_EXTRA_ASSERT,== 0);\
  T(WITH_ASSERT_LOCK,== 0);\
  T(WITH_TESTING_TIMEOUTS,== 0);\
  T(WITH_TESTING_UNBLOCK,== 0);\
  T(WITH_TESTING_REALLOC,== 0);\
  T(WITH_PTHREAD_LOCK,== 1);\
  T(WITH_ZIPENTRY_PLACEHOLDER,== 1);\

#define CHECK_TIMEOUT_ON_OFF(on)\
  T(WITH_TIMEOUT_OPENFILE,== on);\
  T(WITH_TIMEOUT_OPENZIP,== on);\
  T(WITH_TIMEOUT_STAT,== on);\
  T(WITH_TIMEOUT_READDIR,== on);\
  T(WITH_CANCEL_BLOCKED_THREADS,== on);

#define CHECK_CACHES_ON()\
  T(WITH_DIRCACHE,== 1);\
  T(WITH_PRELOADFILERAM,== 1);\
  T(WITH_ZIPINLINE_CACHE,== 1);\
  T(WITH_STAT_CACHE,== 1);\
  T(WITH_TRANSIENT_ZIPENTRY_CACHES,== 1)

static bool check_configuration1(){
 bool warn=false;
  CHECK_DEBUG_OFF();
  CHECK_CACHES_ON();
  CHECK_TIMEOUT_ON_OFF(0);
  T(WITH_EVICT_FROM_PAGECACHE,== 1);
  T(WITH_ZIPINLINE,== 1);
  T(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,== 0);
  T(WITH_ZIPENTRY_PLACEHOLDER,== 1);
  T(WITH_AUTOGEN,== 1);
  T(WITH_CCODE,== 1);
  T(WITH_INTERNET_DOWNLOAD,== 1);
  T(WITH_POPEN_NOSHELL,== 0);
  T(READDIR_TIMEOUT_SECONDS,> 9);
  T(WITH_ZIPENTRY_PLACEHOLDER,== 1);
  return warn;
}
#if !(defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0)
static bool check_configuration(const char *mnt){
  bool warn=check_configuration1();
  char *m=strdup_untracked(mnt);
  cg_str_replace(0,m,0,".charite.de",0,"",0);
  cg_str_replace(0,m,0,"//",2,"/",1);
  {
    const char *M="/s-mcpb-ms03/union/.mountpoints/is/";
    if (!strncmp(m,M,strlen(M))){
      log_verbose("Recognized mount-point %s",M);
      T(WITH_AUTOGEN,== 0);
      T(WITH_ZIPINLINE,== 1);
    }
  }
  //log_debug_now("m: %s mnt: %s",m,mnt);
  free_untracked(m);
  if (!HAS_BACKTRACE){
    fprintf(stderr,"Warning: No stack-traces can be written in case of a program error.\n");
    warn=true;
  }else if (!HAS_ADDR2LINE && !HAS_ATOS){
    fprintf(stderr,"For better stack-traces (debugging) it is recommended to install "ANSI_FG_BLUE IF01(IS_APPLE,"addr2line (package binutils)","atos")ANSI_RESET".\n");
    warn=true;
  }
#if WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
  if (DIRECTORY_CACHE_SIZE*NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE<64*1024*1024){
    log_msg(RED_WARNING"Small file attribute and directory cache of only %d\n",NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE*NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE/1024);
    warn=true;
  }
#endif

  if (_root_writable){
    #define EXPLAIN_WRITABLE "The first upstream root is writable.'nIf an empty string is given as first root, it will not be possible to store files in the virtual file system.\n"
    if(_root_writable->retain_dirname_l){
      fprintf(stderr,"%s"RED_WARNING"The first root retains the path component '%s'\n"
              "Writing files will not work. This can be fixed by adding a terminal slash: '%s"ANSI_FG_RED"/"ANSI_RESET".'\n\n",EXPLAIN_WRITABLE,_root_writable->retain_dirname,_root_writable->rootpath);
      warn=true;
    }
    if (_root_writable->remote){
      fprintf(stderr,"%sThe leading double slash indicates a remote path which is probably not intended\n",EXPLAIN_WRITABLE);
      warn=true;
    }
  }else{
    warn=true;
    fprintf(stderr,"The first root-path is an empty string.\n%s\n",EXPLAIN_WRITABLE);
  }
  IF1(WITH_AUTOGEN,if (!cg_is_member_of_group("docker")){ log_warn(HINT_GRP_DOCKER); warn=true;});
  return warn;
}
#else //__INCLUDE_LEVEL__



int main(int argc, char *argv[]){

  return check_configuration1();
}


#endif //__INCLUDE_LEVEL__
#undef T
