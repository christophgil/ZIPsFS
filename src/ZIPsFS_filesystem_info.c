/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////
#define END_HR()  PRINTINFO("</TR>%s",isHeader?"</THEAD>":"")
#define BEGIN_H1(is_html) (is_html?"<HR><H1>":ANSI_INVERSE)
#define END_H1(is_html) (is_html?"</H1>":ANSI_RESET)
#define BEGIN_PRE(is_html) (is_html?"<PRE>":"")
#define END_PRE(is_html) (is_html?"</PRE>":"")
#define INFO_TABLE_DESCRIPTION() n=info_table_description(n,describe,sizeof(describe),dc)


static int log_print_roots(int n);
static int _info_capacity=0;
static char *_info=NULL;
static char *_info_sprint_address(const int n){
  return _info && n>=0 && _info_capacity>n?_info+n:NULL;
}
static int _info_sprint_max(const int n){
  return _info && n>=0 && _info_capacity>n?_info_capacity-n:0;
}


#define PRINTINFO(...)   n+=snprintf(_info_sprint_address(n),_info_sprint_max(n),__VA_ARGS__)
#if IS_CHECKING_CODE
#define R(cond,title,descr,format,...) if (cond && *title && *descr){ printf(format,__VA_ARGS__); }
#else
#define H(title) if(*title=='<') PRINTINFO("%s",title); else PRINTINFO("<TH>%s</TH>",title);
#define R(cond,title,descr,format,...)\
  if (isHeader) { H(title);  dc+=append_description(title,descr,describe+dc,sizeof(describe)-dc);}\
  else if (cond){ PRINTINFO("<TD>" format "</TD>\n",__VA_ARGS__); }\
  else { PRINTINFO("<TD></TD>\n"); }
#endif //IS_CHECKING_CODE
#define SF(title,descr,field) R(true,"<TH colspan=\"2\">" title "</TH>",descr,"%'d</TD><TD>%'d",G(field##_SUCCESS),G(field##_FAIL));
static int append_description(const char *title, const char *descr, char *describe, const int max){
  if (!*descr || max<=0) return 0;
  char noTag[99]={0};
  int i=0;
  bool start=*title!='<';
  for(const char *t=title;*t;t++){
    if (!start){
      start=(*t=='>');
    }else{
      if (*t=='<') break;
      noTag[i++]=*t;
    }
  }
  return snprintf(describe,max,"<LI><B>%s</B>: %s</LI>",noTag,descr);
}
#if IS_CHECKING_CODE
#define COLUMN_FILE_TYPE(ext)
#else
#define COLUMN_FILE_TYPE(ext)\
  R(ext!=NULL,"X","Extension","%s",ext)\
  R(ext!=NULL,"K","Counting this known extension is optimized in source code","%c",isKnownExt(ext,0)?'+':' ')
#endif //IS_CHECKING_CODE




static int info_table_description(int n,char *txt, const int txt_max, const int txt_l){
  if (txt_l<txt_max){
    txt[txt_l]=0;
    PRINTINFO("<UL>%s</UL>\n",txt);
  }else{
    warning(WARN_FLAG_ERROR,SPECIAL_FILES[SFILE_INFO],"String variable '<I>describe</I>' too small");
  }
  return n;
}





static bool isKnownExt(const char *path, int len){
  static int index_not_needed;
  return config_some_file_path_extensions(path,len?len:strlen(path),&index_not_needed);
}
static int info_counts_by_filetype_by_root(int n){
  int count_explain=0;
  foreach_root(r) n=_counts_by_filetype_r(n,r,&count_explain);
  return n;
}
static int _counts_by_filetype_r(int n,struct rootdata *r, const int *count_explain){
  struct ht_entry *ee=HT_ENTRIES(&r->ht_filetypedata);
  int headerDone=0;

  int dc=0; // cppcheck-suppress variableScope
  char describe[3333]; // cppcheck-suppress variableScope
  FOR(pass,0,r->ht_filetypedata.capacity){
    counter_rootdata_t *x=NULL;
    RLOOP(i,r->ht_filetypedata.capacity){
      counter_rootdata_t *d=ee[i].value;
      if (!d) continue;
      if(!pass){
#define C(significance,x) (((long)atomic_load(d->counts+x))<<significance)
        d->rank=
          C(00,ZIP_OPEN_SUCCESS)+C(10,ZIP_OPEN_FAIL)+
          C(00,ZIP_READ_NOCACHE_SUCCESS)+C(00,ZIP_READ_NOCACHE_FAIL)+C(00,ZIP_READ_NOCACHE_ZERO)+
          C(00,ZIP_READ_NOCACHE_SEEK_SUCCESS)+C(8,ZIP_READ_NOCACHE_SEEK_FAIL)+
          C(00,ZIP_READ_CACHE_SUCCESS)+C(12,ZIP_READ_CACHE_FAIL)+
          C(14,ZIP_READ_CACHE_CRC32_SUCCESS)+C(00,ZIP_READ_CACHE_CRC32_FAIL)+
          C(00,COUNT_RETRY_PRELOADFILERAM)+
          (isKnownExt(d->ext,0)?(1L<<28):0);
#undef C
      }else if (!x || d->rank>x->rank){
        x=d;
      }
    }
#undef C
    if (pass){
      if (!x) break;
      if (!x->rank) continue;
      assert(x->ext!=NULL);
      x->rank=0;
#define G(field) atomic_load(x->counts+field)
      RLOOP(isHeader,(!headerDone++?2:1)){
        if (isHeader) PRINTINFO("<HR><H1>Counts by file type for root %s</H1>\n<TABLE border=\"1\">\n<THEAD><TR>\n",report_rootpath(r));
        PRINTINFO("<TR>");
        COLUMN_FILE_TYPE(x->ext);
        #define sf  "  success/fail"
        SF("ZIP open","How often are ZIP entries opened"sf,ZIP_OPEN);
        SF("ZIP read no-cache","Count direct read operations for ZIP entries"sf,ZIP_READ_NOCACHE);
        SF("ZIP read cache","Count loading ZIP entries into RAM"sf,ZIP_READ_CACHE);
        R(true,"ZIP read 0","","%'u",G(ZIP_READ_NOCACHE_ZERO));
        SF("Seek","Count setting file pointer to a different position"sf,ZIP_READ_NOCACHE_SEEK);
        SF("Checksum match/mismatch","If entirely read, count computation of CRC32 checksum"sf,ZIP_READ_CACHE_CRC32);
        R(true,"Cumul wait","Threads reading cached file data need to wait until the cache is filled to the respective position. Cumulative waiting time in s","%.2f",((float)x->wait)/CLOCKS_PER_SEC);
        R(true,"Retry cache","Count success on retry reading into RAM cache","%d",G(COUNT_RETRY_PRELOADFILERAM));
#undef sf
#undef G
        END_HR();
      }/*isHeader*/
    }/*if pass*/
  }/*for pass*/
  if (headerDone){
    PRINTINFO("</TABLE>\n");
    if (!*count_explain++) INFO_TABLE_DESCRIPTION();
  }
  return n;
}




static long counter_getattr_rank(const struct ht_entry *e){
  const int *vv=e->value;
  return !vv?0:
    ((vv[COUNTER_GETATTR_FAIL]+vv[COUNTER_READDIR_FAIL])<<10L)+
    ((vv[COUNTER_STAT_FAIL]+vv[COUNTER_OPENDIR_SUCCESS])<<10L)+
    vv[COUNTER_GETATTR_SUCCESS]+vv[COUNTER_READDIR_SUCCESS]+
    vv[COUNTER_STAT_SUCCESS]+vv[COUNTER_OPENDIR_SUCCESS]+
    (isKnownExt(e->key,e->keylen_hash>>HT_KEYLEN_SHIFT)?(1L<<30):0);
}
static int info_counts_by_filetype(int n){
  const int c=ht_count_by_ext.capacity;
  int headerDone=0;
  char describe[3333]; int dc=0;
  bool done[c];
  memset(done,0,sizeof(done));
  FOR(pass,0,c){
    const struct ht_entry *x=NULL;
    struct ht_entry *ee=HT_ENTRIES(&ht_count_by_ext);
    RLOOP(i,c){
      if (!done[i] && ee[i].key && (!x || counter_getattr_rank(ee+i) < counter_getattr_rank(x))) x=ee+i;
    }
    if (!x || !x->value) break;
    RLOOP(isHeader,(!headerDone++?2:1)){
      if (isHeader) PRINTINFO("<HR><H1>Count function calls per file type</H1><TABLE border=\"1\">\n<THEAD>\n");
      PRINTINFO("<TR>\n");
      done[x-ee]=true;
      COLUMN_FILE_TYPE(x->key);
      const int *vv=x->value; // cppcheck-suppress variableScope
      /* see inc_count_by_ext() */
#define G(field) vv[field]
      SF("getattr","Number of calls to this FUSE call-back function success/fail",COUNTER_GETATTR);
      SF("readdir","&#12291;",COUNTER_READDIR);
      SF("opendir","Number of calls to this C library function  success/fail",COUNTER_OPENDIR);
      SF("zip_open","&#12291;",COUNTER_ZIPOPEN);
      SF("lstat","&#12291;",COUNTER_STAT);
#undef G
      END_HR();
    }
  }
  if (headerDone){
    PRINTINFO("</TABLE>\n");
    INFO_TABLE_DESCRIPTION();
    PRINTINFO("<BR>Note: The transient cache (WITH_TRANSIENT_ZIPENTRY_CACHES) aims at reduction of calls to the file API.<BR>\n");
  }
  return n;
}
static int info_print_fuse_argv(int n){
  PRINTINFO("<HR><H1>Fuse Parameters</H1>\n<OL>");
  for(int i=1;i<_fuse_argc;i++) PRINTINFO("<LI>%s</LI>\n",_fuse_argv[i]);
  PRINTINFO("</OL>\n");
  return n;
}


 /***************/
 /* Linux only  */
 /***************/
static int print_proc_status(int n,char *filter,int *val){
  if (n>0) PRINTINFO("<B>/proc/%'ld/status</B>\n<PRE>\n", (long)getpid());
  ASSERT(has_proc_fs());
  char buffer[1024]="";
  FILE* file=fopen("/proc/self/status","r");
  while (fscanf(file,"%1023s",buffer)==1){
    for(char *f=filter;*f;f++){
      const char *t=cg_strchrnul(f,'|');
      if ((f==filter || f[-1]=='|') && !strncmp(buffer,f,t-f)){
        fscanf(file," %d",val);
        if (n>0) PRINTINFO("%s %'d kB\n",buffer,*val);
        break;
      }
    }
  }
  if (n>0) PRINTINFO("</PRE>\n");
  fclose(file);
  return n;
}
static void log_virtual_memory_size(void){
  if (has_proc_fs()){
    int val;print_proc_status(-1,"VmSize:",&val);
    log_msg("Virtual memory size: %'d kB\n",val);
  }
}


static int log_memusage_ht(int n,const bool html){
  PRINTINFO("%sMemory usage of hash maps%s%s\n",BEGIN_H1(html),END_H1(html),BEGIN_PRE(html));
#define M(ht) n+=ht_report_memusage_to_strg(_info_sprint_address(n), _info_sprint_max(n),ht,html)
  M(NULL);
  FOR(i,0,9){
    struct ht *ht=
      IF1(WITH_STAT_CACHE, i==0?&stat_ht:)
      IF1(WITH_ZIPINLINE_CACHE, i==1?&ht_zinline_cache_vpath_to_zippath:)
      IF1(WITH_AUTOGEN,i==2?&ht_fsize:)
      i==3?&ht_intern_fileext:
      i==4?&ht_count_by_ext:
      i==5?&_ht_valid_chars:
      i==6?&ht_inodes_vp:

      NULL;
    if (ht) M(ht);
  }
  foreach_root(r){
    PRINTINFO("\n%sRoot %s%s\n",html?"<B>":ANSI_BOLD,r->rootpath,html?"</B>":ANSI_RESET);
    M(NULL);
    FOR(i,0,12){
#define C(n,ht) i==n?&r->ht:
      struct ht *ht=
        C(0,dircache_queue)
        C(1,dircache_ht)
        C(3,ht_inodes)
        C(4,ht_filetypedata)
#if WITH_DIRCACHE || WITH_STAT_CACHE
        C(7,dircache_ht_fname)
        C(8,dircache_ht_fnamearray)
#endif
        NULL;
#undef C
      if (ht) M(ht);
    }
    PRINTINFO("\n");
    IF1(WITH_DIRCACHE,    n+=mstore_report_memusage_to_strg(_info+n,_info_capacity-n,&r->dircache_mstore));
  }
  PRINTINFO("\n%sMemory storages%s\n",html?"<B>":ANSI_BOLD,html?"</B>":ANSI_RESET);
  n+=mstore_report_memusage_to_strg(_info+n,_info_capacity-n, &mstore_persistent);
  PRINTINFO("\n\n%s\n",END_PRE(html));
  return n;
#undef M
}


static int info_mutex_locks(int n,const bool html){
#if WITH_ASSERT_LOCK
  PRINTINFO("%sMutex lock level%s%s\n",BEGIN_H1(html),END_H1(html),BEGIN_PRE(html));
  FOR(m,0,mutex_len){
    const int c=cg_mutex_count(m,0);
    if (c) PRINTINFO("  %24s %3d\n",MUTEX_S[m],c);
  }
  PRINTINFO("\n%s",END_PRE(html));
#endif //WITH_ASSERT_LOCK
  return n;
}



static int info_malloc(int n,const bool html){
  PRINTINFO("%sGlobal counters%s%s\n",BEGIN_H1(html),END_H1(html),BEGIN_PRE(html));
  int header=0;
  const char *diffMarker=html?"<font color=\"#FF0000\">x</font>":ANSI_FG_RED"X"ANSI_RESET;
  FOR(i,1,COUNT_NUM){
    if (i==COUNT_PAIRS_END){
      header=0;
      continue;
    }
    const long x=_counters1[i],y=i<COUNT_PAIRS_END?_counters2[i]:0;
    const long xB=_countersB1[i],yB=i<COUNT_PAIRS_END?_countersB2[i]:0;
    if (x||y){
      if (i<COUNT_PAIRS_END){
        const char *mark=(x==y||x-y==1&&(i==COUNT_MALLOC_PRELOADFILERAM_TXTBUF||i==COUNT_FHANDLE_CONSTRUCT||i==COUNTm_FHANDLE_ARRAY_MALLOC))?" ":diffMarker;
        if (!header++) PRINTINFO("\n%s%44s %14s %14s %s %20s %20s %s %s\n",html?"<u>":ANSI_UNDERLINE,"ID", "Count","Count release","&#916;","Bytes","Bytes released","&#916;",html?"</u>":ANSI_RESET);
        PRINTINFO("%44s %'14ld %'14ld %s %'20ld %'20ld %s\n", COUNT_S[i], x,y,mark,xB,yB,xB==yB?" ":diffMarker);
      }else{
        if (!header++) PRINTINFO("\n%s%44s %20s%s\n",html?"<u>":ANSI_UNDERLINE,"ID", "Count",html?"</u>":ANSI_RESET);
        PRINTINFO("%44s %'20ld\n",COUNT_S[i],x);
      }
    }
  }
  PRINTINFO("\n%s",END_PRE(html));
  return n;
}




static int info_print_open_files(int n, int *fd_count){
  if (has_proc_fs()){
    PRINTINFO("<HR><H1>All OS-level file handles</H1>\n(Should be almost empty if idle).<BR>");
    if (!has_proc_fs()){
      PRINTINFO(ERROR_MSG_NO_PROC_FS"\n");
    }else{
      static char path[256];
      const struct dirent *dp;
      DIR *dir=opendir("/proc/self/fd/");
      PRINTINFO("<OL>\n");
      for(int i=0;(dp=readdir(dir));i++){
        if (fd_count) (*fd_count)++;
        if (n<0 || atoi(dp->d_name)<4) continue;
        static char proc_path[PATH_MAX+1];
        snprintf(proc_path,PATH_MAX,"/proc/%d/fd/%s",getpid(),dp->d_name);
        const int l=readlink(proc_path,path,255);path[MAX(l,0)]=0;
        if (cg_endsWith(ENDSWITH_FLAG_PRECEED_SLASH,path,0,SPECIAL_FILES[SFILE_LOG_WARNINGS],SPECIAL_FILES_L[SFILE_LOG_WARNINGS])||
            !strncmp(path,"/dev/",5)|| !strncmp(path,"/proc/",6) || !strncmp(path,"pipe:",5)) continue;
        PRINTINFO("<LI>%47s %s</LI>\n",proc_path,path);
      }
      closedir(dir);
      PRINTINFO("</OL>\n");
    }
  }
  return n;
}
static int print_maps(int n){
  if (has_proc_fs()){
    PRINTINFO("<HR><H1>/proc/%ld/maps</H1>\n", (long)getpid());
    if (!has_proc_fs()){
      PRINTINFO(ERROR_MSG_NO_PROC_FS"\n");
      return n;
    }
    size_t total=0;
    FILE *f=fopen("/proc/self/maps","r");
    if(!f) {
      PRINTINFO("/proc/%d/maps: %s\n",getpid(),strerror(errno));
      return n;
    }
    //const int L=333;
    int headerDone=0;
    char describe[999];int dc=0;
    while(!feof(f)) {
      char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX+1]={0};
      unsigned long long begin,end,inode,foo;
      if(fgets(buf,sizeof(buf),f)==0) break;
      *mapname=0;
      const int k=sscanf(buf, "%llx-%llx %4s %llx %5s %llu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
      if (k>=6){
        const int64_t size=end-begin;
        total+=size;
        if (!strchr(mapname,'/') && size>=SIZE_CUTOFF_MMAP_vs_MALLOC){
          RLOOP(isHeader,(!headerDone++?2:1)){
            PRINTINFO("%s<TR>\n",isHeader?"<TABLE border=\"1\">\n<THEAD>\n":"");
            R(true,"Addr&gt;&gt;12", "Address divided by 4096", "%08llx",(LLU)(begin>>12));
            R(true,"Name",           "Name of map",             "%s",mapname);
            R(true,"kB",             "Size of map (kB)",        "%'lld",(LLD)(size>>10));
            //n=repeat_chars_info(n,'-',MIN_int(L,(int)(3*log(size))));
            END_HR();
          }
        }
      }else warning(WARN_FORMAT,buf,"sscanf scanned n=%d",k);
    }
    fclose(f);
    if (headerDone){
      PRINTINFO("<TFOOT><TR><TD></TD><TD></TD><TD>%'lu</TD><TD></TD></TR></TFOOT>\n</TABLE>\n",(unsigned long)(total>>10));
      INFO_TABLE_DESCRIPTION();
    }
  }
  return n;
}


static int info_print_CPU(int n){
  if (has_proc_fs()){
    PRINTINFO("<HR><H1>CPU</H1>\n<PRE>");
    PRINTINFO("Current CPU usage user: %.2f system: %.2f    Also try top -p %d\n",_ucpu_usage,_scpu_usage,getpid());
    PRINTINFO("\n</PRE>\n");
  }
  return n;
}


////////////////////////////////////////////////////////////////////

static int info_print_memory(int n){
  PRINTINFO("<HR><H1>Virtual memory</H1>\n");
  if (has_proc_fs()){
    int val=-1;print_proc_status(-1,"VmSize:",&val);
    //if(rlset)  n=print_bar(n,val*1024/(.01+rl.rlim_cur));
    PRINTINFO("Virt memory: %'d MB<BR>\n",val>>10);
#if ! defined(HAS_RLIMIT) || HAS_RLIMIT
    struct rlimit rl={0}; getrlimit(RLIMIT_AS,&rl);
    const bool rlset=rl.rlim_cur!=-1;
    if(rlset) PRINTINFO(" / rlimit %lld MB<BR>\n",(LLD)(rl.rlim_cur>>20));
#endif
  }

#if IS_LINUX
#include <malloc.h>
#if defined(__GLIBC__) && __GLIBC__>=2 && __GLIBC_MINOR__>=33
#define MALLINFO() mallinfo2()
#else
#define MALLINFO() mallinfo()
#endif
  PRINTINFO("uordblks: %'lld bytes\n",(LLD)MALLINFO().uordblks);
#endif // IS_LINUX
  if (has_proc_fs()){
    int val; n=print_proc_status(n,"VmRSS:|VmHWM:|VmSize:|VmPeak:",&val);
  }
#if ! defined(HAS_RLIMIT) || HAS_RLIMIT
  {
    struct rlimit rl={0};
    getrlimit(RLIMIT_AS,&rl);
    if (rl.rlim_cur!=-1) PRINTINFO("Rlim soft: %lx MB   hard: %lx MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  }
#endif
  return n;
}

static int print_fhandle(int n,const char *title){
  PRINTINFO("<HR><H1>Data associated to file descriptors (struct fHandle)</H1>\n_fhandle_n: %d (Maximum %d)<BR>Table should be empty when idle.<BR>\n<TABLE border=\"1\">\n",n,FHANDLE_MAX);
  char describe[999]; int dc=0;
  const time_t t0=time(NULL);
#define K(x) (((1<<10)-1+x)>>10)
  FOR(ir,-1,_fhandle_n){
    const bool isHeader=ir<0;
    struct fHandle *d=isHeader?NULL:fhandle_at_index(ir);
    if (d && !d->flags) continue;
    PRINTINFO("%s<TR>\n",isHeader?"<THEAD>\n":"");
    R(true,"Path","","%s",!d?"Null":D_VP(d));
    R(true,"Last-access","How many s ago","%'lld s",!d?0:(LLD)(d->accesstime?(t0-d->accesstime):-1));
#if WITH_PRELOADFILERAM
    const struct preloadfileram *m=d?d->preloadfileram:NULL;
    const struct textbuffer *tb=m?m->txtbuf:NULL; // cppcheck-suppress unreadVariable
    R(tb!=NULL,"Cache-ID",        "",                                                     "%05x",!m?0:m->id);
    R(tb!=NULL,"Cache-read-kB",   "Number of KB already read into cache",                 "%'lld",!m?0:(LLD)K(m->preloadfileram_already));
    R(tb!=NULL,"Entry-kB",        "Total size of file content",                           "%'lld",!m?0:(LLD)K(m->preloadfileram_l));
    R(tb!=NULL,"Millisec",        "How long did it take to read file content into cache.","%'lld",!m?0:(LLD)m->preloadfileram_took_mseconds);
#endif //WITH_PRELOADFILERAM
    const bool locked=d && pthread_mutex_trylock(&d->mutex_read);
    if (d && !locked) pthread_mutex_unlock(&d->mutex_read);
    const char *status_descr="<UL><LI>(D)elete indicates that it is marked for closing</LI>"
    "<LI>(K)eep indicates that it can currently not be closed.</li>"
    "<LI>(L)ocked: Cannot be released. Two reasons:"
    "<OL><LI>xmp_read() is currently run</LI><LI>The cache is needed by another file descriptor  with the same virtual path.</LI></OL>"
    "</UL>";
    R(true,"Status",status_descr,"%c%c%c",locked?'L':' ',(d->flags&FHANDLE_FLAG_DESTROY_LATER)?'D':' ', fhandle_currently_reading_writing(d)?'K':' ');
    R(true,"Busy","Number of threads in currently in xmp_read()",  "%d",d->is_busy);
#if WITH_TRANSIENT_ZIPENTRY_CACHES
    const struct ht *ht=d?d->ht_transient_cache:NULL;
    R(ht!=NULL,"Transient-cache","Number of cached paths, count fetched non-existing paths, count fetched existing","%'u %d %d", ht->length, ht->client_value_int[0], ht->client_value_int[1]);
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES
    END_HR();
  }

    PRINTINFO("</TABLE>\n");
    INFO_TABLE_DESCRIPTION();

#undef K
  return n;
}
#define MAKE_INFO_HTML (1<<1)
#define MAKE_INFO_ALL (1<<2)

static int print_all_info(const int flags){
  //log_entered_function("print_all_info _info_capacity=%d \n",_info_capacity);
  int n=0;
  const bool html=flags&MAKE_INFO_HTML;
  if (html){
    PRINTINFO("<!DOCTYPE html><HTML>\n<HEAD>\n\
<TITLE>ZIPsFS File system info</TITLE>\n\
<META http-equiv=\"Cache-Control\" content=\"no-cache,no-store,must-revalidate\"/>\n\
<META http-equiv=\"Pragma\" content=\"no-cache\"/>\n\
<META http-equiv=\"Expires\" content=\"0\"/>\n\
<STYLE>\n\
EM{color:red;}\n\
H1{color:blue;}\n\
TABLE{borborder-color:black;border-style:groove;}\n\
table,th,td{border:1pxsolidblack;border-collapse:collapse;}\n\
TBODY>TR:nth-child(even){background-color:#FFFFaa;}\n\
TBODY>TR:nth-child(odd){background-color:#CCccFF;}\n\
THEAD{background-color:black;color:white;}\n\
TD,TH {padding:5px;padding-right:2EM;}\n\
TD {text-align:right;}\n\
</STYLE>\n</HEAD>\n<BODY>\n\
<B>ZIPsFS:</B> <A href=\"%s\">%s</A><BR> \n",HOMEPAGE,HOMEPAGE);
    PRINTINFO("Compiled: &nbsp; "__DATE__"  "__TIME__"<BR>\nStarted: &nbsp; %s &nbsp; %s &nbsp; PID: %d<BR>\n",ctime(&_whenStarted), snull(this_executable()), getpid());
  }
  if (flags&MAKE_INFO_ALL){
    time_t rawtime;  time(&rawtime);  PRINTINFO("Now: &nbsp; %s<BR>\n",ctime(&rawtime));
    n=log_print_roots(n);
    n=info_print_fuse_argv(n);
    n=info_print_open_files(n,NULL);
    n=info_print_memory(n);
    n=info_print_CPU(n);
    LOCK(mutex_fhandle,n=print_fhandle(n,""));
    //    n=print_maps(n);
    PRINTINFO("<HR><H1>Cache</H1>");
    PRINTINFO("Policy for caching ZIP entries: %s<BR>\n",  IF01(WITH_PRELOADFILERAM,"!WITH_PRELOADFILERAM",WHEN_PRELOADFILERAM_S[_preloadfileram_policy]));
    //  PRINTINFO("_cumul_time_store=%lf\n<BR>",((double)_cumul_time_store)/CLOCKS_PER_SEC);
    LOCK(mutex_fhandle,n=info_counts_by_filetype(n));
    LOCK(mutex_fhandle,n=info_counts_by_filetype_by_root(n));
  }
  // n=log_memusage_ht(n,html);
  n=info_malloc(n,html);
  n=info_mutex_locks(n,html);

  if (html) PRINTINFO("</BODY>\n</HTML>\n");
  if (_info && n<_info_capacity) _info[n]=0;
  return n+1;
}

#if 0 // Currently not needed
static void info_fuse_process(void){
  if (!has_proc_fs()) return;
  const struct fuse_context *fc=fuse_get_context();
  const int BUF=333;
  char buf[BUF+1];
  snprintf(buf,BUF-1,"/proc/%d/cmdline",fc->pid);
  FILE *f=fopen(buf,"r");
  if (!f){
    log_errno("Open %s failed\n",buf);
  }else{
    fscanf(f,"%333s",buf);
    log_msg("PID=%d cmd=%s\n",fc->pid,buf);
    fclose(f);
  }
}
static const char *zip_fdopen_err(int err){
#define  ZIP_FDOPEN_ERR(code,descr) if (err==code) return descr
  ZIP_FDOPEN_ERR(ZIP_ER_INCONS,"Inconsistencies were found in the file specified by path. This error is often caused by specifying ZIP_CHECKCONS but can also happen without it.");
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
static void fhandle_log_cache(const struct fHandle *d){
  if (!d){ log_char('\n');return;}
  ASSERT_LOCKED_FHANDLE();
  const struct preloadfileram *m=d->preloadfileram;
  log_msg("log_cache: d: %p path: %s cache: %s,%s cache_l: %lld/%lld   hasc: %s\n",d,D_VP(d),yes_no(m && m->txtbuf),!m?0:PRELOADFILERAM_STATUS_S[m->preloadfileram_status],(LLD)(!m?-1:m->preloadfileram_already),(LLD)(!m?-1:m->preloadfileram_l),yes_no(m && m->preloadfileram_l>0));
}
#endif //0

#undef R
#undef H
#undef SF
#undef END_HR
#undef BEGIN_H1
#undef END_H1
#undef BEGIN_PRE
#undef END_PRE
