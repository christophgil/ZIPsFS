#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#endif

static int config_containing_zipfile_of_virtual_file(const int approach,const char *path,const int path_l,char *suffix[]){
#define A(x,c) ((d=sizeof(#x)-1),(e[-d]==c && !strcmp(e-d,#x)))
#define C(x) A(x,'.')
#define S(x) if (suffix)*suffix=x;return path_l-d
  if (path_l>20){
    const char *e=path+path_l;
    int d=0;
    if (suffix) *suffix=NULL;
    if (C(.wiff) || C(.wiff2) || C(.wiff.scan)){
      switch(approach){
      case 0: S(".wiff.Zip");
      case 1: S(".rawIdx.Zip");
      case 2: S(".wiff2.Zip");
      }
    }else if (C(.rawIdx)){
      switch(approach){
      case 0: S(".rawIdx.Zip");
      }
    }else if (C(.raw)){
      switch(approach){
      case 0: S(".raw.Zip");
      case 1: S(".wiff2.Zip");
      case 2: S(".rawIdx.Zip");
      }
    }else if (C(.SSMetaData) || C(.timeseries.data) || A(_report.txt,'_')){
      switch(approach){
      case 0: S(".wiff.Zip");
      case 1: S(".wiff2.Zip");
      }
    }
  }
  return 0;
#undef S
#undef C
#undef A
}

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
int main(int argc, char *argv[]){
  char *ff[]={
    "20230310_aaaaaaaaaaa.wiff",
    "20230310_aaaaaaaaaaa.wiff.scan",
    "20230310_aaaaaaaaaaa.wiff2",
    "20230310_aaaaaaaaaaa.rawIdx",
    "20230310_aaaaaaaaaaa.raw",
    NULL};
  char str[99];
  for(char **f=ff;*f;f++){
    const int f_l=strlen(*f);
    fprintf(stderr,"\n\x1B[7m%s\x1B[0m\n",*f);
    char *suffix;
    for(int len,i=0;(len=config_containing_zipfile_of_virtual_file(i,*f,f_l, &suffix))!=0;i++){
      strcpy(str,*f);
      strcpy(str+len,suffix);
      fprintf(stderr,"%d  approach=%d %s\n",i,len,str);
    }
  }
  fprintf(stderr,"Going to EXIT from "__FILE_NAME__":%d\n",__LINE__);
  exit(0);
}
#endif
