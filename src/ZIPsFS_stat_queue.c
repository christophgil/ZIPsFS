///////////////////////////////////////////////////////////////////////
/// statqueue - running stat() in separate thread to avoid blocking ///
/// COMPILE_MAIN=ZIPsFS                                             ///
///////////////////////////////////////////////////////////////////////
#define E(q) (q->rp_l==rp_l && q->rp_hash==hash && !strncmp(q->rp,rp,MAX_PATHLEN))
#define foreach_statqueue_entry(i,q) struct statqueue_entry *q=r->statqueue;for(int i=0;i<STATQUEUE_ENTRY_N;q++,i++)
#define STATQUEUE_DONE(status) (status==STATQUEUE_FAILED||status==STATQUEUE_OK)
static int debug_statqueue_count_entries(struct rootdata *r){
  int count=0;
  foreach_statqueue_entry(i,q) if (q->refcount>0) count++;
  return count;
}
static struct statqueue_entry *statqueue_add(const bool verbose,struct stat *stbuf, const char *rp, int rp_l, ht_hash_t hash, struct rootdata *r){
  cg_thread_assert_locked(mutex_statqueue); ASSERT(rp_l<MAX_PATHLEN);
  struct statqueue_entry *unused=NULL;
  foreach_statqueue_entry(i,q){
    if (q->status==STATQUEUE_QUEUED && E(q)) return q; /* Use entry same path */
    if (q->refcount<=0){ unused=q; break; } /* Free slot */
  }
  if ((q=unused)){
    ASSERT(q->refcount>=0);
    memcpy(q->rp,rp,rp_l);
    q->rp[q->rp_l=rp_l]=0;
    q->rp_hash=hash;
    q->status=STATQUEUE_QUEUED;
    q->flags=verbose?STATQUEUE_FLAGS_VERBOSE:0;
  }
  return q;
}


static int _stat_queue1(const bool verbose,const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r){
  struct statqueue_entry *q=NULL,*u=NULL;
  for(int j=0;;j++){
    LOCK(mutex_statqueue, q=statqueue_add(verbose,stbuf,rp,rp_l,hash,r); if (q) q->refcount++);
    if (q) break;
    usleep(STATQUEUE_SLEEP_USECONDS);
    if (is_square_number(j))  warning(WARN_STAT,rp,"No empty slot in queue %d",j);
  }
  if (verbose) log_verbose("q->status=%d %s",q->status,rp);
  int sleep=STATQUEUE_SLEEP_USECONDS/8,sum=0, ok=0;
  while((sum+=sleep)<STATQUEUE_TIMEOUT_SECONDS*1000*1000){
    usleep(sleep);
    sleep+=(sleep>>3);
    if (STATQUEUE_DONE(q->status)){ u=q; break;}
  }
  LOCK(mutex_statqueue,
       if (u && STATQUEUE_DONE(u->status) && E(u)){
         *stbuf=u->stat;
         ok=(STATQUEUE_OK==u->status)?1:-1;
       }
       q->refcount--;
       ASSERT(q->refcount>=0));
  if (verbose) log_verbose(" %s %s ok: %d",report_rootpath(r),rp,ok);
  return ok;
}

static bool stat_queue(const bool verbose,const char *rp, const int rp_l,const ht_hash_t hash,struct stat *stbuf,struct rootdata *r){
  FOR(iTry,0,4){
    int ok=_stat_queue1(verbose,rp,rp_l,hash,stbuf,r);
    if (ok){
      if (iTry){
        warning(WARN_RETRY,rp,"stat_queue succeeded on attempt %d\n",iTry);
        rootdata_counter_inc(rp,COUNT_RETRY_STAT,r);
      }
      return ok==1;
    }
  }
  return false;
}
/* Queue for stat and cyclic statvfs to check whether root is responding */
static void *infloop_statqueue(void *arg){
  struct rootdata *r=arg;
  pthread_cleanup_push(infloop_statqueue_start,r);
  char rp[MAX_PATHLEN+1];
  struct stat stbuf;
  const int sleep_min=10;
  int sleep=sleep_min;
  for(int j=0;;j++){
    foreach_statqueue_entry(i,q){
      if (q->status==STATQUEUE_QUEUED){
        ht_hash_t hash;
        int rp_l=0;
        LOCK_NCANCEL(mutex_statqueue,if (q->status==STATQUEUE_QUEUED){ memcpy(rp,q->rp,(rp_l=q->rp_l)+1);hash=q->rp_hash;});
        rp[rp_l]=0;
        if (rp_l){
          const bool ok=stat_maybe_cache(STAT_ALSO_SYSCALL|STAT_USE_CACHE_FOR_ROOT(r),rp,rp_l,hash,&stbuf);
          if (q->flags&STATQUEUE_FLAGS_VERBOSE) log_verbose("stat_maybe_cache(STATQUEUE_FLAGS_VERBOSE,%s  %s",rp,success_or_fail(ok));
          LOCK_NCANCEL(mutex_statqueue,
                       if (q->status==STATQUEUE_QUEUED && E(q)){
                         timespec_get(&q->time,TIME_UTC);
                         q->stat=stbuf;
                         q->status=ok?STATQUEUE_OK:STATQUEUE_FAILED;});
          //        log_msg(" %sSQ%d.%d "ANSI_RESET,ok?ANSI_FG_GREEN:ANSI_FG_RED,rootindex(r),j);
          sleep=sleep_min;
        }
      }
    }
    observe_thread(r,PTHREAD_STATQUEUE);
    usleep(STATQUEUE_SLEEP_USECONDS);
    if (sleep++>STATQUEUE_SLEEP_USECONDS) sleep=STATQUEUE_SLEEP_USECONDS;
    //if (!(j&0xfFF)) log_msg(" Q%d ",rootindex(r));
  }
  pthread_cleanup_pop(0);
}
#undef E
