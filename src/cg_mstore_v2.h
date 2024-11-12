#ifndef _cg_mstore_dot_h
#define _cg_mstore_dot_h


#define _MSTORE_BLOCKS_STACK 0xFF
// #define _MSTORE_BLOCK_ON_STACK_CAPACITY (ULIMIT_S*8)
#define _MSTORE_BLOCK_ON_STACK_CAPACITY 4096
#define SIZEOF_OFF_T sizeof(off_t)
#define _MSTORE_LEADING (SIZEOF_OFF_T*2)

#if WITH_DEBUG_MALLOC
#define _MSTORE_MALLOC_ID(m) ((m->opt&MSTORE_OPT_COUNTMALLOC)?MALLOC_MSTORE:MALLOC_MSTORE_IMBALANCE)
#else
#define _MSTORE_MALLOC_ID(m) 0
#endif // WITH_DEBUG_MALLOC

enum _mstore_operation{_mstore_destroy,_mstore_usage,_mstore_clear,_mstore_contains,_mstore_blocks};
struct mstore{
  char *pointers_data_on_stack[_MSTORE_BLOCKS_STACK];
  char _block_on_stack[_MSTORE_LEADING+_MSTORE_BLOCK_ON_STACK_CAPACITY];
  char **data;
  char *_previous_block;
  const char *name;
  IF1(WITH_DEBUG_MALLOC,int id);
  int mutex;
  int iinstance,_count_malloc;
  off_t bytes_per_block;
  uint32_t opt,capacity;

#ifdef CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
};

typedef uint32_t ht_hash_t;
#endif
