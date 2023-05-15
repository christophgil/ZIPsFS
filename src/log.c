//   (global-set-key (kbd "<f4>") '(lambda() (interactive) (switch-to-buffer "log.c")))
static void log_path(const char *f,const char *path){
  logmsg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,my_strlen(path));
}
static int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s:",func);
  perror("");
  return ret;
}
static void log_strings(const char *pfx, char *ss[],int n){
  if (ss){
    for(int i=0;i<n;i++){
      logmsg("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
      puts(ANSI_RESET);
    }
  }
}
static void log_fh(char *title,long fh){
  char p[MAX_PATHLEN];
  path_for_fd(title,p,fh);
  log_debug_now("%s  fh: %ld %s \n",title?title:"", fh,p);
}
#define MAX_INFO 33333 // DEBUG_NOW
static char _info[MAX_INFO+1];
#define PRINTINFO(...)  n+=snprintf(_info+n,MAX_INFO-n,__VA_ARGS__)
#define PROC_PATH_MAX 256

static int print_fuse_argv(int n){
  PRINTINFO("<H1>Fuse Parameters</H1><OL>");
  for(int i=1;i<_fuse_argc;i++) PRINTINFO("<LI>%s</LI>\n",_fuse_argv[i]);
  PRINTINFO("</OL>\n");
  return n;
}
static int print_roots(int n){
  PRINTINFO("<H1>Roots</H1>\n<TABLE><THEAD><TR><TH>Path</TH><TH>Writable</TH><TH align=\"right\">Response</TH><TH align=\"right\">Delayed</TH><TH>Free GB</TH></TR></THEAD>\n");

  for(int i=0;i<_root_n;i++){
    struct rootdata *r=_root+i;
    const int f=r->features, freeGB=(int)((r->statfs.f_bsize*r->statfs.f_bfree)>>30), diff=currentTimeMillis()-r->statfs_when;
    PRINTINFO("<TR><TD>%s</TD><TD align=\"center\">%s</TD>",r->path,yes_no(f&ROOT_WRITABLE));
    if (f&ROOT_REMOTE) PRINTINFO("<TD align=\"right\">%'d ms</TD><TD align=\"right\">%'d</TD>",  diff>ROOT_OBSERVE_EVERY_MSECONDS*3?diff:r->statfs_mseconds,r->delayed);
    else PRINTINFO("<TD align=\"right\">Local</TD><TD></TD>");
    PRINTINFO("<TD align=\"right\">%'d</TD></TR>\n",freeGB);
  }
  PRINTINFO("</TABLE>\n");
  return n;
}

static int print_bar(int n,float rel){
  const int L=100;
  static char A[L],B[L];
  memset(A,'#',L);
  memset(B,'~',L);\
  //  const int k=min_int(L,(int)(L*(rel+.5)));
  const int a=max_int(0,min_int(L,(int)(L*rel)));
  PRINTINFO("<SPAN style=\"border-style:solid;\"><SPAN style=\"background-color:red;color:red;\">%s</SPAN><SPAN style=\"background-color:blue;color:blue;\">%s</SPAN></SPAN> ",A+L-a,B+a);
  return n;
}
//////////////////
/// Linux only ///
//////////////////
#ifdef __linux__
static int print_proc_status(int n,char *filter,int *val){
  if (n>=0)PRINTINFO("<B>/proc/%ld/status</B>\n<PRE>\n", (long)getpid());
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
  if (n>=0) PRINTINFO("</PRE>\n");
  fclose(file);
  return n;
}
static void log_virtual_memory_size(){
  int val;print_proc_status(-1,"VmSize:",&val);
  logmsg("Virtual memory size: %'d kB\n",val);
}
static int print_open_files(int n, int *fd_count){
  PRINTINFO("<H1>File handles</H1>\n(Should be empty if idle).<BR>");
  static char path[256];
  struct dirent *dp;
  DIR *dir=opendir("/proc/self/fd/");
  if (n>=0) PRINTINFO("<OL>\n");
  for(int i=0;(dp=readdir(dir));i++){
    if (fd_count) (*fd_count)++;
    if (n<0 || atoi(dp->d_name)<4) continue;
    static char proc_path[PROC_PATH_MAX];
    snprintf(proc_path,PROC_PATH_MAX,"/proc/self/fd/%s",dp->d_name);
    const int l=readlink(proc_path,path,255);path[MAX(l,0)]=0;
    if (endsWith(path,FILE_LOG_WARNINGS)||!strncmp(path,"/dev/",5)|| !strncmp(path,"/proc/",6) || !strncmp(path,"pipe:",5)) continue;
    PRINTINFO("<LI>%s -- %s</LI>\n",proc_path,path);
  }
  closedir(dir);
  if (n>=0) PRINTINFO("</OL>\n");
  return n;
}
static int print_maps(int n){
  PRINTINFO("<H1>/proc/%ld/maps</H1>\n", (long)getpid());
  unsigned long total=0;
  //char fname[99];snprintf(fname,sizeof(fname),"/proc/%ld/maps", (long)getpid());
  FILE *f=fopen("/proc/self/maps","r");
  if(!f) {
    PRINTINFO("/proc/self/maps: %s\n",strerror(errno));
    return n;
  }
  PRINTINFO("<TABLE>\n<THEAD><TR><TH>Addr&gt;&gt;12</TH><TH>Name</TH><TH>kB</TH><TH></TH></TR></THEAD>\n");
  const int L=333;
  static char A[L];
  memset(A,'-',L);
  while(!feof(f)) {
    char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX]={0};
    unsigned long begin,end,inode,foo;
    if(fgets(buf,sizeof(buf),f)==0) break;
    //mapname[0]=0;
    sscanf(buf, "%lx-%lx %4s %lx %5s %lu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
    const long size=end-begin;
    total+=size;
    if (!strchr(mapname,'/') && size>=SIZE_CUTOFF_MMAP_vs_MALLOC){
      PRINTINFO("<TR><TD>%08lx</TD><TD>%s</TD><TD align=\"right\">%'ld</TD><TD>%s</TD></TR>\n",begin>>12,mapname,size>>10,A+L-min_int(L,(int)(3*log(size))));
    }
  }
  fclose(f);
  PRINTINFO("<TFOOT><TR><TD></TD><TD></TD><TD>%'lu</TD><TD></TD></TR></TFOOT>\n</TABLE>\n",total>>10);
  return n;
}
static int print_memory_details(int n){
  PRINTINFO("<H1>Memory - details</H1>\n<PRE>");
  {
    struct rlimit rl={0};
    getrlimit(RLIMIT_AS,&rl);
    if (rl.rlim_cur!=-1) PRINTINFO("Rlim soft: %lx MB   hard: %lx MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  }
  PRINTINFO("uordblks: %'d bytes (Total allocated heap space)\n</PRE>\n",mallinfo().uordblks);
  n=print_maps(n);
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
  n=print_bar(n,trackMemUsage(0,0)/(float)_maxMemForCache);  PRINTINFO("%'ld MB of %'ld MB \n",trackMemUsage(memusage_get_curr,0)>>20,_maxMemForCache>>20);
  n=print_bar(n,trackMemUsage(0,0)/(float)_maxMemForCache);  PRINTINFO("%'ld MB of %'ld MB \n",trackMemUsage(memusage_get_peak,0)>>20,_maxMemForCache>>20);
  {
#ifdef __linux__
    struct rlimit rl={0}; getrlimit(RLIMIT_AS,&rl);
    const bool rlset=rl.rlim_cur!=-1;
    int val=-1;print_proc_status(-1,"VmSize:",&val);
    if(rlset)  n=print_bar(n,val*1024/(.01+rl.rlim_cur));
    PRINTINFO("Virt memory %'d kB",val);
    if(rlset) PRINTINFO(" / rlimit %zu MB<BR>\n",rl.rlim_cur>>20);
#endif
  }
  PRINTINFO("</PRE>\n");
  PRINTINFO("Policy for caching ZIP entries: %s<BR>\n",WHEN_CACHE_S[_when_cache]);
  PRINTINFO("Number of calls to mmap: %d to munmap:%d<BR>\n",_memusage_count[2*memusage_mmap],_memusage_count[2*memusage_mmap+1]);
  PRINTINFO("Number of calls to malloc: %d to free:%d<BR>\n",_memusage_count[2*memusage_malloc],_memusage_count[2*memusage_malloc+1]);
  PRINTINFO("Sqlite-db stores file listings: %s<BR>\n",_sqlitefile);

  return n;
}


static int print_read_statistics(int n){
  PRINTINFO("<H1>Read bytes statistics</H1>\n<TABLE><THEAD><TR><TH>Event</TH><TH>Count</TH></TR></THEAD>\n");
#define TABLEROW(a,skip) PRINTINFO("<TR><TD><B>%s</B></TD><TD align=\"right\">%'d</TD></TR>\n",(#a)+skip,a)
  TABLEROW(_count_read_zip_cached,7);
  TABLEROW(_count_read_zip_regular,7);
  TABLEROW(_count_read_zip_seekable,7);
  TABLEROW(_count_read_zip_no_seek,7);
  TABLEROW(_count_read_zip_seek_fwd,7);
  TABLEROW(_count_read_zip_seek_bwd,7);
  TABLEROW(_count_close_later,7);
  TABLEROW(_read_max_size,7);
  PRINTINFO("</TABLE>");
#undef TABLEROW
  return n;
}


static int print_fhdata(int n,char *title){
  if (n<0) log_cache("print_fhdata %s\n",snull(title));
  else PRINTINFO("<H1>Data associated to file descriptors</H1>Table should be empty when idle. Column <I><B>fd</B></I> file descriptor. Column <I><B>Last access</B></I>:    Column <I><B>Millisec</B></I>:  time to read entry en bloc into cache.  Column <I><B>R</B></I>: number of threads in currently in xmp_read(). \
Column <I><B>F</B></I>: flags. (D)elete insicates that it is marked for closing and (K)eep indicates that it can currently not be closed. Two possible reasons why data cannot be released: (I)  xmp_read() is currently run (II) the cached zip entry is needed by another file descriptor  with the same virtual path.<BR>\n \
<TABLE>\n<THEAD><TR><TH>Path</TH><TH>fd</TH><TH>Last access</TH><TH>Cache addr</TH><TH>Cache kB</TH><TH>Millisec</TH><TH> F</TH><TH>R</TH></TR></THEAD>\n");
  char buf[999];
  foreach_fhdata_path_not_null(d){
    if (n<0) logmsg("\t%p\t%s\t%p\t%zu\n",d, d->path,d->cache,d->cache_l);
    else{
      PRINTINFO("<TR><TD>%s</TD><TD>%lu</TD>",d->path,d->fh);
      PRINTINFO("<TD align=\"right\">"); if (d->access) PRINTINFO("%'ld s</TD>",time(NULL)-d->access); else PRINTINFO("Never</TD>");
      strcpy(buf,"<TD></TD><TD></TD><TD></TD>");
      if (d->cache) sprintf(buf,"<TD>%p</TD><TD align=\"right\">%'zu</TD><TD align=\"right\">%'d</TD>",d->cache,MAX(d->cache_l>>10,!!d->cache_l),d->cache_read_seconds);
      PRINTINFO("%s<TD>%c%c</TD><TD>%d</TD></TR>\n",buf,d->close_later?'D':' ', fhdata_can_destroy(d)?' ':'K',d->xmp_read);
    }
  }
  if (n>=0) PRINTINFO("</TABLE>");
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
<B>ZIPsFS:</B> <A href=\"%s\">%s</A> \n",HOMEPAGE,HOMEPAGE);

  n=print_roots(n);
  n=print_fuse_argv(n);
  n=print_open_files(n,NULL);
  n=print_memory(n);

  n=print_fhdata(n,"");
  n=print_memory_details(n);
  PRINTINFO("<H1>Inodes</H1>Created sequentially: %'d\n",_countSeqInode);
  n=print_read_statistics(n);
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
  if (!d){ logmsg("\n");return;}
  const char
    *c=d->cache,
    *s= !c?"Null":c==CACHE_FAILED?"FAILED":c==CACHE_FILLING?"FILLING":"Yes";
  const int index=(int)(_fhdata-d);
  logmsg("log_cache: d= %p path= %s cache: %s cache_l= %zu can_destroy: %s  index: %d  hasc: %s\n",d,d->path,s,d->cache_l,yes_no(fhdata_can_destroy(d)),index,yes_no(fhdata_has_cache(d)));
}
static void log_zpath(char *msg, struct zippath *zpath){
  logmsg(ANSI_UNDERLINE"%s"ANSI_RESET,msg);
  logmsg("   this= %p   only slash: %d\n",zpath,0!=(zpath->flags&ZP_PATH_IS_ONLY_SLASH));
  logmsg("    %p strgs="ANSI_FG_BLUE"%s"ANSI_RESET"  "ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, (zpath->flags&ZP_STRGS_ON_HEAP)?"Heap":"Stack", zpath->strgs_l);
  logmsg("    %p    VP="ANSI_FG_BLUE"'%s' VP_LEN: %d"ANSI_RESET,VP(),snull(VP()),VP_LEN()); log_file_stat("",&zpath->stat_vp);
  logmsg("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  logmsg("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  logmsg("    %p    RP="ANSI_FG_BLUE"'%s'"ANSI_RESET,RP(), snull(RP())); log_file_stat("",&zpath->stat_rp);
  logmsg("    zip: %s  ZIP %s"ANSI_RESET"\n",yes_no(ZPATH_IS_ZIP()),  zpath->zarchive?ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}
#define FHDATA_ALREADY_LOGGED_VIA_ZIP (1<<0)
#define FHDATA_ALREADY_LOGGED_FAILED_DIFF (1<<1)
static bool fhdata_not_yet_logged(unsigned int flag,struct fhdata *d){
  if (d->already_logged&flag) return false;
  d->already_logged|=flag;
  return true;
}
