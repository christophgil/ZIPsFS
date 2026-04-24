/////////////////////////////////
/// COMPILE_MAIN=cg_ht_v7.c   ///
/////////////////////////////////

#ifndef _cg_ht_dot_h
#define _cg_ht_dot_h
#include "cg_mstore_v2.h"

typedef uint64_t ht_keylen_hash_t;
/**************************************************************/
/* Check size requirements for ZIPsFS                         */
/* See function make_inode()                                  */
/* Maybe need change type ht_keylen_hash_t in the far future  */
/**************************************************************/
_Static_assert(sizeof(ino_t)<=sizeof(ht_keylen_hash_t),"At least 64 bit and same or larger than ino_t. Please change definition of ht_keylen_hash_t");
_Static_assert(sizeof(char*)>=8,"Pointers at least  64 bit because it will be used to encode root index and zip entry index.");



typedef struct {
  ht_keylen_hash_t keylen_hash; /* upper 32 bits keylen  lower 32 bits hash */
  const char* key;  /* key  and keylen_hash are NULL if this slot is empty */
  void* value;
} ht_entry_t;
enum { _HT_LDDIM=4, _HT_DIM_STACK=1024};




typedef struct struct_ht{
  const char *name;
  IF1(WITH_DEBUG_MALLOC,int id);
  int mutex, iinstance, ht_counter_malloc, key_malloc_id;
  enum {HT_FLAG_KEYS_ARE_STORED_EXTERN=1<<30,HT_FLAG_NUMKEY=1<<29,HT_FLAG_BINARY_KEY=1<<30} flags;
  uint32_t capacity,length;
  ht_entry_t entry_zero,*_entries,_stack_ht_entry[_HT_DIM_STACK];
  mstore_t keystore_buf, *keystore,  *valuestore /* For transient cache */;
  struct struct_ht *key_interner;
#ifdef CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
  int client_value_int[3];
#ifdef  STRUCT_HT_EXTRA_FIELDS
  STRUCT_HT_EXTRA_FIELDS;
#endif
} ht_t;
static ht_t *_ht_last_initialized;
#define HT_SET_MUTEX(m) ht_set_mutex(m,_ht_last_initialized)
#define HT_SET_ID(id) ht_set_id(id,_ht_last_initialized)

typedef uint32_t ht_keylen_t;


#endif
