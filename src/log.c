static void log_path(const char *f,const char *path){
  printf("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,my_strlen(path));
}
static int log_func_error(char *func){
  int ret=-errno;
  log_error(" %s:",func);
  perror("");
  return ret;
}
static void log_strings(const char *pfx, char *ss[],int n){
  printf(ANSI_UNDERLINE"%s"ANSI_RESET" %p\n",pfx,ss);
  if (ss){
    for(int i=0;i<n;i++){
      printf("   %s %3d./.%d "ANSI_FG_BLUE"'%s'",pfx,i,n,ss[i]);
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
static int print_open_files(int n, int *fd_count){
  static char path[256];
  struct dirent *dp;
  //static char proc_path[PROC_PATH_MAX]; const int len_proc_path=snprintf(proc_path,64,"/proc/%i/fd/",getpid());
  DIR *dir=opendir("/proc/self/fd/");
  if (n>=0) PRINTINFO("<OL>\n");
  for(int i=0;(dp=readdir(dir));i++){
    if (fd_count) (*fd_count)++;
    if (n<0 || atoi(dp->d_name)<4) continue;
    //my_strncpy(proc_path+len_proc_path,dp->d_name,PROC_PATH_MAX-len_proc_path);
    static char proc_path[PROC_PATH_MAX];
    snprintf(proc_path,PROC_PATH_MAX,"/proc/self/fd/%s",dp->d_name);
    const int l=readlink(proc_path,path,255);path[MAX(l,0)]=0;
    PRINTINFO("<LI>%s -- %s</LI>\n",proc_path,path);
  }
  closedir(dir);
  if (n>=0) PRINTINFO("</OL>\n");
  return n;
}
static int  printProcStatus(int n){
  char buffer[1024]="";
  FILE* file=fopen("/proc/self/status","r");
  PRINTINFO("<H1>/proc/%ld/status</H1>\n<PRE>\n", (long)getpid());
  while (fscanf(file,"%1023s",buffer)==1){
#define C(a) !strcmp(buffer, # a ":")
    int val;
    if (C(VmRSS) || C(VmHWM) || C(VmSize) || C(VmPeak)){ fscanf(file," %d",&val); PRINTINFO("%s %'d kB\n",buffer,val);}
#undef C
  }
  PRINTINFO("</PRE>\n");
  fclose(file);
  return n;
}
static int print_maps(int n){
    PRINTINFO("<H1>/proc/%ld/maps</H1>\n", (long)getpid());
  unsigned long total=0;
  //char fname[99];snprintf(fname,sizeof(fname),"/proc/%ld/maps", (long)getpid());
  FILE *f=fopen("/proc/self/maps","r");
  if(!f) {
    PRINTINFO("/proc/self/maps: %s\n", strerror(errno));
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
    if (!strchr(mapname,'/') && end-begin>=SIZE_CUTOFF_MMAP_vs_MALLOC){
      PRINTINFO("<TR><TD>%08lx</TD><TD align=\"right\">%'ld</TD><TD>%s</TD></TR>\n",begin>>12,(end-begin)>>10,mapname);
    }
  }
  fclose(f);
  PRINTINFO("<TFOOT><TR><TD></TD><TD>%'lu</TD><TD></TD></TR></TFOOT>\n</TABLE>\n",total>>10);
  return n;
}
static int log_cached(int n,char *title){
  if (n<0) log_cache("log_cached %s\n",snull(title));
  else PRINTINFO("<TABLE>\n<THEAD><TR><TH></TH><TH>Path</TH><TH>Access</TH><TH>Addr&gt;&gt;12</TH><TH>KByte</TH><TH>Millisec</TH></TR></THEAD>\n");
  char stime[99];
  for(int i=_fhdata_n; --i>=0;){
    struct fhdata *d=_fhdata+i;
    if (!d->path) continue; /* empty position where an instance had been destroyed */
    if (n<0) printf("\t%4d\t%p\t%s\t%p\t%zu\n",i,d, d->path,d->cache,d->cache_l);
    else{
      struct tm tm = *localtime(&d->access);
      sprintf(stime,"%d-%02d-%02d_%02d:%02d:%02d\n",tm.tm_year+1900,tm.tm_mon+1,tm.tm_mday,tm.tm_hour,tm.tm_min,tm.tm_sec);
      //  PRINTINFO("<TR><TD>%4d</TD><TD>%s</TD><TD>%s</TD><TD>%lx</TD><TD align=\"right\">%'zu</TD><TD align=\"right\">%'d</TD></TR>\n",i,d->path,stime,((long)d->cache)>>12,MAX(d->cache_l>>10,1),d->cache_read_seconds);
      PRINTINFO("<TR><TD>%4d</TD>",i);
      PRINTINFO("<TD>%s</TD>",d->path);
      PRINTINFO("<TD>%s</TD>",stime);
      PRINTINFO("<TD>%lx</TD>",((long)d->cache)>>12);
      PRINTINFO("<TD align=\"right\">%'zu</TD>",MAX(d->cache_l>>10,1));
      PRINTINFO("<TD align=\"right\">%'d</TD></TR>\n",d->cache_read_seconds);
    }
  }
  if (n>=0) PRINTINFO("</TABLE>");
  return n;
}
static int get_info(){
  log_entered_function("get_info\n");
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
<TABLE><THEAD><TR><TH>Path</TH><TH>Writable</TH><TH align=\"right\">Response</TH><TH align=\"right\">Delayed</TH><TH>Free GB</TH></TR></THEAD>\n");
  char remote[99];
  for(int i=0;i<_root_n;i++){
    struct rootdata *r=_root+i;
    const int f=r->features, freeGB=(int)((r->statfs.f_bsize*r->statfs.f_bfree)>>30), diff=currentTimeMillis()-r->statfs_when;
    if (f&ROOT_REMOTE) sprintf(remote,"%'d ms",diff>ROOT_OBSERVE_EVERY_MSECONDS*3?diff:r->statfs_mseconds);
    else strcpy(remote,"Local");
    PRINTINFO("<TR><TD>%s</TD><TD align=\"center\">%s</TD><TD align=\"right\">%s</TD><TD align=\"right\">%'d</TD><TD align=\"right\">%'d</TD></TR>\n",r->path,yes_no(f&ROOT_WRITABLE),remote,r->delayed,freeGB);
  }
  PRINTINFO("</TABLE>\n<H1>Heap</H1>\nuordblks: %'d<BR>\n",mallinfo().uordblks);
  PRINTINFO("<H1>File handles</H1>\n");
  n=print_open_files(n,NULL);
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
  PRINTINFO("<H1>Cache</H1>\nWhen cache Zip entry: %s<BR>sqlite-db for directory listings: %s<BR>\n",WHEN_CACHE_S[_when_cache],_sqlitefile);
  n=log_cached(n,"");
  n=print_maps(n);
  n=printProcStatus(n);
  struct rlimit rl={0};
  getrlimit(RLIMIT_AS,&rl);
  PRINTINFO("Cache ZIP-entries<BR><UL>\n");
  PRINTINFO("<LI>Number of calls mmap:%d munmap:%d</LI>\n", _memusage_count[2*memusage_mmap],_memusage_count[2*memusage_mmap+1]);
  PRINTINFO("<LI>Number of calls malloc:%d free:%d</LI>\n", _memusage_count[2*memusage_malloc],_memusage_count[2*memusage_malloc+1]);
  PRINTINFO("</UL>\n");
  PRINTINFO("trackMemUsage %ld MB<BR>\n",trackMemUsage(0,0)/(1024*1024));
  PRINTINFO("Rlim soft: %'zu MB   hard: %'zu MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  PRINTINFO("<BR><H1>Inodes</H1>Created sequentially: %d\n",_countSeqInode);
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
static void fhdata_log_cache(struct fhdata *d){
  if (!d){ log("\n");return;}
  const char
    *c=d->cache,
    *s= !c?"Null":c==CACHE_FAILED?"FAILED":c==CACHE_FILLING?"FILLING":"Yes";
  const int index=(int)(_fhdata-d);
  log("log_cache: d= %p path= %s cache: %s cache_l= %zu can_destroy: %s  index: %d  hasc: %s\n",d,d->path,s,d->cache_l,yes_no(fhdata_can_destroy(d)),index,yes_no(fhdata_has_cache(d)));
}
static void log_zpath(char *msg, struct zippath *zpath){
  prints(ANSI_UNDERLINE);
  prints(msg);
  puts(ANSI_RESET);
  printf("   this= %p   only slash: %d\n",zpath,0!=(zpath->flags&ZP_PATH_IS_ONLY_SLASH));
  printf("    %p strgs="ANSI_FG_BLUE"%s"ANSI_RESET"  "ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, (zpath->flags&ZP_STRGS_ON_HEAP)?"Heap":"Stack", zpath->strgs_l);
  printf("    %p    VP="ANSI_FG_BLUE"'%s' VP_LEN: %d"ANSI_RESET,VP(),snull(VP()),VP_LEN()); log_file_stat("",&zpath->stat_vp);
  printf("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  printf("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  printf("    %p    RP="ANSI_FG_BLUE"'%s'"ANSI_RESET,RP(), snull(RP())); log_file_stat("",&zpath->stat_rp);
  printf("    zip: %s  ZIP %s"ANSI_RESET"\n",yes_no(ZPATH_IS_ZIP()),  zpath->zarchive?ANSI_FG_GREEN"opened":ANSI_FG_RED"closed");
}
#define FHDATA_ALREADY_LOGGED_VIA_ZIP (1<<0)
#define FHDATA_ALREADY_LOGGED_FAILED_DIFF (1<<1)
static bool fhdata_not_yet_logged(unsigned int flag,struct fhdata *d){
  if (d->already_logged&flag) return false;
  d->already_logged|=flag;
  return true;
}
