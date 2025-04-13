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
// vfork pthread_create
#define textbuffer_new(malloc_id) cg_calloc(malloc_id,1,sizeof(struct textbuffer))
#define TEXTBUFFER_MEMUSAGE_MMAP (1<<0)
#define TEXTBUFFER_MEMUSAGE_PEAK (1<<1)
#define TEXTBUFFER_MEMUSAGE_COUNT_ALLOC (1<<2)
#define TEXTBUFFER_MEMUSAGE_COUNT_FREE (1<<3)
#define textbuffer_memusage_get(flags) textbuffer_memusage(flags,0)
static pthread_mutex_t *_textbuffer_memusage_lock=NULL;
static off_t textbuffer_memusage(const int flags,const off_t delta){
  if (_textbuffer_memusage_lock) pthread_mutex_lock(_textbuffer_memusage_lock);
  static off_t usage[1<<4];
  if (delta){
    usage[flags]+=delta;
    usage[flags|(delta>0?TEXTBUFFER_MEMUSAGE_COUNT_ALLOC:TEXTBUFFER_MEMUSAGE_COUNT_FREE)]++;
    if (usage[flags]>usage[flags|TEXTBUFFER_MEMUSAGE_PEAK]) usage[flags|TEXTBUFFER_MEMUSAGE_PEAK]=usage[flags];
  }
  off_t u=usage[flags];
  if (_textbuffer_memusage_lock) pthread_mutex_unlock(_textbuffer_memusage_lock);
  return u;
}
static void textbuffer_memusage_change(const uint8_t flags,const struct textbuffer *b,const off_t delta){
  textbuffer_memusage(((flags&TEXTBUFFER_MUNMAP)!=0)?TEXTBUFFER_MEMUSAGE_MMAP:0,delta);
}

static off_t textbuffer_length(const struct textbuffer *b){
  return  !b || !b->n || !b->segment ? 0 : b->segment_e[b->n-1];
}
static int textbuffer_char_at(const struct textbuffer *b, const off_t pos){
  if (pos<0||!b) return -1;
  const off_t *ee=b->segment_e;
  if (pos<*ee) return b->segment[0][pos];
  int i=b->n-1;
  while(--i>=0 && pos<ee[i]);
  if (i>=0){
    const int j=pos-ee[i];
    if (j>=0 && pos<ee[i+1]) return b->segment[i+1][j];
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
    const off_t e0=b->segment_e[i];
    if (e0>from){
      const off_t f=MAX(f0,from),t=MIN(e0,to);
      if (f>=to) break;
      if (t>f){
        if (isCopy){


          assert(f-from+t-f<=to);

          memset(dst+f-from,7,t-f); // DEBUG_NOW



          memcpy(dst+f-from,b->segment[i]+(f-f0),t-f);
        }else{
          if (memcmp(dst+f-from,b->segment[i]+(f-f0),t-f)) return 1;
        }
        count+=(t-f);
      }
    }
    f0=e0;
  }
  //if(count!=(MIN_long(to,textbuffer_lengthuffer_length(b))-from)) warning(1,__func__,"count:%ld, from:%ld to:%ld textbuffer_length:%ld",count,from,to,textbuffer_length(b));
  assert(count==(MIN_long(to,textbuffer_length(b))-from));
  return isCopy?count:0;
}

#define textbuffer_copy_to(b,from,to,dst) _textbuffer_copy_to_or_compare(true,b,from,to,dst)
#define textbuffer_differs_from_string(b,from,to,dst) (0!=_textbuffer_copy_to_or_compare(false,b,from,to,dst))
#define textbuffer_add_segment_const(b,txt) textbuffer_add_segment(TEXTBUFFER_NODESTROY,b,txt,sizeof(txt)-1)
#define malloc_or_mmap(is_mmap,size) (is_mmap?cg_mmap(MALLOC_textbuffer,size,0):cg_malloc(MALLOC_textbuffer,size))
#define free_or_munmap(is_mmap,b,size) {if (is_mmap) cg_munmap(MALLOC_textbuffer,b,size); else cg_free(MALLOC_textbuffer,b);}

static int textbuffer_add_segment(const u_int8_t flags,struct textbuffer *b, const char *bytes, const off_t size_or_zero){
  if (!b || !bytes) return -1;
  const off_t size=size_or_zero?size_or_zero:strlen(bytes);
  void *new_segment=NULL, *new_segment_e=NULL, *new_segment_flags=NULL;
  if (b->max_length && textbuffer_length(b)+size>b->max_length) goto enomem;
  const int n=b->n;
  if (n>=b->capacity){
    if (n<TEXTBUFFER_DIM_STACK){
      assert(!b->capacity);
      b->capacity=TEXTBUFFER_DIM_STACK;
      b->segment=b->_onstack_segment;
      b->segment_e=b->_onstack_segment_e;
      b->segment_flags=b->_onstack_segment_flags;
    }else{
      const int c=b->capacity=(n<<2);
#define C(f)  new_##f=cg_calloc(MALLOC_textbuffer, c,sizeof(*b->f));\
      if (!new_##f) goto enomem;\
      memcpy(new_##f,b->f,n*sizeof(*b->f));\
      if (b->f!=b->_onstack_##f) cg_free(MALLOC_textbuffer,b->f); b->f=new_##f;
      C(segment);
      C(segment_e);
      C(segment_flags);
#undef C
    }
  }
  b->n++;
  const int length=(n?b->segment_e[n-1]:0);
  if (size>0){
    b->segment_e[n]=length+size;
    b->segment[n]=(char*)bytes;
    b->segment_flags[n]=flags;
  }
  return 0;
 enomem:
  cg_free(MALLOC_textbuffer,new_segment);
  cg_free(MALLOC_textbuffer,new_segment_e);
  cg_free(MALLOC_textbuffer,new_segment_flags);
  if (0==(flags&TEXTBUFFER_NODESTROY)) free_or_munmap(flags&TEXTBUFFER_MUNMAP,(char*)bytes,size);
  TEXTBUFFER_SET_ENOMEM(b);
  return ENOMEM;
}
static char *textbuffer_malloc(const int flags,struct textbuffer *b, off_t size){
  char *buf=malloc_or_mmap(flags&TEXTBUFFER_MUNMAP,size);
  if (buf){
    if (textbuffer_add_segment(flags,b,buf,size)) return NULL;
    textbuffer_memusage_change(flags,b,size);
  }else{
    TEXTBUFFER_SET_ENOMEM(b);
  }
  return buf;
}

static off_t textbuffer_read(const uint8_t flags,struct textbuffer *b,const int fd){
  const int blocksize=b->read_bufsize?b->read_bufsize:1024*1024;
  off_t count=0;
  while(true){
    char *buf=malloc_or_mmap(flags&TEXTBUFFER_MUNMAP,blocksize);
    if (!buf){
    TEXTBUFFER_SET_ENOMEM(b);
    break;
    }
    const ssize_t n=read(fd,buf,blocksize);
    if (n<=0){ free_or_munmap(flags&TEXTBUFFER_MUNMAP,buf,blocksize); break;}
    count+=n;
    if (n!=blocksize){
      char *buf2=malloc_or_mmap(flags&TEXTBUFFER_MUNMAP,n);
      assert(buf2);
      memcpy(buf2,buf,n);
      free_or_munmap(flags&TEXTBUFFER_MUNMAP,buf,blocksize);
      buf=buf2;
    }
    textbuffer_add_segment(flags,b,buf,n);
    textbuffer_memusage_change(flags,b,n);
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
    return (b->flags&TEXTBUFFER_ENOMEM)?ENOMEM:cg_waitpid_logtofile_return_exitcode(pid,path_stderr);
  }else{
    close(pipefd[0]);
    const int fd_err=!path_stderr?-1:open(path_stderr,O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
    cg_exec(env,cmd,pipefd[1],fd_err);
    if (fd_err>0) close(fd_err);
  }
  log_errno("After execvp should not happen\n");
  return -1;
}
static void textbuffer_reset(struct textbuffer *b){
  if (!b) return;
  char **ss=b->segment;
  off_t *ee=b->segment_e;
  RLOOP(i,b->n){
    if (ss && ss[i]){
      const off_t size=ee[i]-(i?ee[i-1]:0);
      u_int8_t f=b->segment_flags[i];
      if (0==(f&TEXTBUFFER_NODESTROY)){
        free_or_munmap(f&TEXTBUFFER_MUNMAP,ss[i],size);
      }

      ss[i]=NULL;
      textbuffer_memusage_change(f,b,-size);
    }
  }
  b->capacity=0;
  b->n=0;
}
static char *textbuffer_first_segment_with_min_capacity(const int flags,struct textbuffer *b, off_t min_size){
  assert(b!=NULL);
  if (b->n>0 && b->segment_e[0]<min_size) textbuffer_reset(b);
  if (!b->n) textbuffer_malloc(flags,b,min_size);
  return b->segment[0];
}
static void textbuffer_destroy(struct textbuffer *b){
  if (b){
    textbuffer_reset(b);
#define C(f) if (b->f!=b->_onstack_##f) cg_free(MALLOC_textbuffer,b->f);
    C(segment);
    C(segment_e);
    C(segment_flags);
#undef C
    memset(b,0,sizeof(struct textbuffer));
  }
}
static bool textbuffer_write_fd(struct textbuffer *b,const int fd){
  if (!b) return false;
  FOR(i,0,b->n){
    const off_t n=b->segment_e[i]-(i?b->segment_e[i-1]:0);
    assert(n>0);
    if (!cg_fd_write(fd,b->segment[i],n)) return false;
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
  char const * const cmd[]={"/usr/bin/seq","-s"," ","20",NULL};
  //char *cmd[]={"/usr/bin/ls","/",NULL};
  const char *path_stderr="/home/cgille/tmp/test/cg_textbuffer_stderr.txt";
  struct textbuffer b={0};
  RLOOP(i,10){
    textbuffer_reset(&b);
    b.read_bufsize=5;
    textbuffer_from_exec_output(0,&b,cmd,NULL,path_stderr);
  }
  fprintf(stderr," Read %lld bytes \n",(LLD)textbuffer_length(&b));
  //  test_write_bytes_block(-1,&b);
  test_write_bytes_block(STDOUT_FILENO,&b);
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
  switch(4){
  case 0: test_exec();break;
  case 1: test_ps_pid(getpid());break;
  case 2:{
    long  count=131072,  from=2147479552,  to=2147610624, textbuffer_l=2177810432;
    fprintf(stderr,"count=%ld\n",count);
    fprintf(stderr,"expre=%ld\n",(MIN_long(to,textbuffer_l)-from));
  } break;
  case 3:{
    struct textbuffer b={0};
    textbuffer_add_segment(0,&b,strdup_untracked(argv[1]),strlen(argv[1]));
    textbuffer_write_file(&b,"/home/cgille/tmp/t.txt",0644);
    textbuffer_destroy(&b);
  } break;
  case 4:{
    struct textbuffer b={0};
    textbuffer_add_segment(TEXTBUFFER_NODESTROY,&b,argv[1],strlen(argv[1]));
    textbuffer_write_file(&b,"/home/cgille/tmp/t.txt",0644);
    textbuffer_destroy(&b);
  } break;
  }
  //log_list_filedescriptors(STDERR_FILENO);

}
#endif //  __INCLUDE_LEVEL__
