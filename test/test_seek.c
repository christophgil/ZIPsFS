#define _GNU_SOURCE 1
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <assert.h>

#define TEST_BLOCK 5000
void test_seek(long begin,const char *path,const char *bb,const size_t len){
  const int fh=open(path,O_RDONLY);
  putchar('.');
  //  printf("begin=%ld\n",begin);
  assert(begin+TEST_BLOCK<len);
  lseek(fh,begin,SEEK_SET);
  char buf[TEST_BLOCK];
  long already=0;
  while(already<TEST_BLOCK){
    const int n=read(fh,buf+already,TEST_BLOCK-already);
    if (n<0) break;
    already+=n;
  }
  if (already!=TEST_BLOCK) printf("Error test_seek already=%ld  TEST_BLOCK=%d \n",already,TEST_BLOCK);

  int cmp=memcmp(buf,bb+begin,TEST_BLOCK);

  if (cmp) printf("Error\n");
}
int main(int argc, char *argv[]){
  for(int k=1;k<argc;k++){
    const char *path=argv[k];
    puts(path);

    struct stat stbuf;
    stat(path,&stbuf);
    int blksize = (int)stbuf.st_blksize;
    const long len=stbuf.st_size;
    if (len<=TEST_BLOCK) { printf(" File len must be much larger than %d\n",TEST_BLOCK); continue;}
    char *bb=mmap(NULL,len+blksize,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,0,0);
    {
      const int fh=open(path,O_RDONLY|O_DIRECT); //|O_DIRECT
      long already=0;
      while(already<len){
        //const int n=read(fh,bb+already,len-already);
        const int n=read(fh,bb+already,blksize);
        //printf("Read %d\n",n);
        if (n<0){
          printf("n<0  %d\n",n);
          break;
        }
        already+=n;
      }
      close(fh);
      printf("len=%ld already=%ld\n",len,already);
      usleep(4*1000*1000);
      assert(len==already);
    }


    int imax=100;
    for(int i=imax;--i>=0;){
      long begin;
      if (1){
        long rand=random();
        begin=rand*(len-TEST_BLOCK)/RAND_MAX;
      }else{
        begin=i*(len-TEST_BLOCK)/imax;
      }
      test_seek(begin,path,bb,len);

      //usleep(1000*500);
    }


  }
}
