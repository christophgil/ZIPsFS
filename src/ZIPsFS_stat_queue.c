///////////////////////////////////////////////////////////////////////
/// statqueue - running stat() in separate thread to avoid blocking ///
/// COMPILE_MAIN=ZIPsFS                                             ///
///////////////////////////////////////////////////////////////////////
#define foreach_statqueue_entry(i,q) struct statqueue_entry *q=r->statqueue;for(int i=0;i<STATQUEUE_ENTRY_N;q++,i++)
#define STATQUEUE_ADD_FOUND_SUCCESS (STATQUEUE_ENTRY_N+1)
#define STATQUEUE_ADD_FOUND_FAILURE (STATQUEUE_ENTRY_N+2)
//#define STATQUEUE_ADD_NO_FREE_SLOT (STATQUEUE_ENTRY_N+3)
#define STATQUEUE_DONE(status) (status==STATQUEUE_FAILED||status==STATQUEUE_OK)
static int statqueue_add(const bool verbose,struct stat *stbuf, const char *rp, int rp_l, ht_hash_t rp_hash, struct rootdata *r){
  assert_locked(mutex_statqueue); ASSERT(rp_l<MAX_PATHLEN);
#if WITH_STAT_CACHE
  if (r!=_root_writable){
    LOCK(mutex_dircache,const bool ok=stat_maybe_cache(0,rp,rp_l,rp_hash,stbuf));
    if (ok) return 0;
  }
#endif //WITH_STAT_CACHE
  const int time=deciSecondsSinceStart();
  int iOldestOrFree=0;
  { /* Maybe there is already this path in the queue */
    foreach_statqueue_entry(i,q){
      if (STATQUEUE_DONE(q->status) && q->time+STATQUEUE_TIMEOUT_SECONDS*10>time && q->rp_l==rp_l && q->rp_hash==rp_hash && !strncmp(q->rp,rp,MAX_PATHLEN)){
        if (q->status==STATQUEUE_OK){ *stbuf=q->stat; return STATQUEUE_ADD_FOUND_SUCCESS;}
        return STATQUEUE_ADD_FOUND_FAILURE;
      }
      if (!q->status || r->statqueue[iOldestOrFree].time>q->time) iOldestOrFree=i;
      if (q->status==STATQUEUE_QUEUED && q->time+STATQUEUE_TIMEOUT_SECONDS*10/2>time) return i;
    }
  }
  //  foreach_statqueue_entry(i,q){    if (!q->status || q->time+STATQUEUE_TIMEOUT_SECONDS*10<time){
  //         const int i=iOldestOrFree;
  struct statqueue_entry *q=r->statqueue+iOldestOrFree;
  log_debug_now("iOldestOrFree=%d  q=%s  time=%d ",iOldestOrFree,q->rp,q->time);
  memcpy(q->rp,rp,rp_l);
  q->rp[q->rp_l=rp_l]=0;
  q->rp_hash=rp_hash;
  q->status=STATQUEUE_QUEUED;
  q->flags=verbose?STATQUEUE_FLAGS_VERBOSE:0;
  return iOldestOrFree;
  //}
  //}
  //  log_debug_now("STATQUEUE_ADD_NO_FREE_SLOT %s",rp);
  //  return STATQUEUE_ADD_NO_FREE_SLOT;
}
static bool _statqueue_stat(const bool verbose,const char *path, const int path_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r){
  const int TRY=1000*100;
  enum statqueue_status status=0;
  LOCK(mutex_statqueue,const int i=statqueue_add(verbose,stbuf,path,path_l,hash,r));
    //  int i;
  /* while(true){ */
  /*   LOCK(mutex_statqueue,i=statqueue_add(verbose,stbuf,path,path_l,hash,r)); */
  /*   if (i!=STATQUEUE_ADD_NO_FREE_SLOT) break; */
  /*   usleep(STATQUEUE_TIMEOUT_SECONDS*1000L*1000/10); */
  /*   log_debug_now("STATQUEUE_ADD_NO_FREE_SLOT"); */
  /* } */
  if (i==STATQUEUE_ADD_FOUND_FAILURE){ if (verbose) log_verbose(ANSI_FG_RED"STATQUEUE_ADD_FOUND_FAILURE %s %s"ANSI_RESET,rootpath(r),path);return false;}
  if (i==STATQUEUE_ADD_FOUND_SUCCESS){ if (verbose) log_verbose(ANSI_FG_GREEN"STATQUEUE_ADD_FOUND_SUCCESS %s %s"ANSI_RESET,rootpath(r),path); return true;}
  struct statqueue_entry *q=r->statqueue+i;
  if (verbose)  log_verbose("q->status=%d %s",q->status,path);
  RLOOP(try,TRY){
    status=0;
    if (STATQUEUE_DONE(q->status)){
      LOCK(mutex_statqueue,
           if (STATQUEUE_DONE(q->status) && hash==q->rp_hash && q->rp_l==path_l && !strncmp(q->rp,path,MAX_PATHLEN)){
             *stbuf=q->stat;
             status=q->status;
             /* Keep it in the cache */
           });
    }
    if (STATQUEUE_DONE(status)){
      if (status!=STATQUEUE_OK && hash==q->rp_hash && !config_do_remember_not_exists(path,path_l) && stat_maybe_cache(STAT_ALSO_SYSCALL|STAT_USE_CACHE_FOR_ROOT(r),path,path_l,hash,stbuf)){
        LOCK(mutex_statqueue,if (hash==q->rp_hash) warning(WARN_STAT,path,"stat succeeded on second attempt"));
      }
      if (verbose) log_verbose("status==STATQUEUE_OK %s %s",rootpath(r),path);
      return status==STATQUEUE_OK;
    }
    if (verbose) log_msg("w ");
    usleep(MAX((STATQUEUE_TIMEOUT_SECONDS*1000*1000)/TRY,1));
  }
  if (verbose) log_verbose("returning false  %s %s",rootpath(r),path);
  return false; /* Timeout */
}





#define IS_Q() (q->status==STATQUEUE_QUEUED)
//#define W r->pthread_when_loop_deciSec[PTHREAD_STATQUEUE]
/* Queue for stat and cyclic statfs to check whether root is responding */
static void *infloop_statqueue(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_statqueue_start,r);
  char path[MAX_PATHLEN+1];
  struct stat stbuf;
  for(int j=0;;j++){
    foreach_statqueue_entry(i,q){
      if IS_Q(){
          ht_hash_t hash;
          int l=*path=0;
          LOCK_NCANCEL(mutex_statqueue,if IS_Q(){ memcpy(path,q->rp,(l=q->rp_l)+1);hash=q->rp_hash;});
          path[l]=0;
          if (*path){
            //W=deciSecondsSinceStart();
            const bool ok=stat_maybe_cache(STAT_ALSO_SYSCALL|STAT_USE_CACHE_FOR_ROOT(r),path,l,hash,&stbuf);
            if (q->flags&STATQUEUE_FLAGS_VERBOSE) log_verbose("stat_maybe_cache(STATQUEUE_FLAGS_VERBOSE,%s  %s",path,success_or_fail(ok));
            LOCK_NCANCEL(mutex_statqueue,
                         if (IS_Q() && l==q->rp_l && hash==q->rp_hash && !memcmp(path,q->rp,l)){
                           q->time=deciSecondsSinceStart();
                           q->stat=stbuf;
                           q->status=ok?STATQUEUE_OK:STATQUEUE_FAILED;});
          }
        }
    }
    PRETEND_BLOCKED(PTHREAD_STATQUEUE);
    //W=deciSecondsSinceStart();
    usleep(1000*ROOT_OBSERVE_EVERY_MSECONDS_STATQUE);
    //if (!(j&0xfFF)) log_msg(" SQ%d ",rootindex(r)); // DEBUG_NOW
  }
  pthread_cleanup_pop(0);
}
#undef W
#undef IS_Q
