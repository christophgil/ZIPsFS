/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Controling ZIPsFS by the user accessing magic file names  ///
/////////////////////////////////////////////////////////////////
_Static_assert(WITH_PRELOADRAM,"");
#define MAGIC_SFX_SET_ATIME  ".magicSfxSetAccessTime"

//enum enum_ctrl_action{ACT_NIL,ACT_KILL_ZIPSFS,ACT_FORCE_UNBLOCK,ACT_CANCEL_THREAD,ACT_NO_LOCK,ACT_BAD_LOCK,ACT_CLEAR_CACHE};

static char *ctrl_file_end(){
  static char s[222]={0};
  if (!*s){
    struct timespec t;
    timespec_get(&t,TIME_UTC);
    srand(time(0));
    int r1=rand();
    srand(t.tv_nsec);
    int r2=rand();
    srand(getpid());
    sprintf(s,"%x%x%x%lx%llx",r1,r2,rand(),t.tv_nsec,(LLD)t.tv_sec);
  }
  return s;
}


static bool trigger_files(const bool isGenerated,const char *path,const int path_l){
  if (cg_endsWith(0,path,path_l,ctrl_file_end(),0)){
    int action=-1, para=-1;
    sscanf(path+cg_last_slash(path)+1,"%d_%d_",&action,&para);
    const int thread=PTHREAD_NIL<para && para<PTHREAD_LEN?para:0;
    warning(WARN_DEBUG,path,"Triggered action: %d / %s  para: %d ",action,action<0?"":CTRL_ACTION_S[action],para);
    if (action>0){
      foreach_root(r){
        switch(action){
        case ACT_FORCE_UNBLOCK: _thread_unblock_ignore_existing_pid=true; break;
        case ACT_CANCEL_THREAD:
          if (thread && r->thread[thread]) pthread_cancel(r->thread[thread]);
          break;
        case ACT_CLEAR_CACHE:
          IF1(WITH_CLEAR_CACHE, if (0<=para && para<CLEAR_CACHE_LEN) dircache_clear_if_reached_limit_all(true,para?(1<<para):0xFFFF));
          return true;
        case ACT_KILL_ZIPSFS:
          LOCK(mutex_special_file,make_info(0);if (_info) fputs(_info,stderr));
          IF1(WITH_PROFILER,print_profile());
          DIE("Killed due to  ACT_KILL_ZIPSFS");
          break;
        case ACT_BAD_LOCK:
        case ACT_NO_LOCK:
          IF0(WITH_ASSERT_LOCK, log_warn("WITH_ASSERT_LOCK is 0");return true);
#define p() log_strg("The following will pass ...\n")
#define f() log_strg("The following will trigger assertion ...\n")
#define e() log_error("The previous line should have triggered assertion.\n");
#if 1
#define G() ht_get(&r->dircache_ht,"key",3,0)
#else
#define G() cg_assert_lockedmutex_dircache)
#endif
          if (action==ACT_NO_LOCK){
            LOCK(mutex_dircache,p();G());
            log_strg(GREEN_SUCCESS"\n");
            f();
            G();
            e();
          }else{
            p();
            cg_thread_assert_not_locked(mutex_fhandle);
            log_strg(GREEN_SUCCESS"\n");
            f();
            LOCK(mutex_fhandle,cg_thread_assert_not_locked(mutex_fhandle));
            e();
          }
#undef p
#undef f
#undef e
#undef G
        }/*switch*/
      }/* foreach_root*/
      return true;
    }/*if(action)*/
  }
  char *posHours=strstr(path,MAGIC_SFX_SET_ATIME);
  if (posHours){
    const int len=(int)(posHours-path);
    posHours+=sizeof(MAGIC_SFX_SET_ATIME)-1;
    char path2[MAX_PATHLEN+1];
    cg_stpncpy0(path2,path,len);
    NEW_VIRTUALPATH(path2);

    bool found;FIND_REALPATH(&vipa);
    if (found){
      cg_file_set_atime(RP(),&zpath->stat_rp,3600L*atoi(posHours));
      return true;
    }
  }/*posHours*/
  return false;
}/*trigger_files()*/



// https://www.vbforums.com/showthread.php?557980-RESOLVED-2008-Force-a-refresh-of-all-Win-Explorer-windows-showing-a-particular-folder
// https://ccms-ucsd.github.io/GNPSDocumentation/fileconversion/
// sprintf
