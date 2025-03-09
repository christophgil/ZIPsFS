#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <locale.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>
#include <pthread.h>
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_RESET "\x1B[0m"
#define TEST_BLOCK 5000

enum fh_policy{FH_THREAD, FH_EACH_BLOCK, FH_ALL_SAME};
static enum fh_policy _fh_policy;


pthread_mutex_t _mutex;
static void print_path_for_fd(int fd){
  char buf[99],path[512];
  sprintf(buf,"/proc/%d/fd/%d",getpid(),fd);
  const ssize_t n=readlink(buf,path,511);
  if (n<0){
    printf(ANSI_FG_RED"Warning %s  No path\n"ANSI_RESET,buf);
    perror(" ");
  }else{
    printf("Path for %d:  %s\n",fd,path);
  }
}

static int cg_getc_tty(void){
  static FILE *tty;
  if (!tty && !(tty=fopen("/dev/tty","r"))) tty=stdin;
  return getc(tty);
}

void press_enter(){
  fprintf(stderr,"Press enter to continue!");
  cg_getc_tty();
}
void test_seek(long begin,const char *path,const char *bb,const size_t len,const int fh_common){
  //  pthread_mutex_lock(&_mutex);
  const int fh=fh_common?fh_common:open(path,O_RDONLY);
  //  pthread_mutex_unlock(&_mutex);
  assert(fh);
  static int seqn,count_errors;
  printf("#%d Errors: %d begin=%ld\n",seqn++, count_errors, begin);
  assert(begin+TEST_BLOCK<len);
  lseek(fh,begin,SEEK_SET);
  char buf[TEST_BLOCK];
  long already=0;
  int n=0;
  while(already<TEST_BLOCK){
    n=read(fh,buf+already,TEST_BLOCK-already);
    if (n<0) break;
    already+=n;
  }
  if (already!=TEST_BLOCK){
    printf(ANSI_FG_RED"Error "ANSI_RESET"test_seek begin=%'ld, end=%'ld   len=%'ld\n",begin,begin+TEST_BLOCK,len);
    printf("                already=%ld  TEST_BLOCK=%d n=%d   \n",already,TEST_BLOCK,n);
    printf("                path=");print_path_for_fd(fh);
      count_errors++;
    press_enter();
  }
  //pthread_mutex_lock(&_mutex);
  if (fh!=fh_common) close(fh);
  //pthread_mutex_unlock(&_mutex);

  if (memcmp(buf,bb+begin,TEST_BLOCK)){
    printf(ANSI_FG_RED"Error memcmp\n"ANSI_RESET);
    count_errors++;
    press_enter();
  }
}
char *bb, *path;
long len;
int _fh=0;
void *my_thread(void *arg){
  fprintf(stderr,"\x1B[7m my_thread \x1B[0m;\n");
  const int fh=_fh_policy==FH_ALL_SAME?_fh:_fh_policy==FH_THREAD?open(path,O_RDONLY):0;
  while(1){
    int imax=1000;
    for(int i=imax;--i>=0;){
      long begin;
      if (1){
        long rand=random();
        begin=rand*(len-TEST_BLOCK)/RAND_MAX;
      }else{
        begin=i*(len-TEST_BLOCK)/imax;
      }
      if (_fh_policy==FH_ALL_SAME) pthread_mutex_lock(&_mutex);
      test_seek(begin,path,bb,len,fh);
      if (_fh_policy==FH_ALL_SAME)  pthread_mutex_unlock(&_mutex);
      usleep(1000*500);
    }
  }
}

int main(int argc, char *argv[]){
  pthread_mutex_init(&_mutex,NULL);
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in printf */
  fprintf(stderr,"Testing file seek\n\
The program reads the given file entirely into RAM.\n\
It then reads random parts using fseek() and compars.\n\
A red error message will appear on error\n\
Options:\n\
 -1: One file handle for all.\n\
 -m: Each block-read with new own file handle.\n");


  for(int c;(c=getopt(argc,(char**)argv,"1m"))!=-1;){
    switch(c){
    case '1': _fh_policy=FH_ALL_SAME; break;
    case 'm': _fh_policy=FH_EACH_BLOCK;break;
    }
  }

  if (argc!=1+optind){
    fprintf(stderr,"argc: %d, optind: %d  Expected one parameter:   file-path     \n",argc,optind);
    fprintf(stderr,"%s ~/tmp/ZIPsFS/mnt/misc_tests/local_files/PRO3/20230612_PRO3_AF_004_MA_HEK200ng_5minHF_001_V11-20-HSoff_INF-B_1.d/analysis.tdf\n",argv[0]);
    exit(1);
  }


    path=argv[optind];
    if (_fh_policy==FH_ALL_SAME){ _fh=open(path,O_RDONLY); assert(_fh>0); }
  fprintf(stderr,"path: %s,  policy: %s\n", path, _fh_policy==FH_EACH_BLOCK?"FH_EACH_BLOCK": _fh_policy==FH_ALL_SAME?"FH_ALL_SAME": _fh_policy==FH_THREAD?"FH_THREAD":"????");
  press_enter();
  struct stat stbuf;
  stat(path,&stbuf);
  int blksize = (int)stbuf.st_blksize;
  len=stbuf.st_size;
  if (len<=TEST_BLOCK) { printf(" File len must be much larger than %d\n",TEST_BLOCK); return 1;}
  fprintf(stderr,"path=%s blksize=%d\n",path,blksize);
  bb=mmap(NULL,len+blksize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
  {
    const int fh=open(path,O_RDONLY);
    long already=0;
    while(already<len){
      const int n=read(fh,bb+already,blksize);
      //printf("Read n=%d blksize=%d\n",n,blksize);
      if (n<0){
        printf("n<0  %d fh=%d \n",n,fh);
        break;
      }
      already+=n;
    }
    close(fh);
    printf("len=%ld already=%ld\n",len,already);
    usleep(4*1000*1000);
    assert(len==already);
  }

#define THREAD_NUM 10
  pthread_t thread[THREAD_NUM];
  for(int i=THREAD_NUM;--i>=0;){
    fprintf(stderr,"Going to start thread %d ...\n",i);
    pthread_create(thread+i,NULL,&my_thread,NULL);
  }
  usleep(1000*1000*1000);
}


/*




 */
