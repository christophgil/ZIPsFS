/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=passthrough_simulate_error                   ///
/////////////////////////////////////////////////////////////////

/*

  This file is included by passthrough_simulate_error.c.
  It will also be compiled into passthrough_simulate_error_ctrl

*/
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <utime.h>

#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#define CG_FUNC_GETATTR 0
#define CG_FUNC_OPEN 1
#define CG_FUNC_READ 2
#define CG_FUNC_READDIR 3
#define CG_FUNC_STATFS 4
#define CG_FUNC_NUM 5
#define BASEDIR "/mnt/tmpfs"
static char _cg_files_blocking[CG_FUNC_NUM][PATH_MAX]={0};
static char _cg_files_ioerrors[CG_FUNC_NUM][PATH_MAX]={0};
// #define CG_SIMULATION_MASK(m) (m<<CG_SIMULATE_SHIFT)

/*errno
 */

static bool mk_parent_dir(const char *path){
  char p[PATH_MAX];
  strcpy(p,path);
  const int n=strlen(p);
  for(int i=2; i<n;i++){
    if (p[i]=='/'){
      p[i]=0;
      const int res=mkdir(p,S_IRWXU);
      if (res && errno!=EEXIST){
        fprintf(stderr,"Error %d \n",errno);
        return false;
      }
      p[i]='/';
    }
  }
  return true;
}

static bool _cg_block_init(const char *basedir){
  fprintf(stderr,"GOING to cg_block_init('%s') ...\n",basedir);
  for(int i=CG_FUNC_NUM;--i>=0;){
    const char *fname=
      i==CG_FUNC_GETATTR?"getattr":
      i==CG_FUNC_OPEN?"open":
      i==CG_FUNC_READ?"read":
      i==CG_FUNC_READDIR?"readdir":
      i==CG_FUNC_STATFS?"statfs":
      "ERROR";
    for(int j=2;--j>=0;){
      char *s=j?_cg_files_blocking[i]:_cg_files_ioerrors[i];
      sprintf(s,j?"%s/passthrough_block/%s":"%s/passthrough_ioerrors/%s",basedir,fname);
      //fprintf(stderr," Going to mk_parent_dir(%s)\n",s);
      if (!mk_parent_dir(s)){
        fprintf(stderr,"Failed creating directory '%s'\n",s);
        return false;
      }
    }
    //    fprintf(stderr,"%d %s  %s\n",i,_cg_files_blocking[i],_cg_files_ioerrors[i]);
  }
  return true;
}
static void cg_block_init(){
  char basedir[PATH_MAX+1];
  getlogin_r(stpcpy(basedir,"/mnt/tmpfs/"),99);
  if (!_cg_block_init(basedir) && !_cg_block_init(getenv("HOME"))){
    fprintf(stderr,"Failed init\n");
    exit(1);
  }
}

static bool cg_block(int i){
  struct stat statbuf;
  static int count;
#if 0
  if (i==CG_FUNC_READ) {
    fprintf(stderr," cg_block %d/n",i);
    usleep(100*1000);
    static int count;
    fprintf(stderr,"read # %5d\n",count++);
  }
#endif
  //  if (!(count++&0xf)  && unlink(_cg_files_ioerrors[i])) return true;
  if (!unlink(_cg_files_ioerrors[i])){
    // fprintf(stderr," removed %s\n",_cg_files_ioerrors[i]);
    return true;
  }else{
    //fprintf(stderr," not removed %s\n",_cg_files_ioerrors[i]);
  }

#if 1
  while(!stat(_cg_files_blocking[i],&statbuf)) usleep(1000*1000);
#else
  if(!stat(_cg_files_blocking[i],&statbuf)){
    unlink(_cg_files_blocking[i]);
    usleep(1000*1000*1000);
  }
#endif
  return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if __INCLUDE_LEVEL__ == 0
#define ABBREV "ROADSroads"
static void go(bool *opts){
  bool is_release_all=opts['u'];
  for(int j=sizeof(ABBREV);--j>=0;) if (opts[ABBREV[j]]) is_release_all=false;
  const bool is_release=opts['u'];
  for(int j=sizeof(ABBREV);--j>=0;){
    const char c=ABBREV[j];
    if (!opts[c] && !is_release_all) continue;
    const int iFile=
      c=='r'||c=='R'?CG_FUNC_READ:
      c=='o'||c=='O'?CG_FUNC_OPEN:
      c=='a'||c=='A'?CG_FUNC_GETATTR:
      c=='d'||c=='D'?CG_FUNC_READDIR:
      c=='s'||c=='S'?CG_FUNC_STATFS:
      -1;
    const bool is_block=!(c&32);
    const char *path=iFile<0?NULL: (is_block?_cg_files_blocking:_cg_files_ioerrors)[iFile];
    if (path){
      if (is_release||is_release_all){
        unlink(path);
      }else{
        //printf("Going to write '%s' %d\n",path,iFile);
        int lastSlash=0;
        for(int i=0;path[i];i++) if (path[i]=='/') lastSlash=i;
        if (!lastSlash){ fprintf(stderr,"Error: lastSlash: 0 for path: %s\n",path); exit(1);}
        mk_parent_dir(path);
        FILE *f=fopen(path,"w");
        if (!f){
          perror(path);
        }else{
          fputc('X',f);
          fclose(f);
        }
      }
    }
  }
  struct stat statbuf;
  for(int block=2;--block>=0;){
    for(int i=0;i<CG_FUNC_NUM;i++){
      const char *p=(block?_cg_files_blocking:_cg_files_ioerrors)[i];
      fprintf(stderr,"%d\t%s\t%s\n",i,p,stat(p,&statbuf)?"No":"Yes");
    }
  }
}
int main(int argc, char *argv[]){
  cg_block_init();
  fprintf(stderr,"To create a block use options -R (read)  -O (open)  -A (getattr)  -D (readdir)\n");
  fprintf(stderr,"To create a single i/o error use above opts in lower case\n");
  fprintf(stderr,"Add -u to release a block\n");
  bool opts[128]={0}, is_sleep=false;
  srandom(time(NULL));
  for(int c;(c=getopt(argc,argv,ABBREV"ux"))!=-1;){
    if (c=='x') is_sleep=true;
    if (strchr(ABBREV"u",c)) opts[c]=true;
  }
  while(is_sleep){
    const char c="ROADu"[random()%strlen("ROADu")];
    fprintf(stderr,"Random option -%c\n",c);
    memset(opts,0,128);
    if (c!='u') opts[c]=true;
    go(opts);
    const int sec=(random()%10)+1;
    fprintf(stderr,"Going to pause for %d s\n",sec);
    usleep(sec<<20);
  }
  go(opts);

}

#endif
