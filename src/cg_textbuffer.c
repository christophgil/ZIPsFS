/*
  Copyright (C) 2023   christoph Gille
  Simple buffer for texts
*/
// cppcheck-suppress-file unusedFunction
#ifndef cg_textbuffer_dot_c
#define cg_textbuffer_dot_c
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif  // !MAP_ANONYMOUS
#include <unistd.h>
#include <pthread.h>
#include "cg_utils.c"
#include "cg_textbuffer.h"
#define TEXTBUFFER_MEMUSAGE_MMAP (1<<0)
#define TEXTBUFFER_MEMUSAGE_PEAK (1<<1)
#define TEXTBUFFER_MEMUSAGE_COUNT_ALLOC (1<<2)
#define TEXTBUFFER_MEMUSAGE_COUNT_FREE (1<<3)
#define _TXTBFFR(f,b,i) (i<TEXTBUFFER_DIM_STACK?b->_onstack##f:b->f)[i]
#define _TXTBFFR_S(b,i) _TXTBFFR(_segment,b,i)
#define _TXTBFFR_E(b,i) _TXTBFFR(_segment_e,b,i)
#define _TXTBFFR_F(b,i) _TXTBFFR(_segment_flags,b,i)
#define textbuffer_memusage_get(flags) textbuffer_memusage(flags,0)
static pthread_mutex_t *_textbuffer_memusage_lock=NULL;

static struct textbuffer *textbuffer_new(const int malloc_id){
  struct textbuffer *b=cg_calloc(malloc_id,1,sizeof(struct textbuffer));
  b->malloc_id=malloc_id;
  return b;
}



static off_t textbuffer_memusage(const int flags,const off_t delta){
  const int id=((flags&TXTBUFSGMT_MUNMAP)!=0)?TEXTBUFFER_MEMUSAGE_MMAP:0;
  static off_t usage[1<<4];
  if (_textbuffer_memusage_lock) pthread_mutex_lock(_textbuffer_memusage_lock);
  if (delta){
    if ((usage[id]+=delta)>usage[id|TEXTBUFFER_MEMUSAGE_PEAK]) usage[id|TEXTBUFFER_MEMUSAGE_PEAK]=usage[id];
    usage[id|(delta>0?TEXTBUFFER_MEMUSAGE_COUNT_ALLOC:TEXTBUFFER_MEMUSAGE_COUNT_FREE)]++;

  }
  const off_t u=usage[id];
  if (_textbuffer_memusage_lock) pthread_mutex_unlock(_textbuffer_memusage_lock);
  return u;
}


static off_t textbuffer_length(const struct textbuffer *b){
  return  !b || !b->n ? 0 : _TXTBFFR_E(b,b->n-1);
}


static int textbuffer_char_at(const struct textbuffer *b, const off_t pos){
  if (pos<0||!b) return -1; //
  if (pos<*b->_onstack_segment_e) return b->_onstack_segment[0][pos];
  int i=b->n-1;
  while(--i>=0 && pos<_TXTBFFR_E(b,i));
  if (i>=0){
    const int j=pos-_TXTBFFR_E(b,i);
    if (j>=0 && pos<_TXTBFFR_E(b,i+1)) return _TXTBFFR_S(b,i+1)[j];
  }
  return -1;
}


static off_t _textbuffer_copy_to_or_compare(const bool isCopy,const struct textbuffer *b,const off_t from,const off_t to, char *dst){
  if (!b) return 0;
  static int invocation;++invocation;
  assert(from>=0);
  if (from>=to) return 0;
  off_t f0=0,count=0;
  FOR(i,0,b->n){
    const off_t e0=_TXTBFFR_E(b,i);
    if (e0>from){
      const off_t f=MAX(f0,from),t=MIN(e0,to);
      if (f>=to) break;
      if (t>f){
        if (isCopy){
          //assert(f-from+t-f<=to);
          memcpy(dst+f-from, _TXTBFFR_S(b,i)+(f-f0),t-f);
        }else{
          if (memcmp(dst+f-from,_TXTBFFR_S(b,i)+(f-f0),t-f)) return 1;
        }
        count+=(t-f);
      }
    }
    f0=e0;
  }
  //assert(count==(MIN_long(to,textbuffer_length(b))-from));
  return isCopy?count:0;
}

#define textbuffer_copy_to(b,from,to,dst) _textbuffer_copy_to_or_compare(true,b,from,to,dst)
#define textbuffer_differs_from_string(b,from,to,dst) (0!=_textbuffer_copy_to_or_compare(false,b,from,to,dst))


#define textbuffer_add_segment_const(b,txt) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,b,txt,sizeof(txt)-1)
static void *malloc_or_mmap(const int flags, const off_t size){
  void *p=((flags&TXTBUFSGMT_MUNMAP)?cg_mmap(COUNT_TXTBUF_SEGMENT_MMAP,size,0):cg_malloc(COUNT_TXTBUF_SEGMENT_MALLOC,size));
  textbuffer_memusage(flags,size);
  return p;
}
#define free_or_munmap(flags,p,size) { _free_or_munmap(flags,p,size);p=NULL;}

static void _free_or_munmap(const int flags,const void *p,const off_t size){
  if (!p) return;
  if (flags&TXTBUFSGMT_MUNMAP) cg_munmap(COUNT_TXTBUF_SEGMENT_MMAP,p,size);
  else cg_free(COUNT_TXTBUF_SEGMENT_MALLOC,p);
  textbuffer_memusage(flags,-size);

}

static bool _textbuffer_assert_capacity(struct textbuffer *b,const int n){
  if (n>=TEXTBUFFER_DIM_STACK && n>=b->capacity){
    const int c=b->capacity=(n<<2);
#define C(f) assert(b->f!=b->_onstack##f); if (!(b->f=cg_realloc_array(COUNT_TXTBUF_SEGMENT_MALLOC, sizeof(*b->f), b->f, n, c))) return false;
    C(_segment);
    C(_segment_e);
    C(_segment_flags);
#undef C
  }
  return true;
}
static int textbuffer_add_segment(const uint8_t flags,struct textbuffer *b, const char *bytes, const off_t size_or_zero){
  if (!b || !bytes) return -1;
  const off_t size=size_or_zero?size_or_zero:strlen(bytes);
  if (flags&TXTBUFSGMT_DUP){
    bytes=COPY_TO_HEAP(COUNT_TXTBUF_SEGMENT_MALLOC,bytes,size);
    textbuffer_memusage(0,size);
  }
  if (b->max_length && textbuffer_length(b)+size>b->max_length) goto enomem;
  const int n=b->n++;
  if (!_textbuffer_assert_capacity(b,n)) goto enomem;
  const off_t length=(n?_TXTBFFR_E(b,n-1):0);
  if (size>0){
    _TXTBFFR_E(b,n)=length+size;
    _TXTBFFR_S(b,n)=(char*)bytes;
    _TXTBFFR_F(b,n)=flags;
  }
  return 0;
 enomem:
  log_error("enomem size: %'lld",(LLD)size);
#define C(f) cg_free_null(COUNT_TXTBUF_SEGMENT_MALLOC,b->f)
  C(_segment);
  C(_segment_e);
  C(_segment_flags);
#undef C
  if (0==(flags&TXTBUFSGMT_NO_FREE)) free_or_munmap(flags,bytes,size);
  TEXTBUFFER_SET_ENOMEM(b);
  return ENOMEM;
}
static char *textbuffer_malloc(const int flags,struct textbuffer *b, off_t size){
  char *buf=malloc_or_mmap(flags,size);
  if (buf && textbuffer_add_segment(flags,b,buf,size)) return NULL;
  TEXTBUFFER_SET_ENOMEM(b);
  return buf;
}

static off_t textbuffer_read(const uint8_t flags,struct textbuffer *b,const int fd){
  const int blocksize=b->read_bufsize?b->read_bufsize:1024*1024;
  off_t count=0;
  while(true){
    char *buf=malloc_or_mmap(flags,blocksize);
    if (!buf){
      TEXTBUFFER_SET_ENOMEM(b);
      break;
    }
    const ssize_t n=read(fd,buf,blocksize);
    if (n<=0){ free_or_munmap(flags,buf,blocksize); break;}
    count+=n;
    if (n!=blocksize){
      char *buf2=malloc_or_mmap(flags,n);
      assert(buf2);
      memcpy(buf2,buf,n);
      free_or_munmap(flags,buf,blocksize);
      buf=buf2;
    }
    textbuffer_add_segment(flags,b,buf,n);
  }
  close(fd);
  return count;
}
static int textbuffer_from_exec_output(const uint8_t flags,struct textbuffer *b,char const * const cmd[],char const * const env[], const char *path_stderr){
  if (!b || !cmd || !cmd[0]) return -1;
  int pipefd[2];
  pipe(pipefd);

  const pid_t pid=fork();
  if (!cg_recursive_mk_parentdir(path_stderr)) path_stderr=NULL;
  if (pid<0){ log_errno("fork() returned %d",pid); return EIO;}
  if (pid){
    close(pipefd[1]);
    textbuffer_read(flags,b,pipefd[0]);
    int status;
    return (b->flags&TEXTBUFFER_ENOMEM)?ENOMEM:waitpid(pid,&status,0),cg_log_waitpid(pid,status,path_stderr,false,cmd,env);
  }else{
    close(pipefd[0]);
    const int fd_err=!path_stderr?-1:open(path_stderr,O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
    cg_exec(cmd,env,pipefd[1],fd_err);
    if (fd_err>0) close(fd_err);
  }
  log_errno("After execvp should not happen\n");
  return -1;
}
static void textbuffer_reset(struct textbuffer *b){
  if (!b) return;
  //log_entered_function("%d",b->n);
  off_t e1=0;
  FOR(i,0,b->n){
    const off_t e=_TXTBFFR_E(b,i), n=e-e1;
    e1=e;
    const uint8_t f=_TXTBFFR_F(b,i);
    if (!(f&TXTBUFSGMT_NO_FREE)){
      free_or_munmap(f,_TXTBFFR_S(b,i),n);  /* No free for  strings constants */
    }
    if (i>=TEXTBUFFER_DIM_STACK) b->_segment[i]=NULL;
  }
  b->capacity=b->n=0;
}
static char *textbuffer_first_segment_with_min_capacity(const int flags,struct textbuffer *b, off_t min_size){
  assert(b!=NULL);
  if (b->n>0 && _TXTBFFR_E(b,0)<min_size) textbuffer_reset(b);
  if (!b->n) textbuffer_malloc(flags,b,min_size);
  return _TXTBFFR_S(b,0);
}

static void textbuffer_destroy(struct textbuffer *b){
  if (b && !(b->flags&TEXTBUFFER_DESTROYED)){
    b->flags&=~TEXTBUFFER_DESTROYED;
    textbuffer_reset(b);
#define C(f) assert(b->f!=b->_onstack##f); /*log_debug_now("Going free %p",b->f);*/ cg_free_null(COUNT_TXTBUF_SEGMENT_MALLOC,b->f);
    C(_segment);
    C(_segment_e);
    C(_segment_flags);
#undef C
  }
}
static bool textbuffer_write_fd(struct textbuffer *b,const int fd){
  if (!b) return false;
  off_t e1=0;
  FOR(i,0,b->n){
    const off_t e=_TXTBFFR_E(b,i), n=e-e1;
    e1=e;
    assert(n>0);
    if (!cg_fd_write(fd,_TXTBFFR_S(b,i),n)) return false;
  }
  return true;
}
static bool textbuffer_write_file(struct textbuffer *b,const char *path,const int mode){
  const int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,mode);
  if (fd<0){ log_errno("open(\"%s\",O_WRONLY|O_CREAT|O_TRUNC)",path); return false;}
  textbuffer_write_fd(b,fd);
  if (close(fd)) { log_errno("close(fd) %s",path); return false;}
  return true;
}

static int textbuffer_differs_from_filecontent_fd(const struct textbuffer *b,const int fd){
  char buf[4096];
  int n;
  long pos=0;
  for(;(n=read(fd,buf,sizeof(buf)))>0;pos+=n){
    if (textbuffer_differs_from_string(b,pos,pos+n,buf)) return 1;
  }
  if (n<0) log_errno("read(fd,...)");
  return n<0||pos<textbuffer_length(b)?-1:0;
}





static int exec_on_file(const int opts,const enum enum_exec_on_file type, char *output, const int output_max, const char *path ){
#define C(p) cmd[i++]=p
  struct textbuffer b={0};
  const char *cmd[9];
  {
  int i=0;
  switch(type){
  case EXECF_MOUNTPOINT_USING_DF: C("df"); C(path); break;
  case EXECF_MOUNTPOINT_USING_FINDMNT: C("findmnt"); C("-n"); C("-o"); C("TARGET"); C("--target"); C(path); break;
  default: return -1;
  }
  C(NULL);
  }
#undef C
  textbuffer_reset(&b);
  b.read_bufsize=1024;
  textbuffer_from_exec_output(0,&b,cmd,NULL,(opts&EXECF_SILENT)?"/dev/null":NULL);
  off_t len=textbuffer_copy_to(&b,0,output_max,output);
  textbuffer_destroy(&b);
  int pathB=0, pathE=0;
  bool prevSpc=type!=EXECF_MOUNTPOINT_USING_DF;
  FOR(i,0,len){
    const char c=output[i];
    const bool spc=(c==' '), lf=(c=='\n');
    if (prevSpc && c=='/'){
      if (!pathB) pathB=i+1;
    }
    if (spc||lf){
      if (pathB && !pathE) pathE=i+1;
    }
    prevSpc=spc;
  }/*for*/
  if (pathB && !pathE) pathE=len;
  const char *src=NULL;
  char *dst=NULL;
  if (type==EXECF_MOUNTPOINT_USING_FINDMNT || type==EXECF_MOUNTPOINT_USING_DF){
    if (!pathB) return (*output=0);
    src=output+pathB-1;
    dst=output;
    len=pathE-pathB;
  }
  if (dst && src>dst) while ((*dst++=*src++));
  output[len]=0;
  return len;
}




#endif // cg_textbuffer_dot_c
#if __INCLUDE_LEVEL__ == 0 && ! defined(__cppcheck__)
static void test_ps_pid(const int pid){
  char spid[99];
  sprintf(spid,"%d",pid);
  char const * const cmd[]={"ps","-p",spid,NULL};
  //char *cmd[]={"/usr/bin/ls","/",NULL};
  struct textbuffer b={0};
  textbuffer_reset(&b);
  textbuffer_from_exec_output(0,&b,cmd,NULL,NULL);
  fprintf(stderr," Read %lld bytes \n",(LLD)textbuffer_length(&b));
  textbuffer_write_fd(&b,STDOUT_FILENO);
  textbuffer_destroy(&b);
}

static void test_write_byte_by_byte(const struct textbuffer *b){
  const off_t l=textbuffer_length(b);
  FOR(i,0,l){
    const char c=textbuffer_char_at(b,i);
    putchar(c<0?'!':c);
  }
}
static void test_write_bytes_block(const int fd,const struct textbuffer *b){
#define BUF 7
  char buf[BUF];
  const off_t l=textbuffer_length(b);
  for(off_t i=0;i<l-BUF-1;i+=BUF){
    const int n=textbuffer_copy_to(b,i,i+BUF,buf);
    if (fd>=0) write(fd,buf,n);
  }
#undef BUF
}
static void test_exec(void){
  FOR(i,0,EXECF_NUM){
    char output[4444]={0};
    FOR(k,0,2){
      const char *dir=k?"/var/log":"/etc/fstab";
      exec_on_file(0,i,output,sizeof(output)-1,dir);
      printf("type: %d   %s -> output='%s';\n",i,dir,output);
    }
  }
}

static void _argc_n(const int n, const int argc){
  if (argc!=n+1){
    fprintf(stderr,"Need %d parameters\n",n);
    exit(1);
  }
}
static void _test_write_file(int argc,  char *argv[]){
  bool dup=false;
  struct textbuffer b={0};
  FOR(i,1,argc){
    const char *txt=argv[i];
    textbuffer_add_segment(dup?0:TXTBUFSGMT_NO_FREE,&b,dup?strdup_untracked(txt):txt,strlen(txt));
    textbuffer_add_segment(TXTBUFSGMT_NO_FREE,&b,",",1);
  }
  const char *f="/home/cgille/tmp/t.txt";
  textbuffer_write_file(&b,f,0644);
  fprintf(stderr,"dup: %d\nLength: %ld\ncat %s\n argument",dup,textbuffer_length(&b),f);
  textbuffer_destroy(&b);
}

int main(int argc,char *argv[]){

  /*
    struct textbuffer b={0};
    FOR(i,0,10){
    char *buf=malloc_untracked(10);
    textbuffer_add_segment(0,&b,buf,10);
    }
    textbuffer_destroy(&b);
    return 0;
  */
  switch(0){
  case 0: test_exec();break;
  case 1: test_ps_pid(getpid());break;
  case 2:{
    long  count=131072,  from=2147479552,  to=2147610624, textbuffer_l=2177810432;
    fprintf(stderr,"count=%ld\n",count);
    fprintf(stderr,"expre=%ld\n",(MIN_long(to,textbuffer_l)-from));
  } break;
  case 3:
    _test_write_file(argc,argv);
    break;
  }
  //log_list_filedescriptors(STDERR_FILENO);

}

#undef _TXTBFFR
#endif //  __INCLUDE_LEVEL__
// COUNT_TXTBUF_SEGMENT_MALLOC
