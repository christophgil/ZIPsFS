/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=cg_mstore_v2.c                               ///
/////////////////////////////////////////////////////////////////


#ifndef _cg_mstore_dot_h
#define _cg_mstore_dot_h


#define _MSTORE_BLOCK_ON_STACK_CAPACITY IF01(WITH_TESTING_REALLOC,4096,512)

#define SIZEOF_OFF_T sizeof(off_t)
#define _MSTORE_LEADING (SIZEOF_OFF_T*2)
#define NEXT_MULTIPLE(x,word) ((x+(word-1))&~(word-1))   /* NEXT_MULTIPLE(13,4) -> 16      Word is a power of two */
#define _MSTORE_MASK_SIZE (MSTORE_OPT_MMAP_WITH_FILE-1) // Must be lowest

#define MSTORE_OFFSET_NEXT_FREE(d) ((off_t*)d)[0]
#define MSTORE_BLOCK_CAPACITY(d) ((off_t*)d)[1]
#define mstore_usage(m)           _mstore_common(m,_mstore_usage,NULL)
#define mstore_count_blocks(m)  _mstore_common(m,_mstore_blocks,NULL)
#define mstore_clear(m)           _mstore_common(m,_mstore_clear,NULL)
#define mstore_add(m,src,bytes,align)  memcpy(mstore_malloc(m,bytes,align),src,bytes)
#define mstore_contains(m,pointer)  (0!=_mstore_common(m,_mstore_contains,pointer))
#define mstore_base_path() mstore_set_base_path(NULL)

#if WITH_DEBUG_MALLOC
#define _MSTORE_COUNTER_MMAP(m)   ((m->mstore_counter_mmap?m->mstore_counter_mmap:COUNTm_MSTORE_MMAP))
#define _MSTORE_COUNTER_MALLOC(m) ((m->mstore_counter_mmap?m->mstore_counter_mmap:COUNTm_MSTORE_MMAP)+1)
//#define _MSTORE_MMAP_ID(m) (m->mstore_counter_mmap?m->mstore_counter_mmap:COUNTm_MSTORE_MMAP)
#else
#define _MSTORE_COUNTER_MMAP(m) 0
//#define _MSTORE_MMAP_ID(m) 0
#endif //WITH_DEBUG_MALLOC

enum enum_mstore_operation{_mstore_destroy,_mstore_usage,_mstore_clear,_mstore_contains,_mstore_blocks};
struct mstore{
  char _block_on_stack[_MSTORE_LEADING+_MSTORE_BLOCK_ON_STACK_CAPACITY];
  char **_data;
  char *_previous_sgmt;
  const char *name;
  IF1(WITH_DEBUG_MALLOC,int id);
  int mutex;
  int iinstance, mstore_counter_mmap;
  off_t bytes_per_block;
  uint32_t opt;
  #define MSTORE_OPT_MALLOC (1U<<30)
#define MSTORE_OPT_MMAP_WITH_FILE (1U<<29)  /* Default is anonymous MMAP */
  uint32_t capacity;
#ifdef CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
};



static void mstore_file(char path[PATH_MAX+1],const struct mstore *m,const int block);


#endif
