// ############################################################
// ### Reimplementation of some non-portable GNU functions  ###
// ############################################################
#ifndef _cg_gnu_dot_c
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <grp.h>
#include <unistd.h>
#include <limits.h>

/*
  static int strcasecmpxxxxx(const char *s1, const char *s2) {
  const unsigned char *us1=(const unsigned char *)s1, *us2=(const unsigned char *)s2;
  while (tolower(*us1)==tolower(*us2++))
  if (*us1++=='\0') return 0;
  return (tolower(*us1)-tolower(*--us2));
  }
*/

static int cg_group_member(gid_t gid) {
#ifdef __APPLE__
  int groups[NGROUPS_MAX];
#else
  gid_t groups[NGROUPS_MAX];
#endif
  int ngroups=NGROUPS_MAX;
  if (getgrouplist(getlogin(),-1,groups,&ngroups)==-1) fprintf(stderr,"Warning: Groups array is too small.n");
  for(int i=0;i<ngroups;i++) if (gid==groups[i]) return 1;
  return 0;
}
char *cg_strcasestr(const char *haystack, const char *needle){
  if (haystack && needle && *needle){
    const int nl=strlen(needle), imax=strlen(haystack)-nl;
    const unsigned char n32=needle[0]|32;
    for(int i=0;i<=imax;i++){
      if ((haystack[i]|32)==n32 && !strncasecmp(haystack+i,needle,nl)) return (char*)haystack+i;
    }
  }
  return NULL;
}
static char *cg_strchrnul(const char *s, const int c){
  if (s) while(*s && (*s!=c)) s++;
  return (char *)s;
}
static void *cg_memmem(const void *haystack, const size_t hlen, const void *needle, const size_t nlen) {
  const char *h=haystack, *n=needle;
  if (!nlen) return (char*)haystack;
  if (hlen<nlen) return NULL;
  const size_t imax=hlen-nlen;
  const char c=*n;
  for(int i=0;i<=imax;i++){
    if (h[i]==c && !memcmp(h+i,n,nlen)) return (void*)(h+i);
  }
  return NULL;
}
#endif
// 1111111111111111111111111111111111111111111111111111111111111
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__==0

int main(int argc, char *argv[]){

  if (1){
    printf("strcasestr %s  %s   ---> %s \n",argv[1],argv[2], cg_strcasestr(argv[1],argv[2]));
    return 0;
  }

  if (0){
    printf("strcasecmp %d \n",strcasecmp(argv[1],argv[2]));
    return 0;
  }
  {
    int g=atoi(argv[1]);

    printf("group_member %d  ... \n",g);
    puts(cg_group_member(g)?"Yes":"No");
    return 0;
  }
  if (0)  {
    char *res=(char*)cg_memmem(argv[1],atoi(argv[2]),argv[3],atoi(argv[4]));
    printf("cg_memmem: %ld",!res?-1: res-argv[1]);
    return 0;
  }
}


#endif // main
