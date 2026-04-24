/********************************************************************************************************************/
/* This  configuration controls  ZIP-files that should not be shown as zip files or folders in directory listings.  */
/* Instead their Zip-entries should be shown.                                                                       */
/* This is needed to stire the Sciex mass-spectrometry files belonging to  one measurement as a zip file.           */
/********************************************************************************************************************/



#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#endif


/***************************************************************************************************************/
/* PARAMETER                                                                                                   */
/* path: Usually the virtual path of the file.                                                                 */
/* For the cache (WITH_ZIPFLATCACHE) it is also called with strings containing only the extension like ".wiff" */
/* In the later case, false positives are ok.                                                                  */
/* rule: The caller iterates from 0 to ... until this funciont returns -1.                                     */
/* RETURN:                                                                                                     */
/* Length of base name to which the suffix needs to be appended to obtain the zipfilename                      */
/* suffix_or_null the address where to store the suffix for forming  the zipfilename                        */
/* The caller can form the zipfile name by taking the first len chars of path and append suffix                */
/***************************************************************************************************************/
static int config_containing_zipfile_of_virtual_file(const int rule,const char *path,const int path_l,char *suffix_or_null[]){
#define S(x)   (sizeof(#x)-1) /* Length of extension */
#define M(dot,x) ((d=S(x)),(path_l>=S(x) && e[-S(x)]==dot  && !strcmp(e-S(x),#x)))  /* Test whether path matches extension x */
#define X(x) if (suffix_or_null)*suffix_or_null=x;return path_l-d   /* Write string to the address given in parameter suffix_or_null  and return length of zipfile name - len(suffix_or_null)  */
  const char *e=path+path_l;
  int d=0;
  if (suffix_or_null) *suffix_or_null=NULL;
  if (M('.',.wiff) || M('.',.wiff2) || M('.',.wiff.scan)){
    //fprintf(stderr,"xxxxxxxx %s path_l=%d rule=%d len=%d\n",path, path_l,rule,path_l-d);
    switch(rule){
    case 0: X(".wiff.Zip");
    case 1: X(".rawIdx.Zip");
    case 2: X(".wiff2.Zip");
    }
  }else if (M('.',.rawIdx)){
    switch(rule){
    case 0: X(".rawIdx.Zip");
    }
  }else if (M('.',.raw)){
    switch(rule){
    case 0: X(".raw.Zip");
    case 1: X(".wiff2.Zip");
    case 2: X(".rawIdx.Zip");
    }
  }else if (M('.',.SSMetaData) || M('.',.timeseries.data) || M('_',_report.txt)){
    switch(rule){
    case 0: X(".wiff.Zip");
    case 1: X(".wiff2.Zip");
    }
  }
  return -1; /* Not found or no further possible suffices */
#undef X
#undef M
#undef S
}

#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__==0
int main(int argc, char *argv[]){
  char *ff[]={
    "aaaaaaaaaaa.wiff",
    "aaaaaaaaaaa.wiff.scan",
    "aaaaaaaaaaa.wiff2",
    "aaaaaaaaaaa.rawIdx",
    "aaaaaaaaaaa.raw",
    ".wiff",
    ".wiff.scan",
    ".wiff2",
    ".rawIdx",
    ".raw",

    NULL};
  char str[99];
  for(char **f=ff;*f;f++){
    const int f_l=strlen(*f);
    fprintf(stderr,"\n\x1B[7m%s\x1B[0m\n",*f);
    char *suffix;
    for(int len,i=0;(len=config_containing_zipfile_of_virtual_file(i,*f,f_l, &suffix))>=0;i++){
      strcpy(str,*f);
      strcpy(str+len,suffix);
      fprintf(stderr,"rule:%d  len=%d %s\n",i,len,str);
    }
  }
  fprintf(stderr,"Going to EXIT from "__FILE_NAME__":%d\n",__LINE__);
  exit(0);
}
#endif
