/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////

// cppcheck-suppress-file unusedFunction
/////////////////////////////////////////////////////////////////////////////////
/// The file _log_flags contains a decimal number specifying what is logged.  ///
/////////////////////////////////////////////////////////////////////////////////

static void log_flags_update(){
  static char path[MAX_PATHLEN+1]={0};
  if (!*path){
#define P PATH_DOT_ZIPSFS"/log_flags.conf"
#define R P".readme"
    cg_copy_path(path,P);
    FILE *f;
    if (!cg_file_exists(path) && (f=fopen(path,"w"))) fclose(f);
    char tmp[MAX_PATHLEN+1];
    if ((f=fopen(cg_copy_path(tmp,R),"w"))){
      fprintf(f,
              "# The file defines variables and a shell script to specify additional logs for ZIPsFS.\n"
              "# The log messages go exclusively to stderr.\n"
              "# Changes take immediate effect\n#\n"
              "# BITS:\n#\n");
      FOR(i,1,LOG_FLAG_LENGTH) fprintf(f,"%s=%d\n",LOG_FLAG_S[i],i);
      fprintf(f,"activate_log(){\n  local flags=0 f\n  for f in $*; do ((flags|=(1<<f))); done\n  echo $flags |tee "P"\n}\n"
              "#\n# To source this script,  run\n#\n#    source "R
              "\n#\n# Usage example:\n#\n"
              "#     activate_log $%s $%s \n",LOG_FLAG_S[LOG_FUSE_METHODS_ENTER],LOG_FLAG_S[LOG_FUSE_METHODS_EXIT]);
      fclose(f);
    }
  }
#undef P
#undef R
  static struct stat st;
  static struct timespec t;
  if (!stat(path,&st) && !CG_TIMESPEC_EQ(t,st.ST_MTIMESPEC)){
    t=st.ST_MTIMESPEC;
    FILE *f=fopen(path,"r");
    if (f){
      fscanf(f," ");
      fscanf(f,"%d",&_log_flags);
      fclose(f);
      log_verbose("Log-flags: "ANSI_FG_BLUE"%x"ANSI_RESET"   from  '%s'",_log_flags,path);
    }else{
      warning(WARN_FLAG_ERRNO|WARN_FLAG_ERROR,path,"Reading log-flags");
    }
  }
}





//////////////////////////////////////
// logs per file type
/////////////////////////////////////

//enum memusage{memusage_mmap,memusage_malloc,memusage_n,memusage_get_curr,memusage_get_peak};
static void init_count_getattr(void){
  static bool initialized;
  if (!initialized){
    initialized=true;
    ht_set_mutex(mutex_fhandle,ht_init_with_keystore(&ht_count_getattr,11,&mstore_persistent)); ht_set_id(HT_MALLOC_ht_count_getattr,&ht_count_getattr);
  }
}
static void inc_count_getattr(const char *path,enum enum_count_getattr field){
  init_count_getattr();
  if (path && ht_count_getattr.length<1024){
    const char *ext=strrchr(path+cg_last_slash(path)+1,'.');
    if (!ext || !*ext) ext="-No extension-";
    struct ht_entry *e=ht_sget_entry(&ht_count_getattr,ext,true);
    assert(e!=NULL);assert(e->key!=NULL);
    int *counts=e->value;
    if (!counts) e->value=counts=mstore_malloc(&mstore_persistent,sizeof(int)*enum_count_getattr_length,8);
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
  ASSERT_LOCKED_FHANDLE();
  assert(!r || r>=_root);
  if (!r) r=&root_dummy;
  if(!r->filetypedata_initialized){
    assert(FILETYPEDATA_FREQUENT_NUM>_FILE_EXT_TO-_FILE_EXT_FROM);
    init_count_getattr();
    r->filetypedata_initialized=true;
    ht_set_mutex(mutex_fhandle,ht_init_with_keystore(&r->ht_filetypedata,10,&mstore_persistent)); ht_set_id(HT_MALLOC_file_ext,&r->ht_filetypedata);
    ht_sset(&r->ht_filetypedata, r->filetypedata_all.ext=".*",&r->filetypedata_all);
    ht_sset(&r->ht_filetypedata, r->filetypedata_dummy.ext="*",&r->filetypedata_dummy);
  }
  const int len=cg_strlen(path);
  counter_rootdata_t *d=NULL;
  const char *ext;
  if (len){
    static int return_i;
    if ((ext=(char*)config_some_file_path_extensions(path,len,&return_i))){
      d=r->filetypedata_frequent+return_i;
      if (!d->ext) ht_sset(&r->ht_filetypedata, d->ext=ext,d);
    }
    if(!d && (ext=(char*)fileExtension(path,len))){
      static int i;
      struct ht_entry *e=ht_sget_entry(&r->ht_filetypedata,ext, i<FILETYPEDATA_NUM);
      if (!(d=e->value) && i<FILETYPEDATA_NUM  && (d=e->value=r->filetypedata+i++)){
        d->ext=e->key=ht_sinternalize(&ht_intern_fileext,ext);
      }
    }
  }
  return d?d:&r->filetypedata_dummy;
}
static void _rootdata_counter_inc(counter_rootdata_t *c, enum enum_counter_rootdata f){
  if (c->counts[f]<UINT32_MAX) atomic_fetch_add(c->counts+f,1);
}
static void fhandle_counter_inc( struct fHandle* d, enum enum_counter_rootdata f){
  if (!d->filetypedata) d->filetypedata=filetypedata_for_ext(D_VP(d),d->zpath.root);
  _rootdata_counter_inc(d->filetypedata,f);

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
#define lTD() TD("%'lld")
#define luTD() TD("%'llu")

static const char *_fuse_argv[99]={0};
static int _fuse_argc;
static void log_path(const char *f,const char *path){
  log_msg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,cg_strlen(path));
}
static void log_fh(const char *title,const long fh){
  char p[MAX_PATHLEN+1];
  cg_path_for_fd(title,p,fh);
  log_msg("%s  fh: %ld %s \n",title?title:"", fh,p);
}
static int _info_capacity=0;
static char *_info=NULL;

#define xxxxxxxxPRINTINFO(...)   { if (_info && n>=0 && _info_capacity>n) n+=snprintf(_info+n,_info_capacity-n,__VA_ARGS__);}

#define PRINTINFO(...)   n+=snprintf(_info_sprint_address(n),_info_sprint_max(n),__VA_ARGS__);

#if WITH_MEMCACHE
static char *_info_sprint_address(const int n){
  return _info && n>=0 && _info_capacity>n?_info+n:NULL;
}
static int _info_sprint_max(const int n){
  return _info && n>=0 && _info_capacity>n?_info_capacity-n:0;
}
#else
#define _info_sprint_max(n) 0
#define _info_sprint_address(n) NULL
#endif //WITH_MEMCACHE


#define BEGIN_H1(is_html) (is_html?"<H1>":ANSI_INVERSE)
#define END_H1(is_html) (is_html?"<H1>":ANSI_RESET)
#define BEGIN_B(is_html) (is_html?"<B>":ANSI_BOLD)
#define END_B(is_html) (is_html?"<B>":ANSI_RESET)
#define BEGIN_PRE(is_html) (is_html?"<PRE>":"")
#define END_PRE(is_html) (is_html?"</PRE>":"")

static bool isKnownExt(const char *path, int len){
  static int index_not_needed;
  return config_some_file_path_extensions(path,len?len:strlen(path),&index_not_needed);
}

static int counts_by_filetype_row0(const char *ext,int n){
  if (ext) PRINTINFO("<TR>"TD("%c")sTDlb(),isKnownExt(ext,0)?'+':' ',ext);
  return n;
}
static int counts_by_filetype_r(int n,struct rootdata *r, int *count_explain){
  ASSERT_LOCKED_FHANDLE();
  struct ht_entry *ee=HT_ENTRIES(&r->ht_filetypedata);
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
          C(00,COUNT_RETRY_STAT)+C(00,COUNT_RETRY_MEMCACHE)+
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
        PRINTINFO("<TABLE border=\"1\">\n<THEAD><TR>"TH("")TH("Ext")THcolspan(2,"ZIP open") THcolspan(5,"ZIP read no-cache") THcolspan(6,"ZIP read cache") THcolspan(2,"Success when tried again")"</TR>\n\
<TR>"TH("")TH("")  TH("ZIP open &check;")THfail()  TH("Read &check;")THfail()TH("Read 0 bytes") TH("Seek &check;")THfail()  TH("Read &check;")THfail() TH("Checksum match")TH("~ mismatch")TH("Cumul wait")  TH("lstat()")TH("Cache entry")TH("zip_fread()") "</TR></THEAD>\n");
      }
#define C(field) atomic_load(x->counts+field)
#define SF(field) atomic_load(x->counts+field##_SUCCESS),atomic_load(x->counts+field##_FAIL)
      n=counts_by_filetype_row0(x->ext,n);
      PRINTINFO(uTD()uTDfail() uTD()uTDfail()uTD()uTD()uTDfail() uTD()uTDfail()uTD()uTDfail()TD("%.2f") uTD()uTD()"</TR>\n",
                SF(ZIP_OPEN),
                SF(ZIP_READ_NOCACHE),C(ZIP_READ_NOCACHE_ZERO),SF(ZIP_READ_NOCACHE_SEEK),
                SF(ZIP_READ_CACHE),SF(ZIP_READ_CACHE_CRC32),((float)x->wait)/CLOCKS_PER_SEC,
                C(COUNT_RETRY_STAT),C(COUNT_RETRY_MEMCACHE));
      //      ZIP_READ_NOCACHE_SUCCESS
#undef C
#undef SF
    }/*if pass*/
  }/*for pass*/
  if (count) PRINTINFO("</TABLE>\n");
  return n;
}
static long counter_getattr_rank(const struct ht_entry *e){
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
  foreach_root(r) n=counts_by_filetype_r(n,r,&count_explain);
  int count=0;
  bool done[c];
  memset(done,0,sizeof(done));
  FOR(pass,0,c){
    const struct ht_entry *x=NULL;
    struct ht_entry *ee=HT_ENTRIES(&ht_count_getattr);
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
    done[x-ee]=true;
    n=counts_by_filetype_row0(x->key,n);
    const int *vv=x->value; // cppcheck-suppress variableScope
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

#define U(code) cg_unicode(unicode,code,n!=0)
#define P() if (n) {PRINTINFO("%s",unicode);} else fputs(unicode,stderr)
static int table_draw_horizontal(int n, const int type, const int nColumn, const int *width){ /* n==0 for UNIX console with UTF8 else for HTML with unicode */
  static const int ccc[4][4]={
    {BD_HEAVY_DOWN_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_DOWN_AND_HORIZONTAL,BD_HEAVY_DOWN_AND_LEFT}, /* above header */
    {BD_HEAVY_VERTICAL_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_VERTICAL_AND_HORIZONTAL,BD_HEAVY_VERTICAL_AND_LEFT}, /* below header */
    {BD_VERTICAL_HEAVY_AND_RIGHT_LIGHT,BD_LIGHT_HORIZONTAL,BD_VERTICAL_HEAVY_AND_HORIZONTAL_LIGHT,BD_VERTICAL_HEAVY_AND_LEFT_LIGHT}, /* body */
    {BD_HEAVY_UP_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_UP_AND_HORIZONTAL,BD_HEAVY_UP_AND_LEFT}}; /*bottom */ // cppcheck-suppress constVariable
  const int *cc=ccc[type];
  char unicode[9];
  U(cc[0]); P();
  FOR(ic,0,nColumn){
    U(cc[1]); RLOOP(iTimes,width[ic]+2) P();
    U(cc[ic<nColumn-1?2:3]); P();
  }
  if (n){ PRINTINFO("\n"); } else fputc('\n',stderr);
  return n;
}
#undef P
#undef U
static int log_print_roots(int n){ /* n==0 for UNIX console with UTF8 else for HTML with unicode */
  if (n) PRINTINFO("<H1>Roots</H1><PRE>\n\n"); /* if n==0 then output to UNIX console */
#define C(title,format,...) posOld=pos;\
  pos+=r?S(format,__VA_ARGS__):S("%s",title);\
  spaces=width[col]-pos+posOld;\
  if (print){ memset(line+pos,' ',spaces); pos+=spaces; pos+=S(" %s ",colsep);} else width[col]=MAX_int(width[col],pos-posOld);\
  col++
#define S(...) snprintf(line+pos,sizeof(line)-pos,__VA_ARGS__)
  int width[33]={0};
  char line[1024], colsep[9];
  cg_unicode(colsep,BD_HEAVY_VERTICAL,n!=0);
  FOR(print,0,2){
    FOR(ir,-1,_root_n){
      char tmp[MAX_PATHLEN+1];
      struct rootdata *r=ir<0?NULL:_root+ir;
      if (!r && !n) fputs(ANSI_BOLD,stderr);
      int posOld=0,col=0,spaces, pos=cg_unicode(line,BD_HEAVY_VERTICAL,n!=0);
      line[pos++]=' ';
      C("No","%2d",1+rootindex(r));
      C("Path","%s",r->rootpath);
      if (r) sprintf(tmp,"%s%s%s%s",r->remote?"R ":"",r->writable?"W ":"",r->with_timeout?"T ":"",r->blocked?"B ":"");
      C("Features","%s",tmp);
      C("Retained directory","%s",r->retain_dirname);
      if (r){ if (*r->rootpath_mountpoint) sprintf(tmp,"(%d) %s",1+r->seq_fsid,r->rootpath_mountpoint); else sprintf(tmp,"(%d) %16lx",1+r->seq_fsid,r->f_fsid);}
      C("Filesystem","%s",tmp);
      C("Free [GB]","%'9ld",((r->statvfs.f_frsize*r->statvfs.f_bfree)>>30));
      if (n){
        if (r){ if (ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC)) sprintf(tmp,"%'ld seconds ago",ROOT_SUCCESS_SECONDS_AGO(r)); else strcpy(tmp,"Never");}
        C("Last response","%s", tmp);
        C("Blocked","%'u times",r->log_count_delayed);
      }
      if (!print) continue;
      if (!r) n=table_draw_horizontal(n,0,col,width);
      line[sizeof(line)-1]=0;
      if (n){ PRINTINFO("%s\n",line);} else fprintf(stderr,"%s"ANSI_RESET"\n",line);
      n=table_draw_horizontal(n,ir==-1?1:ir<_root_n-1?2:3,col,width);
    }
  }
  static const char *info="Explain table:\n"
    "    Retained directory: Without trailing slash in provided path, the last path-component will be part of the virtual path.\n"
    "                        This is consistent with the trailing slash semantics of UNIX tools like rsync, scp and cp.\n\n"
    "    Feature flags: W=Writable (First path)   R=Remote (Path starts with two slashes)     T=Supports timeout (Path starts with three slashes)";
  if (n){ PRINTINFO("%s   B=Blocked (frozen)\n</PRE>",info);} else fprintf(stderr,ANSI_FG_GRAY"%s"ANSI_RESET"\n\n",info);
  return n;
#undef C
#undef S
}
static int repeat_chars_info(int n,char c, int i){
  if (i>0 && n+i<_info_capacity)  memset(_info+n,c,i);
  return n+=i;
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
  if (n>0) PRINTINFO("<B>/proc/%'ld/status</B>\n<PRE>\n", (long)getpid());
  ASSERT(has_proc_fs());
  char buffer[1024]="";
  FILE* file=fopen("/proc/self/status","r");
  while (fscanf(file,"%1023s",buffer)==1){
    for(char *f=filter;*f;f++){
      char *t=strchrnul(f,'|');
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
#define R(ht) n+=ht_report_memusage_to_strg(_info_sprint_address(n), _info_sprint_max(n),ht,html)
  R(NULL);
  FOR(i,0,9){
    struct ht *ht=
      IF1(WITH_STAT_CACHE, i==0?&stat_ht:)
      IF1(WITH_ZIPINLINE_CACHE, i==1?&ht_zinline_cache_vpath_to_zippath:)
      IF1(WITH_AUTOGEN,i==2?&ht_fsize:)
      i==3?&ht_intern_fileext:
      i==4?&ht_count_getattr:
      i==5?&_ht_valid_chars:
      i==6?&ht_inodes_vp:

      NULL;
    if (ht) R(ht);
  }
  foreach_root(r){
    PRINTINFO("\n%sRoot %s%s\n",BEGIN_B(html),r->rootpath,END_B(html));
    R(NULL);
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
      if (ht) R(ht);
    }
    PRINTINFO("\n");
    IF1(WITH_DIRCACHE,    n+=mstore_report_memusage_to_strg(_info+n,_info_capacity-n,&r->dircache_mstore));
  }
  PRINTINFO("\n%sMemory storages%s\n",BEGIN_B(html),END_B(html));
  n+=mstore_report_memusage_to_strg(_info+n,_info_capacity-n, &mstore_persistent);
  PRINTINFO("\n\n%s",END_PRE(html));
  PRINTINFO("\n");
  return n;
#undef R
}


static int log_mutex_locks(int n,const bool html){
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



static int log_malloc(int n,const bool html){
  PRINTINFO("%sGlobal counters%s%s\n",BEGIN_H1(html),END_H1(html),BEGIN_PRE(html));
  int header=0;
  FOR(i,1,COUNT_NUM){
    if (i==COUNT_PAIRS_END){
      header=0;
      continue;
    }
    const long x=_counters1[i],y=i<COUNT_PAIRS_END?_counters2[i]:0;
    if (x||y){
      if (i<COUNT_PAIRS_END){
        if (!header++)     PRINTINFO("\n%s%44s %20s %20s %20s%s\n",html?"<u>":ANSI_UNDERLINE,"ID", "Count","Count release","Diff",html?"</u>":ANSI_RESET);
        PRINTINFO("%s%44s %'20ld %'20ld %'20ld%s\n", html?"":x==0&&y==0?ANSI_FG_GRAY:x==y?ANSI_FG_GREEN:ANSI_FG_RED, COUNT_S[i], x,y,x-y,html?"":ANSI_RESET);
      }else{
        if (!header++)     PRINTINFO("\n%s%44s %20s%s\n",html?"<u>":ANSI_UNDERLINE,"ID", "Count",html?"</u>":ANSI_RESET);
        PRINTINFO("%44s %'20ld\n",COUNT_S[i],x);
      }
    }
  }
  PRINTINFO("\n%s",END_PRE(html));
  return n;
}




static int log_print_open_files(int n, int *fd_count){
  if (has_proc_fs()){
    PRINTINFO("<H1>All OS-level file handles</H1>\n(Should be almost empty if idle).<BR>");
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
        snprintf(proc_path,PATH_MAX,"/proc/self/fd/%s",dp->d_name);
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
        char buf[PATH_MAX+100],perm[5],dev[6],mapname[PATH_MAX+1]={0};
        unsigned long long begin,end,inode,foo;
        if(fgets(buf,sizeof(buf),f)==0) break;
        *mapname=0;
        const int k=sscanf(buf, "%llx-%llx %4s %llx %5s %llu %100s",&begin,&end,perm,&foo,dev, &inode,mapname);
        if (k>=6){
          const int64_t size=end-begin;
          total+=size;
          if (!strchr(mapname,'/') && size>=SIZE_CUTOFF_MMAP_vs_MALLOC){
            PRINTINFO("<TR>"TD("%08llx")sTDl()""lTD()"<TD>",(LLU)(begin>>12),mapname,(LLD)(size>>10));
            n=repeat_chars_info(n,'-',MIN_int(L,(int)(3*log(size))));
            PRINTINFO("</TD></TR>\n");
          }
        }else warning(WARN_FORMAT,buf,"sscanf scanned n=%d",k);
      }
      fclose(f);
      PRINTINFO("<TFOOT><TR>"TD("")TD("")TD("%'lu")TD("")"</TR></TFOOT>\n</TABLE>\n",(unsigned long)(total>>10));
    }
  }
  return n;
}


static int log_print_CPU(int n){
  if (has_proc_fs()){
    PRINTINFO("<H1>CPU</H1>\n<PRE>");
    PRINTINFO("Current CPU usage user: %.2f system: %.2f    Also try top -p %d\n",_ucpu_usage,_scpu_usage,getpid());
    PRINTINFO("\n</PRE>\n");
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
    PRINTINFO("<LI>Current anonymous MMAP usage %'lld MB of %'lld MB</LI>\n",(LLD)(current>>20),(LLD)(_memcache_bytes_limit>>20)); //n=print_bar(n,current/(float)_memcache_bytes_limit);
    PRINTINFO("<LI>Peak MMAP usage    %'lld MB of %'lld MB</LI>\n",(LLD)(peak>>20),(LLD)(_memcache_bytes_limit>>20));   //n=print_bar(n,peak/(float)_memcache_bytes_limit);
    PRINTINFO("</UL>\n");
  }
#endif //WITH_MEMCACHE
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
    if (rl.rlim_cur!=-1) PRINTINFO("Rlim soft: %'lx MB   hard: %'lx MB\n",rl.rlim_cur>>20,rl.rlim_max>>20);
  }
#endif
  PRINTINFO("Textbuffer - Number of calls to mmap: %'lld to munmap: %'lld<BR>\n",(LLD)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP|TEXTBUFFER_MEMUSAGE_COUNT_ALLOC),(LLD)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_MMAP|TEXTBUFFER_MEMUSAGE_COUNT_FREE));
  PRINTINFO("Textbuffer - Number of calls to Malloc: %'lld to Free: %'lld<BR>\n",(LLD)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_COUNT_ALLOC),(LLD)textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_COUNT_FREE));
  return n;
}

static size_t _kilo(size_t x){
  return ((1<<10)-1+x)>>10;
}
static int print_fhandle(int n,const char *title){
  PRINTINFO("<H1>Data associated to file descriptors (struct fHandle)</H1>\n_fhandle_n: %d (Maximum %d)<BR>\n",n,FHANDLE_MAX);
  if (!_fhandle_n){
    PRINTINFO("The cache is empty which is good. It means that no entry is locked or leaked.<BR>\n");
  }else{
    PRINTINFO("Table should be empty when idle.<BR>Column <I><B>fd</B></I> file descriptor. Column <I><B>Last-access</B></I>:    Column <I><B>Millisec</B></I>:  time to read entry en bloc into cache.  Column <I><B>R</B></I>: number of threads in currently in xmp_read().\
              Column <I><B>F</B></I>: flags. (D)elete indicates that it is marked for closing and (K)eep indicates that it can currently not be closed. Two possible reasons why data cannot be released: (I)  xmp_read() is currently run (II) the cached zip entry is needed by another file descriptor  with the same virtual path. Column <I><B>Tansient cache: Hex address, number of cached paths, count fetched non-existing paths, count fetched existing</B><BR>\n\
              <TABLE border=\"1\">\n<THEAD><TR>"TH("Path")TH("Last-access")IF1(WITH_MEMCACHE,TH("Cache-ID")TH("Cache-read-kB")TH("Entry-kB")TH("Millisec")TH("MillisecWaitLock"))TH("Locked")TH(" F")TH("R")IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,TH("Transient-cache"))"</TR></THEAD>\n");
    const time_t t0=time(NULL);
    foreach_fhandle(id,d){
      PRINTINFO("<TR>"sTDl(),snull(D_VP(d)));
      if (d->accesstime){
        PRINTINFO(TD("%'lld s"),(LLD)(t0-d->accesstime));
      }else{
        PRINTINFO(TD("Never"));
      }
#if WITH_MEMCACHE
      const struct memcache *m=d->memcache;
      if (m && m->txtbuf){
        PRINTINFO(TD("%05x")luTD()luTD()lTD()lTD(),m->id,(LLU)_kilo(m->memcache_already),(LLU)_kilo(m->memcache_l),(LLD)m->memcache_took_mseconds,(LLD)m->memcache_took_mseconds_in_lock);
      }else{
        PRINTINFO(TD("")TD("")TD("")TD("")TD("")TD(""));
      }
#endif //WITH_MEMCACHE
      const char *locked="!Locked"; if (pthread_mutex_trylock(&d->mutex_read)) locked="Locked"; else pthread_mutex_unlock(&d->mutex_read);
      PRINTINFO(TD("%s")TD("%c%c")dTD(),locked,(d->flags&FHANDLE_FLAG_DESTROY_LATER)?'D':' ', fhandle_currently_reading_writing(d)?'K':' ',d->is_busy);
      {
        // cppcheck-suppress variableScope
        IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,const struct ht *ht=d->ht_transient_cache;PRINTINFO(TD("%0lx %d %d %d"),(long)ht,!ht?-1: (int)ht->length, !ht?-1:ht->client_value_int[0],!ht?-1:ht->client_value_int[1]));
      }
      PRINTINFO("</TR>\n");
    }
    PRINTINFO("</TABLE>");
  }
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
    n=log_print_fuse_argv(n);
    n=log_print_open_files(n,NULL);
    n=log_print_memory(n);
    n=log_print_CPU(n);
    LOCK(mutex_fhandle,n=print_fhandle(n,""));
    //n=print_maps(n);
    PRINTINFO("<H1>Cache</H1>");
    PRINTINFO("Policy for caching ZIP entries: %s<BR>\n",  IF01(WITH_MEMCACHE,"!WITH_MEMCACHE",WHEN_MEMCACHE_S[_memcache_policy]));
    //  PRINTINFO("_cumul_time_store=%lf\n<BR>",((double)_cumul_time_store)/CLOCKS_PER_SEC);
    LOCK(mutex_fhandle,n=counts_by_filetype(n));
  }
  //  n=log_memusage_ht(n,html);
  n=log_malloc(n,html);
  n=log_mutex_locks(n,html);

  if (html) PRINTINFO("</BODY>\n</HTML>\n");
  if (_info && n<_info_capacity) _info[n]=0;
  return n+1;
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
static void fhandle_log_cache(const struct fHandle *d){
  if (!d){ log_char('\n');return;}
  ASSERT_LOCKED_FHANDLE();
  const struct memcache *m=d->memcache;
  log_msg("log_cache: d: %p path: %s cache: %s,%s cache_l: %lld/%lld   hasc: %s\n",d,D_VP(d),yes_no(m && m->txtbuf),!m?0:MEMCACHE_STATUS_S[m->memcache_status],(LLD)(!m?-1:m->memcache_already),(LLD)(!m?-1:m->memcache_l),yes_no(m && m->memcache_l>0));
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
  log_msg("       flags="ANSI_FG_BLUE"%s %s %s %s\n"ANSI_RESET,C(ZP_ZIP),C(ZP_DOES_NOT_EXIST),C(ZP_IS_COMPRESSED),C(ZP_STARTS_AUTOGEN));
#undef C
}
#define log_zpath(...) _log_zpath(__func__,__LINE__,__VA_ARGS__) /*TO_HEADER*/

#define FHANDLE_ALREADY_LOGGED_VIA_ZIP (1<<0)
#define FHANDLE_ALREADY_LOGGED_FAILED_DIFF (1<<1)
static bool fhandle_not_yet_logged(unsigned int flag,struct fHandle *d){
  if (d->already_logged&flag) return false;
  d->already_logged|=flag;
  return true;
}
static void log_fuse_process(void){
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
static void suggest_help(void){
  fprintf(stderr,"For help run\n   %s -h\n\n",_thisPrg);
  exit(1);
}
static void ZIPsFS_usage(void){
  puts_stderr(ANSI_INVERSE"ZIPsFS"ANSI_RESET"\n\nCompiled: "__DATE__"  "__TIME__"\n\n");
  puts_stderr(ANSI_UNDERLINE"Usage:"ANSI_RESET"  ZIPsFS [options]  root-dir1 root-dir2 ... root-dir-n : [libfuse-options]  mount-Point\n\n"
              ANSI_UNDERLINE"Example:"ANSI_RESET"  ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable  ~/local/folder //computer/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt\n\n\
The first root directory (here  ~/tmp/ZIPsFS/writable)  is writable and allows file creation and deletion, the others are read-only.\n\
Root directories starting with double slash (here //computer/pub) are regarded as less reliable and potentially blocking.\n\n\
Options and arguments before the colon are interpreted by ZIPsFS.  Those after the colon are passed to fuse_main(...).\n\n");
  puts_stderr(ANSI_UNDERLINE"ZIPsFS options:"ANSI_RESET"\n\n\
      -h Print usage.\n\n\
      -q quiet, no debug messages to stdout\n\
         Without the option -f (foreground) all logs are suppressed anyway.\n\n\
      -c  ");
  //  for(char **s=WHEN_MEMCACHE_S;*s;s++){if (s!=WHEN_MEMCACHE_S) putchar('|');puts_stderr(*s);}
  IF1(WITH_MEMCACHE,for(int i=0;WHEN_MEMCACHE_S[i];i++){ if (i) putc('|',stderr);puts_stderr(WHEN_MEMCACHE_S[i]);});
  puts_stderr("    When to read zip entries into RAM\n\
         seek: When the data is not read continuously\n\
         rule: According to rules based on the file name encoded in configuration.h\n\
         The default is when backward seeking would be requires otherwise\n\n\
      -k Kill on error. For debugging only.\n\n\
      -l Limit memory usage for caching ZIP entries.\n\
         Without caching, moving read positions backwards for an non-seek-able ZIP-stream would require closing, reopening and skipping.\n\
         To avoid this, the limit is multiplied with factor 1.5 in such cases.\n\n\
      -T 0 or -T 1 or -T 2  Generate a stack trace to check whether ZIPsFS can produce stack traces with line numbers.\n\
         This requires /usr/bin/addr2line (package binutils) or atos (MacOSx).\n\n");
  puts_stderr(ANSI_UNDERLINE"Fuse options:"ANSI_RESET"These options are given after a colon.\n\n\
      -d Debug information\n\n\
      -f File system should not detach from the controlling terminal and run in the foreground.\n\n\
      -s Single threaded mode. Maybe tried in case the default mode does not work properly.\n\n\
      -h Print usage information.\n\n\
      -V Version\n\n");

  puts_stderr(ANSI_UNDERLINE"Reporting bugs:"ANSI_RESET"\n\n\
If ZIPsFS crashes, please report the stack-trace and the ZIPsFS version.\n\n");
}
// Number of calls to Malloc  textbuffer_memusage_get(TEXTBUFFER_MEMUSAGE_COUNT_ALLOC)
