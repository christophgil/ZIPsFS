#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0 || defined(__cppcheck__)
#define WITH_ASSERT_LOCK 1
#define mutex_fhandle 1
#define mutex_mutex_count 10
#endif


#ifndef _cg_pthread_dot_c
#define _cg_pthread_dot_c
#include <pthread.h>
#include <assert.h>
#include "cg_utils.h"
/////////////////////////////////////////////////////////////////
/// lock
/// Lock with WITH_ASSERT_LOCK 50 nanosec.
/// Lock Without WITH_ASSERT_LOCK 25 nanosec.
/// Not recursive 30 nanosec.
/////////////////////////////////////////////////////////////////
#include "cg_stacktrace.c"
#ifndef NUM_MUTEX //  defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#define NUM_MUTEX 99
#endif


static pthread_mutex_t _mutex[NUM_MUTEX];
#if WITH_ASSERT_LOCK
static void destroy_thread_data(void *x){  // cppcheck-suppress constParameterCallback
}
/* Count recursive locks with (_mutex+mutex). Maybe inc or dec. */
static int cg_mutex_count(int mutex,int inc){
  pthread_mutex_lock(_mutex+mutex_mutex_count);
  static pthread_key_t _pthread_key;
  static bool initialized=false;
  if (!initialized){ pthread_key_create(&_pthread_key,destroy_thread_data); initialized=true;}
  int *locked=pthread_getspecific(_pthread_key);
  if (!locked) pthread_setspecific(_pthread_key,locked=calloc_untracked(NUM_MUTEX,sizeof(int)));
  const int count=locked[mutex]+=inc;
  pthread_mutex_unlock(_mutex+mutex_mutex_count);
  if (inc) assert(count>=0);
  return count;
}
#endif //WITH_ASSERT_LOCK
/////////////////////
/// Lock / unlock ///
/////////////////////
// See pthread_cancel
//static int64_t _wait_lock_ms[mutex_len]; //mutex_len
#define lock_ncancel(mutex) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&_lock_oldstate_unused);lock(mutex)
#define unlock_ncancel(mutex) unlock(mutex);pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&_lock_oldstate_unused)
#define LOCK_NCANCEL_N(mutex,code) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE,&_lock_oldstate_unused);lock(mutex);code;unlock(mutex);pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,&_lock_oldstate_unused)
#define LOCK_NCANCEL(mutex,code) {LOCK_NCANCEL_N(mutex,code);}
#define LOCK_N(mutex,code) lock(mutex); code;  unlock(mutex)

#define LOCK(mutex,code) { LOCK_N(mutex,code);}

/////////////////////
///  __cppcheck__ ///
/////////////////////
#define CPPCHECK_NO_BRANCH_BEGIN() IF1(IS_CHECKING_CODE,FILE *_cppcheck_rsc_leak_no_branch=fopen("mutex","r"))
#define CPPCHECK_NO_BRANCH_END()   IF1(IS_CHECKING_CODE,FCLOSE(_cppcheck_rsc_leak_no_branch))

#ifdef __cppcheck__
// cppcheck-suppress-macro [constVariablePointer,shadowVariable]
#define lock(mutex)     FILE *_cppcheck_rsc_leak_##mutex=fopen("mutex","r")
#define unlock(mutex)   FCLOSE(_cppcheck_rsc_leak_##mutex)
#else /*__cppcheck__*/
#if ! defined(WITH_PTHREAD_LOCK) || WITH_PTHREAD_LOCK
static MAYBE_INLINE void lock(int mutex){
  if (mutex){
    IF1(WITH_ZIPsFS_COUNTERS,COUNTER1_INC(COUNT_PTHREAD_LOCK));
    pthread_mutex_lock(_mutex+mutex);
    IF1(WITH_ASSERT_LOCK,cg_mutex_count(mutex,1));
  }
}
static MAYBE_INLINE void unlock(int mutex){
  if (mutex){
    IF1(WITH_ASSERT_LOCK,cg_mutex_count(mutex,-1));
    pthread_mutex_unlock(_mutex+mutex);
  }
}
#else /*WITH_PTHREAD_LOCK*/
#define lock(mutex) {}
#define unlock(mutex) {}
#endif /*WITH_PTHREAD_LOCK*/
#endif /*__cppcheck__*/
///////////////////////////
/// Debbugging / assert ///
///////////////////////////

#if WITH_ASSERT_LOCK
#define cg_thread_assert_locked(mutex)        assert(cg_mutex_count(mutex,0)>0)
#define cg_thread_assert_not_locked(mutex)    assert(cg_mutex_count(mutex,0)==0)
#define CG_THREAD_FIELDS int mutex
#define CG_THREAD_OBJECT_ASSERT_LOCK(x)       assert(x!=NULL); if (x->mutex) cg_thread_assert_locked(x->mutex)   /*x->thread*/
#else // !WITH_ASSERT_LOCK
#define cg_thread_assert_not_locked(mutex)
#define CG_THREAD_OBJECT_ASSERT_LOCK(x)
#define cg_mutex_count(mutex,inc);
#define cg_thread_assert_locked(mutex)
#define assert_not_locked(mutex)
#endif // !WITH_ASSERT_LOCK
#define ASSERT_LOCKED_FHANDLE() cg_thread_assert_locked(mutex_fhandle)


/////////////////
///  Testing  ///
/////////////////
#if !defined(DIR_ZIPsFS) && defined(__cppcheck__) || defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
static void cg_mutex_challenge_1(void){
  log_verbose("");
  lock(mutex_fhandle);
  log_verbose("Within mutex_fhandle");
  log_verbose("count=%d",cg_mutex_count(mutex_fhandle,0));
  {
    LOCK_N(mutex_fhandle, log_verbose("count2=%d",cg_mutex_count(mutex_fhandle,0)));
  }
  ASSERT_LOCKED_FHANDLE();
  log_msg("count2=%d",cg_mutex_count(mutex_fhandle,0));
  unlock(mutex_fhandle);
  ASSERT_LOCKED_FHANDLE();
}

static void cg_mutex_challenge_2(void){
  log_verbose("");
    for(int i=0;i<2;i++){
  LOCK(mutex_fhandle,
       cg_thread_assert_not_locked(mutex_fhandle);
       //       if (!time(NULL))
       break;
       );
   }
}
#endif // !WITH_ASSERT_LOCK

#endif //_cg_pthread_dot_c
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0 || defined(__cppcheck__)
int main(int argc, const char *argv[]){
  {
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  pthread_mutex_init(_mutex+mutex_fhandle,&_mutex_attr_recursive);
  }
  cg_mutex_challenge_1();
  //  cg_mutex_challenge_2();
}
#endif
