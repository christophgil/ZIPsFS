/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////

/*  Copyright (C) 2023   christoph Gille   This program can be distributed under the terms of the GNU GPLv3. */
// cppcheck-suppress-file unusedFunction
#ifndef _cg_utils_dot_c
#define _cg_utils_dot_c
#include "cg_gnu.c"
#include "cg_utils.h"
#include <errno.h>
#include <fcntl.h>// provides posix_fadvise
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/time.h>
#include <sys/types.h>
#include <limits.h>
#include <time.h>
#include <utime.h>
#include <grp.h>
#include <math.h>
#include <stdbool.h>
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stddef.h>
//#include <malloc.h>
#include <unistd.h>
#include <stdatomic.h>

#include <dirent.h>

#include <sys/mman.h>

#include <sys/param.h>
//#include <sys/mount.h>
#include <sys/syscall.h>
#include <locale.h>// Provides decimal grouping

#ifndef PROFILED
#define PROFILED(function) function
#endif
#ifndef WITH_ASSERT_LOCK
#define WITH_ASSERT_LOCK 0
#endif
////////////////////////////////////////

static time_t _whenStarted;

static void *malloc_untracked(const size_t size){
  void *m=malloc(size);
  if (!m){ fprintf(stderr,RED_ERROR" malloc(%'lld)\n",(LLD)size); perror(""); exit(ENOMEM); }
  return m;
}
static void *calloc_untracked(const size_t nmemb,const size_t size){
  void *m=calloc(nmemb,size);
  if (!m){ fprintf(stderr,RED_ERROR" calloc(%'lld,%'lld)\n",(LLD)nmemb,(LLD)size); perror(""); exit(ENOMEM); }
  return m;
}
static char *strdup_untracked(const char *s){
  char *m=strdup(s);
  if (!m){ fprintf(stderr,RED_ERROR" strdup()\n"); perror(""); exit(ENOMEM); }
  return m;
}

/*********************************************************************************/
static bool has_proc_fs(void){
  struct stat st;
  static int v;
  if (!v) v=PROFILED(stat)("/proc/self/fd",&st)?-1:1;
  return v==1;
}
static void puts_stderr(const char *s){
  if(s)fputs(s,stderr);
}

//////////////
/// perror ///
//////////////
#define C(x)  case x: return #x
static const char *error_symbol(const int x){
  switch(x){
#include "cg_utils_error_codes.c"
  };
  return "?";
}
#ifdef ZIP_ER_OK
static const char *error_symbol_zip(const int x){
  switch(x){
    C(ZIP_ER_OK); C(ZIP_ER_MULTIDISK); C(ZIP_ER_RENAME); C(ZIP_ER_CLOSE); C(ZIP_ER_SEEK); C(ZIP_ER_READ); C(ZIP_ER_WRITE); C(ZIP_ER_CRC); C(ZIP_ER_ZIPCLOSED); C(ZIP_ER_NOENT);
    C(ZIP_ER_EXISTS); C(ZIP_ER_OPEN); C(ZIP_ER_TMPOPEN); C(ZIP_ER_ZLIB); C(ZIP_ER_MEMORY); C(ZIP_ER_CHANGED);
    C(ZIP_ER_COMPNOTSUPP); C(ZIP_ER_EOF); C(ZIP_ER_INVAL); C(ZIP_ER_NOZIP); C(ZIP_ER_INTERNAL); C(ZIP_ER_INCONS); C(ZIP_ER_REMOVE); C(ZIP_ER_DELETED); C(ZIP_ER_ENCRNOTSUPP); C(ZIP_ER_RDONLY); C(ZIP_ER_NOPASSWD);
    C(ZIP_ER_WRONGPASSWD); C(ZIP_ER_OPNOTSUPP); C(ZIP_ER_INUSE); C(ZIP_ER_TELL); C(ZIP_ER_COMPRESSED_DATA);C(ZIP_ER_CANCELLED);
  };
  return "?";
}
#endif //defined ZIP_ER_OK
#undef C



static void fprint_strerror(FILE *f,int err){
  if (err && f){

    //    char s[1024];  strerror_r(err,s,1023);   fprintf(f," strerror_r=%s \n",s);
    fprintf(f," Error %d %s: %s ",err,error_symbol(err),strerror(err));
  }
}

#if ! defined WITH_DEBUG_MALLOC
#define WITH_DEBUG_MALLOC 0
#endif

#if ! defined WITH_ZIPsFS_COUNTERS
#define WITH_ZIPsFS_COUNTERS 0
#endif


#if ! WITH_ZIPsFS_COUNTERS || ! defined MALLOC_ID_COUNT
#define cg_free(id,...)   free_untracked((void*)__VA_ARGS__)
#define cg_malloc(id,...) malloc_untracked(__VA_ARGS__)
#define cg_calloc(id,...) calloc_untracked(__VA_ARGS__)
#define cg_strdup(id,...) strdup_untracked(__VA_ARGS__)
#define cg_mmap(id,...) _cg_mmap(0,__VA_ARGS__,__FILE_NAME__,__LINE__)
#define cg_munmap(id,...) _cg_munmap(0,__VA_ARGS__)
#define cg_realloc_array(id,...) _cg_realloc_array(0,__VA_ARGS__)
#define COUNTER1_ADD(id,n)
#define COUNTER2_ADD(id,n)
#define COUNTER1_INC(id)
#define COUNTER2_INC(id)
#else
static atomic_long _counters1[COUNT_NUM],_counters2[COUNT_PAIRS_END],_countersB1[COUNT_NUM],_countersB2[COUNT_PAIRS_END];
// cppcheck-suppress-macro duplicateCondition
#define COUNTER1_ADD(id,n) if (id) atomic_fetch_add(_countersB1+id,n)
// cppcheck-suppress-macro duplicateCondition
#define COUNTER2_ADD(id,n) if (id) atomic_fetch_add(_countersB2+id,n)
// cppcheck-suppress-macro duplicateCondition
#define COUNTER1_INC(id) if (id) atomic_fetch_add(_counters1+id,1)
// cppcheck-suppress-macro duplicateCondition
#define COUNTER2_INC(id) if (id) atomic_fetch_add(_counters2+id,1)


#define cg_mmap(...) _cg_mmap(__VA_ARGS__,__FILE_NAME__,__LINE__)
#define cg_munmap(...) _cg_munmap(__VA_ARGS__)
#define cg_realloc_array(...) _cg_realloc_array(__VA_ARGS__)
static bool _malloc_is_count_mstore[MALLOC_ID_COUNT];
static void *cg_malloc(const int id, const size_t size){
  COUNTER1_INC(id);
  void *p=malloc_untracked(size);
  assert(p!=NULL);
  return p;
}
static void *cg_calloc(const int id,size_t nmemb, size_t size){
  COUNTER1_INC(id);
  void *p=calloc_untracked(nmemb,size);
  assert(p!=NULL);
  return p;
}
static char *cg_strdup(const int id,const char *s){
  COUNTER1_INC(id);
  char *p=strdup_untracked(s);
  assert(p!=NULL);
  return p;
}
static void cg_free(const int id,const void *ptr){
  if (!ptr) return;
  COUNTER2_INC(id);
  free_untracked((void*)ptr);
}
#endif
#define REALLOC_ARRAY_NO_FREE (1<<30)
static void *_cg_realloc_array(const int id,const int size1AndOpt,const void *pOld, const size_t nOld, const size_t nNew){
  const int size1=size1AndOpt&~(REALLOC_ARRAY_NO_FREE);
  void *pNew=cg_calloc(id,nNew,size1);
  if (pOld){
    memcpy(pNew,pOld,nOld*size1);
    if (!(size1AndOpt&REALLOC_ARRAY_NO_FREE)) cg_free(id,pOld);
  }
  assert(pNew);
  return pNew;
}


//static struct ht _mmap_debug;

static void *_cg_mmap(const int id, const off_t length, const int fd_or_zero, const char *file, const int line){
  const int fd=fd_or_zero?fd_or_zero:-1;
  const off_t offset=0;
  const int flags=fd==-1?(MAP_SHARED|MAP_ANONYMOUS):MAP_SHARED;  /* FreeBSD requires fd -1 for MAP_ANONYMOUS. Otherwise EINVAL */
  void *ptr=mmap(NULL,length,PROT_READ|PROT_WRITE,flags,fd, offset);
  if (ptr){
    //    if (!_mmap_debug.capacity){ ht_init(&_mmap_debug,"_mmap_debug",HT_FLAG_NUMKEY|16);}
    COUNTER1_ADD(id,length);
    COUNTER1_INC(id);
  }
  if (ptr==MAP_FAILED){
    fprintf(stderr,"%s:%d  ",file,line);    perror(__func__);
  }
  return ptr;
}
static int _cg_munmap(const int id,const void *ptr,const off_t length){
  if (!ptr) return -1;
  assert(length>0);
  COUNTER2_ADD(id,length);
  COUNTER2_INC(id);
  return munmap((void*)ptr,length);
}



//////////////
/// String ///
//////////////
static char* cg_strrchr_null(const char *path, const char c){
  const char *found=strrchr(path,c);
  return (char*)(found?found:path+strlen(path));
}




static uint32_t hash32(const char* key, const uint32_t len){
  uint32_t hash=2166136261U;
  RLOOP(i,len){
    hash^=(uint32_t)(unsigned char)(key[i]);
    hash*=16777619U;
  }
  return !hash?1:hash; /* Zero often means that hash still needs to be computed */
}
static uint64_t hash64(const char* key, const off_t len){
  uint64_t hash64=14695981039346656037UL;
  RLOOP(i,len){
    hash64^=(uint64_t)(unsigned char)(key[i]);
    hash64*=1099511628211UL;
  }
  return hash64;
}

static struct strg *strg_init(struct strg *strg,const char *s){
  assert(s);
  const int l=strlen(s);
  assert(l<=MAX_PATHLEN);
  strcpy(strg->s,s);
  strg->l=l;
  strg->hash=hash32(strg->s,strg->l);
  return strg;
}


static int cg_str_str(const char *s,const char *substr){
  if (!s||!substr) return -1;
  const char *h=strstr(s,substr);
  return h?(int)(h-s):-1;
}

static const char *cg_str_lremove(const char *s, const char *pfx,const int  pfx_l){
  return s+(strncmp(s,pfx,pfx_l)?0:pfx_l);
}
static int cg_empty_dot_dotdot(const char *s){
  return !s || !*s || (*s=='.' && (!s[1] || (s[1]=='.' && !s[2])));
}
#define  ASSERT_STRGS_NO_OVERLAP(dst,src,n) assert(!(dst<=src && dst+n>=src) && !(src<=dst && src+n>=dst))
#define cg_strncpy0(...) _cg_strncpy(false,__VA_ARGS__)
#define cg_stpncpy0(...) _cg_strncpy(true,__VA_ARGS__)
static char *_cg_strncpy(const bool stpcpy,char *dst,const char *src,const int num){
  const int n=src?strnlen(src,num):0;
  if (n){
    ASSERT_STRGS_NO_OVERLAP(dst,src,n);
    memcpy(dst,src,n);
  }
  dst[n]=0;
  return dst+(stpcpy?n:0);
}
#define SNPRINTF(dest,max,...)   (max<=snprintf(dest,max,__VA_ARGS__) && (log_error("Exceed snprintf "),true))
static uint32_t cg_strlen(const char *s){
  return s?strlen(s):0;
}




static int cg_sum_strlen(const char **ss, const int n){
  int sum=0;
  FOREACH_CSTRING(s,ss) sum+=cg_strlen(*s);
  return sum;
}

#define cg_idx_of_NULL(aa,n) cg_idx_of_pointer(aa,n,NULL)
static int cg_idx_of_pointer(void **aa, const int n, const void *a){
  if (aa){
    FOR(i,0,n){
      if (aa[i]==a) return i;
      if (!aa[i]) break;
    }
  }
  return -1;
}


static const char* snull(const char *s){ return s?s:"Null";}
static MAYBE_INLINE char *yes_no(int i){ return i?"Yes":"No";}


static bool cg_endsWith(const int flags, const char* s,int s_l,const char* e,int e_l){
  if (!s || !e) return false;
  if (!s_l) s_l=strlen(s);
  if (!e_l) e_l=strlen(e);
  if ((flags&ENDSWITH_FLAG_PRECEED_SLASH) && (s_l<=e_l || s[s_l-e_l-1]!='/')) return false;
  return e_l<=s_l && 0==((flags&ENDSWITH_FLAG_IC)?strncasecmp(s+s_l-e_l,e,e_l): memcmp(s+s_l-e_l,e,e_l));
}

static bool cg_startsWith(const char* s,int s_l,const char* e,int e_l){
  if (!s || !e) return false;
  if (!s_l) s_l=strlen(s);
  if (!e_l) e_l=strlen(e);
  return e_l<=s_l && 0==memcmp(s,e,e_l);
}


static bool cg_endsWithZip(const char *s, const int len_or_0){
  const int len=len_or_0?len_or_0:cg_strlen(s);
  return s && ENDSWITHI(s,len,".zip");
}

static bool cg_endsWithGZ(const char *s, const int len_or_0){
  const int l=len_or_0?len_or_0:cg_strlen(s);
  return l>2 && s[l-3]=='.' && (s[l-2]|32)=='g' && (s[l-1]|32)=='z';
}


static bool cg_isURL(const char *url){
  const int url_l=cg_strlen(url);
  if (url_l<8) return false;
    switch(*url){
#define S(s) cg_startsWith(url,url_l,s,sizeof(s)-1)
    case 'h': if(S("http://")||S("https://")) return true; break;
    case 'f': if(S("ftp://")||S("ftps://")) return true; break;
#undef S
    }
    return false;
}

/*
  static char *cg_remove_zipext(char *s,const int s_l_or_zero){
  if (!s) return NULL;
  const int s_l=s_l_or_zero?s_l_or_zero:strlen(s);
  if (cg_endsWithZip(s,s_l)) s[s_l-4]=0;
  return s;
  }
*/
static bool cg_endsWithDotD(const char *s, int len){
  if(!len)len=cg_strlen(s);
  return s && ENDSWITHI(s,len,".d");
}

#define FIND_SUFFIX_IC (1<<1)
static int cg_find_suffix(const int opt,const char *s, const int s_l,const char **xx,const int *xx_l){
  if (xx && s){
    for(int i=0; xx[i]; i++){
      if (cg_endsWith((opt&FIND_SUFFIX_IC)!=0?ENDSWITH_FLAG_IC:0, s,s_l,xx[i],xx_l?xx_l[i]:0)) return i;
    }
  }
  return -1;
}



static int cg_last_slash_l(const char *path,const int path_l ){
  RLOOP(i,path_l){
    if (path[i]=='/') return i;
  }
  return -1;
}
#define cg_last_slash(path) cg_last_slash_l(path,cg_strlen(path))

static int cg_leading_slashes(const char *s){
  int c=-1;
  if (s) while(s[++c]=='/');
  return c;
}



static bool cg_starts_digits_char(const char *s,const int nDigit, const int c){
  if (!s) return false;
  FOR(i,0,nDigit) if (!isdigit(s[i])) return false;
  return c<0||s[nDigit]==c;
}


#define OPT_STR_REPLACE_DRYRUN (1<<0)
#define OPT_STR_REPLACE_ASSERT (1<<1)
static int cg_str_replace(const int opt,char *haystack, const int h_l_or_zero, const char *needle,  const int n_l_or_zero, const char *replacement,  const int r_l_or_zero){
  assert(haystack!=NULL);assert(needle!=NULL);assert(replacement!=NULL);
  int h_l=h_l_or_zero?h_l_or_zero:strlen(haystack);
  const int r_l=r_l_or_zero?r_l_or_zero:strlen(replacement);
  const int n_l=n_l_or_zero?n_l_or_zero:strlen(needle);
  const bool ends_null=haystack[h_l]==0;
  //fprintf(stderr,"literal %d dyr: %d assert: %d  h %s %d     n %s n_l=%d   r %s r_l=%d  \n",IS_LITERAL_STRING(haystack),0!=(opt&OPT_STR_REPLACE_DRYRUN),0!=(opt&OPT_STR_REPLACE_ASSERT),haystack,h_l,needle,n_l,replacement,r_l);
  assert(n_l>0);
  bool replaced=false;
  RLOOP(h,h_l-n_l+1){
    if (haystack[h]!=*needle || memcmp(haystack+h,needle,n_l)) continue;
    replaced=true;
    const int dl=r_l-n_l;
    h_l+=dl;
    if (0==(opt&OPT_STR_REPLACE_DRYRUN)){
      if (dl<0){ /* Shift left, gets-smaller */
        FOR(p,h+r_l,h_l) haystack[p]=haystack[p-dl];
      }else if (dl>0){ /* Shift right */
        for(int p=h_l; --p>=h+r_l;) haystack[p]=haystack[p-dl];
      }
      //fprintf(stderr,"haystack: %s  h: %d  replacement: %s r_l: %d ",haystack,h,replacement,r_l);
      memcpy(haystack+h,replacement,r_l);
    }
  }
  if (0!=(opt&OPT_STR_REPLACE_ASSERT)){
    //fprintf(stderr,"needle=%s replacement=%s",needle,replacement);
    //fprintf(stderr,ANSI_FG_BLUE"%s"ANSI_RESET,haystack);
    assert(replaced);
  }
  if (0==(opt&OPT_STR_REPLACE_DRYRUN) && ends_null) haystack[h_l]=0;
  return h_l;
}


#define OPT_CG_STRSPLIT_WITH_EMPTY_TOKENS (1<<8)
#define OPT_CG_STRSPLIT_NO_HEAP (1<<9)
static int cg_strsplit(int opt_and_sep, const char *s, const int s_l, const char *tokens[], int *tokens_l){
  int count=0;
  const char *tok=NULL;
  if (s){
    bool prev_sep=true;
    for(int i=0;;i++){
      const bool isend=s_l?(i>=s_l):!s[i];
      const bool issep=isend || s[i]==(opt_and_sep&0xff);
      if (prev_sep &&  ( (opt_and_sep&OPT_CG_STRSPLIT_WITH_EMPTY_TOKENS) || !issep)){
        tok=s+i;
      }
      if (tok && issep){
        const int l=s+i-tok;
        if (tokens_l) tokens_l[count]=l;
        if (tokens)   tokens[count]=((opt_and_sep&OPT_CG_STRSPLIT_NO_HEAP))?tok:strndup(tok,l);
        count++;
        tok=NULL;
      }
      if (isend) break;
      prev_sep=issep;
    }
  }
  if (tokens) tokens[count]=NULL;
  return count;
}

static const char *rm_pfx_us(const char *s){
  const char *us=!s?NULL:strchr(s,'_');
  return us?us+1:NULL;
}
///////////////////
/// time       ///
///////////////////
static int64_t currentTimeMillis(void){
  struct timeval tv={0};
  gettimeofday(&tv,NULL);
  return tv.tv_sec*1000+tv.tv_usec/1000;
}



#define cg_sleep_ms(...) _cg_sleep_ms(__VA_ARGS__,__func__,__LINE__)
static void _cg_sleep_ms(const int millisec, const char *msg, const char *func,const int line){
  if (millisec>0){
    if (!msg||!*msg) log_verbose("%s:%d Going sleep %d ms ...",func,line,millisec);
    else log_verbose("%s",msg);
    usleep(millisec<<10);
  }
}

static void cg_nanosleep(long nanos){
  struct timespec ts={nanos>>30,(nanos&((1<<30)-1))};
  nanosleep(&ts,NULL);
}

static void cg_usleep(int usec){
  struct timespec ts={usec>>20,((usec&((1<<20)-1)))<<10};
  //log_debug_now("ts=%ld   %'ld ", ts.tv_sec,ts.tv_nsec);
  nanosleep(&ts,NULL);
}





///////////////////
/// file path   ///
///////////////////



// static int slash_not_trailing(const char *path){ const char *p=strchr(path,'/');  return p && p[1]?(int)(p-path):-1; }
static int cg_pathlen_ignore_trailing_slash(const char *p){
  if (!p) return 0;
  int n=strlen(p);
  //return n && p[n-1]=='/'?n-1:n;
  while(n && p[n-1]=='/') n--;
  return n;
}
static bool cg_path_equals_or_is_parent(const char *subpath,const int subpath_l,const char *path,const int path_l){
  return subpath && path && (subpath_l==path_l||(subpath_l<path_l&&path[subpath_l]=='/')) && !memcmp(path,subpath,subpath_l);
}
static bool *cg_validchars(enum enum_validchars type){
  static bool ccc[VALIDCHARS_NUM][128];
  static bool initialized;
  if (!initialized){
    if (type==VALIDCHARS_FILE||type==VALIDCHARS_PATH||type==VALIDCHARS_NOQUOTE){
      RLOOP(t,VALIDCHARS_NUM){
        bool *cc=ccc[t];
        FOR(i,'A','Z'+1) cc[i|32]=cc[i]=true;
        FOR(i,'0','9'+1) cc[i]=true;
        cc['=']=cc['+']=cc['-']=cc['_']=cc['$']=cc['@']=cc['.']=cc['~']=cc['%']=cc[',']=true;
      }
    }
    ccc[VALIDCHARS_PATH]['/']=ccc[VALIDCHARS_PATH][' ']=ccc[VALIDCHARS_FILE][' ']=ccc[VALIDCHARS_NOQUOTE]['/']=ccc[VALIDCHARS_NOQUOTE]['%']=true;
    ccc[VALIDCHARS_NOQUOTE][':']=true;

    initialized=true;
  }
  return ccc[type];
}

static int cg_find_invalidchar(enum enum_validchars type,const char *s,const int len){
  if (s){
    const bool *bb=cg_validchars(type);
    FOR(i,0,len){
      if (s[i]<0||s[i]>127||!bb[s[i]]) return i;
    }
  }
  return -1;
}

static int url_encode(char *dst, const int dst_l, const char *name){
  static const bool *bb;
  bb=cg_validchars(VALIDCHARS_FILE);
  int i=0;
  for(const char *t=name; *t; t++){
    const unsigned char c=*t;

    if (c<128 && bb[c]){
      if (i+1<dst_l) dst[i]=c;
      i++;
    }else{
      if (i+3<dst_l) sprintf(dst+i,"%%%02x",c);
      i+=3;
    }
  }
  dst[i]=0;
  return i;
}


static void cg_path_for_fd(const char *title, char *path, int fd){
  *path=0;
  if (!has_proc_fs()) return;
  char buf[99];
  sprintf(buf,"/proc/self/fd/%d",fd);

  const ssize_t n=readlink(buf,path, MAX_PATHLEN-1);
  if (n<=0){
    *path=0;
    log_errno("\n%s  %s: ",snull(title),buf);
  }else{
    path[n]=0;
  }
}

static int cg_count_fd_this_prg(void){
  int n=0;
  if (has_proc_fs()){
    DIR *dir=opendir("/proc/self/fd");
    while(readdir(dir)) n++;
    closedir(dir);
  }
  return n;
}


static bool cg_check_path_for_fd(const char *title, const char *path, int fd){
  char check_path[MAX_PATHLEN+1],rp[PATH_MAX+1];
  if (!realpath(path,rp)){
    log_error("%s  Failed realpath(%s)\n",snull(title),path);
    return false;
  }
  cg_path_for_fd(title,check_path,fd);
  if (strncmp(rp,path,MAX_PATHLEN-1)){
    log_error("%s  fd=%d,%s   D_VP(d)=%s   realpath(path)=%s\n",title,fd,check_path,path,rp);
    return false;
  }
  return true;
}

static void cg_print_path_for_fd(int fd){
  if (!has_proc_fs()){
    fprintf(stderr,ERROR_MSG_NO_PROC_FS"\n");
  }else{
    char buf[99],path[512];
    sprintf(buf,"/proc/self/fd/%d",fd);
    const ssize_t n=readlink(buf,path,511);
    if (n<0){
      log_errno("%s  No path",buf);
    }else{
      path[n]=0;
      fprintf(stderr,"Path for %d:  %s\n",fd,path);
    }
  }
}



/////////////
/// Array ///
/////////////

static void cg_array_remove_element(const char *aa[],const void *element){
  for(const char **s=aa,**d=aa; ;s++,d++){
    while (*s==element) s++;
    if (d!=s) *d=*s;
    if (!*d) break;
  }
}

static int cg_array_length(const char **xx){
  int i=0;
  if (xx) while(xx[i]) i++;
  return i;
}

///////////////////
/// Arithmetics ///
///////////////////

static int isPowerOfTwo(unsigned int n){
  return n && (n&(n-1))==0;
}

/* Integer sqrt from https://en.wikipedia.org/wiki/Integer_square_root */
static unsigned int isqrt(unsigned int y){
  unsigned int L=0,R=y+1;
  while(L!=R-1){
    const unsigned int M=(L+R)/2;
    if (M*M<=y) L=M; else R=M;
  }
  return L;
}
static bool is_square_number(unsigned int y){
  unsigned int s=isqrt(y);
  return s*s==y;
}

static long closest_with_identical_digits(const long num){
  int count=0;
  long n=num;
  while(n>9){
    n/=10;
    count++;
  }
  long ret=n;RLOOP(i,count) ret=10*ret+n;
  //fprintf(stderr,"num: %ld  ret=%ld n: %ld\n\n",num,ret,n);
  if (num>ret){
    ret=++n; RLOOP(i,count) ret=10*ret+n;
  }
  return ret;
}


/* static inline int MAX_int(int a,int b){ return MAX(a,b);} */
/* static inline int64_t MIN_long(int64_t a,int64_t b){ return MIN(a,b);} */
/* static inline int64_t MAX_long(int64_t a,int64_t b){ return MAX(a,b);} */
static MAYBE_INLINE int64_t cg_atol_kmgt(const char *s){
  if (!s) return 0;
  char *c=(char*)s;
  while(*c && '0'<=*c && *c<='9') c++;
  *c&=~32;
  return atol(s)<<(*c=='K'?10:*c=='M'?20:*c=='G'?30:*c=='T'?40:0);
}
static void cg_log_file_mode(mode_t m){
  char txt[11];
  int i=0;
  txt[i++]=S_ISDIR(m)?'d': S_ISSOCK(m)?'s': S_ISBLK(m)?'m': S_ISLNK(m)?'l': S_ISCHR(m)? 'c': '-';
#define C(mask,f) txt[i++]=(m&mask)?f:'-';
  C(S_IRUSR,'r');C(S_IWUSR,'w');C(S_IXUSR,'x');
  C(S_IRGRP,'r');C(S_IWGRP,'w');C(S_IXGRP,'x');
  C(S_IROTH,'r');C(S_IWOTH,'w');C(S_IXOTH,'x');
#undef C
  txt[i++]=0;
  puts_stderr(txt);
}


//
///////////////////
/// file stat   ///
///////////////////

static const struct stat empty_stat={0};
static long cg_file_size(const char *path){
  if (!path) return -1;
  struct stat st;
  return stat(path,&st)?-1:st.st_size;
}


static bool cg_file_exists(const char *path){
  struct stat st;
  return path && !stat(path,&st);
}


#define cg_pid_exists(pid)  (!kill(pid,0)) // Or   getpgid(pid)>=0

#define ST_BLKSIZE 4096
static void cg_clear_stat(struct stat *st){ if(st) *st=empty_stat;}
static void stat_init(struct stat *st, int64_t size,const struct stat *uid_gid){
  const bool isdir=size<0;
  cg_clear_stat(st);
  st->st_mode=isdir?(S_IFDIR|0777):(S_IFREG|0666);
  st->st_nlink=1;
  if (!isdir){
    st->st_size=size;
    st->st_blocks=(size+511)>>9;
  }
  st->st_blksize=ST_BLKSIZE;
  if (uid_gid){
    st->st_gid=uid_gid->st_gid;
    st->st_uid=uid_gid->st_uid;
    st->ST_MTIMESPEC=uid_gid->ST_MTIMESPEC;
  }else{
    // geteuid()  effective user ID of the calling process.
    // getuid()   real user ID of the calling process.
    // getgid()   real group ID of the calling process.
    // getegid()  effective group ID of the calling process.
    st->st_gid=getgid();
    st->st_uid=getuid();
  }
}

#define cg_log_file_stat(...) _cg_log_file_stat(__func__,__VA_ARGS__)
static void _cg_log_file_stat(const char *func,const char * name,const struct stat *s){

  const char *color=ANSI_FG_BLUE;
#ifdef SHIFT_INODE_ROOT
  if (s->st_ino>(1L<<SHIFT_INODE_ROOT)) color=ANSI_FG_MAGENTA;
#endif
  fprintf(stderr,"%s() '%s' stat: %s",func,name,s?" ":"NULL");
  if (s){
    fprintf(stderr,"size=%lld lm:%s blksize=%lld blocks=%lld links=%u inode=%s%llu"ANSI_RESET" dir=%s uid=%u gid=%u ",(LLD)s->st_size,ST_MTIME(s),(LLD)s->st_blksize,(LLD)s->st_blocks,  (uint32_t) s->st_nlink,color,(LLU)s->st_ino,  yes_no(S_ISDIR(s->st_mode)), s->st_uid,s->st_gid);
    cg_log_file_mode(s->st_mode);
  }
  fputc('\n',stderr);
}
static void cg_log_open_flags(int flags){
  fprintf(stderr,"flags=%x{",flags);
#define C(a) if (flags&a) fprintf(stderr,#a" ")
  C(O_RDONLY); C(O_WRONLY);C(O_RDWR);C(O_APPEND);C(O_CLOEXEC);C(O_CREAT);C(O_DIRECTORY);C(O_DSYNC);C(O_EXCL);C(O_NOCTTY);C(O_NOFOLLOW);C(O_NONBLOCK);C(O_SYNC);C(O_TRUNC);
#ifdef O_DIRECT
  C(O_DIRECT);
#endif
#ifdef O_PATH
  C(O_PATH);
#endif
#ifdef O_LARGEFILE
  C(O_LARGEFILE);
#endif

#ifdef O_NOATIME
  C(O_NOATIME);
#endif
#ifdef O_TMPFILE
  C(O_TMPFILE);
#endif
#ifdef O_ASYNC
  C(O_ASYNC);
#endif
#ifdef O_NDELAY
  C(O_NDELAY);
#endif

#undef C
  fputc('}',stderr);
}



static bool cg_stat_differ(const char *title,const struct stat *s1,const struct stat *s2){
  if (!s1||!s2) return false; // memcmp would lead to false positives
  const char *wrong=NULL;
#define C(f) (wrong=#f,s1->f!=s2->f)
  //  if (C(st_ino)||C(st_mode)||C(st_uid)||C(st_gid)||C(st_size)||C(st_blksize)||C(st_blocks)||C(st_mtime)||C(st_ctime)||(wrong=NULL,false)){  knownConditionTrueFalse
  if (C(st_ino)||C(st_mode)||C(st_uid)||C(st_gid)||C(st_size)||C(st_blksize)||C(st_blocks)||C(st_mtime)||C(st_ctime)){
    log_warn("stat_t.%s\n",wrong);
    cg_log_file_stat(title,s1);
    cg_log_file_stat(title,s2);
    return true;
  }
#undef C
  //  log_succes("Stat are identical: %s\n",title);
  return false;
}
#define cg_is_dir(f) cg_is_stat_mode(S_IFDIR,f)
#define cg_is_symlink(f) cg_is_stat_mode(S_IFLNK,f)
#define cg_is_regular_file(f) cg_is_stat_mode(S_IFREG,f)


static bool cg_is_stat_mode(const mode_t mode,const char *f){
  struct stat st={0};
  return !PROFILED(lstat)(f,&st) &&  (st.st_mode&S_IFMT)==mode;
}

static bool cg_access_from_stat(const struct stat *stats,int mode){ // equivaletn to access(path,mode)
  int granted;
  mode&=(X_OK|W_OK|R_OK);
#if R_OK!=S_IROTH || W_OK!=S_IWOTH || X_OK!=S_IXOTH
  ?error Oops, portability assumptions incorrect.;
#endif
  if (mode==F_OK) return 0;
  if (getuid()==stats->st_uid)
    granted=(unsigned int) (stats->st_mode&(mode<<6))>>6;
  else if (getgid()==stats->st_gid || cg_group_member(stats->st_gid))
    granted=(unsigned int) (stats->st_mode&(mode<<3))>>3;
  else
    granted=(stats->st_mode&mode);
  return granted==mode;
}
static bool cg_file_set_atime(const char *path, const struct stat *st_or_null,long secondsFuture){
  const struct stat st;
  if (!st_or_null && stat(path,(struct stat*)(st_or_null=&st))) return false;
  //log_verbose("secondsFuture=%ld\n",secondsFuture);
  struct utimbuf new_times={.actime=time(NULL)+secondsFuture,.modtime=st_or_null->st_mtime};
  return !utime(path,&new_times);
}

static bool cg_file_set_mtimes(const char *path, const struct timespec mtime){
  struct timeval tt[2]={0};
  tt[1].tv_sec=mtime.tv_sec;
  tt[1].tv_usec=mtime.tv_nsec/1000;
  return !utimes(path,tt);
}
// timespec
static bool cg_file_set_mtime(const char *path, const time_t time){
  struct timeval tt[2]={0};
  tt[1].tv_sec=time;
  if (utimes(path,tt)){
    perror(path);
    return false;
  }
  return true;

}


#define  cg_set_executable(path)  cg_set_st_mode_flag(path,S_IRWXU)
static bool cg_set_st_mode_flag(const char *path, mode_t mode){
  struct stat st;
  return path && *path && !PROFILED(stat)(path,&st) && !chmod(path,mode|st.st_mode);
}

static bool _cg_is_none_interactive;
static int cg_getc_tty(void){
  if (_cg_is_none_interactive) return EOF;
  static FILE *tty;
  if (!tty && !(tty=fopen("/dev/tty","r"))) tty=stdin;
  return getc(tty);
}

/* write() may be write only part of the data */
static bool cg_fd_write(const int fd,const char *t,const off_t size0){
  for(off_t size=size0; size>0;){
    off_t n=write(fd,t,size);
    if (n<0) return false;
    t+=n;
    size-=n;
  }
  return true;
}

static bool cg_fd_write_str(const int fd,const char *t){
  return t && cg_fd_write(fd,t,strlen(t));
}


static int cg_rename(const char *old, const char *n){
  if (!old || !n || !*old || !*n) return -1;
  const int ret=rename(old,n);
  if (ret) log_errno("rename(%s,%s)",old,n);
  return ret;
}

static bool cg_unlink(const char *f){
  if (!f || !*f) return false;
  int err=0;
  if (unlink(f)==-1) err=errno;
  if (cg_file_exists(f)){ log_errno("unlink(%s)  %s",f,error_symbol(err));return false;}
  return true;
}


static int cg_symlink_overwrite_atomically(const char *src,const char *lnk){
  log_verbose("src: %s lnk: %s \n",src,lnk);
  if (!cg_is_symlink(lnk)) unlink(lnk);
  char lnk_tmp[MAX_PATHLEN+1]; strcpy(stpcpy(lnk_tmp,lnk),".tmp");
  unlink(lnk_tmp);
  symlink(src,lnk_tmp);
  return cg_rename(lnk_tmp,lnk);
}



static void cg_print_substring(int fd,const char *s,int f,int t){  write(fd,s,MIN_int(cg_strlen(s),t)); }


static bool cg_mkdir(const char *path,const mode_t mode){
  if (!path) return false;
  const bool ok=!mkdir(path,mode) || errno==EEXIST;
  if (!ok) perror(path);
  return ok;

}
static bool _cg_recursive_mkdir(const bool parentOnly,const char *path){
  if (!path) return false;
  const int n=cg_pathlen_ignore_trailing_slash(path);
  char p[n+1]; strcpy(p,path);
  FOR(i,2,n){
    if (p[i]=='/'){
      p[i]=0;
      if (!cg_mkdir(p,S_IRWXU)){
        log_error("path: %s",path);
        return false;
      }
      p[i]='/';
    }
  }
  if (!parentOnly &&  !cg_mkdir(p,S_IRWXU)) return false;
  return true;
}
#define cg_recursive_mkdir(path) _cg_recursive_mkdir(false,path)
#define cg_recursive_mk_parentdir(path) _cg_recursive_mkdir(true,path)


static void log_list_filedescriptors(const int fd){
  if (!has_proc_fs()){
    fprintf(stderr,ERROR_MSG_NO_PROC_FS"n");
  }else{
    const pid_t pid0=getpid();
    if (!fork()){
      char path[33];
      sprintf(path,"/proc/%d/fd",pid0);
      execl("/usr/bin/ls","/usr/bin/ls",path,(char*)NULL);
      exit(errno);
    }
  }
}


static char* cg_path_expand_tilde(char *dst, const int dst_max, const char *path){

  if (!dst) dst=(char*)path;
  if (dst){
    char *d=dst;
    if (path){
      const char *s=path;
      if (*path=='~'){
        const char *h=getenv("HOME");
        assert(h!=NULL);
        if (h){
          const int hl=strlen(h),path_l=strlen(path);
          assert(hl+path_l<=(dst_max?dst_max:PATH_MAX));
          memmove(dst+hl-1,path,path_l+1); /* Overlapping allowed. dst and path may be identical*/
          memcpy(dst,h,hl);
          s=dst;
        }
      }
      while(*s){
        if ((*d++=*s++)=='/')  while(*s=='/') s++;
      }
      while(d>path && d[-1]=='/') --d;
    }
    *d=0;
  }
  return dst;
}
#define cg_copy_path(dst,src) cg_path_expand_tilde(dst,PATH_MAX,src)
///////////////////
///    time     ///
///////////////////
static double cg_timespec_diff(const struct timespec a, const struct timespec b){
  double v=(a.tv_sec-b.tv_sec)+(a.tv_nsec-b.tv_nsec)/(1000*1000*1000.0);
  return v;
}
#define CG_STAT_B_BEFORE_A(a,b) cg_timespec_b_before_a(a.ST_MTIMESPEC,b.ST_MTIMESPEC)
static bool cg_timespec_b_before_a(struct timespec a, struct timespec b){  //Returns true if b happened first.
  return a.tv_sec==b.tv_sec ? a.tv_nsec>b.tv_nsec : a.tv_sec>b.tv_sec;
}
static struct timespec cg_file_last_modified(const char *path){
  struct stat st;
  static struct timespec TZERO={0};
  if (!path || !*path || PROFILED(stat)(path,&st)) return TZERO;
  return st.ST_MTIMESPEC;
}
static bool cg_file_is_newer_than(const char *path1,const char *path2){
  struct timespec t1=cg_file_last_modified(path1);
  struct timespec t2=cg_file_last_modified(path2);
  return cg_timespec_b_before_a(t1,t2);
}


static struct timespec cg_file_mtim(const char *path){
  struct stat st={0};
  stat(path,&st);
  return st.ST_MTIMESPEC;
}

static time_t cg_file_mtime(const char *path){
  struct stat st={0};
  stat(path,&st);
  return st.st_mtime;
}


static char *cg_ctime(time_t t){
  return ctime(&t);
}

/////////////
////  id  ///
/////////////
static bool cg_is_member_of_group(char *group){
  int size=getgroups(0,NULL);
  gid_t gg[size];
  getgroups(size,gg);
  FOR(i,-1,size){
    struct group *g=getgrgid(i<0?getegid():gg[i]);
    if (!strcmp(group,g->gr_name)) return true;
  }

  return false;
}
#define HINT_GRP_DOCKER "The current user is not member of group 'docker'. Docker based auto-generation will not work.\nIf you do not need auto-generation, then ignore this message.\nConsider to run 'newgrp docker' before starting ZIPsFS.\n"
static bool cg_is_member_of_group_docker(void){
  static int r=0;
  if (!r) r=cg_is_member_of_group("docker")?1:-1;
  return r==1;
}

///////////////////
///  process    ///
///////////////////
//static bool cg_log_exec_fd(const int fd, char const * const * const cmd, char const * const * const env){
static bool cg_log_exec_fd(const int fd, const char *cmd[], const char  *env[]){
  bool compact=cmd && !env;
  if (compact){
    int len=0;
    for(const char **a=cmd; *a; a++) len+=cg_strlen(*a);
    if (len<60) compact=true;
  }

  RLOOP(j,2){
    char **s=(char**)(j?env:cmd);
    if (s){
      cg_fd_write_str(fd,j?"ENVIRONMENT VARIABLES:\n":"COMMAND-LINE:");
      if (!compact) cg_fd_write_str(fd,"\n");
      while(*s){
        cg_fd_write_str(fd,"  ");
        const char quote=cg_find_invalidchar(VALIDCHARS_NOQUOTE,*s,strlen(*s))<0?0 :strchr(*s,'\'')||strchr(*cmd,'\\')?'"':'\'';
        if (quote) write(fd,&quote,1);
        cg_fd_write_str(fd,*s++);
        if (quote) write(fd,&quote,1);
        if (j) cg_fd_write_str(fd,"\n");
      }
      cg_fd_write_str(fd,"\n");
      if (!j && *cmd && !strcmp(*cmd,"docker") && !cg_is_member_of_group_docker()) cg_fd_write_str(fd,HINT_GRP_DOCKER);
    }

  }
  cg_fd_write_str(fd,"\n");
  return true;
}
static bool cg_log_waitpid_status(FILE *f,const unsigned int status,const char *msg){
  int logged=0;
  if (status){
    FOR(j,0,f?2:1){
      if (logged && f) fputs("STATUS OF fork() - exec() - waitpid()\n",f);
#define C(m) if (m(status)){logged++; if (j) fprintf(f,"    %s",#m);}
      const int current=logged;
      C(WIFEXITED); C(WIFSIGNALED); C(WIFSTOPPED);   C(WIFCONTINUED);
      if (WIFSIGNALED(status)) C(WCOREDUMP);
      if (j && current!=logged) fputc('\n',f);
#undef C
#define C(m) if (m(status)){logged++; if(j) fprintf(f,"  %s=%u\n",#m,m(status));}
      if (WIFSIGNALED(status)) C(WTERMSIG);
      if (WIFSTOPPED(status))  C(WSTOPSIG);
      if (WIFEXITED(status)){
        C(WEXITSTATUS);
        if (j){fprint_strerror(f,WEXITSTATUS(status)); fputc('\n',f);}
      }
#undef C
      if (!logged) break;
    }
  }
  return logged;
}

#define cg_log_waitpid(...) _cg_log_waitpid(__VA_ARGS__,__func__)
//static int _cg_log_waitpid(const int pid, const int status, const char *err,const bool append, char const * const cmd[],char const * const env[], const char *func){
static int _cg_log_waitpid(const int pid, const int status, const char *err,const bool append, const char  *cmd[], const char *env[], const char *func){
  if (pid<0 || cg_log_waitpid_status(stderr,status,func)){
    FILE *f=fopen(err,"a");
    if (f){
      if (pid==-1) fputs("waitpid() failed.\n",f);
      if (pid==-2) fputs("Output file missing.\n",f);
      if (cmd || env) cg_log_exec_fd(fileno(f),cmd,env);
      fprint_strerror(f,errno);
      fclose(f);
    }
  }
  return pid<0?-1:WIFEXITED(status)?WEXITSTATUS(status):INT_MIN;
}
//static void cg_exec(char const * const * const cmd,char const * const * const env,const int fd_out,const int fd_err){
#define E(msg) perror(__FILE__":"STRINGIZE(__LINE__)" "msg);
//static bool cg_exec(char const * const * const cmd,char const * const * const env,const int fd_out,const int fd_err){
static int fd_dev_null(){
  static int fd;
  if (!fd && (fd=open("/dev/null",O_WRONLY))<=0){ perror(__FILE__":"STRINGIZE(__LINE__)" /dev/null"); exit(1);}
  return fd;
}
static int cg_exec(const char  *cmd[], const char *env[],const int fd_out,const int fd_err){
  errno=0;
  if(fd_out>0) dup2(fd_out,STDOUT_FILENO);
  if(fd_err>0) dup2(fd_err,STDERR_FILENO);
  if(fd_out>0) close(fd_out);
  if(fd_err>0 && fd_err!=fd_out) close(fd_err);
  cg_log_exec_fd(STDERR_FILENO,cmd,env);
#if defined(HAS_EXECVPE) && HAS_EXECVPE
  execvpe(cmd[0],(char *const *)cmd,(char *const *)env);
#elif ! defined(HAS_UNDERSCORE_ENVIRON) || HAS_UNDERSCORE_ENVIRON
  extern char **_environ; /* default */
  if (env) _environ=(char**)env;
  execvp(cmd[0],(char *const *)cmd);
#else
  execvp(cmd[0],(char *const *)cmd);
#endif
  E("cg_exec");
  return errno;
}
static bool cg_fork_exec(const char *cmd[], const char *env[],const int fd_out,const int fd_err){
  const pid_t pid=fork(); /* Make real file by running external prg */
  if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);return false;}
  if (!pid){
    cg_array_remove_element(cmd,"");
    cg_log_exec_fd(STDERR_FILENO,cmd,NULL);
    exit(cg_exec(cmd,env,fd_out,fd_err));
  }
  int wstatus;
  waitpid(pid,&wstatus,0);
  errno=(WIFEXITED(wstatus)?WEXITSTATUS(wstatus):0);
  return !errno;
}
static bool is_installed_program(const char *prg){
  const char *cmd[]={prg,"--version",(char*)0};
  return cg_fork_exec(cmd,NULL,fd_dev_null(),fd_dev_null());
}

#define I(program) static int i;  if (!i) i=is_installed_program(program)?1:-1;  return i==1
static bool is_installed_curl(){ I("curl");}
static bool is_installed_wget(){
  //if (DEBUG_NOW==DEBUG_NOW) return false;
  I("wget");}
#undef I

//////////////////////////////////////////////////////////////////////
// Compare text and file content
//////////////////////////////////////////////////////////////////////
#if CODE_NOT_NEEDED
#define DIFFERS_F_S_REPORT_NEXIST (1<<1)
static int differs_filecontent_from_string(const int opt,const char* path, const long seek,const char* text,const long text_l){
  const int fd=open(path,O_RDONLY);
  if (fd<0){ if (0!=(opt&DIFFERS_F_S_REPORT_NEXIST)) log_errno("open(\"%s\",O_RDONLY)",path); return -1;}
  char buf[4096];
  int n;
  long pos=0;
  while((n=read(fd,buf,sizeof(buf)))>0){
    RLOOP(i_buf,n){
      const long i_txt=pos-seek+i_buf;/* TODO inefficient */
      if (i_txt>=0 && i_txt<text_l){
        //        printf("%c %c \n", text[i_txt],buf[i_buf]);
        if (text[i_txt]!=buf[i_buf]) return true;
      }
    }
    pos+=n;
  }
  if (pos<seek+text_l) return true;
  if (n<0) log_errno("read(fd,...) %s",path);
  close(fd);
  return 0;
}
#endif //CODE_NOT_NEEDED

#undef E
#endif // _cg_utils_dot_c
// 1111111111111111111111111111111111111111111111111111111111111
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0

static bool cg_pid_exists_proc(const pid_t pid){
  static char path[99];
  sprintf(path,"/proc/%ld",(long)pid);
  return cg_file_exists(path);
}

int main(int argc, char *argv[]){


  struct stat stbuf, *st=&stbuf;
  stat_init(st,0,NULL);
  st->st_mtime=time(NULL);
  cg_log_file_stat("",st);
  //  cg_log_file_mode();  fputc('\n',stderr);
  //  printf("  cg_endsWith: %d \n",cg_endsWith(0,argv[1],0,argv[2],0));
  //printf("  cg_endsWith: %d \n",cg_endsWith(ENDSWITH_FLAG_PRECEED_SLASH,argv[1],0,argv[2],0));
  //FOR(i,1,argc) printf("%s %d  ",argv[i],cg_starts_digits_char(argv[i],8,'_'));
  //cg_mkdir(argv[1],S_IRWXU);
  return 0;

  /*  char *h=strdup("/data/PLACEHOLDER_INFILE_NAMEx"); */
  /*  cg_str_replace(0,h,0,"PLACEHOLDER_INFILE_NAMEx",0, "report.parquet",0); */
  /*  fprintf(stderr,"h: %s\n",h); */
  /* return 0; */
  switch(14){
  case 0:{
    bool *ccpath=cg_validchars(VALIDCHARS_PATH);
    fprintf(stderr,"ccpath\n");
    FOR(c,0,256){
      if (ccpath[c]) putc(c,stderr);
    }
    fprintf(stderr,"\n");
    fprintf(stderr,"%s  %d\n",__func__,cg_find_invalidchar(VALIDCHARS_PATH,argv[1],strlen(argv[1])));
  } break;
  case 1:{
    struct stat stbuf;
    const char *path=argv[1];
    stat(path,&stbuf);
    cg_file_set_atime(path,&stbuf,3600L*atoi(argv[2]));
  } break;
  case 2:{
    char *h=malloc_untracked(9999);
    strcpy(h,argv[1]);
    int l1=cg_str_replace(OPT_STR_REPLACE_DRYRUN,h,0, argv[2],0,argv[3],0);

    int l2=cg_str_replace(0,h,0, argv[2],0,argv[3],0);
    printf("l1=%d l2=%d h=%s\n",l1,l2,h);
    free_untracked(h);
  } break;
  case 3:{
  } break;
  case 4:{
    int tokens_l[99];
    const char *tokens[99];
    assert(argc==2);
    const int n=cg_strsplit(':'|OPT_CG_STRSPLIT_NO_HEAP,   argv[1],0,NULL,NULL);
    cg_strsplit(':'|OPT_CG_STRSPLIT_NO_HEAP,argv[1],0,tokens,tokens_l);
    printf("n=%d\n",n);
    char token[999];
    FOR(i,0,n){
      cg_stpncpy0(token,tokens[i],tokens_l[i]);
      token[tokens_l[i]]=0;
      printf("%d/%d %s  %d  %ld\n",i,n,token,tokens_l[i], strlen(tokens[i]));
    }
  }
  case 5:{
    char *s=strdup_untracked("hello");
    // CG_REALLOC_ARRAY(char *,s,10);
    char *tmp=realloc(s,10);
    if (!tmp){fprintf(stderr,"realloc failed.\n"); EXIT(1);};
    s=tmp;
  } break;
  case 6:{
    int a=atoi(argv[1]),b=atoi(argv[2]);
    int c=(2);
    printf("max(%d,%d)=%d\n",a,b,c);
  } break;
  case 7:{
    //    log_verbose("cg_find_invalidchar=%d\n",cg_find_invalidchar(VALIDCHARS_PATH,argv[1],strlen(argv[1])));    EXIT(1);
    const char *env[]={"a=1",NULL};
    const char *cmd[]={"ls","-l","space char","backslash\\","single'quote'",NULL};
    cg_log_exec_fd(2,cmd,env);
  } break;
  case 8:{
    printf("isqrt=%d\n",isqrt(atoi(argv[1])));
  } break;
  case 9:{
    char *s=argv[1];
    puts(s);
    cg_path_expand_tilde(s,PATH_MAX,s);
    puts(s);

  } break;
  case 10:{
    char m[10]; memset(m,9,10000);
  } break;
  case 11:
    cg_symlink_overwrite_atomically(argv[1],argv[2]);
    break;
  case 12:
    //    static int cg_find_suffix(const int opt,const char *s, const int s_l,const char **xx,const int *xx_l){
    {
      assert(argc==2);
      const char *ss[]={".jpeg",".png",".gif",NULL};
      int x=cg_find_suffix(FIND_SUFFIX_IC,argv[1],strlen(argv[1]),ss,NULL);
      printf("x=%d\n",x);
    }
    break;

  case 13:
    assert(argc==3);
    printf("cg_file_is_newer_than %d \n",cg_file_is_newer_than(argv[1],argv[2]));
    break;
  case 14:{
    const char *aa[]={"","a","b","","","c","",NULL};
    //    const char *aa[]={"",NULL};
    cg_array_remove_element(aa,"");
    for(int i=0;aa[i];i++) printf("%2d '%s'\n",i,aa[i]);
    printf("cg_array_length: %d\n",cg_array_length(aa));
    break;
  }

    /* case 14:{ */
    /*   char *aa[99]; */
    /*   FOR(i,0,argc) aa[i]=*argv[i]?argv[i]:NULL; */
    /*   const int n=cg_array_remove_null_pointer(aa,argc); */
    /*   FOR(i,0,n) printf("%d '%s'\n",i,(char*)aa[i]); */
    /*   fprintf(stderr,"idx 0: %d\n",cg_idx_of_NULL(aa,argc)); */
    /* } */
  }
}

#endif
// ftp:ftp.ebi.ac.uk/pub/databases/uniprot/current_release/knowledgebase/complete/docs/keywlist.xml.gz

// ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml.gz
