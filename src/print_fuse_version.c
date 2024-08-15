#include <stdio.h>
#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 33

#include <fuse.h>

int main(int argc,char *argv[]){
#ifdef HELLO_WORLD
  puts("Hello World\n");
#else
  printf("-DFUSE_MAJOR_V=%d -DFUSE_MINOR_V=%d\n",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION);
#endif
}
