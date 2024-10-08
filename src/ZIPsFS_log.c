/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////
//////////////////////////////////////
// logs per file type
/////////////////////////////////////

//enum memusage{memusage_mmap,memusage_malloc,memusage_n,memusage_get_curr,memusage_get_peak};

static struct ht ht_count_getattr;
static void init_count_getattr(){
  static bool initialized;
  if (!initialized){
    initialized=true;
    ht_set_mutex(mutex_fhdata,ht_init_with_keystore(&ht_count_getattr,11,&mstore_persistent));
  }
}
static void inc_count_getattr(const char *path,enum enum_count_getattr field){
  init_count_getattr();
  if (path && ht_count_getattr.length<1024){
    char *ext=strrchr(path+cg_last_slash(path)+1,'.');
    if (!ext || !*ext) ext="-No extension-";
    struct ht_entry *e=ht_sget_entry(&ht_count_getattr,ext,true);
    assert(e!=NULL);assert(e->key!=NULL);
    int *counts=e->value;
    if (!counts) e->value=counts=mstore_malloc(&mstore_persistent,sizeof(int)*enum_count_getattr_length,8  ,MSTOREID(inc_count_getattr),path);
    if (counts) counts[field]++;
  }
}
//static void inc_count_getattr(const char *path,enum enum_count_getattr field){int *counts=}
//#define FT_INC_PATH(path,field,r) { filetypedata_for_ext(path,r)->field++;r->filetypedata_all.field++;}
static const char *fileExtension(const char *path,const int len){
  if (len){
    const char *ext=strrchr(path+cg_last_slash(path)+1,'.');
    if (ext && ext[1] && /* empty */
        ext!=path && ext[-1]!='/' &&  /* dotted file */
        path+len-ext<9){ /* Extension too long */
      return ext;
    }
  }
  return NULL;
}
static struct rootdata root_dummy;
static counter_rootdata_t *filetypedata_for_ext(const char *path,struct rootdata *r){
  ASSERT_LOCKED_FHDATA();
  assert(r>=_root);
  if (!r) r=&root_dummy;
  if(!r->filetypedata_initialized){
    assert(FILETYPEDATA_FREQUENT_NUM>_FILE_EXT_TO-_FILE_EXT_FROM);
    init_count_getattr();
    r->filetypedata_initialized=true;
    ht_set_mutex(mutex_fhdata,ht_init_with_keystore(&r->ht_filetypedata,10,&mstore_persistent));
    ht_sset(&r->ht_filetypedata, r->filetypedata_all.ext=".*",&r->filetypedata_all);
    ht_sset(&r->ht_filetypedata, r->filetypedata_dummy.ext="*",&r->filetypedata_dummy);
  }
  const int len=cg_strlen(path);
  counter_rootdata_t *d=NULL;
  const char *ext;
  if (len){
    static int return_i,i;
    if ((ext=(char*)config_some_file_path_extensions(path,len,&return_i))){
      d=r->filetypedata_frequent+return_i;
      if (!d->ext) ht_sset(&r->ht_filetypedata, d->ext=ext,d);
    }
    if(!d && (ext=(char*)fileExtension(path,len))){
      struct ht_entry *e=ht_sget_entry(&r->ht_filetypedata,ext, i<FILETYPEDATA_NUM);
      if (!(d=e->value) && i<FILETYPEDATA_NUM  && (d=e->value=r->filetypedata+i++)){
        d->ext=e->key=ext=ht_sinternalize(&ht_intern_fileext,ext);
      }
    }
  }
  return d?d:&r->filetypedata_dummy;
}
static void _rootdata_counter_inc(counter_rootdata_t *c, enum enum_counter_rootdata f){
  if (c->counts[f]<UINT32_MAX) atomic_fetch_add(c->counts+f,1);
}
static void fhdata_counter_inc( struct fhdata* d, enum enum_counter_rootdata f){
  if (d->zpath.root>=0){
    if (!d->filetypedata) d->filetypedata=filetypedata_for_ext(D_VP(d), ROOTd(d));
    _rootdata_counter_inc(d->filetypedata,f);
  }
}
static void rootdata_counter_inc(const char *path, enum enum_counter_rootdata f, struct rootdata* r){
  _rootdata_counter_inc(filetypedata_for_ext(path,r),f);
}

#define TH(x) "<TH>" x "</TH>"
#define THcolspan(n,x) "<TH colspan=\"" #n "\">" x "</TH>"
#define THfail() TH("<EM>&#x26A0;</EM>") // Consider sadsmily&#9785; &#x2639;fail
#define TD(x) "<TD>" x "</TD>"
#define TDl(x) "<TD style=\"text-align:left;\">"x"</TD>"
#define TDc(x) "<TD style=\"text-align:center;\">"x"</TD>"
#define dTDfail(x) "<TD><EM>%'d</EM></TD>"
#define uTDfail(x) "<TD><EM>%'u</EM></TD>"

#define sTDl() TDl("%s")
#define sTDlb() TDl("<B>%s</B>")
#define sTDc(x) TDc("%s")
#define dTD() TD("%'d")
#define uTD() TD("%'u")
#define lTD() TD("%'ld")
#define zTD() TD("%'zu")
static char **_fuse_argv;
static int _fuse_argc;
static void log_path(const char *f,const char *path){
  log_msg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,cg_strlen(path));
}
static void log_fh(char *title,long fh){
  char p[MAX_PATHLEN+1];
  cg_path_for_fd(title,p,fh);
  log_msg("%s  fh: %ld %s \n",title?title:"", fh,p);
}
static int _info_capacity=0, _info_l=0, _info_count_open=0;
static char *_info=NULL;
#ifdef __cppcheck__
#define PRINTINFO(...)   fprintf(stderr,__VA_ARGS__)
#else
#define PRINTINFO(...)   n=printinfo(n,__VA_ARGS__)
#endif
static int printinfo(int n, const char *format,...){
  if (n<_info_capacity && n>=0){
    va_list argptr;
    va_start(argptr,format);
    //fprintf(stderr,"printinfo: _info_capacity=%d n=%d _info=%p \n",_info_capacity,n,_info);
    //vprintf(format,argptr);
    n+=vsnprintf(_info+n,_info_capacity-n,format,argptr);
    va_end(argptr);
  }
  return n;
}
static bool isKnownExt(const char *path, int len){
  static int index_not_needed;
  return config_some_file_path_extensions(path,len?len:strlen(path),&index_not_needed);
}

static int counts_by_filetype_row0(const char *ext,int n){
  if (ext) PRINTINFO("<TR>"TD("%c")sTDlb(),isKnownExt(ext,0)?'+':' ',ext);
  return n;
}
static int counts_by_filetype_r(int n,struct rootdata *r, int *count_explain){
  ASSERT_LOCKED_FHDATA();
  struct ht_entry *ee=r->ht_filetypedata.entries;
  int count=0;
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
          C(00,COUNT_RETRY_STAT)+C(00,COUNT_RETRY_MEMCACHE)+C(00,COUNT_RETRY_ZIP_FREAD)+
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
      if (!count++){
        PRINTINFO("<H1>Counts by file type for root %s</H1>\n",report_rootpath(r));
        if (!count_explain[0]++) PRINTINFO("<B>Columns explained:</B><UL>\n\
  <LI>zip_open(): How often are ZIP entries opened..</LI>\
  <LI>ZIP read no-cache: Count direct read operations for ZIP entries</LI>\n\
  <LI>ZIP read cache: Count loading ZIP entries into RAM. If entirely read, count computation of CRC32 checksum</LI>\n\
  <LI>How often operation succeed, that failed before.</LI>\
  <LI>Num interrupt: When a file pointer is closed before the entire entry is stored in RAM, reading is terminated.</LI>\
<LI>Cumul wait: Threads reading file data need to wait until the cache is filled to the respective position. This is the cumulative waiting time in seconds.</LI>\
</UL>\n");
        PRINTINFO("<TABLE border=\"1\">\n<THEAD><TR>"TH("")TH("Ext")THcolspan(2,"ZIP open") THcolspan(5,"ZIP read no-cache") THcolspan(6,"ZIP read cache") THcolspan(3,"Success when tried again")"</TR>\n\
<TR>"TH("")TH("")  TH("ZIP open &check;")THfail()  TH("Read &check;")THfail()TH("Read 0 bytes") TH("Seek &check;")THfail()  TH("Read &check;")THfail() TH("Checksum match")TH("~ mismatch")TH("Num interrupt")TH("Cumul wait")  TH("lstat()")TH("Cache entry")TH("zip_fread()") "</TR></THEAD>\n");
      }
#define C(field) atomic_load(x->counts+field)
#define SF(field) atomic_load(x->counts+field##_SUCCESS),atomic_load(x->counts+field##_FAIL)
      n=counts_by_filetype_row0(x->ext,n);
      PRINTINFO(uTD()uTDfail() uTD()uTDfail()uTD()uTD()uTDfail() uTD()uTDfail()uTD()uTDfail()uTD()TD("%.2f") uTD()uTD()uTD()"</TR>\n",
                SF(ZIP_OPEN),
                SF(ZIP_READ_NOCACHE),C(ZIP_READ_NOCACHE_ZERO),SF(ZIP_READ_NOCACHE_SEEK),
                SF(ZIP_READ_CACHE),SF(ZIP_READ_CACHE_CRC32),C(COUNT_MEMCACHE_INTERRUPT),((float)x->wait)/CLOCKS_PER_SEC,
                C(COUNT_RETRY_STAT),C(COUNT_RETRY_MEMCACHE), C(COUNT_RETRY_ZIP_FREAD));
      //      ZIP_READ_NOCACHE_SUCCESS
#undef C
#undef SF
    }/*if pass*/
  }/*for pass*/
  if (count) PRINTINFO("</TABLE>\n");
  return n;
}
static long counter_getattr_rank(struct ht_entry *e){
  const int *vv=e->value;
  return !vv?0:
    ((vv[COUNTER_GETATTR_FAIL]+vv[COUNTER_READDIR_FAIL]+vv[COUNTER_ACCESS_FAIL])<<10L)+
    ((vv[COUNTER_STAT_FAIL]+vv[COUNTER_OPENDIR_SUCCESS])<<10L)+
    vv[COUNTER_GETATTR_SUCCESS]+vv[COUNTER_READDIR_SUCCESS]+vv[COUNTER_ACCESS_SUCCESS]+
    vv[COUNTER_STAT_SUCCESS]+vv[COUNTER_OPENDIR_SUCCESS]+
    (isKnownExt(e->key,e->keylen_hash>>HT_KEYLEN_SHIFT)?(1L<<30):0);
}
static int counts_by_filetype(int n){
  const int c=ht_count_getattr.capacity;
  int count_explain=0;
  foreach_root(ir,r) n=counts_by_filetype_r(n,r,&count_explain);
  int count=0;
  bool done[c];
  memset(done,0,sizeof(done));
  FOR(pass,0,c){
    struct ht_entry *x=NULL, *ee=ht_count_getattr.entries;
    RLOOP(i,c){
      if (!done[i] && ee[i].key && (!x || counter_getattr_rank(ee+i) < counter_getattr_rank(x))) x=ee+i;
    }
    if (!x || !x->value) break;
    if (!count++){
#define SF(counter) TH(#counter"() &check;")THfail()
      PRINTINFO("Count calls to getattr() and lstat() per file type:\n\
<B>Columns explained:</B><OL>\n\
<LI>getattr(), readdir(): Number of successful and failed calls to the FUSE call-back function.</LI>\n\
<LI>lstat(): Number of successful and failed calls to the C function. The goal of the cache is to reduce the number of these calls.</LI></OL>\n\
<TABLE border=\"1\">\n<THEAD>\n\
<TR>"TH("")TH("Ext")THcolspan(4,"Count call to FUSE call-back")THcolspan(8,"Count calls to C-libraries")"</TR>\n\
<TR>"TH("")TH("")SF(getattr)SF(readdir)SF(access)SF(lstat)SF(opendir)SF(zip_open)"</TR>\n</THEAD>\n");
#undef SF
    }
    done[x-ht_count_getattr.entries]=true;
    const int *vv=x->value;
    n=counts_by_filetype_row0(x->key,n);
#define C(counter) vv[COUNTER_##counter##_SUCCESS],vv[COUNTER_##counter##_FAIL]
    PRINTINFO(dTD()dTDfail() dTD()dTDfail() dTD()dTDfail() dTD()dTDfail() dTD()dTDfail() dTD()dTDfail()"</TR>\n",C(GETATTR),C(READDIR),C(ACCESS),C(STAT),C(OPENDIR),C(ZIPOPEN));
#undef C
  }
  if (count) PRINTINFO("</TABLE>\n");
  return n;
}
static int log_print_fuse_argv(int n){
  PRINTINFO("<H1>Fuse Parameters</H1>\n<OL>");
  for(int i=1;i<_fuse_argc;i++) PRINTINFO("<LI>%s</LI>\n",_fuse_argv[i]);
  PRINTINFO("</OL>\n");
  return n;
}
static int log_print_roots(int n){
  PRINTINFO("<H1>Roots</H1>\n<TABLE border=\"1\"><THEAD><TR>"TH("Path")TH("Writable")TH("Last response")TH("Blocked")TH("Free[GB]")TH("Dir-Cache[kB]")TH("# entries in stat queue ")"</TR></THEAD>\n");
  foreach_root(i,r){
    const int f=r->features, freeGB=(int)((r->statfs.f_bsize*r->statfs.f_bfree)>>30), last_response=deciSecondsSinceStart()-r->pthread_when_loop_deciSec[PTHREAD_RESPONDING];
    //10L*(last_response>ROOT_OBSERVE_EVERY_MSECONDS_RESPONDING*10/1000*3?last_response:r->statfs_took_deciseconds)
    PRINTINFO("<TR>"sTDl()sTDc(),rootpath(r),yes_no(f&ROOT_WRITABLE));
    if (f&ROOT_REMOTE) PRINTINFO(TD("%'d s ago")TD("%'u times"), last_response/10,r->log_count_delayed);
    else PRINTINFO(TD("Local")TD(""));
    uint64_t u=0; IF1(WITH_DIRCACHE,LOCK(mutex_dircache, u=mstore_usage(DIRCACHE(r))/1024));
    PRINTINFO(dTD()zTD()dTD()"</TR>\n",freeGB,u,IF1(WITH_STAT_SEPARATE_THREADS,debug_statqueue_count_entries(r))IF0(WITH_STAT_SEPARATE_THREADS,0));
  }
  PRINTINFO("</TABLE>\n");
  return n;
}
static int repeat_chars_info(int n,char c, int i){
  if (i>0 && n+i<_info_capacity){
    memset(_info+n,c,i);
    n+=i;
  }
  return n;
}

static int print_bar(int n,float rel){
  const int L=100, a=MAX_int(0,MIN_int(L,(int)(L*rel)));
  PRINTINFO("<SPAN style=\"border-style:solid;\"><SPAN style=\"background-color:red;color:red;\">");
  n=repeat_chars_info(n,'#',a);
  PRINTINFO("</SPAN>\n<SPAN style=\"background-color:blue;color:blue;\">");
  n=repeat_chars_info(n,'~',L-a);
  PRINTINFO("</SPAN>\n</SPAN>\n");
  return n;
}
//////////////////
/// Linux only ///
//////////////////

static int print_proc_status(int n,char *filter,int *val){
  PRINTINFO("<B>/proc/%ld/status</B>\n<PRE>\n", (long)getpid());
  if (!has_proc_fs()){
    PRINTINFO(ERROR_MSG_NO_PROC_FS"\n");
  }else{
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
  }
  return n;
}
static void log_virtual_memory_size(){
  if (has_proc_fs()){
  int val;print_proc_status(-1,"VmSize:",&val);
  log_msg("Virtual memory size: %'d kB\n",val);
  }
}

static int log_print_open_files(int n, int *fd_count){
  if (has_proc_fs()){
  PRINTINFO("<H1>All OS-level file handles</H1>\n(Should be almost empty if idle).<BR>");
  if (!has_proc_fs()){
    PRINTINFO(ERROR_MSG_NO_PROC_FS"\n");
  }else{
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
      if (cg_endsWith(path,0,SPECIAL_FILES[SFILE_LOG_WARNINGS],0)||!strncmp(path,"/dev/",5)|| !strncmp(path,"/proc/",6) || !strncmp(path,"pipe:",5)) continue;
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
  PRINTINFO("<H1>/proc/%ld/maps</H1>\n", (long)getpid());
  if (!has_proc_fs()){
    PRINTINFO(ERROR_MSG_NO_PROC_FS"\n");
  }else{
    size_t total=0;
    FILE *f=fopen("/proc/self/maps","r");
    if(!f) {
      PRINTINFO("/proc/self/maps: %s\n",strerror(errno));
      return n;
    }
    PRINTINFO("<TABLE border=\"1\">\n<THEAD><TR>"TH("Addr&gt;&gt;12")TH("Name")TH("kB")TH("")"</TR></THEAD>\n");
    const int L=333;
    while(!feof(f)) {
      char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX]={0};
      unsigned long long begin,end,inode,foo;
      if(fgets(buf,sizeof(buf),f)==0) break;
      *mapname=0;
      const int k=sscanf(buf, "%llx-%llx %4s %llx %5s %llu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
      if (k>=6){
        const int64_t size=end-begin;
        total+=size;
        if (!strchr(mapname,'/') && size>=SIZE_CUTOFF_MMAP_vs_MALLOC){
          PRINTINFO("<TR>"TD("%08lx")sTDl()""lTD()"<TD>",begin>>12,mapname,size>>10);
          n=repeat_chars_info(n,'-',MIN_int(L,(int)(3*log(size))));
          PRINTINFO("</TD></TR>\n");
        }
      }else warning(WARN_FORMAT,buf,"sscanf scanned n=%d",k);
    }
    fclose(f);
    PRINTINFO("<TFOOT><TR>"TD("")TD("")TD("%'lu")TD("")"</TR></TFOOT>\n</TABLE>\n",total>>10);
  }
  }
  return n;
}


static int log_print_CPU(int n){
  if (has_proc_fs()){
  PRINTINFO("<H1>CPU</H1>\n<PRE>");
  PRINTINFO("Current CPU usage user: %.2f system: %.2f    Also try top -p %d\n",_ucpu_usage,_scpu_usage,getpid());
  PRINTINFO("</PRE>\n");
  }
  return n;
}


////////////////////////////////////////////////////////////////////

static int log_print_memory(int n){
  PRINTINFO("<H1>Virtual memory</H1>\n");
  #if WITH_MEMCACHE
  {
    const uint64_t current=textbuffer_memusage_get(0)+textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP);
    const uint64_t peak=textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_PEAK)+textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP|TEXTBUFFER_MEMUSAGE_PEAK);
    PRINTINFO("<H2>Cache ZIP entries in RAM (WITH_MEMCACHE)</H2>\n<UL>");
    PRINTINFO("<LI>Current anonymous MMAP usage %'ld MB of %'ld MB</LI>\n",current>>20,_memcache_maxbytes>>20); //n=print_bar(n,current/(float)_memcache_maxbytes);
    PRINTINFO("<LI>Peak MMAP usage    %'ld MB of %'ld MB</LI>\n",peak>>20,_memcache_maxbytes>>20);   //n=print_bar(n,peak/(float)_memcache_maxbytes);
    PRINTINFO("</UL>\n");
  }
#endif //WITH_MEMCACHE
  if (has_proc_fs()){
    struct rlimit rl={0}; getrlimit(RLIMIT_AS,&rl);
    const bool rlset=rl.rlim_cur!=-1;
    int val=-1;print_proc_status(-1,"VmSize:",&val);
    //if(rlset)  n=print_bar(n,val*1024/(.01+rl.rlim_cur));
    PRINTINFO("Virt memory: %'d MB<BR>\n",val>>10);
    if(rlset) PRINTINFO(" / rlimit %zu MB<BR>\n",rl.rlim_cur>>20);
  }

#if IS_LINUX
#include <malloc.h>
#if defined(__GLIBC__) && __GLIBC__>=2 && __GLIBC_MINOR__>=33
#define MALLINFO() mallinfo2()
#else
#define MALLINFO() mallinfo()
#endif
  PRINTINFO("uordblks: %'jd bytes (Total allocated heap space)\n",(intmax_t)MALLINFO().uordblks);
#endif // IS_LINUX
  if (has_proc_fs()){
    int val; n=print_proc_status(n,"VmRSS:|VmHWM:|VmSize:|VmPeak:",&val);
  }
  {
    struct rlimit rl={0};
    getrlimit(RLIMIT_AS,&rl);
    if (rl.rlim_cur!=-1) PRINTINFO("Rlim soft: %'lx MB   hard: %'lx MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  }
  PRINTINFO("Number of calls to mmap: %'jd to munmap: %'jd<BR>\n",(intmax_t)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP|TEXTBUFFER_MEMUSAGE_COUNT_ALLOC),(intmax_t)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP|TEXTBUFFER_MEMUSAGE_COUNT_FREE));
  PRINTINFO("Number of calls to malloc: %'jd to free: %'jd<BR>\n",(intmax_t)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_COUNT_ALLOC),(intmax_t)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_COUNT_FREE));
  return n;
}


static long _count_readzip_memcache=0,_count_readzip_memcache_because_seek_bwd=0,_log_read_max_size=0,_count_SeqInode=0;
static int log_print_statistics_of_read(int n){
  PRINTINFO("Count events related to cache:\n<TABLE border=\"1\"><THEAD><TR>"TH("Event")TH("Count")"</TR></THEAD>\n");
#define TABLEROW(a) { const char *us=strchr(#a,'_'); PRINTINFO("<TR>"sTDlb()lTD()"</TR>\n",us?us+1:"Null",a);}
  TABLEROW(_count_readzip_memcache);
  TABLEROW(_count_readzip_memcache_because_seek_bwd);
  TABLEROW(_count_stat_from_cache);
  TABLEROW(_log_read_max_size);
  TABLEROW(_log_count_pthread_lock);// 30 ns per lock
  PRINTINFO("</TABLE>");

#undef TABLEROW
  return n;
}

static size_t _kilo(size_t x){
  return ((1<<10)-1+x)>>10;
}
static int print_fhdata(int n,const char *title){
  PRINTINFO("<H1>Data associated to file descriptors (struct fhdata)</H1>\n_fhdata_n: %d (Maximum %d)<BR>\n",n,FHDATA_MAX);
  if (!_fhdata_n){
    PRINTINFO("The cache is empty which is good. It means that no entry is locked or leaked.<BR>\n");
  }else{
    PRINTINFO("Table should be empty when idle.<BR>Column <I><B>fd</B></I> file descriptor. Column <I><B>Last access</B></I>:    Column <I><B>Millisec</B></I>:  time to read entry en bloc into cache.  Column <I><B>R</B></I>: number of threads in currently in xmp_read().\
              Column <I><B>F</B></I>: flags. (D)elete indicates that it is marked for closing and (K)eep indicates that it can currently not be closed. Two possible reasons why data cannot be released: (I)  xmp_read() is currently run (II) the cached zip entry is needed by another file descriptor  with the same virtual path. Column <I><B>Tansient cache: Hex address, number of cached paths, count fetched non-existing paths, count fetched existing</B><BR>\n\
              <TABLE border=\"1\">\n<THEAD><TR>"TH("Path")TH("Last access")IF1(WITH_MEMCACHE,TH("Cache ID")TH("Cache read kB")TH("Entry kB")TH("Millisec"))TH(" F")TH("R")IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,TH("Transient cache"))"</TR></THEAD>\n");
    const time_t t0=time(NULL);
    foreach_fhdata(id,d){
      PRINTINFO("<TR>"sTDl(),snull(D_VP(d)));
      if (d->accesstime) PRINTINFO(TD("%'ld s"),t0-d->accesstime); else PRINTINFO(TD("Never"));
      IF1(WITH_MEMCACHE, struct memcache *m=d->memcache; if (m && m->memcache2) PRINTINFO(TD("%05x")zTD()zTD()lTD(),m->id,_kilo(m->memcache_already),_kilo(m->memcache_l),m->memcache_took_mseconds); else PRINTINFO(TD("")TD("")TD("")TD("")));

      PRINTINFO(TD("%c%c")dTD(),(d->flags&FHDATA_FLAGS_DESTROY_LATER)?'D':' ', fhdata_can_destroy(d)?' ':'K',d->is_xmp_read);
      IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,const struct ht *ht=d->ht_transient_cache;PRINTINFO(TD("%0lx %d %d %d"),(long)ht,!ht?-1: (int)ht->length, !ht?-1:ht->client_value_int[0],!ht?-1:ht->client_value_int[1]));
      PRINTINFO("</TR>\n");
    }
    PRINTINFO("</TABLE>");
  }
  return n;
}

static int print_all_info(){
  //log_entered_function("print_all_info _info_capacity=%d \n",_info_capacity);
  int n=0;
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
  PRINTINFO("Compiled: &nbsp; "__DATE__"  "__TIME__"<BR>\nStarted: &nbsp; %s &nbsp; %s &nbsp; PID: %d<BR>\n",ctime(&_startTime.tv_sec), _thisPrg, getpid());
  time_t rawtime;  time(&rawtime);  PRINTINFO("Now: &nbsp; %s<BR>\n",ctime(&rawtime));
  n=log_print_roots(n);
  n=log_print_fuse_argv(n);
  n=log_print_open_files(n,NULL);
  n=log_print_memory(n);
  n=log_print_CPU(n);
  LOCK(mutex_fhdata,n=print_fhdata(n,""));
  PRINTINFO("<H1>Inodes</H1>\nCreated sequentially: %'ld\n",_count_SeqInode);
  //n=print_maps(n);
  PRINTINFO("<H1>Cache</H1>");
  PRINTINFO("Policy for caching ZIP entries: %s<BR>\n",  IF1(WITH_MEMCACHE,WHEN_MEMCACHE_S[_memcache_policy])IF0(WITH_MEMCACHE,"!WITH_MEMCACHE"));
  n=log_print_statistics_of_read(n);
  //  PRINTINFO("_cumul_time_store=%lf\n<BR>",((double)_cumul_time_store)/CLOCKS_PER_SEC);
  LOCK(mutex_fhdata,n=counts_by_filetype(n));
  PRINTINFO("</BODY>\n</HTML>\n\0\0\0");
  return n;
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
#if WITH_MEMCACHE
static void fhdata_log_cache(const struct fhdata *d){
  if (!d){ log_char('\n');return;}
  struct memcache *m=d->memcache;
  log_msg("log_cache: d: %p path: %s cache: %s,%s cache_l: %jd/%jd   hasc: %s\n",d,D_VP(d),yes_no(m && m->memcache2),!m?0:MEMCACHE_STATUS_S[m->memcache_status],(intmax_t)(!m?-1:m->memcache_already),(intmax_t)(!m?-1:m->memcache_l),yes_no(m && m->memcache_l>0));
}
#endif //WITH_MEMCACHE
static void _log_zpath(const char *fn,const int line,const char *msg, struct zippath *zpath){
  log_msg(ANSI_INVERSE"%s():%d %s"ANSI_RESET,fn,line,msg);
  if (!zpath){
    log_strg("zpath is NULL\n");
    return;
  }
  log_msg("   this= %p   \n",zpath);
  log_msg("    %p strgs="ANSI_FG_BLUE"%d\n"ANSI_RESET   ,zpath->strgs, zpath->strgs_l);
  log_msg("    %p    VP="ANSI_FG_BLUE"'%s' VP_LEN: %d"ANSI_RESET,VP(),snull(VP()),VP_L()); cg_log_file_stat("",&zpath->stat_vp);
  log_msg("    %p   VP0="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,VP0(),  snull(VP0()));
  log_msg("    %p entry="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,EP(), snull(EP()));
  log_msg("    %p    RP="ANSI_FG_BLUE"'%s' "ANSI_RESET,RP(), snull(RP())); cg_log_file_stat("",&zpath->stat_rp);
  log_msg("    %p  root="ANSI_FG_BLUE"'%s'\n"ANSI_RESET,zpath->root,rootpath(zpath->root));
#define C(f) ((zpath->flags&f)?#f:"")
  log_msg("       flags="ANSI_FG_BLUE"%s %s %s %s %s\n"ANSI_RESET,C(ZP_ZIP),C(ZP_DOES_NOT_EXIST),C(ZP_IS_COMPRESSED),C(ZP_VERBOSE),C(ZP_STARTS_AUTOGEN));
#undef C
}
#define log_zpath(...) _log_zpath(__func__,__LINE__,__VA_ARGS__) /*TO_HEADER*/

#define FHDATA_ALREADY_LOGGED_VIA_ZIP (1<<0)
#define FHDATA_ALREADY_LOGGED_FAILED_DIFF (1<<1)
static bool fhdata_not_yet_logged(unsigned int flag,struct fhdata *d){
  if (d->already_logged&flag) return false;
  d->already_logged|=flag;
  return true;
}
static void log_fuse_process(){
  if (!has_proc_fs()) return;

  struct fuse_context *fc=fuse_get_context();
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
static void usage(){
  fputs(ANSI_INVERSE"ZIPsFS"ANSI_RESET"\n\nCompiled: "__DATE__"  "__TIME__"\n\n",stderr);
  fputs(ANSI_UNDERLINE"Usage:"ANSI_RESET"  ZIPsFS [options]  root-dir1 root-dir2 ... root-dir-n : [libfuse-options]  mount-Point\n\n"
        ANSI_UNDERLINE"Example:"ANSI_RESET"  ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable  ~/local/folder //computer/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt\n\n\
The first root directory (here  ~/tmp/ZIPsFS/writable)  is writable and allows file creation and deletion, the others are read-only.\n\
Root directories starting with double slash (here //computer/pub) are regarded as less reliable and potentially blocking.\n\n\
Options and arguments before the colon are interpreted by ZIPsFS.  Those after the colon are passed to fuse_main(...).\n\n",stderr);
    fputs(ANSI_UNDERLINE"ZIPsFS options:"ANSI_RESET"\n\n\
      -h Print usage.\n\
      -q quiet, no debug messages to stdout\n\
         Without the option -f (foreground) all logs are suppressed anyway.\n\
      -c  ",stderr);
  //  for(char **s=WHEN_MEMCACHE_S;*s;s++){if (s!=WHEN_MEMCACHE_S) putchar('|');fputs(*s,stderr);}
  IF1(WITH_MEMCACHE,for(int i=0;WHEN_MEMCACHE_S[i];i++){ if (i) putc('|',stderr);fputs(WHEN_MEMCACHE_S[i],stderr);});
  fputs("    When to read zip entries into RAM\n\
         seek: When the data is not read continuously\n\
         rule: According to rules based on the file name encoded in configuration.h\n\
         The default is when backward seeking would be requires otherwise\n\
      -k Kill on error. For debugging only.\n\
      -l Limit memory usage for caching ZIP entries.\n\
         Without caching, moving read positions backwards for an non-seek-able ZIP-stream would require closing, reopening and skipping.\n\
         To avoid this, the limit is multiplied with factor 1.5 in such cases.\n\n",stderr);
  fputs(ANSI_UNDERLINE"Fuse options:"ANSI_RESET"These options are given after a colon.\n\n\
 -d Debug information\n\
 -f File system should not detach from the controlling terminal and run in the foreground.\n\
 -s Single threaded mode. Maybe tried in case the default mode does not work properly.\n\
 -h Print usage information.\n\
 -V Version\n\n",stderr);
}
