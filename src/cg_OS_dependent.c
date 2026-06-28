#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>



#define R(e)  { fprintf(stderr,__FILE_NAME__":%d ",__LINE__); perror(__func__);return e?e:-1;}




#if defined(HAS_PID2EXE_NETBSD) && HAS_PID2EXE_NETBSD
#include <sys/sysctl.h>
static int _pid2exe_os_dependent(pid_t pid, char buf[], const int buf_l){
  //fprintf(stderr,"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee \n");
  struct kinfo_proc2 kp;
  size_t len=sizeof(kp);
  int mib[]={CTL_KERN,KERN_PROC2,KERN_PROC_PID,pid,sizeof(kp),1};
  if (sysctl(mib,6,&kp,&len,NULL,0)==-1) R(errno);
  //fprintf(stderr,"kkkkkkkkkkkkkkkkkkkkkkkkkkkk  kp.p_comm=%s\n",kp.p_comm);
  strlcpy(buf,kp.p_comm,buf_l);
  return 0;
}
#elif defined(HAS_PID2EXE_FREEBSD) && HAS_PID2EXE_FREEBSD
#include <sys/sysctl.h>
#include <sys/user.h>
static int _pid2exe_os_dependent(pid_t pid, char buf[], const int buf_l){
  struct kinfo_proc kp;
  size_t len=sizeof(kp);
  int mib[]={CTL_KERN,KERN_PROC,KERN_PROC_PID,pid};
  if (sysctl(mib,4,&kp,&len,NULL,0)==-1) R(errno);
  strlcpy(buf,kp.ki_comm,buf_l);
  return 0;
}
#elif defined(HAS_PID2EXE_MACOSX) && HAS_PID2EXE_MACOSX
#include <sys/sysctl.h>
static int _pid2exe_os_dependent(pid_t pid,char buf[],const int buf_l){
  *buf=0;
  int mib[]={CTL_KERN,KERN_PROC,KERN_PROC_PID,pid};
  struct kinfo_proc kp;
  size_t len=sizeof(kp);
  //fprintf(stderr,"Going to call sysctl() pid:%d",pid);
  if (sysctl(mib,4,&kp,&len,NULL,0)==-1) R(ENOMEM);
  if (len==0){ R(ESRCH);}
  strncpy(buf,kp.kp_proc.p_comm,buf_l);
  //fprintf(stderr,"comm: %s",buf);
  buf[buf_l-1]=0;
  return 0;
}
#endif //HAS_PID2EXE_MACOSX
#undef R
