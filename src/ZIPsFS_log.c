//   (global-set-key (kbd "<f4>") '(lambda() (interactive) (switch-to-buffer "log.c")))
#define TDr(x) "<TD align=\"right\">" x "</TD>"
#define TDc(x) "<TD align=\"center\">" x "</TD>"
#define THr(x) "<TH align=\"right\">" x "</TH>"
#define TH(x) "<TH>" x "</TH>"
#define TD(x) "<TD>" x "</TD>"
//#define TD_zu() "<TD align=\"right\">%'zu</TD>"



static char **_fuse_argv;
static int _fuse_argc;


static time_t _timeAtStart;
static void log_path(const char *f,const char *path){
  log_msg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,my_strlen(path));
}
static int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s:",func);
  perror("");
  return ret;
}
/*
  static void log_strings(const char *pfx, char *ss[],int n){
  if (ss){
  for(int i=0;i<n;i++){
  log_msg("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
  puts(ANSI_RESET);
  }
  }
  }
*/
static void log_fh(char *title,long fh){
  char p[MAX_PATHLEN];
  path_for_fd(title,p,fh);
  log_debug_now("%s  fh: %ld %s \n",title?title:"", fh,p);
}
#define MAX_INFO 55555
static  char _info[MAX_INFO+2];
#ifdef __cppcheck__
#define PRINTINFO(...)   printf(__VA_ARGS__)
#else
#define PRINTINFO(...)   n=printinfo(n,__VA_ARGS__)
#endif
static int printinfo(int n, const char *format,...){
  if (n<MAX_INFO && n>=0){
    va_list argptr;
    va_start(argptr,format);
    //fprintf(stderr,"printinfo: MAX_INFO=%d n=%d _info=%p \n",MAX_INFO,n,_info);
    //vprintf(format,argptr);
    n+=vsnprintf(_info+n,MAX_INFO-n,format,argptr);
    va_end(argptr);
  }
  return n;
}

static int print_fuse_argv(int n){
  PRINTINFO("<H1>Fuse Parameters</H1><OL>");
  for(int i=1;i<_fuse_argc;i++) PRINTINFO("<LI>%s</LI>\n",_fuse_argv[i]);
  PRINTINFO("</OL>\n");
  return n;
}
static int print_roots(int n){
  PRINTINFO("<H1>Roots</H1>\n<TABLE><THEAD><TR>"TH("Path")TH("Writable")THr("Response")THr("Blocked")TH("Free[GB]")TH("Dir-Cache[kB]")"</TR></THEAD>\n");
  foreach_root(i,r){
    const int f=r->features, freeGB=(int)((r->statfs.f_bsize*r->statfs.f_bfree)>>30),diff=deciSecondsSinceStart()-r->rthread_when_response_deciSec;
    if (n>=0){
      PRINTINFO("<TR>"TD("%s")TDc("%s"),r->path,yes_no(f&ROOT_WRITABLE));

      if (f&ROOT_REMOTE) PRINTINFO(TDr("%'ld ms")TDr("%'u"), 10L*(diff>ROOT_OBSERVE_EVERY_DECISECONDS*3?diff:r->statfs_took_deciseconds),r->log_count_delayed);
      else PRINTINFO(TDr("Local")TD(""));
      uint64_t u; LOCK(mutex_dircache, u=MSTORE_USAGE(&r->dircache)/1024);
      PRINTINFO(TDr("%'d")TDr("%'zu")"</TR>\n",freeGB,u);
    }else{
      log_msg("\t%d\t%s\t%s\n",i,r->path,!my_strlen(r->path)?"":(f&ROOT_REMOTE)?"Remote":(f&ROOT_WRITABLE)?"Writable":"Local");
    }
  }
  PRINTINFO("</TABLE>\n");
  return n;
}
static int repeat_chars_info(int n,char c, int i){
  if (i>0 && n+i<MAX_INFO){
    memset(_info+n,c,i);
    n+=i;
  }
  return n;
}

static int print_bar(int n,float rel){
  const int L=100, a=max_int(0,min_int(L,(int)(L*rel)));
  PRINTINFO("<SPAN style=\"border-style:solid;\"><SPAN style=\"background-color:red;color:red;\">");
  n=repeat_chars_info(n,'#',a);
  PRINTINFO("</SPAN><SPAN style=\"background-color:blue;color:blue;\">");
  n=repeat_chars_info(n,'~',L-a);
  PRINTINFO("</SPAN></SPAN>\n");
  return n;
}
//////////////////
/// Linux only ///
//////////////////
#ifdef __linux__
static int print_proc_status(int n,char *filter,int *val){
  PRINTINFO("<B>/proc/%ld/status</B>\n<PRE>\n", (long)getpid());
  char buffer[1024]="";
  FILE* file=fopen("/proc/self/status","r");
  while (fscanf(file,"%1023s",buffer)==1){
    for(char *f=filter;*f;f++){
      if ((f==filter || f[-1]=='|') && !strcmp(buffer,f)){
        fscanf(file," %d",val);
        if (n>0) PRINTINFO("%s %'d kB\n",buffer,*val);
        break;
      }
    }
  }
  PRINTINFO("</PRE>\n");
  fclose(file);
  return n;
}
static void log_virtual_memory_size(){
  int val;print_proc_status(-1,"VmSize:",&val);
  log_msg("Virtual memory size: %'d kB\n",val);
}
static int print_open_files(int n, int *fd_count){
  PRINTINFO("<H1>File handles</H1>\n(Should be empty if idle).<BR>");
  static char path[256];
  struct dirent *dp;
  DIR *dir=opendir("/proc/self/fd/");
  PRINTINFO("<OL>\n");
  for(int i=0;(dp=readdir(dir));i++){
    if (fd_count) (*fd_count)++;
    if (n<0 || atoi(dp->d_name)<4) continue;
    static char proc_path[PATH_MAX];
    snprintf(proc_path,PATH_MAX,"/proc/self/fd/%s",dp->d_name);
    const int l=readlink(proc_path,path,255);path[MAX(l,0)]=0;
    if (endsWith(path,FILE_LOG_WARNINGS)||!strncmp(path,"/dev/",5)|| !strncmp(path,"/proc/",6) || !strncmp(path,"pipe:",5)) continue;
    PRINTINFO("<LI>%s -- %s</LI>\n",proc_path,path);
  }
  closedir(dir);
  PRINTINFO("</OL>\n");
  return n;
}
#if 0
// TODO Error in sscanf
static int print_maps(int n){
  PRINTINFO("<H1>/proc/%ld/maps</H1>\n", (long)getpid());
  size_t total=0;
  //char fname[99];snprintf(fname,sizeof(fname),"/proc/%ld/maps", (long)getpid());
  FILE *f=fopen("/proc/self/maps","r");
  if(!f) {
    PRINTINFO("/proc/self/maps: %s\n",strerror(errno));
    return n;
  }
  PRINTINFO("<TABLE>\n<THEAD><TR>"TH("Addr&gt;&gt;12")TH("Name")TH("kB")TH("")"</TR></THEAD>\n");
  const int L=333;
  while(!feof(f)) {
    char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX]={0};
   uint64_t begin,end,inode,foo;
    if(fgets(buf,sizeof(buf),f)==0) break;
    *mapname=0;
    //    %*[^\n]\n   /home/cgille/tmp/map.txt
    const int k=sscanf(buf, "%lx-%lx %4s %lx %5s %lu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
    if (k>=6){
      const int64_t size=end-begin;
      total+=size;
      if (!strchr(mapname,'/') && size>=SIZE_CUTOFF_MMAP_vs_MALLOC){
        PRINTINFO("<TR>"TD("%08lx")TD("%s")""TDr("%'ld")"<TD>",begin>>12,mapname,size>>10);
        n=repeat_chars_info(n,'-',min_int(L,(int)(3*log(size))));
        PRINTINFO("</TD></TR>\n");
      }
    } else log_debug_now("sscanf scanned n=%d\n",k);
  }
  fclose(f);
  PRINTINFO("<TFOOT><TR>"TD("")TD("")TD("%'lu")TD("")"</TR></TFOOT>\n</TABLE>\n",total>>10);
  return n;
}
#endif
static int print_memory_details(int n){
  PRINTINFO("<H1>Memory - details</H1>\n<PRE>");
  {
    struct rlimit rl={0};
    getrlimit(RLIMIT_AS,&rl);
    if (rl.rlim_cur!=-1) PRINTINFO("Rlim soft: %lx MB   hard: %lx MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  }
  PRINTINFO("uordblks: %'d bytes (Total allocated heap space)\n",mallinfo().uordblks);
  //    PRINTINFO("Memory for storing directories=%'ld kB\n",mmapstore_used(_db_readdir)<<10);
  PRINTINFO("</PRE>\n");
  //n=print_maps(n);
  {
    int val; n=print_proc_status(n,"VmRSS:|VmHWM:|VmSize:|VmPeak:",&val);
  }

  return n;
}
#else
#define print_proc_status(...) 1
#define log_virtual_memory_size(...) 1
#define print_open_files(...) 1
#define print_memory_details(...) 1
#define print_maps(...) 1
#endif
////////////////////////////////////////////////////////////////////

static int print_memory(int n){
  PRINTINFO("<H1>Cache of ZIP entries</H1><PRE>");
  n=print_bar(n,trackMemUsage(0,0)/(float)_memcache_maxbytes);  PRINTINFO("Current usage %'ld MB of %'ld MB \n",trackMemUsage(memusage_get_curr,0)>>20,_memcache_maxbytes>>20);
  n=print_bar(n,trackMemUsage(0,0)/(float)_memcache_maxbytes);  PRINTINFO("Peak usage    %'ld MB of %'ld MB  \n",trackMemUsage(memusage_get_peak,0)>>20,_memcache_maxbytes>>20);
  {
#ifdef __linux__
    struct rlimit rl={0}; getrlimit(RLIMIT_AS,&rl);
    const bool rlset=rl.rlim_cur!=-1;
    int val=-1;print_proc_status(-1,"VmSize:",&val);
    if(rlset)  n=print_bar(n,val*1024/(.01+rl.rlim_cur));
    PRINTINFO("Virt memory %'d MB",val>>10);
    if(rlset) PRINTINFO(" / rlimit %zu MB<BR>\n",rl.rlim_cur>>20);
#endif
  }
  PRINTINFO("</PRE>\n");
  PRINTINFO("Policy for caching ZIP entries: %s<BR>\n",WHEN_MEMCACHE_S[_memcache_policy]);
  PRINTINFO("Number of calls to mmap: %d to munmap:%d<BR>\n",_memusage_count[2*memusage_mmap],_memusage_count[2*memusage_mmap+1]);
  PRINTINFO("Number of calls to malloc: %d to free:%d<BR>\n",_memusage_count[2*memusage_malloc],_memusage_count[2*memusage_malloc+1]);
  return n;
}


static int print_read_statistics(int n){
  PRINTINFO("<H1>Read bytes statistics</H1>\n<TABLE><THEAD><TR>"TH("Event")TH("Count")"</TR></THEAD>\n");
#define TABLEROW(a,skip) PRINTINFO("<TR>"TD("<B>%s</B>")TDr("%'d")"</TR>\n",(#a)+skip,a)
  TABLEROW(_count_readzip_memcache,7);
  TABLEROW(_count_readzip_regular,7);
  TABLEROW(_count_readzip_seekable[1],7);
  TABLEROW(_count_readzip_seekable[0],7);
  TABLEROW(_count_readzip_no_seek,7);
  TABLEROW(_count_readzip_seek_fwd,7);
  TABLEROW(_count_readzip_memcache_because_seek_bwd,7);
  TABLEROW(_count_readzip_reopen_because_seek_bwd,7);
  TABLEROW(_count_close_later,7);
  TABLEROW(_count_statcache_get,7);
  TABLEROW(_log_read_max_size,7);
  TABLEROW(_log_count_lock,7);
  PRINTINFO("</TABLE>");
#undef TABLEROW
  return n;
}

static size_t _kilo(size_t x){
  return ((1<<10)-1+x)>>10;
}
static int print_fhdata(int n,const char *title){
  if (n<0) log_cache("print_fhdata %s\n",snull(title));
  else PRINTINFO("<H1>Data associated to file descriptors</H1>\n");
  if (!_fhdata_n){
    if (n<0) log_msg("_fhdata_n is 0\n");
    else PRINTINFO("The cache is empty which is good. It means that no entry is locked or leaked.<BR>\n");
  }else{
    PRINTINFO("Table should be empty when idle.<BR>Column <I><B>fd</B></I> file descriptor. Column <I><B>Last access</B></I>:    Column <I><B>Millisec</B></I>:  time to read entry en bloc into cache.  Column <I><B>R</B></I>: number of threads in currently in xmp_read(). \
Column <I><B>F</B></I>: flags. (D)elete insicates that it is marked for closing and (K)eep indicates that it can currently not be closed. Two possible reasons why data cannot be released: (I)  xmp_read() is currently run (II) the cached zip entry is needed by another file descriptor  with the same virtual path.<BR>\n \
<TABLE>\n<THEAD><TR>"TH("Path")TH("fd")TH("Last access")TH("Cache addr")TH("Cache read kB")TH("Entry kB")TH("Millisec")TH(" F")TH("R")"</TR></THEAD>\n");
    const time_t t0=time(NULL);

    foreach_fhdata_path_not_null(d){
      if (n<0) log_msg("\t%p\t%s\t%p\t%zu\n",d,snull(d->path),d->memcache,d->memcache_l);
      else{
        PRINTINFO("<TR>"TD("%s")TD("%llu"),snull(d->path),d->fh);
        if (d->accesstime) PRINTINFO(TDr("%'ld s"),t0-d->accesstime);  else PRINTINFO(TDr("Never"));
        if (d->memcache) PRINTINFO(TD("%p")""TDr("%'zu")TDr("%'zu")TDr("%'ld"),d->memcache,_kilo(d->memcache_already),_kilo(d->memcache_l),d->memcache_took_mseconds);
        else PRINTINFO(TD("")TD("")TD(""));
        LOCK(mutex_fhdata,  const bool can_destroy=fhdata_can_destroy(d));
        PRINTINFO(TD("%c%c")TD("%d")"</TR>\n",d->close_later?'D':' ', can_destroy?' ':'K',d->is_xmp_read);
      }
    }
    PRINTINFO("</TABLE>");
  }
  return n;
}

static int print_all_info(){
  log_entered_function("print_all_info\n");
  int n=0;
  PRINTINFO("<HTML>\n<HEAD>\n\
<TITLE>ZIPsFS File system info</TITLE>\n\
<META http-equiv=\"Cache-Control\" content=\"no-cache,no-store,must-revalidate\"/>\n\
<META http-equiv=\"Pragma\" content=\"no-cache\"/>\n\
<META http-equiv=\"Expires\" content=\"0\"/>\n\
<STYLE>\n\
H1{color:blue;}\n\
TABLE{borborder-color:black;border-style:groove;}\n\
table,th,td{border:1pxsolidblack;border-collapse:collapse;}\n\
TBODY>TR:nth-child(even){background-color:#FFFFaa;}\n\
TBODY>TR:nth-child(odd){background-color:#CCccFF;}\n\
THEAD{background-color:black;color:white;}\n\
TD{padding:5px;}\n\
</STYLE>\n</HEAD>\n<BODY>\n\
<B>ZIPsFS:</B> <A href=\"%s\">%s</A><BR> \n",HOMEPAGE,HOMEPAGE);
  {
    struct stat statbuf;
    stat(_thisPrg,&statbuf);
    PRINTINFO("\nStarted %s:  %s &nbsp; Compiled: %s<BR>",_thisPrg,ctime(&_timeAtStart),ctime(&statbuf.st_mtime));
  }
  n=print_roots(n);
  n=print_fuse_argv(n);
  n=print_open_files(n,NULL);
  n=print_memory(n);
  n=print_fhdata(n,"");
  n=print_memory_details(n);
  PRINTINFO("_mutex=%p\n<BR>",_mutex);
  PRINTINFO("<H1>Inodes</H1>Created sequentially: %'d\n",_count_SeqInode);
  n=print_read_statistics(n);
  //  PRINTINFO("_cumul_time_store=%lf\n<BR>",((double)_cumul_time_store)/CLOCKS_PER_SEC);
  //n=print_misc(n);

  PRINTINFO("</BODY>\n</HTML>\n");
  log_exited_function("get_info\n %d",n);
  return n;
}
static const char *zip_fdopen_err(int err){
#define  ZIP_FDOPEN_ERR(code,descr) if (err==code) return descr
  ZIP_FDOPEN_ERR(ZIP_ER_INCONS,"Inconsistencies were found in the file specified by path. This error is often caused by specifying ZIP_CHECKCONS but can also happen withot it.");
  ZIP_FDOPEN_ERR(ZIP_ER_INVAL,"The flags argument is invalid. Not all zip_open(3) flags are allowed for zip_fdopen, see DESCRIPTION.");
  ZIP_FDOPEN_ERR(ZIP_ER_MEMORY,"Required memory could not be allocated.");
  ZIP_FDOPEN_ERR(ZIP_ER_NOZIP,"The file specified by fd is not a zip archive.");
  ZIP_FDOPEN_ERR(ZIP_ER_OPEN,"The file specified by fd could not be prepared for use by libzip(3).");
  ZIP_FDOPEN_ERR(ZIP_ER_READ,"A read error occurred; see errno for details.");
  ZIP_FDOPEN_ERR(ZIP_ER_SEEK,"The file specified by fd does not allow seeks.");
  ZIP_FDOPEN_ERR(ZIP_ER_INCONS,"Inconsistencies were found in the file specified by path. This error is often caused by specifying ZIP_CHECKCONS but can also happen without it.");
  ZIP_FDOPEN_ERR(ZIP_ER_INVAL,"The flags argument is invalid. Not all zip_open(3) flags are allowed for zip_fdopen, see DESCRIPTION.");
  return "zip_fdopen - Unknown error";
#undef ZIP_FDOPEN_ERR
}
static void fhdata_log_cache(const struct fhdata *d){
  if (!d){ log_msg("\n");return;}
  const int idx=(int)(_fhdata-d);
  log_msg("log_cache: d= %p path= %s cache: %s,%s cache_l= %zu/%zu idx: %d  hasc: %s\n",d,d->path,yes_no(d->memcache!=NULL),MEMCACHE_STATUS_S[d->memcache_status],d->memcache_already/d->memcache_l,idx,yes_no(fhdata_has_memcache(d)));
}
static void _log_zpath(const char *fn,const char *msg, struct zippath *zpath){
  log_msg(ANSI_INVERSE"%s() %s"ANSI_RESET,fn,msg);
  log_msg("   this= %p   \n",zpath);
  log_msg("    %p strgs="ANSI_FG_BLUE"%s"ANSI_RESET"  "ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, (zpath->flags&ZP_STRGS_ON_HEAP)?"Heap":"Stack", zpath->strgs_l);
  log_msg("    %p    VP="ANSI_FG_BLUE"'%s' VP_LEN: %d"ANSI_RESET,VP(),snull(VP()),VP_LEN()); log_file_stat("",&zpath->stat_vp);
  log_msg("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  log_msg("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  log_msg("    %p    RP="ANSI_FG_BLUE"'%s' "ANSI_RESET,RP(), snull(RP())); log_file_stat("",&zpath->stat_rp);
  log_msg("    zip: %s"ANSI_RESET"\n",yes_no(ZPATH_IS_ZIP()));
}
#define log_zpath(...) _log_zpath(__func__,__VA_ARGS__)

#define FHDATA_ALREADY_LOGGED_VIA_ZIP (1<<0)
#define FHDATA_ALREADY_LOGGED_FAILED_DIFF (1<<1)
static bool fhdata_not_yet_logged(unsigned int flag,struct fhdata *d){
  if (d->already_logged&flag) return false;
  d->already_logged|=flag;
  return true;
}

static void log_fuse_process(){
  //log_debug_now(ANSI_YELLOW"log_fuse_process"ANSI_RESET" ");
  struct fuse_context *fc=fuse_get_context();
  const int BUF=333;
  char buf[BUF+1];
  snprintf(buf,BUF-1,"/proc/%d/cmdline",fc->pid);
  FILE *f=fopen(buf,"r");
  if (!f){
    perror("log_fuse_process");
    log_error("Open %s failed\n",buf);
  }else{
    fscanf(f,"%333s",buf);
    log_msg("PID=%d cmd=%s\n",fc->pid,buf);
    fclose(f);
  }
}
static void usage(){
  fputs(ANSI_INVERSE"ZIPsFS"ANSI_RESET"\n\nCompiled: "__DATE__"  "__TIME__"\n\n",LOG_STREAM);
  fputs(ANSI_UNDERLINE"Usage:"ANSI_RESET"  ZIPsFS [options]  root-dir1 root-dir2 ... root-dir-n : [libfuse-options]  mount-Point\n\n"
        ANSI_UNDERLINE"Example:"ANSI_RESET"  ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable  ~/local/folder //computer/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt\n\n\
The first root directory (here  ~/tmp/ZIPsFS/writable)  is writable and allows file creation and deletion, the others are read-only.\n\
Root directories starting with double slash (here //computer/pub) are regarded as less reliable and potentially blocking.\n\n\
Options and arguments before the colon are interpreted by ZIPsFS.  Those after the colon are passed to fuse_main(...).\n\n",LOG_STREAM);
    fputs(ANSI_UNDERLINE"ZIPsFS options:"ANSI_RESET"\n\n\
      -h Print usage.\n\
      -q quiet, no debug messages to stdout\n\
         Without the option -f (foreground) all logs are suppressed anyway.\n\
      -c [",LOG_STREAM);
  //  for(char **s=WHEN_MEMCACHE_S;*s;s++){if (s!=WHEN_MEMCACHE_S) putchar('|');fputs(*s,LOG_STREAM);}
  for(int i=0;WHEN_MEMCACHE_S[i];i++){ if (i) putchar('|');fputs(WHEN_MEMCACHE_S[i],LOG_STREAM);}
    fputs("]    When to read zip entries into RAM\n\
         seek: When the data is not read continuously\n\
         rule: According to rules based on the file name encoded in configuration.h\n\
         The default is when backward seeking would be requires otherwise\n\
      -k Kill on error. For debugging only.\n\
      -l Limit memory usage for caching ZIP entries.\n\
         Without caching, moving read positions backwards for an non-seek-able ZIP-stream would require closing, reopening and skipping.\n\
         To avoid this, the limit is multiplied with factor 1.5 in such cases.\n\n",LOG_STREAM);
    fputs(ANSI_UNDERLINE"Fuse options:"ANSI_RESET"These options are given after a colon.\n\n\
 -d Debug information\n\
 -f File system should not detach from the controlling terminal and run in the foreground.\n\
 -s Single threaded mode. Maybe tried in case the default mode does not work properly.\n\
 -h Print usage information.\n\
 -V Version\n\n",LOG_STREAM);
}
