#ifndef _cg_cpuusage_pid_c
#define _cg_cpuusage_pid_c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
/* https://github.com/RohitPanda/CPU_utilization/blob/master/main.c by Fabian Holler */


#include "cg_cpu_usage_pid.h"

static int cpuusage_read_proc(struct pstat* r,const pid_t pid){
  if (!has_proc_fs()) return 0;
  memset(r,0,sizeof(struct pstat));
  static int pid_error=-1;
  if (pid==pid_error) return -1;
  {
     char p[30];
    sprintf(p,"/proc/%d/stat",pid);
    FILE *fpstat=fopen(p,"r");
    if (fpstat==NULL){
      //  CG_PERROR("FOPEN ERROR ");
      pid_error=pid;
      return -1;
    }
    long int rss;
    if (fscanf(fpstat,"%*d %*s %*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu %ld %ld %*d %*d %*d %*d %*u %lu %ld",
               &r->utime_ticks, &r->stime_ticks,&r->cutime_ticks, &r->cstime_ticks, &r->vsize,&rss)==EOF){
      fclose(fpstat);
      pid_error=pid;
      return -1;
    }
    fclose(fpstat);
    r->rss=rss*getpagesize();
  }
  long unsigned int cpu_time[10];
  memset(cpu_time,0,sizeof(cpu_time));
  FILE *fstat=fopen("/proc/stat", "r");
  if (fstat==NULL){CG_PERROR("FOPEN ERROR ");pid_error=pid;return -1;}
  if (fscanf(fstat,"%*s %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",&cpu_time[0],&cpu_time[1],&cpu_time[2],&cpu_time[3],&cpu_time[4],&cpu_time[5],&cpu_time[6],&cpu_time[7],&cpu_time[8],&cpu_time[9])==EOF){
    fclose(fstat);
    pid_error=pid;
    return -1;
  }
  static int ncpu=0;
  if (!ncpu){
    char *line=NULL;
    size_t len=0;
    for(ssize_t nread;(nread=getline(&line,&len,fstat))!=EOF;){
      if (*line=='c'&&line[1]=='p'&&line[2]=='u') ncpu=atoi(line+3)+1;
    }
    free_untracked(line);
  }
  r->ncpu=!ncpu?1:ncpu;
  fclose(fstat);
  for(int i=10;--i>=0;) r->cpu_total_time+=cpu_time[i];
  return 0;
}
/* calculates the elapsed CPU usage between 2 measuring points. in percent */
#define U() *ucpu_usage=((cur_usage->utime_ticks+cur_usage->cutime_ticks) - (last_usage->utime_ticks+last_usage->cutime_ticks))
#define S() *scpu_usage=((cur_usage->stime_ticks+cur_usage->cstime_ticks) - (last_usage->stime_ticks+last_usage->cstime_ticks))
static void cpuusage_calc_pct(const struct pstat* cur_usage,const struct pstat* last_usage,float* ucpu_usage, float* scpu_usage){
  if (has_proc_fs()){
  const float total_time_diff=(cur_usage->cpu_total_time-last_usage->cpu_total_time)/(100.0*cur_usage->ncpu);
  U()/total_time_diff;
  S()/total_time_diff;
  }
}
/* calculates the elapsed CPU usage between 2 measuring points in ticks */
static void cpuusage_calc(const struct pstat* cur_usage,const struct pstat* last_usage,long unsigned int* ucpu_usage,long unsigned int* scpu_usage){
  U();
  S();
}
#undef U
#undef S
#endif //_cg_cpuusage_pid
#if defined(__INCLUDE_LEVEL__) && __INCLUDE_LEVEL__ == 0
int main(int argc,char *argv[]){
  struct pstat first, second;
  float ucpu_usage, scpu_usage;
  pid_t pid=atoi(argv[1]);
 struct pstat pstat1,pstat2;
  while(1){
    if (0){
      if (cpuusage_read_proc(&first,pid)==-1) break;
      usleep(1000*500);
      if (cpuusage_read_proc(&second,pid)==-1) break;
      cpuusage_calc_pct(&second, &first, &ucpu_usage, &scpu_usage);

    }else{
      if (cpuusage_read_proc(&second,pid)==-1) break;
      cpuusage_calc_pct(&second,&first,&ucpu_usage,&scpu_usage);
            first=second;
      usleep(1000*500);
    }
    printf("%f  %f\n", ucpu_usage,scpu_usage);
  }
  return 0;
}
#endif //_cg_cpuusage_pid
