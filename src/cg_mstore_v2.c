// cppcheck-suppress-file unusedFunction
/////////////////////////////////////////
/// xxxxCOMPILE_MAIN=ZIPsFS               ///
/// Storage place for misc variables  ///
/////////////////////////////////////////

#ifndef _cg_mstore_dot_c
#define _cg_mstore_dot_c
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif  // !MAP_ANONYMOUS
#include <stdatomic.h>
#include <fcntl.h> /* open .. */
#include <errno.h>
#include <assert.h>


#include "cg_utils.h"
#include "cg_mstore_v2.h"
#include "cg_utils.c"
#include "cg_stacktrace.c"
#include "cg_pthread.c"
#ifndef CG_THREAD_OBJECT_ASSERT_LOCK
#define CG_THREAD_OBJECT_ASSERT_LOCK(x)
#endif

////////////
/// Hash ///
////////////
/* Retur0n 64-bit FNV-1a hash for key (NUL-terminated). See  https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function */
/* https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed */
#define _WITH_MSTORE_PREVIOUS 0
#define _MSTORE_BLOCK(m,i) (i?m->_data[i]:m->_block_on_stack)
#define _MSTORE_IS_MALLOC(m) ((m->opt&MSTORE_OPT_MALLOC)!=0)
#define HT_HASH_MAX UINT32_MAX
static int hash32_java(const char *str,const off_t len){
  int hashj=0;
  FOR(i,0,len) hashj=31*hashj+str[i];
  return hashj;
}
static ht_hash_t hash_value_strg(const char* key){
  return !key?0:hash32(key,strlen(key));
}
/////////////////////////////////////////////////////////
static void mstore_set_mutex(int mutex,struct mstore *x){
  x->mutex=mutex;
}
static void _mstore_block_init_with_capacity(const char *d,const off_t capacity){
  assert(d);
  MSTORE_OFFSET_NEXT_FREE(d)=_MSTORE_LEADING;
  MSTORE_BLOCK_CAPACITY(d)=capacity;
}
static off_t _mstore_common(struct mstore *m,int opt,const void *pointer){
  if (!m) return 0;
  lock(m->mutex);
  //  CG_THREAD_OBJECT_ASSERT_LOCK(m);
  off_t sum=0;
  FOR(sgmt,0,m->capacity){
    char *d=_MSTORE_BLOCK(m,sgmt);
    if (d){
      switch(opt){
      case _mstore_destroy:
        if (sgmt){
          m->_data[sgmt]=NULL;
          if (_MSTORE_IS_MALLOC(m)) cg_free(_MSTORE_COUNTER_MALLOC(m),d); else cg_munmap(_MSTORE_COUNTER_MMAP(m),d,_MSTORE_LEADING+MSTORE_BLOCK_CAPACITY(d));
        }
        break;
      case _mstore_usage: sum+=MSTORE_OFFSET_NEXT_FREE(d); break;
      case _mstore_blocks:  if (MSTORE_OFFSET_NEXT_FREE(d)>_MSTORE_LEADING) sum++;  break;
      case _mstore_clear:
        MSTORE_OFFSET_NEXT_FREE(d)=_MSTORE_LEADING; // cppcheck-suppress unreadVariable
#ifdef HOOK_MSTORE_CLEAR
        HOOK_MSTORE_CLEAR(m);
#endif
        break;
      case _mstore_contains:
        if (d+_MSTORE_LEADING<=(char*)pointer && (char*)pointer<d+MSTORE_BLOCK_CAPACITY(d)){
          sum=1;
          break;
        }
        break;
      }
    }
  }
  unlock(m->mutex);
  return sum;
}

static int mstore_report_memusage_to_strg(char *strg,int max_bytes,struct mstore *m){
  int n=0;
#define S(...) n+=PRINTF_STRG_OR_STDERR(strg,n,max_bytes-n,__VA_ARGS__)
  if (!m){
    S("(Name #id  B:#Blocks-used M:Mem(Bytes) B:Bytes-per-block  flags)");
  }else{
    S("(%s %d  B:%'d M:%'ld S:%'ld %s%s)",snull(m->name), m->iinstance,(int)mstore_count_blocks(m),(long)mstore_usage(m),(long)m->bytes_per_block,
      (m->opt&MSTORE_OPT_MMAP_WITH_FILE?" MMAPFILE":""),
      (_MSTORE_IS_MALLOC(m)?" MALLOC":""));

  }
#undef S
  return n;
}

//////////////////////////////////////////////////////////////////////////
// MMAP with file                                                       //
//////////////////////////////////////////////////////////////////////////
static const char *mstore_set_base_path(const char *f){
  static char base[MAX_PATHLEN+1];
  if (f && !*base){
    cg_recursive_mkdir(cg_copy_path(base,f));
    DIR *dir=opendir(base);
    if (dir){
      char fn[MAX_PATHLEN+1];
      const struct dirent *de;
      while((de=readdir(dir))){
        snprintf(fn,MAX_PATHLEN,"%s/%s",base,de->d_name);
        unlink(fn);
      }
      closedir(dir);
    }else log_errno("Deleting old cache files %s",base);
  }
  assert(base);
  return base;
}

//////////////////////////////////////////////////////////////////////////
// Initialize                                                           //
//   dir: Path of existing folder where the MMAP files will be written. //
//   dim: Size of the MMAP files.  Will be rounded to next n*4096       //
//////////////////////////////////////////////////////////////////////////
#define mstore_init_file(m,name,size_and_opt) _mstore_init(m,name,size_and_opt|MSTORE_OPT_MMAP_WITH_FILE)
#define mstore_init(m,name,size_and_opt) _mstore_init(m,name,size_and_opt)
static struct mstore *_mstore_init(struct mstore *m,const char *name, const int size_and_opt){
  assert(m);
  memset(m,0,sizeof(struct mstore));
  m->name=name;
  static atomic_int count;
  m->iinstance=atomic_fetch_add(&count,1);
  if (size_and_opt&MSTORE_OPT_MMAP_WITH_FILE){
    assert(0==(size_and_opt&MSTORE_OPT_MALLOC));
    assert(name);
  }
  m->bytes_per_block=MAX_int(16,size_and_opt&_MSTORE_MASK_SIZE);
  m->opt=size_and_opt&~_MSTORE_MASK_SIZE;

  m->capacity=1;
  _mstore_block_init_with_capacity(m->_block_on_stack,_MSTORE_BLOCK_ON_STACK_CAPACITY);
  assert((m->opt&(MSTORE_OPT_MALLOC|MSTORE_OPT_MMAP_WITH_FILE))!=(MSTORE_OPT_MALLOC|MSTORE_OPT_MMAP_WITH_FILE));  /* Both opts are mutual exclusive */
  return m;
}
////////////////////////////////////////
// Allocate memory of number of bytes //
////////////////////////////////////////


static void mstore_file(char path[PATH_MAX+1],const struct mstore *m,const int block){
  char b[9]={'*',0};
  if (block>=0) sprintf(b,"%02d",block);
  snprintf(path,PATH_MAX-1,"%s/%03d_%s_%s.cache",mstore_base_path(),m->iinstance,m->name,b);
}
static int _mstore_openfile(struct mstore *m,const uint32_t block,const off_t adim){
  assert(m->name);
  char path[PATH_MAX+1];  mstore_file(path,m,block);
  /* Note, there might be several with same name. Unique file names by adding iinstance to the file name */
  const int fd=open(path,O_RDWR|O_CREAT|O_TRUNC,0640);
  if (fd<2) DIE("Open failed: %s fd: %d\n",path,fd);
  //  struct stat st; if (fstat(fd,&st)<0) log_error("fstat failed: %s\n",path);
  if (ftruncate(fd,adim) || write(fd,"",1)!=1){
    log_errno("write failed: %s\n",path);
    return 0;
  }
  return fd;
}






static char *_mstore_block_try_allocate(struct mstore *m,char *block,const off_t bytes,const int align){
  if (!block) return NULL;
  //  char not_used=block[0];
  off_t not_used_off=((off_t*)block)[0];
  const off_t begin=NEXT_MULTIPLE(MSTORE_OFFSET_NEXT_FREE(block),align);
  if (begin+bytes>MSTORE_BLOCK_CAPACITY(block)) return NULL;
  MSTORE_OFFSET_NEXT_FREE(block)=begin+bytes;
  IF1(_WITH_MSTORE_PREVIOUS,m->_previous_sgmt=block);
  return block+begin;
}

static void _mstore_double_capacity(struct mstore *m){
  assert(m);
  const uint32_t c=m->capacity;
  ASSERT((!m->_data) == (c==1));
  m->_data=cg_realloc_array(_MSTORE_COUNTER_MALLOC(m),SIZEOF_OFF_T,m->_data,c,(m->capacity=c<<1));
  if (!m->_data) DIE("Calloc returns NULL");
}

static void *mstore_malloc(struct mstore *m,const off_t bytes, const int align){
  CG_THREAD_OBJECT_ASSERT_LOCK(m);  assert(align==1||align==2||align==4||align==8);
  char *dst=NULL;
  IF1(_WITH_MSTORE_PREVIOUS, if ((dst=_mstore_block_try_allocate(m,m->_previous_sgmt,bytes,align))) return);
  int ib=0;
  char *block=NULL;
  for(;true;ib++){
    if (ib>=m->capacity) _mstore_double_capacity(m);
    block=_MSTORE_BLOCK(m,ib);
    if (!block) break;
    if ((dst=_mstore_block_try_allocate(m,block,bytes,align))) return dst;
  }
  assert(ib>0);
  assert(!_MSTORE_BLOCK(m,ib));

  const long blockCapacity=m->bytes_per_block=NEXT_MULTIPLE(MAX(bytes,m->bytes_per_block),4096);
  assert(blockCapacity>0);
  if (_MSTORE_IS_MALLOC(m)){
    if (!(block=cg_malloc(_MSTORE_COUNTER_MALLOC(m),blockCapacity+_MSTORE_LEADING))) DIE("Malloc failed");
  }else{
    const int fd=(m->opt&MSTORE_OPT_MMAP_WITH_FILE)?_mstore_openfile(m,ib,blockCapacity+_MSTORE_LEADING):0;
    if (MAP_FAILED==(block=cg_mmap(_MSTORE_COUNTER_MMAP(m),blockCapacity+_MSTORE_LEADING,fd))) block=NULL;
    if (!block) DIE("Allocation failed   %lld  fd: %d",(LLD)(blockCapacity+_MSTORE_LEADING),fd);
  }
  _mstore_block_init_with_capacity(block,blockCapacity);
  m->_data[ib]=block;
  if (!(dst=_mstore_block_try_allocate(m,block,bytes,align))){
    DIE("dst is NULL.  block: %p bytes: %lld align: %d  ib: %d",block, (LLD)bytes,align, ib);
  }
  assert(MSTORE_BLOCK_CAPACITY(block)>=bytes);
  return dst;
}
static void mstore_destroy(struct mstore *m){
  _mstore_common(m,_mstore_destroy,NULL);
  cg_free(_MSTORE_COUNTER_MALLOC(m),m->_data);
}
//////////////////
// Add a string //
// ///////////////
static const char *mstore_addstr(struct mstore *m, const char *str,off_t len_or_zero){
  if (!str) return NULL;
  if (!*str) return "";
  const off_t len=len_or_zero?len_or_zero:strlen(str);
  char *s=mstore_malloc(m,len+1,1);
  ASSERT(s);
  if(s){ memcpy(s,str,len); s[len]=0;}
  return s;
}


#endif //_cg_mstore_dot_c
//////////////////////////////////////////////////////////////////////////
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
//#include "cg_utils.c"
struct mytest{
  int64_t l;
  bool b;
};
static void test_mstore1(int argc,char *argv[]){
  const char *dir="/home/cgille/tmp/test";
  //cg_recursive_mkdir(dir);
  struct mstore m={0};
  mstore_init(&m,"test_mstore1",20|MSTORE_OPT_MALLOC);
  char *tt[99999];
  const int method=atoi(argv[1]);
  printf("method=%d\n",method);
  FOR(i,2,argc){
    char *a=argv[i];
    switch(method){
    case 0:{
      const int align=8;
      tt[i]=mstore_malloc(&m,1+strlen(a), align);
      assert( ((long)tt[i]) % align==0);
      strcpy(tt[i],a);
      break;
    }
    case 1:
      tt[i]=(char*)mstore_addstr(&m,a,strlen(a));
      break;
    case 2:
      tt[i]=(char*)mstore_add(&m,a,strlen(a)+1,4);
      break;
    default: DIE("Wrong para");
    }
  }
  //  tt[argc-3]="This string should rise error";
  FOR(i,2,argc){
    if (!tt[i]){ fprintf(stderr,"Error tt[%d] is NULL\n",i); EXIT(1);}
    fprintf(stderr,"%4d %p  %s\n",i,(void*)tt[i],tt[i]);
    if (strcmp(tt[i],argv[i])){ DIE(RED_ERROR" tt memaddr: %p tt=%s argv=%s\n",(void*)tt[i],tt[i],argv[i]); }
  }
  fprintf(stderr," Usage %lu\n",mstore_usage(&m));
  mstore_destroy(&m);
}

static void test_mstore2(int argc, char *argv[]){
  struct mstore ms;
  assert(argc==2);
  mstore_init(&ms,"test_mstore2",256|MSTORE_OPT_MALLOC);
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
    //intern[i]=(int*)ht_set(&ht,(char*)data[i],data_l,0,data[i]);
  }
  mstore_destroy(&ms);
}

static void test_duplicate_name(void){
  struct mstore m1,m2;
  mstore_init(&m1,"test",MSTORE_OPT_MMAP_WITH_FILE|10);
  mstore_init(&m2,"test",MSTORE_OPT_MMAP_WITH_FILE|10);
}
int main(int argc,char *argv[]){
  if (0){
    mstore_set_base_path("~/test/mstore_test");
    printf("mstore_base_path %s\n",mstore_base_path());
    struct mstore m={0};
    mstore_init(&m,"test",MSTORE_OPT_MMAP_WITH_FILE|1024);
    fprintf(stderr,"m->bytes_per_block: %lld\n",(LLD)m.bytes_per_block);
    FOR(i,0,8){
      char *s=mstore_malloc(&m,1024,4);
      fprintf(stderr,"%d) s: %p\n",i,s);
      fprintf(stderr,"===================================\n");
      fflush(stderr);
    }
    return 0;
  }


  switch(1){
  case 0: printf("NEXT_MULTIPLE: %d -> %d\n",atoi(argv[1]),NEXT_MULTIPLE(atoi(argv[1]),atoi(argv[2])));break; /* 11 4 */
  case 1: test_mstore1(argc,argv); break; /* 1 $(seq 10) */
  case 2: test_mstore2(argc,argv); break; /* 10 */
  case 3: test_duplicate_name(); break;
  }
}
#endif //__INCLUDE_LEVEL__
