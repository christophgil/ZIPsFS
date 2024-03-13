/*
  Copyright (C) 2023   christoph Gille
  Simple buffer for texts

*/
# define _GNU_SOURCE
#ifndef cg_textbuffer_dot_c
#define cg_textbuffer_dot_c
#define ANSI_RESET "\x1B[0m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_GREEN "\x1B[32m"
#define ANSI_INVERSE "\x1B[7m"
#define ANSI_BOLD "\x1B[1m"
#define ANSI_UNDERLINE "\x1B[4m"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>
#include <stddef.h>
#include <malloc.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <unistd.h>
#ifndef MIN
#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))
#endif

#include "cg_log.c"
// pipe
// 1111111111111111111111111111111111111111111111111111111111111
struct textbuffer{
  bool is_mmap; /* Otherwise heap */
  int n; /* Number segments contained */
  int capacity; /* Number segments allocated */
  off_t *segment_e; /* Next  position after this segment */
  char **segment; /* Segments */
};

static off_t textbuffer_length(struct textbuffer *b){
  return  !b || !b->n || !b->segment ? 0 : b->segment_e[b->n-1];
}
/*
  static void textbuffer_copy_to(struct textbuffer *b, char *dst, const off_t from,  off_t to){
  if (to>textbuffer_length(b)) to=textbuffer_length(b);
  if (from>=to) return;
  off_t f0=0;
  for(int i=0;i<b->n;i++){
  const off_t t0=b->segment_e[i];
  off_t f=MAX(f0,from),t=MIN(t0,to);
  if (f<t){
  memcpy(dst,b->segment[i]+(t-t0),t-f);
  }
  f0=t0;
  }
  }
*/
static void textbuffer_add_segment(struct textbuffer *b, char *bytes, size_t size){
  const int n=b->n++;
  if (!b->capacity || n>=b->capacity){
    const int c=b->capacity=!n?16:(n<<2);
    b->segment=realloc(b->segment,c*sizeof(void*));
    b->segment_e=realloc(b->segment_e,c*sizeof(void*));
  }
  b->segment_e[n]=(n?b->segment_e[n-1]:0)+size;
  b->segment[n]=bytes;
  //log_debug_now("textbuffer_add_segment %ld  current size=%ld\n",size,textbuffer_length(b));
}
static bool textbuffer_from_stdout(struct textbuffer *b,char *cmd[],  char *env[], const int segment_size){
  int pipefd[2];
  const int fd=pipe(pipefd);
  const pid_t pid=fork();
  if (pid<0){ log_errno("fork() returned %d",pid); return false;}
  if (pid){
    close(pipefd[1]);
    while(true){
      char *buf=malloc(segment_size); //SSIZE_MAX
      assert(buf!=NULL);
      const ssize_t n=read(pipefd[0],buf,segment_size);
      if (n<=0){ free(buf); break;}
      textbuffer_add_segment(b,n==segment_size?buf:realloc(buf,n),n);
    }
    close(pipefd[0]);
    int status=0;
    waitpid(pid,&status,0);
    log_wait_status(status);
    return status==0;
  }else{
    //log_debug_now("I am child %s \n",cmd[0]);
    dup2(pipefd[1],STDOUT_FILENO);
    close(pipefd[0]);
    close(pipefd[1]);
    if (env && *env) execvpe(cmd[0],cmd,env); else execvp(cmd[0],cmd);
    log_debug_now("I am child exec failed \n");
  }
  return false;
}
static void textbuffer_reset(struct textbuffer *b){
  if (b){
    char **ss=b->segment;
    off_t *ee=b->segment_e;
    bool is_mmap=b->is_mmap;
    for(int i=b->n;--i>=0;){
      if (ss && ss[i]){
        if (is_mmap) munmap(ss[i],ee[i]-(i?ee[i-1]:0)); else free(ss[i]);
      }
    }
    b->n=0;
  }
}
static void textbuffer_destroy(struct textbuffer *b){
  if (b){
    textbuffer_reset(b);
    free(b->segment_e);
    free(b->segment);
    memset(b,0,sizeof(struct textbuffer));
  }
}
/* Be aware of partial writes */
static bool my_write(int fd,char *t,const size_t size0){
  for(size_t size=size0; size>0;){
    size_t n=write(fd,t,size);
    if (n<0) return false;
    t+=n;
    size-=n;
  }
  return true;
}
static bool textbuffer_to_filedescriptor(const int fd,struct textbuffer *b){
  if (!b) return false;
  for(int i=0,e=0;i<b->n;i++){
    const off_t n=b->segment_e[i]-(i?b->segment_e[i-1]:0);
    assert(n>0);
    if (!my_write(fd,b->segment[i],n)) return false;
  }
  return true;
}
#endif // cg_textbuffer_dot_c
#if __INCLUDE_LEVEL__ == 0

static void test_exec(){
  char *cmd[]={"/usr/bin/seq","-s"," ","1000",NULL};
  struct textbuffer b={0};
  for(int i=100;--i>=0;){
    textbuffer_reset(&b);
    textbuffer_from_stdout(&b,cmd,NULL,40);
  }
  fprintf(stderr," Read %ld bytes \n",textbuffer_length(&b));
  textbuffer_to_filedescriptor(STDOUT_FILENO,&b);
  textbuffer_destroy(&b);
}
int main(int argc,char *argv[]){
  switch(0){
  case 0: test_exec();break;
  }
}
#endif //  __INCLUDE_LEVEL__
