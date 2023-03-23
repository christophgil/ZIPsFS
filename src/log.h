/*
  Copyright (C) 2023
  christoph Gille
  This program can be distributed under the terms of the GNU GPLv3.
*/
#define ANSI_RED "\x1B[41m"
#define ANSI_MAGENTA "\x1B[45m"
#define ANSI_GREEN "\x1B[42m"
#define ANSI_BLUE "\x1B[44m"
#define ANSI_YELLOW "\x1B[43m"
#define ANSI_CYAN "\x1B[46m"
#define ANSI_WHITE "\x1B[47m"
#define ANSI_BLACK "\x1B[40m"
#define ANSI_FG_GREEN "\x1B[32m"
#define ANSI_FG_RED "\x1B[31m"
#define ANSI_FG_MAGENTA "\x1B[35m"
#define ANSI_FG_GRAY "\x1B[30;1m"
#define ANSI_FG_BLUE "\x1B[34;1m"
#define ANSI_FG_BLACK "\x1B[100;1m"
#define ANSI_FG_YELLOW "\x1B[33m"
#define ANSI_FG_WHITE "\x1B[37m"
#define ANSI_INVERSE "\x1B[7m"
#define ANSI_BOLD "\x1B[1m"
#define ANSI_UNDERLINE "\x1B[4m"
#define ANSI_RESET "\x1B[0m"
#define TERMINAL_CLR_LINE "\r\x1B[K"

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
void print_backtrace(){
  int j, nptrs;
  void *buffer[BT_BUF_SIZE];
  char **strings;
  nptrs = backtrace(buffer, BT_BUF_SIZE);
  printf("backtrace() returned %d addresses\n", nptrs);
  /* The call backtrace_symbols_fd(buffer, nptrs, STDOUT_FILENO) would produce similar output to the following: */
  strings = backtrace_symbols(buffer, nptrs);
  if (strings == NULL) {
    perror("backtrace_symbols");
    exit(EXIT_FAILURE);
  }
  for (j = 0; j < nptrs; j++) printf("%s\n", strings[j]);
  free(strings);
}

#define log_abort(...)  { log_error(__VA_ARGS__),puts("Going to exit ..."),exit(-1); }
char *yes_no(int i){ return i?"Yes":"No";}
int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s:",func);
  perror("");
  return ret;
}


void log_strings(const char *pfx, char *ss[],int n){
  printf(ANSI_UNDERLINE"%s"ANSI_RESET" %p\n",pfx,ss);
  if (ss){
    for(int i=0;i<n;i++){
      printf("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
      puts(ANSI_RESET);
    }
  }
}
void log_file_stat(const char * name,const struct stat *s){
  char *color= (s->st_ino>(1L<<SHIFT_INODE_ROOT)) ? ANSI_FG_MAGENTA:ANSI_FG_BLUE;
  printf("%s  st_size=%lu st_blksize=%lu st_blocks=%lu links=%lu inode=%s%lu"ANSI_RESET" dir=%s ",name,s->st_size,s->st_blksize,s->st_blocks,   s->st_nlink,color,s->st_ino,  yes_no(S_ISDIR(s->st_mode)));
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
    log_msg(ANSI_RESET"%d zarchive=%p zip_file=%p\n",i,_fhdata[i].zpath.zarchive,(void*)_fhdata[i].zip_file);
  }
}

#define MAX_INFO 33333
static char _info[MAX_INFO+1];
#define PRINTINFO(...)  n+=snprintf(_info+n,MAX_INFO-n,__VA_ARGS__)
#define PROC_PATH_MAX 256
int print_open_files(int n, int *fd_count){
  static char proc_path[PROC_PATH_MAX], path[256];
  struct dirent *dp;
  const int len_proc_path=snprintf(proc_path,64,"/proc/%i/fd/",getpid());
  DIR *dir=opendir(proc_path);
  if (n>=0) PRINTINFO("<OL>\n");
  for(int i=0;dp=readdir(dir);i++){
    if (fd_count) *(fd_count++);
    if (n<0 || atoi(dp->d_name)<4) continue;
    my_strcpy(proc_path+len_proc_path,dp->d_name,PROC_PATH_MAX-len_proc_path);
    const int l=readlink(proc_path,path,255);path[l]=0;
    PRINTINFO("<LI>%s -- %s</LI>\n",proc_path,path);
  }
  closedir(dir);
  if (n>=0) PRINTINFO("</OL>\n");
  return n;
}

void log_mem(){
  printf(ANSI_MAGENTA"Resources pid=%d"ANSI_RESET" _fhdata_n=%d  uordblks=%'d  mmap/munmap=%'d/%'d\n",getpid(),_fhdata_n,mallinfo().uordblks,_mmap_n,_munmap_n);
}

int print_maps(int n){
  char fname[99];
  unsigned long total=0;
  snprintf(fname,sizeof(fname),"/proc/%ld/maps", (long)getpid());
  FILE *f=fopen(fname,"r");
  if(!f) {
    PRINTINFO("%s: %s\n",fname, strerror(errno));
    return n;
  }
  PRINTINFO("<TABLE>\n<THEAD><TR><TH>Addr&gt;&gt;12</TH><TH>KByte</TH><TH>Name</TH></TR></THEAD>\n");
  while(!feof(f)) {
    char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX]={0};
    unsigned long begin,end,inode,foo;
    if(fgets(buf,sizeof(buf),f)==0) break;
    //mapname[0]=0;
    sscanf(buf, "%lx-%lx %4s %lx %5s %lu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
    total+=(end-begin);
    if (!strchr(mapname,'/')){
      PRINTINFO("<TR><TD>%08lx</TD><TD align=\"right\">%'ld</TD><TD>%s</TD></TR>\n",begin>>12,(end-begin)>>10,mapname);
    }
  }
  fclose(f);
  PRINTINFO("<TFOOT><TR><TD></TD><TD>%'lu</TD><TD></TD></TR></TFOOT>\n</TABLE>\n",total>>10);
  return n;
}

static int log_cached(int n,char *title){
  if (n<0) log_cache("log_cached %s\n",title);
  else PRINTINFO("<TABLE>\n<THEAD><TR><TH></TH><TH>Path</TH><TH>Access</TH><TH>Addr&gt;&gt;12</TH><TH>KByte</TH><TH>Millisec</TH></TR></THEAD>\n");
  char stime[99];
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (n<0) printf("\t%4d\t%s\t%lx\t%p\n",i, d->path, d->path_hash,d->cache);
    else{
      struct tm tm = *localtime(&d->access);
      sprintf(stime,"%d-%02d-%02d_%02d:%02d:%02d\n",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
      PRINTINFO("<TR><TD>%4d</TD><TD>%s</TD><TD>%s</TD><TD>%lx</TD><TD align=\"right\">%'zu</TD><TD align=\"right\">%'d</TD></TR>\n",i,d->path,stime,((long)d->cache)>>12,d->cache_len>>10,d->cache_read_sec);




    }
  }
  if (n>=0) PRINTINFO("</TABLE>");
  return n;
}
int get_info(){
  int n=0;
  PRINTINFO("<HTML>\n<HEAD>\n<STYLE>\n\
H1{color:blue;}\n\
TABLE{borborder-color:black;border-style:groove;}\n\
table,th,td{border:1pxsolidblack;border-collapse:collapse;}\n\
TBODY>TR:nth-child(even){background-color:#FFFFaa;}\n\
TBODY>TR:nth-child(odd){background-color:#CCccFF;}\n\
THEAD{background-color:black;color:white;}\n\
TD{padding:5px;}\n\
</STYLE>\n</HEAD>\n<BODY>\n\
<H1>Roots</H1>\n\
<TABLE><THEAD><TR><TH>Path</TH><TH>Writable</TH><TH>Remote</TH></TR></THEAD>\n");
  for(int i=0;i<_root_n;i++){
    int f=_root[i].features;
    PRINTINFO("<TR><TD>%s</TD><TD>%s</TD><TD>%s</TD></TR>\n",_root[i].path,yes_no(f&ROOT_WRITABLE),yes_no(f&ROOT_REMOTE));
  }
  PRINTINFO("</TABLE>\n<H1>Heap</H1>\nuordblks=%'d<BR>\n",mallinfo().uordblks);
  PRINTINFO("<H1>File handles</H1>\n");
  n=print_open_files(n,NULL);

  PRINTINFO("<H1>Read bytes statistics</H1>\n<TABLE><THEAD><TR><TH>Event</TH><TH>Count</TH></TR></THEAD>\n");
#define TABLEROW(a,skip) PRINTINFO("<TR><TD><B>%s</B></TD><TD align=\"right\">%'d</TD></TR>\n",#a+skip,a)
  TABLEROW(_count_read_zip_cached,7);
  TABLEROW(_count_read_zip_regular,7);
  TABLEROW(_count_read_zip_seekable,7);
  TABLEROW(_count_read_zip_no_seek,7);
  TABLEROW(_count_read_zip_seek_fwd,7);
  TABLEROW(_count_read_zip_seek_bwd,7);
  TABLEROW(_read_max_size,7);
  PRINTINFO("</TABLE>");

#undef TABLEROW
  PRINTINFO("<H1>Cache</H1>\nWhen cache Zip entry: %s\n sqlite-db=%s\n",WHEN_CACHE_S[_when_cache],_sqlitefile);
  n=log_cached(n,"");
  PRINTINFO("<H1>/proc/%ld/maps</H1>\n", (long)getpid());
  n=print_maps(n);
  struct rlimit rl={0};
  getrlimit(RLIMIT_AS,&rl);
  PRINTINFO("Number of calls mmap:%d munmap:%d<BR>\n", _mmap_n,_munmap_n);
  PRINTINFO("Rlim soft=%'zu MB   hard=%'zu MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);

  PRINTINFO("</BODY>\n</HTML>\n");
  return n;
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
