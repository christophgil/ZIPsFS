//Last modified: 18/11/12 19:13:35(CET) by Fabian Holler
#ifndef _cpu_usage_pid
#define _cpu_usage_pid
#include <stdlib.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

struct pstat {
  long unsigned int utime_ticks;
  long int cutime_ticks;
  long unsigned int stime_ticks;
  long int cstime_ticks;
  long unsigned int vsize; // virtual memory size in bytes
  long unsigned int rss; //Resident  Set  Size in bytes
  long unsigned int cpu_total_time;
};
#ifndef STRINGIZE
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define FILE_COLON_LINE __FILE__":"STRINGIZE(__LINE__)
#endif
/* read /proc data into the passed struct pstat  returns 0 on success, -1 on error  */
static int read_proc_stat(struct pstat *r,const pid_t pid){
  static pid_t pid_error=0;
  if (pid_error==pid) return -1;
  char path[30];
  sprintf(path,"/proc/%d/stat",pid);
  FILE *f=fopen(path,"r");
  if (!f){perror(FILE_COLON_LINE" FOPEN ERROR ");return -1;}
  FILE *fstat=fopen("/proc/stat","r");
  if (!fstat){
    perror(FILE_COLON_LINE" FOPEN ERROR ");
    fclose(fstat);
    pid_error=pid;
    return -1;
  }
  memset(r,0,sizeof(struct pstat));
  long int rss;
  if (fscanf(f,"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %ld %ld %*d %*d %*d %*d %*u %lu %ld",&r->utime_ticks,&r->stime_ticks,&r->cutime_ticks,&r->cstime_ticks,&r->vsize,&rss)==EOF){
    fclose(f);
    perror(FILE_COLON_LINE" fscanf 1");
    return -1;
  }
  fclose(f);
  r->rss=rss * getpagesize();
  long unsigned int cpu_time[10];
  memset(cpu_time,0,sizeof(cpu_time));
  if (fscanf(fstat, "%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",&cpu_time[0],&cpu_time[1],&cpu_time[2],&cpu_time[3],&cpu_time[4],&cpu_time[5],&cpu_time[6],&cpu_time[7],&cpu_time[8],&cpu_time[9])==EOF){
    fclose(fstat);
    perror(FILE_COLON_LINE" fscanf 2");
    return -1;
  }
  fclose(fstat);
  for(int i=0;i<10;i++) r->cpu_total_time += cpu_time[i];
  return 0;
}
/* calculates the elapsed CPU usage between 2 measuring points. in percent */
#define U() *ucpu_usage=(((curr->utime_ticks+curr->cutime_ticks) - (last->utime_ticks+last->cutime_ticks)))
#define S() *scpu_usage=(((curr->stime_ticks+curr->cstime_ticks) - (last->stime_ticks+last->cstime_ticks)))
static void calc_cpu_usage_pct(const struct pstat* curr,const struct pstat* last,float* ucpu_usage, float* scpu_usage){
  const float total_time_diff=0.01*(curr->cpu_total_time-last->cpu_total_time);
  U()/ total_time_diff;
  S()/ total_time_diff;
}
/* calculates the elapsed CPU usage between 2 measuring points in ticks */
static void calc_cpu_usage(const struct pstat* curr,const struct pstat* last,long unsigned int* ucpu_usage,long unsigned int* scpu_usage){
  U();
  S();
}
#undef U
#undef S
#endif //_cpu_usage_pid
#if defined __INCLUDE_LEVEL__ && __INCLUDE_LEVEL__ == 0
int main(int argc,char* argv[]){
  const pid_t pid=atol(argv[1]);
  struct pstat pstat1={0}; read_proc_stat(&pstat1,pid);
  usleep(1000*1000*1);
  struct pstat pstat2={0}; read_proc_stat(&pstat2,pid);
  {
    float ucpu_usage,scpu_usage;
    calc_cpu_usage_pct(&pstat1,&pstat2,&ucpu_usage,&scpu_usage);
    printf("pid=%u ucpu_usage=%f scpu_usage=%f\n",pid,ucpu_usage,scpu_usage);
  }
  {
    long unsigned int ucpu_usage, scpu_usage;
    calc_cpu_usage(&pstat1,&pstat2,&ucpu_usage,&scpu_usage);
    printf("pid=%u ucpu_usage=%lu scpu_usage=%lu\n",pid,ucpu_usage,scpu_usage);
  }
}
#endif
