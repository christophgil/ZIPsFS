#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sys/mman.h>
#include <fcntl.h> /* open .. */
#include <errno.h>
#include <assert.h>
#define SIZEOF_OFF_T sizeof(off_t)
#define _MSTORE_LEADING (SIZEOF_OFF_T*2)
#include "cg_utils.c"
#include "cg_stacktrace.c"
#ifndef CG_THREAD_OBJECT_ASSERT_LOCK
#define CG_THREAD_OBJECT_ASSERT_LOCK(x)
#endif

////////////
/// Hash ///
////////////
/* Retur0n 64-bit FNV-1a hash for key (NUL-terminated). See  https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function */
/* https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed */

typedef uint32_t ht_hash_t;
#define HT_HASH_MAX UINT32_MAX
static uint32_t hash32(const char* key, const uint32_t len){
  uint32_t hash=2166136261U;
  RLOOP(i,len){
    hash^=(uint32_t)(unsigned char)(key[i]);
    hash*=16777619U;
  }
  return !hash?1:hash; /* Zero often means that hash still needs to be computed */
  return hash;
}
static uint64_t hash64(const char* key, const off_t len){
  uint64_t hash64=14695981039346656037UL;
  RLOOP(i,len){
    hash64^=(uint64_t)(unsigned char)(key[i]);
    hash64*=1099511628211UL;
  }
  return hash64;
}
static int hash32_java(const char *str,const off_t len){
  int hashj=0;
  FOR(i,0,len) hashj=31*hashj+str[i];
  return hashj;
}
static ht_hash_t hash_value_strg(const char* key){
  return !key?0:hash32(key,strlen(key));
}

/////////////////////////////////////////////////////////
#define _MSTORE_BLOCKS_STACK 0xFF
#define _MSTORE_BYTES_OF_FIRST_BLOCK (ULIMIT_S*8)
#define NEXT_MULTIPLE(x,word) ((x+(word-1))&~(word-1))   /* NEXT_MULTIPLE(13,4) -> 16*/
#define MSTORE_OPT_MALLOC (1U<<30)
#define MSTORE_OPT_MMAP_WITH_FILE (1U<<29)
#define _MSTORE_MASK_OPTS (MSTORE_OPT_MALLOC|MSTORE_OPT_MMAP_WITH_FILE)
#define _MSTORE_FREE_DATA(m)  if (m->data!=m->pointers_data_on_stack) free(m->data)
#define BLOCK_OFFSET_NEXT_FREE(d) ((off_t*)d)[0]
#define BLOCK_CAPACITY(d) ((off_t*)d)[1]
enum _mstore_operation{_mstore_destroy,_mstore_usage,_mstore_clear,_mstore_contains,_mstore_blocks};
struct mstore{
  char *pointers_data_on_stack[_MSTORE_BLOCKS_STACK];
  //char _data_of_first_block_on_stack[ULIMIT_S*8];
  char _data_of_first_block_on_stack[_MSTORE_LEADING+_MSTORE_BYTES_OF_FIRST_BLOCK];
  char **data;
  int id;
  off_t bytes_per_block;
  uint32_t opt,blockPreviouslyFilled,capacity;
#if defined CG_THREAD_FIELDS
  CG_THREAD_FIELDS;
#endif
};
#if defined CG_THREAD_METHODS
CG_THREAD_METHODS(mstore);
#endif

#define mstore_usage(m)           _mstore_common(m,_mstore_usage,NULL)
#define mstore_count_blocks(m)  _mstore_common(m,_mstore_blocks,NULL)
#define mstore_clear(m)           _mstore_common(m,_mstore_clear,NULL)
#define mstore_add(m,src,bytes,align)  memcpy(mstore_malloc(m,bytes,align),src,bytes)
#define mstore_contains(m,pointer)  (0!=_mstore_common(m,_mstore_contains,pointer))
static off_t _mstore_common(struct mstore *m,int opt,const void *pointer){
  CG_THREAD_OBJECT_ASSERT_LOCK(m);
  off_t sum=0;
  RLOOP(block,m->capacity){
    char *d=m->data[block];
    if (d){
      switch(opt){
      case _mstore_destroy:
        m->data[block]=NULL;
        if (d!=m->_data_of_first_block_on_stack){
          if (m->opt&MSTORE_OPT_MALLOC) free(d);
          else munmap(d,_MSTORE_LEADING+BLOCK_CAPACITY(d));
        }
        break;
      case _mstore_usage: sum+=BLOCK_OFFSET_NEXT_FREE(d); break;
      case _mstore_blocks:  if (BLOCK_OFFSET_NEXT_FREE(d)>_MSTORE_LEADING) sum++;  break;
      case _mstore_clear:
        BLOCK_OFFSET_NEXT_FREE(d)=_MSTORE_LEADING;
        //memset(d+_MSTORE_LEADING,0,BLOCK_CAPACITY(d)*SIZEOF_OFF_T);
        // fprintf(stderr," xxxxxxxx BLOCK_OFFSET_NEXT_FREE(d)=%ld   BLOCK_CAPACITY(d)=%ld",BLOCK_OFFSET_NEXT_FREE(d),BLOCK_CAPACITY(d));
        //memset(d+_MSTORE_LEADING,0,BLOCK_CAPACITY(d));
        break;
      case _mstore_contains:{
        if (d+_MSTORE_LEADING<=(char*)pointer && (char*)pointer<d+BLOCK_CAPACITY(d)) return true;
      } break;
      }
    }
  }
  return sum;
}

//////////////////////////////////////////////////////////////////////////
// MMAP with file                                                       //
//////////////////////////////////////////////////////////////////////////
static const char *mstore_set_base_path(const char *f){
  static char base[MAX_PATHLEN];
  if (f && !*base) cg_recursive_mkdir(cg_copy_path(base,f));
  return base;
}

//////////////////////////////////////////////////////////////////////////
// Initialize                                                           //
//   dir: Path of existing folder where the mmap files will be written. //
//   dim: Size of the mmap files.  Will be rounded to next n*4096       //
//////////////////////////////////////////////////////////////////////////
static struct mstore *mstore_init(struct mstore *m,const int size_and_opt){
  memset(m,0,sizeof(struct mstore));
  m->data=m->pointers_data_on_stack;
  if (size_and_opt&MSTORE_OPT_MMAP_WITH_FILE){
    static atomic_int count;
    m->id=atomic_fetch_add(&count,1);
    assert(0==(size_and_opt&MSTORE_OPT_MALLOC));
  }
  m->bytes_per_block=MAX_int(16,size_and_opt&~_MSTORE_MASK_OPTS);
  m->opt=size_and_opt&_MSTORE_MASK_OPTS;
  m->capacity=_MSTORE_BLOCKS_STACK;
  return m;
}
////////////////////////////////////////
// Allocate memory of number of bytes //
////////////////////////////////////////
static int _mstore_openfile(struct mstore *m,uint32_t block,const off_t adim){
  char path[PATH_MAX];
  snprintf(path,PATH_MAX-1,"%s/%02d_%03u.cache",mstore_set_base_path(NULL),m->id,block);
  log_entered_function("path: %s",path);
  const int fd=open(path,O_RDWR|O_CREAT|O_TRUNC,0640);
  if (fd<2) DIE("open failed: %s fd: %d\n",path,fd);
  //  struct stat st; if (fstat(fd,&st)<0) log_error("fstat failed: %s\n",path);
  if (ftruncate(fd,adim) || write(fd,"",1)!=1){
    log_errno("write failed: %s\n",path);
    return 0;
  }
  return fd;
}

static void *mstore_malloc(struct mstore *m,const off_t bytes, const int align){
  //  log_entered_function0("");
  static int count;  bool verbose=0; //++count>27290;
  if (verbose) log_debug("Count=%d\n",count);
  CG_THREAD_OBJECT_ASSERT_LOCK(m);  assert(align==1||align==2||align==4||align==8);
  for(uint32_t s0=0;;s0++){
    const uint32_t block=!s0?m->blockPreviouslyFilled:s0-1;
    if (block>=m->capacity){
      const uint32_t c=m->capacity;
      char **dd=calloc((m->capacity=c<<1),SIZEOF_OFF_T);
      if (!dd) DIE0("calloc returns NULL");
      memcpy(dd,m->data,c*SIZEOF_OFF_T);
      _MSTORE_FREE_DATA(m);
      m->data=dd;
    }
    char *d=m->data[block];
    if (!d){ /* Need allocate memory for block */
      //log_debug_now0("!d\n");
      off_t block_capacity=block?m->bytes_per_block:_MSTORE_BYTES_OF_FIRST_BLOCK;
      block_capacity=MAX(block_capacity,bytes);
      block_capacity=NEXT_MULTIPLE(block_capacity,4096); /* Aligned array dim */
      if (!block && block_capacity<=_MSTORE_BYTES_OF_FIRST_BLOCK){
        d=m->_data_of_first_block_on_stack;
      }else if (m->opt&MSTORE_OPT_MALLOC){
        if (!(d=malloc(_MSTORE_LEADING+block_capacity))) DIE0("malloc failed");
      }else{
        const int fd=(m->opt&MSTORE_OPT_MMAP_WITH_FILE)?_mstore_openfile(m,block,block_capacity):0;
        const int flags=fd?MAP_SHARED:MAP_PRIVATE|MAP_ANONYMOUS;
        if (MAP_FAILED==(d=mmap(0,_MSTORE_LEADING+block_capacity,PROT_READ|PROT_WRITE,flags,fd,0))) d=NULL;
        if (!d) DIE0("mmap failed");
      }
      m->data[block]=d;
      BLOCK_OFFSET_NEXT_FREE(d)=_MSTORE_LEADING;
      BLOCK_CAPACITY(d)=block_capacity;
    }/*if(!d)*/
    assert(d!=NULL);
    off_t begin=BLOCK_OFFSET_NEXT_FREE(d); begin=NEXT_MULTIPLE(begin,align);
    if (begin+bytes<BLOCK_CAPACITY(d)){ /* Block has sufficient capacity*/
      BLOCK_OFFSET_NEXT_FREE(d)=begin+bytes;
      if (verbose) log_debug("begin: %ld bytes: %ld  BLOCK_CAPACITY: %ld\n",begin,bytes,BLOCK_CAPACITY(d));
      m->blockPreviouslyFilled=block;
      assert(mstore_contains(m,d+begin)); /* DEBUG_NOW */
      return d+begin;
    }
  }/*Loop block*/
  //  fprintf(stderr,"}");
  return NULL;
}
static void mstore_destroy(struct mstore *m){
  _mstore_common(m,_mstore_destroy,NULL);
  _MSTORE_FREE_DATA(m);
}
//////////////////
// Add a string //
// ///////////////
static const char *mstore_addstr(struct mstore *m, const char *str,off_t len){
  if (!str) return NULL;
  if (!*str) return "";
  if (!len) len=strlen(str);
  char *s=mstore_malloc(m,len+1,1);
  assert(s!=NULL);
  memcpy(s,str,len);
  s[len]=0;
  //assert(!strncmp(s,str,len));
  return s;
}
//////////////////////////////////////////////////////////////////////////
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__==0
//#include "cg_utils.c"
struct mytest{
  int64_t l;
  bool b;
};
static void test_mstore1(int argc,char *argv[]){
  const char *dir="/home/cgille/tmp/test/mstore_mstore1";
  //cg_recursive_mkdir(dir);
  struct mstore m={0};
  mstore_init(&m,20);
  char *tt[99999];
  const int method=atoi(argv[1]);
  printf("method=%d\n",method);
  FOR(i,2,argc){
    char *a=argv[i];
    switch(2){
    case 0:
      tt[i]=mstore_malloc(&m,1+strlen(a),8);
      assert( ((long)tt[i]) %8==0);
      strcpy(tt[i],a);
      break;
    case 1:
      tt[i]=(char*)mstore_addstr(&m,a,strlen(a));
      break;
    case 2:
      tt[i]=(char*)mstore_add(&m,a,strlen(a)+1,4);
      break;
    }
  }
  tt[argc-3]="This string should rise error";
  FOR(i,2,argc){
    if (!tt[i]){ fprintf(stderr,"Error tt[i] is NULL\n"); EXIT(1);}
    fprintf(stderr,"%4d %p  %s\n",i,(void*)tt[i],tt[i]);
    if (strcmp(tt[i],argv[i])){ DIE(RED_ERROR" tt memaddr: %p tt=%s argv=%s\n",(void*)tt[i],tt[i],argv[i]); }
  }
  fprintf(stderr," Usage %lu\n",mstore_usage(&m));
  mstore_destroy(&m);

}

static void test_mstore2(int argc, char *argv[]){
  struct mstore ms;
  mstore_init(&ms,256|MSTORE_OPT_MALLOC);
  const int N=atoi(argv[1]);
  int data[N][10];
  int *intern[N];
  FOR(i,0,N){
    //fprintf(stderr,"%d\n",i);
    FOR(j,0,5){
      data[i][j]=rand();
    }
  }
  const int data_l=10*sizeof(int);
  FOR(i,0,N){
    //    int * internalized=mstore_add(&ms,data[i],data_l,sizeof(int));
    int * internalized=mstore_malloc(&ms,data_l,sizeof(int));
    //tern[i]=(int*)ht_set(&ht,(char*)data[i],data_l,0,data[i]);
  }
  mstore_destroy(&ms);
}

int main(int argc,char *argv[]){
  switch(1){
  case 0: printf("NEXT_MULTIPLE: %d -> %d\n",atoi(argv[1]),NEXT_MULTIPLE(atoi(argv[1]),atoi(argv[2])));break;
  case 1: test_mstore1(argc,argv); break; /* 1 $(seq 10) */
  case 2: test_mstore2(argc,argv); break; /* 10 */
  }
}
#endif //__INCLUDE_LEVEL__
