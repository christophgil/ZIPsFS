#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#define ULIMIT_S  8192  // Stacksize [kB]  Output of  ulimit -s
#define _MSTORE_SEGMENTS 0xFF
#define _MSTORE_LEADING (SIZE_T*2)
#define SIZE_T sizeof(size_t)
#define ANSI_RESET "\x1B[0m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_GREEN "\x1B[32m"

// mstore_malloc: /home/cgille/.ZIPsFS/_home_cgille_tmp_ZIPsFS_mnt/cachedir/7/1018 Failed to create file
////////////
/// Hash ///
////////////
// Retur0n 64-bit FNV-1a hash for key (NUL-terminated). See  https://en.wikipedia.org/wiki/Fowler–Noll–Vo_hash_function
// https://softwareengineering.stackexchange.com/questions/49550/which-hashing-algorithm-is-best-for-uniqueness-and-speed
typedef uint32_t ht_hash_t;
#define HT_HASH_MAX UINT32_MAX
static uint32_t hash32(const char* key, const uint32_t len){
  uint32_t hash=2166136261U;
  for(int i=len;--i>=0;){
    hash^=(uint32_t)(unsigned char)(key[i]);
    hash*=16777619U;
  }
  return hash;
}
/*
static uint64_t hash64(const char* key, const size_t len){
  uint64_t hash64=14695981039346656037UL;
  for(int i=len;--i>=0;){
    hash64^=(uint64_t)(unsigned char)(key[i]);
    hash64*=1099511628211UL;
  }
  return hash64;
}
static int java_hashCode(const char *str,const size_t len){
  int hashj=0;
  for(int i=0;i<len; i++) hashj=31*hashj+str[i];
  return hashj;
}
*/
static ht_hash_t hash_value_strg(const char* key){
  return !key?0:hash32(key,strlen(key));
}

/////////////////////////////////////////////////////////

#define _STACK_DATA (ULIMIT_S*8)

struct mstore{
  char *pointers_data_on_stack[_MSTORE_SEGMENTS],_stack_data[_STACK_DATA];
  char **data, *files;
  size_t dim;
  uint32_t opt,segmentLast,capacity;
  uint64_t debug_thread;
  int debug_mutex;

};

//////////////////////////////////////////////////////////////////////////
// Initialize                                                           //
//   dir: Path of existing folder where the mmap files will be written. //
//   dim: Size of the mmap files.  Will be rounded to next n*4096       //
//////////////////////////////////////////////////////////////////////////
#define NEXTMULTIPLE2(x,word) ((x+(word-1))&~(word-1))
#define MSTORE_OPT_MALLOC (1LU<<63)
#define _MSTORE_OPTS MSTORE_OPT_MALLOC
static struct mstore *mstore_init(struct mstore *m,const char *dir,size_t size_and_opt){
  memset(m,0,sizeof(struct mstore));
  memset(m->data=m->pointers_data_on_stack,0,_MSTORE_SEGMENTS*SIZE_T);
  m->files=(char*)dir;
  const size_t size=size_and_opt&~_MSTORE_OPTS;
  m->dim=size<16?16:size;
  m->opt=size_and_opt&_MSTORE_OPTS;
  m->capacity=_MSTORE_SEGMENTS;
  return m;
}
////////////////////////////////////////
// Allocate memory of number of bytes //
////////////////////////////////////////
#define _mstore_strerror2(txt1,txt2)  fprintf(stderr,"\x1B[31mError:\x1B[31m %s %s %s %s\n",__func__,txt1,txt2,strerror(errno))
#define _mstore_strerror(txt1)  _mstore_strerror2(txt1,"")

static int _mstoreOpenFile(struct mstore *m,uint32_t segment,const size_t adim){
  char path[PATH_MAX];
  assert(m!=NULL);
  sprintf(path,"%s/%u",m->files,segment);
  unlink(path);
  const int fd=open(path,O_RDWR|O_CREAT|O_TRUNC,0x0777);
  if (fd<2){
    _mstore_strerror2("open failed",path);
    if (!segment) exit(1);
    return 0;
  }
  struct stat statbuf;
  if (fstat(fd,&statbuf)<0) _mstore_strerror2("fstat failed",path);
  if (statbuf.st_size<adim){
    if (lseek(fd,adim-1,SEEK_SET)==-1) _mstore_strerror2("lseek",path);
    if (write(fd,"",1)!=1) _mstore_strerror2("write failed",path);
  }
  return fd;
}

#define _MSTORE_FREE_DATA(m)  if (m->data!=m->pointers_data_on_stack) free(m->data)
static void *mstore_malloc(struct mstore *m,const size_t bytes, const int align){
  assert(align==1||align==2||align==4||align==8);
#define D m->data[segment]
#define DD ((size_t*)d)
  //  assert(bytes%align==0); Nein muss nicht sein
  size_t adim=bytes>m->dim?bytes:m->dim;  adim=NEXTMULTIPLE2(adim,4096); /* Aligned array dim */
  for(uint32_t s0=0;;s0++){
    const uint32_t segment=!s0?m->segmentLast:s0-1;
    {
      const size_t c=m->capacity;
      if (segment>=c){
        char **dd=calloc((m->capacity=2*c),SIZE_T);
        memcpy(dd, m->data,c*SIZE_T);
        _MSTORE_FREE_DATA(m);
        m->data=dd;
      }
    }
    char *d=D;
    if (!d){
      int fd=0,flags=MAP_PRIVATE|MAP_ANONYMOUS;
#define _MSTORE_MEM(adim) _MSTORE_LEADING+adim*SIZE_T
      const size_t c=_MSTORE_MEM(adim);
      if (!segment && c<=_STACK_DATA){
        d=m->_stack_data;
      }else if (m->opt&MSTORE_OPT_MALLOC){
        adim=_STACK_DATA;
        if (!(d=malloc(c))){ _mstore_strerror("malloc failed"); return NULL;}
      }else{
        if (m->files){
          if ((fd=_mstoreOpenFile(m,segment,adim))<3) return NULL;
          flags=MAP_SHARED;
        }
        if ((d=mmap(0,c,PROT_READ|PROT_WRITE,flags,fd,0))==MAP_FAILED || !d){
          _mstore_strerror("mmap failed");
          return NULL;
        }
      }
      D=d;
      DD[0]=_MSTORE_LEADING;
      DD[1]=adim;
    }
    assert(d!=NULL);
    const size_t pos=NEXTMULTIPLE2(DD[0],align);
    if (pos+bytes<DD[1]){
      DD[0]=pos+bytes;
      m->segmentLast=segment;
      return d+pos;
    }
  }
  return NULL;
}
static void *mstore_add(struct mstore *m,const void *src, const size_t bytes,const int align){
  return memcpy(mstore_malloc(m,bytes,align),src,bytes);
}
enum _mstore_operation{_mstore_destroy,_mstore_usage,_mstore_clear,_mstore_contains,_mstore_segments};
static size_t _mstore_common(struct mstore *m,int opt,const char *pointer){
  size_t sum=0;
  for(int segment=m->capacity;--segment>=0;){



    char *d=D;
    if (d){
      switch(opt){
      case _mstore_destroy:
        D=NULL;
        if (d!=m->_stack_data){
          if (m->opt&MSTORE_OPT_MALLOC) free(d);
          else munmap(d,_MSTORE_MEM(DD[1]));
        }
        break;
      case _mstore_usage: sum+=DD[0]; break;
      case _mstore_segments:  if (DD[0]>_MSTORE_LEADING) sum++;  break;
      case _mstore_clear:
        DD[0]=_MSTORE_LEADING;
        //memset(d+_MSTORE_LEADING,0,DD[1]*SIZE_T);
        // fprintf(stderr," xxxxxxxx DD[0]=%ld   DD[1]=%ld",DD[0],DD[1]);
        //memset(d+_MSTORE_LEADING,0,DD[1]);
        break;
      case _mstore_contains:
        if (d<pointer && pointer<d+m->dim) return true;
        break;
      }
    }
  }
  return sum;
}
static void mstore_destroy(struct mstore *m){
  _mstore_common(m,_mstore_destroy,NULL);
  _MSTORE_FREE_DATA(m);
}
static size_t mstore_usage(struct mstore *m){
  return _mstore_common(m,_mstore_usage,NULL);
}
static size_t mstore_count_segments(struct mstore *m){
  return _mstore_common(m,_mstore_segments,NULL);
}
static void mstore_clear(struct mstore *m){
  _mstore_common(m,_mstore_clear,NULL);
}
static bool mstore_contains(struct mstore *m, const char *pointer){
  return 0!=_mstore_common(m,_mstore_contains,pointer);
}



#undef D
#undef DD
#undef _MSTORE_BYHASH
//////////////////
// Add a string //
// ///////////////



static const char *mstore_addstr(struct mstore *m, const char *str,size_t len){
  if (!str) return NULL;
  if (!*str) return "";
  if (!len) len=strlen(str);
  char *s=mstore_malloc(m,len+1,1);
  assert(s!=NULL);
  memcpy(s,str,len);
  s[len]=0;
  assert(!strncmp(s,str,len));
  return s;
}


struct cache_by_hash{
  int size,align;
  char **str;
  size_t *len;
};
static void cache_by_hash_init(struct cache_by_hash *c,int log2size,int align){
  c->size=1<<log2size;
  c->align=align;
  c->str=calloc(c->size,sizeof(char*));
  c->len=calloc(c->size,8);
}
static void cache_by_hash_clear(struct cache_by_hash *c){
  memset(c->str,0,c->size*sizeof(char**));
}
#define MSTORE_ADD_REUSE_LOG (1LU<<62)
#define MSTORE_ADD_REUSE_FLAGS (1LU<<62)
static const void *mstore_add_reuse(struct mstore *m, const void *str, size_t len,const ht_hash_t hash,struct cache_by_hash  *byhash){
  if (!str) return NULL;
  const bool log=(len&MSTORE_ADD_REUSE_LOG)!=0;
  len&=~MSTORE_ADD_REUSE_FLAGS;
  const int i=hash&(byhash->size-1);
  assert(i>=0 && i<byhash->size);
  const char *s=byhash->str[i];
  if (log){
    if (!s) printf("!");
    else if (byhash->len[i]!=len){
      printf("=");
    }else if (memcmp(s,str,len)){
      printf(";");
    }else{
      printf("@");
    }
  }
  if (s && byhash->len[i]==len  && !memcmp(s,str,len)){
    if (log) printf(ANSI_FG_GREEN"~"ANSI_RESET);
    return s;
  }
  if (log) printf(ANSI_FG_RED"~"ANSI_RESET);
  byhash->len[i]=len;
  return byhash->str[i]=(char*)mstore_add(m,str,len,byhash->align);
}

#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__==0
//#include "cg_utils.c"
struct mytest{
  int64_t l;
  bool b;
};


int main(int argc,char *argv[]){
  const char *dir="/home/cgille/tmp/test/mstore_mstore1";
  //recursive_mkdir(dir);
  struct mstore m={0};
  mstore_init(&m,NULL,20);
  char *tt[99999];
  for(int i=1;i<argc;i++) {
    char *a=argv[i];
    switch(2){
    case 0:
      tt[i]=mstore_malloc(&m,1+strlen(a),1);
      strcpy(tt[i],a);
      break;
    case 1:
      tt[i]=(char*)mstore_addstr(&m,a,strlen(a));
      break;
    case 2:
      tt[i]=(char*)mstore_add(&m,a,strlen(a)+1,1);
      break;

    }
  }
  tt[997]="hallo";
  for(int i=1;i<argc;i++){
    if (!tt[i]) { fprintf(stderr,"Error tt[i] is NULL\n"); exit(1);}
    printf("%4d %p  %s\n",i,(void*)tt[i],tt[i]);
    if (strcmp(tt[i],argv[i])){ fprintf(stderr,"Error  i=%d %p tt=%s argv=%s\n",i,(void*)tt[i],tt[i],argv[i]); }
  }
  printf(" Usage %lu\n",mstore_usage(&m));
  mstore_destroy(&m);
  return 0;
}
#endif
