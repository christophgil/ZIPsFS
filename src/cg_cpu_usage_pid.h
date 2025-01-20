#ifndef _cg_cpuusage_pid_h
#define _cg_cpuusage_pid_h

#ifndef CG_PERROR
#define CG_PERROR(msg) fprintf(stderr,"%s:%d ",__FILE_NAME__,__LINE__),perror(msg);
#endif

struct pstat {
  long unsigned int utime_ticks;
  long int cutime_ticks;
  long unsigned int stime_ticks;
  long int cstime_ticks;
  long unsigned int vsize; // virtual memory size in bytes
  long unsigned int rss; //Resident  Set  Size in bytes
  long unsigned int cpu_total_time;
  int ncpu;
};

#endif
