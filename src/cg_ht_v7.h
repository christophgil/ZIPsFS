#ifndef _cg_ht_dot_h
#define _cg_ht_dot_h
#include "cg_mstore_v2.h"
struct ht_entry{
  uint64_t keylen_hash; /* upper 32 bits keylen  lower 32 bits hash */
  const char* key;  /* key  and keylen_hash are NULL if this slot is empty */
  void* value;
};
#define _HT_LDDIM 4
#define _STACK_HT_ENTRY ULIMIT_S
struct ht{
  const char *name;
    IF1(WITH_DEBUG_MALLOC,int id);
    int mutex;
  int iinstance; /* For Debugging */
  uint32_t flags,capacity,length;
  struct ht_entry entry_zero,*entries,_stack_ht_entry[_STACK_HT_ENTRY];
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
