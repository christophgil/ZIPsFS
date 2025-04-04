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


#define ROOTS (1<<LOG2_ROOTS)
static pthread_mutex_t _mutex[NUM_MUTEX];
#if WITH_ASSERT_LOCK
static void destroy_thread_data(void *x){
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
static long _log_count_pthread_lock=0;
#ifdef __cppcheck__
#define lock(mutex) FILE *_cppcheck_rsc_leak_##mutex=fopen("mutex","r")
#define unlock(mutex) if (_cppcheck_rsc_leak_##mutex) fclose(_cppcheck_rsc_leak_##mutex)
#define continue return
#define continue return
#define goto     return
#elif ! defined(WITH_PTHREAD_LOCK) || WITH_PTHREAD_LOCK
static MAYBE_INLINE void lock(int mutex){
  if (mutex){
    _log_count_pthread_lock++;
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
#else
#define lock(mutex) {}
#define unlock(mutex) {}
#endif
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
#if WITH_ASSERT_LOCK
static void cg_mutex_test_1(void){
  log_verbose("");
  lock(mutex_fhandle);
  log_verbose("Within mutex_fhandle");
  log_verbose("count=%d",cg_mutex_count(mutex_fhandle,0));
  LOCK_N(mutex_fhandle,log_verbose("count2=%d",cg_mutex_count(mutex_fhandle,0)));
  ASSERT_LOCKED_FHANDLE();
  log_msg("count2=%d",cg_mutex_count(mutex_fhandle,0));
  unlock(mutex_fhandle);
  ASSERT_LOCKED_FHANDLE();
}
static void cg_mutex_test_2(void){
  log_verbose("");
  LOCK(mutex_fhandle,
       cg_thread_assert_not_locked(mutex_fhandle);
       );
}
#endif // !WITH_ASSERT_LOCK

#endif //_cg_pthread_dot_c
/////////////////////////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////////////////////////
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0 || defined(__cppcheck__)




int main(int argc, const char *argv[]){
  IF1(WITH_ASSERT_LOCK,cg_mutex_count_test());
#ifdef __cppcheck__
  cppcheck_lock_challenge();
#endif
}


#endif
