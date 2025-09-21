/////////////////////////////////
/// COMPILE_MAIN=cg_ht_v7.c   ///
/////////////////////////////////

#ifndef _cg_ht_dot_h
#define _cg_ht_dot_h
#include "cg_mstore_v2.h"
struct ht_entry{
  uint64_t keylen_hash; /* upper 32 bits keylen  lower 32 bits hash */
  const char* key;  /* key  and keylen_hash are NULL if this slot is empty */
  void* value;
};
#define _HT_LDDIM 4

//#define _HT_DIM_STACK IF01(WITH_TESTING_REALLOC,ULIMIT_S,4)
#define _HT_DIM_STACK 1024

struct ht{
  const char *name;
    IF1(WITH_DEBUG_MALLOC,int id);
    int mutex;
  int iinstance, ht_counter_malloc, key_malloc_id; /* For Debugging */
  uint32_t flags;
#define HT_FLAG_KEYS_ARE_STORED_EXTERN (1U<<30)
#define HT_FLAG_NUMKEY (1U<<29)
#define HT_FLAG_BINARY_KEY  (1U<<27)
  uint32_t capacity,length;
  struct ht_entry entry_zero,*_entries,_stack_ht_entry[_HT_DIM_STACK];
  struct mstore keystore_buf, *keystore, *valuestore;
#ifdef CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
  int client_value_int[3];
#ifdef  STRUCT_HT_EXTRA_FIELDS
  STRUCT_HT_EXTRA_FIELDS;
  #endif

};

typedef uint32_t ht_keylen_t;
#endif
