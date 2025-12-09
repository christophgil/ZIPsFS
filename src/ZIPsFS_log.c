/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////

// cppcheck-suppress-file unusedFunction
// cppcheck-suppress-file variableScope


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





/****************************/
/* Print roots html or text */
/****************************/
#define U(code) cg_unicode(unicode,code,n!=0)
#define P() IF1(WITH_PRELOADFILERAM,if (n) {PRINTINFO("%s",unicode);} else) fputs(unicode,stderr)
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
  IF1(WITH_PRELOADFILERAM,if (n){ PRINTINFO("\n"); } else) fputc('\n',stderr);
  return n;
}
#undef P
#undef U
static int log_print_roots(int n){ /* n==0 for UNIX console with UTF8 else for HTML with unicode */
  IF1(WITH_PRELOADFILERAM,if (n) PRINTINFO("<HR><H1>Roots</H1><PRE>\n\n")); /* if n==0 then output to UNIX console */
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
      if (r) sprintf(tmp,"%s%s%s%s%s",r->remote?"R ":"",r->writable?"W ":"",r->with_timeout?"T ":"",r->blocked?"B ":"",r->preload?"L ":"");
      C("Features","%s",tmp);
      C("Retained directory","%s",r->retain_dirname);
      if (r){ if (*r->rootpath_mountpoint) sprintf(tmp,"(%d) %s",1+r->seq_fsid,r->rootpath_mountpoint); else sprintf(tmp,"(%d) %16lx",1+r->seq_fsid,r->f_fsid);}
      C("Filesystem","%s",tmp);
      C("Free [GB]","%'9ld",((r->statvfs.f_frsize*r->statvfs.f_bfree)>>30));
      if (n){
        if (r){ if (!r->remote) strcpy(tmp,"NA"); else if (ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC)) sprintf(tmp,"%'ld seconds ago",ROOT_SUCCESS_SECONDS_AGO(r)); else strcpy(tmp,"Never");}
        C("Last response","%s", tmp);
        C("Blocked","%'u times",r->log_count_delayed);
      }
      if (!print) continue;
      if (!r) n=table_draw_horizontal(n,0,col,width);
      line[sizeof(line)-1]=0;
      IF1(WITH_PRELOADFILERAM,if (n){ PRINTINFO("%s\n",line);} else) fprintf(stderr,"%s"ANSI_RESET"\n",line);
      n=table_draw_horizontal(n,ir==-1?1:ir<_root_n-1?2:3,col,width);
    }
  }
  static const char *info="Explain table:\n"
    "    Retained directory: Without trailing slash in provided path, the last path-component will be part of the virtual path.\n"
    "                        This is consistent with the trailing slash semantics of UNIX tools like rsync, scp and cp.\n\n"
    "    Feature flags: W=Writable (First path)   R=Remote (Path starts with two slashes)  L=Preload files (option --preload)  T=Supports timeout (Path starts with three slashes and activated WITH_TIMEOUT_xxxx macros)";
  IF1(WITH_PRELOADFILERAM,if (n){ PRINTINFO("%s   B=Blocked (frozen)\n</PRE>",info);} else) fprintf(stderr,ANSI_FG_GRAY"%s"ANSI_RESET"\n\n",info);
  return n;
#undef C
#undef S
}


 /*****************************/
 /* Log path and file handle  */
 /*****************************/
static void log_path(const char *f,const char *path){
  log_msg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,cg_strlen(path));
}
static void log_fh(const char *title,const long fh){
  char p[MAX_PATHLEN+1];
  cg_path_for_fd(title,p,fh);
  log_msg("%s  fh: %ld %s \n",title?title:"", fh,p);
}

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
#define C(f) ((ZPF(f))?#f:"")
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

/********/
/* Help */
/********/
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
  //  for(char **s=WHEN_PRELOADFILERAM_S;*s;s++){if (s!=WHEN_PRELOADFILERAM_S) putchar('|');puts_stderr(*s);}
  IF1(WITH_PRELOADFILERAM,for(int i=0;WHEN_PRELOADFILERAM_S[i];i++){ if (i) putc('|',stderr);puts_stderr(WHEN_PRELOADFILERAM_S[i]);});
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


/**************************************/
/* Specific flags to activate logging */
/**************************************/
static void log_flags_update(){
  static char path[MAX_PATHLEN+1]={0};
  if (!*path){
#define P PATH_DOT_ZIPSFS"/log_flags.conf"
#define r P".readme"
    cg_copy_path(path,P);
    FILE *f;
    if (cg_file_size(path)<0 && (f=fopen(path,"w"))) fclose(f);
    char tmp[MAX_PATHLEN+1];
    if ((f=fopen(cg_copy_path(tmp,r),"w"))){
      fprintf(f,
              "# The file defines variables and a shell script to specify additional logs for ZIPsFS.\n"
              "# The log messages go exclusively to stderr.\n"
              "# Changes take immediate effect\n#\n"
              "# BITS:\n#\n");
      FOR(i,1,LOG_FLAG_LENGTH) fprintf(f,"%s=%d\n",LOG_FLAG_S[i],i);
      fprintf(f,"activate_log(){\n  local flags=0 f\n  for f in $*; do ((flags|=(1<<f))); done\n  echo $flags |tee "P"\n}\n"
              "#\n# To source this script,  run\n#\n#    source "r
              "\n#\n# Usage example:\n#\n"
              "#     activate_log $%s $%s \n",LOG_FLAG_S[LOG_FUSE_METHODS_ENTER],LOG_FLAG_S[LOG_FUSE_METHODS_EXIT]);
      fclose(f);
    }
  }
#undef P
#undef r
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






 /**********************/
 /* logs per file type */
 /**********************/

static void init_count_getattr(void){
  static bool initialized;
  if (!initialized){
    initialized=true;
    ht_set_mutex(mutex_fhandle,ht_init_with_keystore(&ht_count_by_ext,11,&mstore_persistent)); ht_set_id(HT_MALLOC_ht_count_by_ext,&ht_count_by_ext);
  }
}
static void inc_count_by_ext(const char *path,enum enum_count_getattr field){
  lock(mutex_fhandle);
  init_count_getattr();
  if (path && ht_count_by_ext.length<1024){
    const char *ext=strrchr(path+cg_last_slash(path)+1,'.');
    if (!ext || !*ext) ext="-No extension-";
    struct ht_entry *e=ht_sget_entry(&ht_count_by_ext,ext,true);
    assert(e!=NULL);assert(e->key!=NULL);
    int *counts=e->value;
    if (!counts) e->value=counts=mstore_malloc(&mstore_persistent,sizeof(int)*enum_count_getattr_length,8);
    if (counts) counts[field]++;
  }
  unlock(mutex_fhandle);
}
static const char *fileExtensionForPath(const char *path,const int len){
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
    if(!d && (ext=(char*)fileExtensionForPath(path,len))){
      static int i;
      struct ht_entry *e=ht_sget_entry(&r->ht_filetypedata,ext, i<FILETYPEDATA_NUM);
      if (!(d=e->value) && i<FILETYPEDATA_NUM  && (d=e->value=r->filetypedata+i++)){
        d->ext=e->key=ht_sinternalize(&ht_intern_fileext,ext);
      }
    }
  }
  return d?d:&r->filetypedata_dummy;
}
