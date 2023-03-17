#include <sys/mount.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
      #include <libgen.h>

/* For security the path must contain any of the patterns. Please customize to your needs */
char *_path_must_contain[]={"ZIPsFS","tmp/fuse",NULL};


void usage(char *prg){
  char *slash=rindex(prg,'/');
  printf("slash=%s \n",slash);
  printf("\
\x1B[7mUsage:\x1B[0m\n\n\
    %s  mount-point1 [mount-point2 ...]\n\n\
\x1B[7mOptions:\x1B[0m\n\n\
 - f    Force\n\
  -l     Lazy\n\n\
\x1B[7mInstallation:\x1B[0m\n\n\
Customize the variable _path_must_contain. Then compile.\n\n\
_inst(){\n\
    d=/usr/local/bin/%s\n\
    sudo cp %s $d\n\
    sudo chown root $d\n\
    sudo chmod u+sx $d\n\
    ls -l $d\n\
}\n\n\
_inst\n\n\
",prg,slash?slash+1:prg,prg);
}
void is_mountpoint(char *path){
  struct stat mountpoint, parent;
  if (stat(path, &mountpoint)==-1) {
    perror("failed to stat mountpoint");
    exit(EXIT_FAILURE);
  }
  char parent_path[99];
  if (strlen(path)>94) return;
  if (stat(strcat(strcpy(parent_path,path),"/.."), &parent)==-1) {
    perror("failed to stat parent");
    exit(EXIT_FAILURE);
  }
  /* Compare the st_dev fields in the results: if they are  equal,
     then both the directory and its parent belong  to the same filesystem,
     and so the directory is not  currently a mount point.
  */
  printf(mountpoint.st_dev==parent.st_dev?"Not":"Is");
  printf(" a mount-point: %s\n\n",path);

}
int main(int argc, char *argv[]){
  int flags=UMOUNT_NOFOLLOW,c;
  printf("The REAL UID=%d\n", getuid());
  printf("The EFFECTIVE UID=%d\n", geteuid());
  while((c=getopt(argc,argv,"fl"))!=-1){
    switch(c){
    case 'f': flags|=MNT_FORCE; break;
    case 'l': flags|=MNT_DETACH; break;
    default: usage(argv[0]);exit(1);break;
    }
  }
  if (optind>=argc){
    usage(argv[0]);
    exit(1);
  }
  int res=0;
  for(int i=optind;i<argc;i++){
    char *m=argv[optind],ok=0;
    for(char **s=_path_must_contain;*s;s++){
      if (strstr(m,*s)){
        //printf("%s matches %s\n",m,*s);
        ok=1;
      }
    }
    if(!ok){
      fprintf(stderr,"Refused because the path does not contain any pattern in the variable _path_must_contain: %s\n",m);
      continue;
    }
    is_mountpoint(m);
    fprintf(stderr,"Going to umount %s ...\n",m);
    const int r=umount2(m,flags);
    if (r){
      perror(" Error umount");
    }else{
      res=r;
      fprintf(stderr,"    Success \n\n");
    }
  }
  return res;
}
