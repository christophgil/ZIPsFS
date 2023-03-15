/*
  Copyright (C) 2023
  christoph Gille
  This program can be distributed under the terms of the GNU GPLv3.
*/
#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B""[42m"
#define ANSI_BLUE "\x1B""[44m"
#define ANSI_YELLOW "\x1B""[43m"
#define ANSI_CYAN "\x1B""[46m"
#define ANSI_WHITE "\x1B""[47m"
#define ANSI_BLACK "\x1B""[40m"
#define ANSI_FG_GREEN "\x1B""[32m"
#define ANSI_FG_RED "\x1B""[31m"
#define ANSI_FG_MAGENTA "\x1B""[35m"
#define ANSI_FG_GRAY "\x1B""[30;1m"
#define ANSI_FG_BLUE "\x1B""[34;1m"
#define ANSI_FG_BLACK "\x1B""[100;1m"
#define ANSI_FG_YELLOW "\x1B""[33m"
#define ANSI_FG_WHITE "\x1B""[37m"
#define ANSI_INVERSE "\x1B""[7m"
#define ANSI_BOLD "\x1B""[1m"
#define ANSI_UNDERLINE "\x1B""[4m"
#define ANSI_RESET "\x1B""[0m"
#define TERMINAL_CLR_LINE "\r\x1B""[K"

#define log_struct(st,field,format)   printf("    " #field "=" #format "\n",st->field)
void prints(char *s){ if (s) fputs(s,stdout);}
void log_reset(){ prints(ANSI_RESET);}
#define log_msg(...) printf(__VA_ARGS__)
#define log_already_exists(...) (prints(" Already exists "ANSI_RESET),printf(__VA_ARGS__)
#define log_failed(...)  prints(ANSI_FG_RED" Failed "ANSI_RESET),printf(__VA_ARGS__)
#define log_warn(...)  prints(ANSI_FG_RED" Warn "ANSI_RESET),printf(__VA_ARGS__)
#define log_error(...)  prints(ANSI_FG_RED" Error "ANSI_RESET),printf(__VA_ARGS__)
#define log_succes(...)  prints(ANSI_FG_GREEN" Success "ANSI_RESET),printf(__VA_ARGS__)
#define log_debug_now(...)   prints(ANSI_FG_MAGENTA" Debug "ANSI_RESET),printf(__VA_ARGS__)
#define log_entered_function(...)   prints(ANSI_INVERSE"> > > >"ANSI_RESET),printf(__VA_ARGS__)
#define log_exited_function(...)   prints(ANSI_INVERSE"< < < <"ANSI_RESET),printf(__VA_ARGS__)
#define log_seek_ZIP(delta,...)   printf(ANSI_FG_RED""ANSI_YELLOW"SEEK ZIP FILE:"ANSI_RESET" %'16ld ",delta),printf(__VA_ARGS__)
#define log_seek(delta,...)  printf(ANSI_FG_RED""ANSI_YELLOW"SEEK REG FILE:"ANSI_RESET" %'16ld ",delta),printf(__VA_ARGS__)
#define log_cache(...)  prints(ANSI_RED"CACHE"ANSI_RESET" "),printf(__VA_ARGS__)

void log_path(const char *f,const char *path){
  printf("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,my_strlen(path));
}

#define log_abort(...)  { log_error(__VA_ARGS__),puts("Going to exit ..."),exit(-1); }
char *yes_no(int i){ return i?"Yes":"No";}
int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s: %s\n",func, strerror(errno));
  return ret;
}


void log_strings(const char *pfx, char *ss[],int n,char *ss2[]){
  printf(ANSI_UNDERLINE"%s"ANSI_RESET" %p\n",pfx,ss);
  if (ss){
    for(int i=0;i<n;i++){
      printf("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
      if (ss2) prints(ss2[i]);
      puts(ANSI_RESET);
    }
  }
}
void log_file_stat(const char * name,const struct stat *s){
  char *color= (s->st_ino>(1L<<SHIFT_INODE_ROOT)) ? ANSI_FG_MAGENTA:ANSI_FG_BLUE;
  printf("%s  st_size=%lu st_blksize=%lu st_blocks=%lu links=%lu inode=%s%lu "ANSI_RESET,name,s->st_size,s->st_blksize,s->st_blocks,   s->st_nlink,color,s->st_ino);
  //st_blksize st_blocks f_bsize
  putchar(  S_ISDIR(s->st_mode)?'d':'-');
  putchar( (s->st_mode&S_IRUSR)?'r':'-');
  putchar( (s->st_mode&S_IWUSR)?'w':'-');
  putchar( (s->st_mode&S_IXUSR)?'x':'-');
  putchar( (s->st_mode&S_IRGRP)?'r':'-');
  putchar( (s->st_mode&S_IWGRP)?'w':'-');
  putchar( (s->st_mode&S_IXGRP)?'x':'-');
  putchar( (s->st_mode&S_IROTH)?'r':'-');
  putchar( (s->st_mode&S_IWOTH)?'w':'-');
  putchar( (s->st_mode&S_IXOTH)?'x':'-');
  putchar('\n');
}

void print_pointers(){
  for(int i=0;i<_fhdata_n;i++){
    log_msg(ANSI_RESET"%d zarchive=%p zip_file=%p\n",i,_fhdata[i].zpath.zarchive,_fhdata[i].zip_file);
  }
}

#define MAX_INFO 33333
static char _info[MAX_INFO];
#define PRINTINFO(...)  n+=snprintf(_info+n,MAX_INFO-n,__VA_ARGS__)
int print_open_files(int n, int *fd_count){
  char proc_path[64], path[256];
  struct dirent *dp;
  snprintf(proc_path,64,"/proc/%i/fd/",getpid());
  DIR *dir=opendir(proc_path);
  const int len_proc_path=strlen(proc_path);
  if (n>=0) PRINTINFO("<OL>\n");
  for(int i=0;dp=readdir(dir);i++){
    *fd_count++;
    if (n<0 || atoi(dp->d_name)<4) continue;
    proc_path[len_proc_path]=0;
    strcat(proc_path+len_proc_path,dp->d_name);
    readlink(proc_path,path,255);
    PRINTINFO("<LI>%s -- %s</LI>\n",proc_path,path);
  }
  closedir(dir);
  if (n>=0) PRINTINFO("</OL>\n");
  return n;
}

void log_mem(){
  int fd_count;
  print_open_files(-1,&fd_count);
  printf(ANSI_MAGENTA"Resources pid=%d"ANSI_RESET" _fhdata_n=%d  uordblks=%'d get_num_fds=%d mmap/munmap=%'d/%'d\n",getpid(),_fhdata_n,mallinfo().uordblks,fd_count,_mmap_n,_munmap_n);
}

int print_maps(int n){
  char fname[99];
  unsigned long writable=0, total=0, shared=0;
  snprintf(fname,sizeof(fname),"/proc/%ld/maps", (long)getpid());
  FILE *f=fopen(fname,"r");
  if(!f) {
    PRINTINFO("%s: %s\n",fname, strerror(errno));
    return n;
  }
  PRINTINFO("<TABLE>\n");
  while(!feof(f)) {
    char buf[PATH_MAX+100], perm[5], dev[6], mapname[PATH_MAX];
    unsigned long begin,end, size,inode, foo;
    if(fgets(buf, sizeof(buf),f)==0) break;
    mapname[0]='\0';
    sscanf(buf, "%lx-%lx %4s %lx %5s %lu %100s", &begin, &end,perm,&foo,dev, &inode,mapname);
    if (!strchr(mapname,'/')){
    PRINTINFO("<TR><TD>%08lx</TD><TD>%'20ld KB</TD><TD>%s</TD></TR>\n",begin,(end-begin)/1024,mapname);
    }
  }
  printf("mapped:   %'lu KB writable/private: %'lu KB shared: %'lu KB\n",total/1024, writable/1024, shared/1024);
  fclose(f);
  PRINTINFO("</TABLE>\n total=%'lu<BR>\n",total);
  return n;
}

char*  get_info(){
  int n=0;
  PRINTINFO("<HTML>\n<BODY style=\"font-family:'Lucida Console',monospace\">\n<H1>Roots</H1>\n<OL>\n");
  for(int i=0;i<_root_n;i++){
    char *d=_root_descript[i];
    PRINTINFO("<LI>%s   %s</LI>\n",_root[i],d?d:"");
  }
  PRINTINFO("</OL>\n<H1>Misc</H1>\n");
  PRINTINFO("\npid=%d\tfhdata=%d\tuordblks=%'d<BR>\n",getpid(),_fhdata_n,mallinfo().uordblks);
  int fd_count=0;
  PRINTINFO("</OL>\n<H1>File handles</H1>\n");
  n=print_open_files(n,&fd_count);
  PRINTINFO("<H1>Cache</H1>\nWhen cache Zip entry: %s\n",WHEN_CACHE_S[_when_cache]);
  n=print_maps(n);
  PRINTINFO("</BODY>\n</HTML>\n");
  return _info;
}




void log_statvfs(char *path){
  struct statvfs info;
  if (-1==statvfs(path, &info)){
    perror("statvfs() error");
  }else{
    printf("statvfs() ");
    printf("  f_bsize    : %lu\n",info.f_bsize);
    printf("  f_blocks   : %lu\n",info.f_blocks),
      printf("  f_bfree    : %lu\n",info.f_bfree),
      printf("  f_files    : %lu\n",info.f_files);
    printf("  f_ffree    : %lu\n",info.f_ffree);
    printf("  f_fsid     : %lu\n",info.f_fsid);
    printf("  f_flag     : %lX\n",info.f_flag);
    printf("  f_namemax  : %lu\n",info.f_namemax);
    //    printf("  f_pathmax  : %u\n",info.f_pathmax);
    //    printf("  f_basetype : %s\n",info.f_basetype);
  }
}




void my_backtrace(){ /*  compile with options -g -rdynamic */
  void *array[10];
  size_t size=backtrace(array,10);
  backtrace_symbols_fd(array,size,STDOUT_FILENO);
}
void handler(int sig) {
  printf( "ZIPsFS Error: signal %d:\n",sig);
  my_backtrace();
  abort();
}
