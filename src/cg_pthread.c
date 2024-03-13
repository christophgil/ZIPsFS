#include <pthread.h>

/////////////////////////////////////////////////////////////////
/// Debugging ZIPsFS                                          ///
/////////////////////////////////////////////////////////////////

///////////////
/// pthread ///
///////////////
static void print_stacktrace(int calledFromSigInt);
#if DO_ASSERT_LOCK
/* Count recursive locks with (_mutex+mutex). Maybe inc or dec. */
static int mutex_count(int mutex,int inc){
  int *locked=pthread_getspecific(_pthread_key);
  assert(mutex<mutex_roots+ROOTS);
  if (!locked){
    pthread_setspecific(_pthread_key,locked=calloc(NUM_MUTEX,sizeof(int)));
  }
  //log_debug_now("mutex_count %s %d \n",MUTEX_S[mutex],locked[mutex]);
  if (locked[mutex]<0 || locked[mutex]+inc<0){
    print_stacktrace(false);
    DIE("mutex_count %s: %d inc=%d\n",MUTEX_S[mutex],locked[mutex],inc);

  }
  if (inc) locked[mutex]+=inc;
  assert(locked[mutex]>=0);
  return locked[mutex];
}
#define assert_locked(mutex)        assert(_assert_locked(mutex,true));
#define assert_not_locked(mutex)    assert(_assert_locked(mutex,false));
static inline bool _assert_locked(int mutex,bool yesno){
  const int count=mutex_count(mutex,0);
  //log_debug_now("_assert_locked %s  %s: %d\n",yes_no(yesno),MUTEX_S[mutex],count);
  return (yesno==(count>0));
}



#else
#define mutex_count(mutex,inc);
#define assert_locked(mutex)
#define assert_not_locked(mutex)
#endif
#define ASSERT_LOCKED_FHDATA() assert_locked(mutex_fhdata)
