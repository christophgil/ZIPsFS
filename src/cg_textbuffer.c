/*
  Copyright (C) 2023   christoph Gille
  Simple buffer for texts

*/
# define _GNU_SOURCE
#ifndef cg_textbuffer_dot_c
#define cg_textbuffer_dot_c
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include "cg_utils.c"

// vfork pthread_create
#define TEXTBUFFER_DIM_STACK 16
#define TEXTBUFFER_MMAP (1<<0)
struct textbuffer{
  int flags; /* Otherwise heap */
  int n; /* Number segments contained */
  int capacity; /* Number segments allocated */
  off_t *segment_e, _onstack_segment_e[TEXTBUFFER_DIM_STACK]; /* Next  position after this segment */
  char **segment,*_onstack_segment[TEXTBUFFER_DIM_STACK]; /* Segments */
  off_t max_length;
  int read_bufsize;
};
#define textbuffer_new() calloc(1,sizeof(struct textbuffer))
#define TEXTBUFFER_MEMUSAGE_MMAP (1<<0)
#define TEXTBUFFER_MEMUSAGE_PEAK (1<<1)
#define TEXTBUFFER_MEMUSAGE_COUNT_ALLOC (1<<2)
#define TEXTBUFFER_MEMUSAGE_COUNT_FREE (1<<3)
#define textbuffer_memusage_get(flags) textbuffer_memusage(flags,0)
static pthread_mutex_t *_textbuffer_memusage_lock=NULL;
static int64_t textbuffer_memusage(const int flags,const int64_t delta){
  if (_textbuffer_memusage_lock) pthread_mutex_lock(_textbuffer_memusage_lock);
  static int64_t usage[1<<4];
  if (delta){
    usage[flags]+=delta;
    usage[flags|(delta>0?TEXTBUFFER_MEMUSAGE_COUNT_ALLOC:TEXTBUFFER_MEMUSAGE_COUNT_FREE)]++;
    if (usage[flags]>usage[flags|TEXTBUFFER_MEMUSAGE_PEAK]) usage[flags|TEXTBUFFER_MEMUSAGE_PEAK]=usage[flags];
  }
  int64_t u=usage[flags];
  if (_textbuffer_memusage_lock) pthread_mutex_unlock(_textbuffer_memusage_lock);
  return u;
}
static void textbuffer_memusage_change(struct textbuffer *b,const int64_t delta){
  textbuffer_memusage(((b->flags&TEXTBUFFER_MMAP)!=0)?TEXTBUFFER_MEMUSAGE_MMAP:0,delta);
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
  //log_entered_function(" from=%ld to=%ld \n",from,to);
  //  const off_t l=textbuffer_length(b);  if (to>l) to=l;
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
        //log_debug_now("%d) [%d/%d] %ld-%ld  %ld-%ld    dst: %ld count:%ld",invocation,i,b->n,from,to,f,t, f-from, count);
        if (0){
          off_t test=0;
          for(off_t j=f;j<t;j++){
            const char c=b->segment[i][j-f0]; assert(f-from<to-from);
            if (isCopy) dst[j-from]=c;
            else if (dst[j-from]!=c) return 1;
          }
        }
        if (isCopy){
        memcpy(dst+f-from,b->segment[i]+(f-f0),t-f);
        }else{
          if (memcmp(dst+f-from,b->segment[i]+(f-f0),t-f)) return 1;
        }
        count+=(t-f);
      }
    }
    f0=e0;
  }
  //if(count!=(MIN_long(to,textbuffer_length(b))-from)) warning(1,__func__,"count:%ld, from:%ld to:%ld textbuffer_length:%ld",count,from,to,textbuffer_length(b));
  assert(count==(MIN_long(to,textbuffer_length(b))-from));

  return isCopy?count:0;
}

#define textbuffer_copy_to(b,from,to,dst) _textbuffer_copy_to_or_compare(true,b,from,to,dst)
#define textbuffer_differs_from_string(b,from,to,dst) (0!=_textbuffer_copy_to_or_compare(false,b,from,to,dst))







static void textbuffer_add_segment(struct textbuffer *b, char *bytes, off_t size){
  if (!b || !bytes) return;
  if (!size) size=strlen(bytes);
  const int n=b->n++;
  if (n>=b->capacity){
    if (n<TEXTBUFFER_DIM_STACK){
      assert(!b->capacity);
      b->capacity=TEXTBUFFER_DIM_STACK;
      b->segment=b->_onstack_segment;
      b->segment_e=b->_onstack_segment_e;
    }else{
      const int c=b->capacity=n?(n<<2):16;
#define C(f,t) void* new_##f=calloc(1,c*sizeof(t));memcpy(new_##f,b->f,n*sizeof(t)); if (b->f!=b->_onstack_##f) free(b->f); b->f=new_##f;
      C(segment,void*);
      C(segment_e,off_t);
#undef C
    }
  }
  const int length=(n?b->segment_e[n-1]:0);
  if (b->max_length && length+size>b->max_length) size=b->max_length-length;
  if (size>0){
    b->segment_e[n]=length+size;
    b->segment[n]=bytes;
  }
}
#define textbuffer_add_segment_const(b,txt) textbuffer_add_segment(b,txt,sizeof(txt)-1)
#define malloc_or_mmap(is_mmap,size) (is_mmap?mmap(NULL,size,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0):malloc(size))
#define free_or_munmap(is_mmap,b,size) if (is_mmap) munmap(b,size); else free(b);
static char *textbuffer_malloc(struct textbuffer *b, off_t size){
  char *buf=malloc_or_mmap(b->flags&TEXTBUFFER_MMAP,size);
  if (buf){
    textbuffer_add_segment(b,buf,size);
    textbuffer_memusage_change(b,size);
  }
  return buf;
}

static off_t textbuffer_read(struct textbuffer *b,const int fd){
  const int blocksize=b->read_bufsize?b->read_bufsize:1024*1024;
  off_t count=0;
  while(true){
    char *buf=malloc_or_mmap(b->flags&TEXTBUFFER_MMAP,blocksize);
    assert(buf!=NULL);
    const ssize_t n=read(fd,buf,blocksize);
    if (n<=0){ free_or_munmap(b->flags&TEXTBUFFER_MMAP,buf,blocksize); break;}
    count+=n;
    if (n!=blocksize){
      char *buf2=malloc_or_mmap(b->flags&TEXTBUFFER_MMAP,n);
      memcpy(buf2,buf,n);
      free_or_munmap(b->flags&TEXTBUFFER_MMAP,buf,blocksize);
      buf=buf2;
    }
    textbuffer_add_segment(b,buf,n);
    textbuffer_memusage_change(b,n);
  }
  close(fd);
  return count;
}
static int textbuffer_from_exec_output(struct textbuffer *b,char *cmd[],char *env[], const char *path_stderr){
  if (!b || !cmd || !cmd[0]) return -1;
  int pipefd[2];
  const int fd=pipe(pipefd);
  const pid_t pid=fork();
  if (!cg_recursive_mk_parentdir(path_stderr)) path_stderr=NULL;
  if (pid<0){ log_errno("fork() returned %d",pid); return EIO;}
  if (pid){
    close(pipefd[1]);
    textbuffer_read(b,pipefd[0]);
    return cg_waitpid_logtofile_return_exitcode(pid,path_stderr);
  }else{
    close(pipefd[0]);
    cg_exec(env,cmd,pipefd[1],!path_stderr?-1:open(path_stderr,O_RDWR|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR));
  }
  log_errno0("After execvp should not happen\n");
  return -1;
}
static void textbuffer_reset(struct textbuffer *b){
  if (!b) return;
  char **ss=b->segment;
  off_t *ee=b->segment_e;
  bool is_mmap=0!=(b->flags&TEXTBUFFER_MMAP);
  RLOOP(i,b->n){
    if (ss && ss[i]){
      const off_t size=ee[i]-(i?ee[i-1]:0);
      free_or_munmap(is_mmap,ss[i],size);
      ss[i]=NULL;
      textbuffer_memusage_change(b,-size);
    }
  }
  b->n=0;
}
static void textbuffer_destroy(struct textbuffer *b){
  if (b){
    textbuffer_reset(b);
#define C(f) if (b->f!=b->_onstack_##f) free(b->f);
    C(segment);
    C(segment_e);
#undef C
    memset(b,0,sizeof(struct textbuffer));
  }
}
static bool textbuffer_write_fd(struct textbuffer *b,const int fd){
  if (!b) return false;
  int e=0;
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
  if (n<0) log_errno0("read(fd,...)");
  return n<0||pos<textbuffer_length(b)?-1:0;
}

#endif // cg_textbuffer_dot_c
#if __INCLUDE_LEVEL__ == 0
static void test_ps_pid(const int pid){
  char spid[99];
  sprintf(spid,"%d",pid);
  char *cmd[]={"ps","-p",spid,NULL};
  //char *cmd[]={"/usr/bin/ls","/",NULL};
  struct textbuffer b={0};
  textbuffer_reset(&b);
  textbuffer_from_exec_output(&b,cmd,NULL,NULL);
  fprintf(stderr," Read %ld bytes \n",textbuffer_length(&b));
  textbuffer_write_fd(&b,STDOUT_FILENO);
  fprintf(stderr,"EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE  \n");
  textbuffer_destroy(&b);
}

static void test_write_byte_by_byte(struct textbuffer *b){
  const off_t l=textbuffer_length(b);
  FOR(i,0,l){
    const char c=textbuffer_char_at(b,i);
    putchar(c<0?'!':c);
  }
}
static void test_write_bytes_block(const int fd,struct textbuffer *b){
#define BUF 7
  char buf[BUF];
  const off_t l=textbuffer_length(b);
  for(off_t i=0;i<l-BUF-1;i+=BUF){
    const int n=textbuffer_copy_to(b,i,i+BUF,buf);
    if (fd>=0) write(fd,buf,n);
  }
#undef BUF
}
static void test_exec(){
  char *cmd[]={"/usr/bin/seq","-s"," ","2000",NULL};
  //char *cmd[]={"/usr/bin/ls","/",NULL};
  char *path_stderr="/home/cgille/tmp/test/cg_textbuffer_stderr.txt";
  struct textbuffer b={0};
  RLOOP(i,10){
    textbuffer_reset(&b);
    b.read_bufsize=5;
    textbuffer_from_exec_output(&b,cmd,NULL,path_stderr);
  }
  fprintf(stderr," Read %ld bytes \n",textbuffer_length(&b));
  //  test_write_bytes_block(-1,&b);
  test_write_bytes_block(STDOUT_FILENO,&b);
  fprintf(stderr,"EEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEEE  \n");
  textbuffer_destroy(&b);
}
int main(int argc,char *argv[]){
  /*
    struct textbuffer b={0};
    FOR(i,0,10){
    char *buf=malloc(10);
    textbuffer_add_segment(&b,buf,10);
    }
    textbuffer_destroy(&b);
    return 0;
  */
  switch(3){
  case 0: test_exec();break;
  case 1: test_ps_pid(getpid());break;
  case 2:{
    long  count=131072,  from=2147479552,  to=2147610624, textbuffer_length=2177810432;
    fprintf(stderr,"count=%ld\n",count);
    fprintf(stderr,"expre=%ld\n",(MIN_long(to,textbuffer_length)-from));
  } break;
  case 3:{
    struct textbuffer b={0};
    textbuffer_add_segment(&b,argv[1],strlen(argv[1]));
    textbuffer_write_file(&b,"/home/cgille/tmp/t.txt",0644);
  } break;
  }
  //log_list_filedescriptors(STDERR_FILENO);

}
#endif //  __INCLUDE_LEVEL__
