/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Logging in  ZIPsFS                                        ///
/////////////////////////////////////////////////////////////////

// cppcheck-suppress-file unusedFunction
// cppcheck-suppress-file variableScope
#define P(txt) fputs(txt,file)

static void _rootdata_counter_inc(counter_rootdata_t *c, enum enum_counter_rootdata f){
  if (c&&c->counts[f]<UINT32_MAX) atomic_fetch_add(c->counts+f,1);
}
static void fhandle_counter_inc( fHandle_t* d, enum enum_counter_rootdata f){
  if (!d->filetypedata) d->filetypedata=filetypedata_for_ext(D_VP(d),D_ROOT(d));
  _rootdata_counter_inc(d->filetypedata,f);

}
/* static void rootdata_counter_inc(const char *path, enum enum_counter_rootdata f, root_t* r){ */
/*   _rootdata_counter_inc(filetypedata_for_ext(path,r),f); */
/* } */

/****************************/
/* Print roots html or text */
/****************************/

#define U(code) cg_unicode(unicode,code,IS_HTML())
#define PU() P(unicode)
static void table_draw_horizontal(FILE *file, const int type, const int nColumn, const int *width){ /* file==stderr for UNIX console with UTF8 else for HTML with unicode */
  static const int ccc[4][4]={
    {BD_HEAVY_DOWN_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_DOWN_AND_HORIZONTAL,BD_HEAVY_DOWN_AND_LEFT}, /* above header */
    {BD_HEAVY_VERTICAL_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_VERTICAL_AND_HORIZONTAL,BD_HEAVY_VERTICAL_AND_LEFT}, /* below header */
    {BD_VERTICAL_HEAVY_AND_RIGHT_LIGHT,BD_LIGHT_HORIZONTAL,BD_VERTICAL_HEAVY_AND_HORIZONTAL_LIGHT,BD_VERTICAL_HEAVY_AND_LEFT_LIGHT}, /* body */
    {BD_HEAVY_UP_AND_RIGHT,BD_HEAVY_HORIZONTAL,BD_HEAVY_UP_AND_HORIZONTAL,BD_HEAVY_UP_AND_LEFT}}; /*bottom */ // cppcheck-suppress constVariable
  const int *cc=ccc[type];
  char unicode[9];

  U(cc[0]); PU();
  FOR(ic,0,nColumn){
    U(cc[1]); RLOOP(iTimes,width[ic]+2) PU();
    U(cc[ic<nColumn-1?2:3]); PU();
  }
  fputc('\n',file);
}
#undef PU
#undef U
static void log_print_roots(FILE *file){ /* n==0 for UNIX console with UTF8 else for HTML with unicode */

#define C(title,format,...)  pos+=(colw=r?S(format,__VA_ARGS__):S("%s",title));if (print) pos+=S(" %*s%s ",width[col]-colw,"",colsep); else width[col]=MAX(width[col],colw);col++;
#define S(...) snprintf(line+pos,sizeof(line)-pos,__VA_ARGS__)
  int width[33]={0};
  char line[1024], colsep[9];
  const bool html=file!=stderr;
  if (html) fputs("If the table appears misaligned in MS-Edge, it can be  copied in MS-Notepad.\n\n<PRE>",file);
  cg_unicode(colsep,BD_HEAVY_VERTICAL,html);
  FOR(print,0,2){
    FOR(ir,-1,_root_n){
      char tmp[MAX_PATHLEN+1];
      root_t *r=ir<0?NULL:_root+ir;
      if (!r && !html) P(ANSI_BOLD);
      int colw,col=0, pos=cg_unicode(line,BD_HEAVY_VERTICAL,html);
      line[pos++]=' ';
      C("No","%2d",1+rootindex(r));
      C("Path","%s",r->rootpath);
      *tmp=0;
      if (r){
        char *e=tmp;
#define A(cond,s) if (r->cond) e=stpcpy(stpcpy(e,s)," ")
        A(remote,"R");A(has_timeout,"T");A(writable,"W");A(blocked,"B");A(decompress_mask,"L");A(noatime,"noatime");A(follow_symlinks,"S"); A(one_file_system,"1");A(worm,"WORM");
        A(immutable,"C");
        FOR(ic,1,COMPRESSION_NUM) A(decompress_mask&(1<<ic),cg_compression_file_ext(ic,NULL));
#undef A
      }
      C("Features","%s",tmp);
      C(ROOT_PROPERTY[ROOT_PROPERTY_path_prefix],"%s",r->path_prefix);
      if (r){ if (r->rootpath_mountpoint) sprintf(tmp,"(%d) %s",1+r->seq_fsid,r->rootpath_mountpoint); else sprintf(tmp,"(%d) %16lx",1+r->seq_fsid,r->f_fsid);}
      C("Filesystem","%s",tmp);
      C("Free [GB]","%'9ld",((r->statvfs.f_frsize*r->statvfs.f_bfree)>>30));
      if (html){
        if (r){ if (!r->remote) strcpy(tmp,"NA"); else if (ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC)) sprintf(tmp,"%'ld seconds ago",ROOT_SUCCESS_SECONDS_AGO(r)); else strcpy(tmp,"Never");}
        C("Last response","%s", tmp);
        C("Blocked","%'u times",r->log_count_delayed);
      }
      if (!print) continue;
      if (!r) table_draw_horizontal(file,0,col,width);
      line[sizeof(line)-1]=0;
      P(line);
      if (!html)P(ANSI_RESET);
      fputc('\n',file);
      table_draw_horizontal(file,ir==-1?1:ir<_root_n-1?2:3,col,width);
    }
  }
  if (!html) P(ANSI_FG_GRAY);
  P("If box-drawing chars are wrong then try 'export LANG=en_US'  or tmux 'tmux set-environment LANG en_US'\nExplain table:\n");
  P(ROOT_PROPERTY[ROOT_PROPERTY_path_prefix]);
  P("  This adds to the beginning of all virtual files.\n\
               Also the last path component of the root-path is used as the prefix unless a trailing slash is given.\n\
               This is consistent with the trailing slash semantics of UNIX tools like rsync, scp and cp.\n\n\
Feature flags: W=Writable (First path)   R=Remote (Path starts with two slashes)  L=Preload files (option --preload)   1=one file system (no cross devices)  S=follow symlinks\n\
               T=Supports timeout (Path starts with three slashes and activated WITH_TIMEOUT_xxxx macros)  I=immutable WORM=Write-once-read-many\n\
               gz xz bz2 lrz Z: decompression on preloading to disk\n    B=Blocked (frozen)");

  root_property_print(html,-1,file);
  P(html?"\n</PRE>":ANSI_RESET"\n\n");

#undef C
#undef S

}


/*****************************/
/* Log path and file handle  */
/*****************************/
static void log_path(const char *f,const char *path){
  log_msg("  %s '"ANSI_FG_BLUE"%s"ANSI_RESET"' len="ANSI_FG_BLUE"%u"ANSI_RESET"\n",f,path,cg_strlen(path));
}

static void __viamacro_log_zpath(const char *fn,const int line,const char *msg, zpath_t *zpath){
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
  log_msg("       flags="ANSI_FG_BLUE"%s %s dir:%s\n"ANSI_RESET,C(ZP_DOES_NOT_EXIST),C(ZP_IS_COMPRESSEDZIPENTRY),zpath->dir);
#undef C
}
#define log_zpath(...) __viamacro_log_zpath(__func__,__LINE__,__VA_ARGS__) /*TO_HEADER*/

enum {FHANDLE_ALREADY_LOGGED_VIA_ZIP=1<<0,FHANDLE_ALREADY_LOGGED_FAILED_DIFF=1<<1};

static bool fhandle_not_yet_logged(unsigned int flag,fHandle_t *d){
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
  cg_print_stacktrace(0);
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
  IF1(WITH_PRELOADRAM,FOR(i,0,enum_when_preloadram_zip_N){ if (i) putc('|',stderr);puts_stderr(enum_when_preloadram_zip_S[i]);});
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
#define p PATH_DOT_ZIPSFS"/log_flags.conf"
#define r p".readme"
    cg_copy_path(path,p);
    FILE *f;
    if (cg_file_size(path)<0 && (f=fopen(path,"w"))) fclose(f);
    char tmp[MAX_PATHLEN+1];
    if ((f=fopen(cg_copy_path(tmp,r),"w"))){
      fprintf(f,
              "# The file defines variables and a shell script to specify additional logs for ZIPsFS.\n"
              "# The log messages go exclusively to stderr.\n"
              "# Changes take immediate effect\n#\n"
              "# BITS:\n#\n");
      FOR(i,1,enum_log_flags_N) fprintf(f,"%s=%d\n",enum_log_flags_S[i],i);
      fprintf(f,"activate_log(){\n  local flags=0 f\n  for f in $*; do ((flags|=(1<<f))); done\n  echo $flags |tee "p"\n}\n"
              "#\n# To source this script,  run\n#\n#    source "r
              "\n#\n# Usage example:\n#\n"
              "#     activate_log $%s $%s \n",enum_log_flags_S[LOG_FUSE_METHODS_ENTER],enum_log_flags_S[LOG_FUSE_METHODS_EXIT]);
      fclose(f);
    }
  }
#undef p
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
static void inc_count_by_ext(const char *path,enum enum_count_getattr field){
  lock(mutex_fhandle);
  if (path && _ht_count_by_ext.length<1024){
    const char *ext=strrchr(path+cg_last_slash(path)+1,'.');
    if (!ext || !*ext) ext="-No extension-";
    ht_entry_t *e=ht_get_entry(&_ht_count_by_ext,ext,strlen(ext),0,true);
    assert(e!=NULL);assert(e->key!=NULL);
    int *counts=e->value;
    if (!counts) e->value=counts=mstore_malloc(&_mstore_persistent,sizeof(int)*enum_count_getattr_length,8);
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
static root_t root_dummy;
static counter_rootdata_t *filetypedata_for_ext(const char *vp,root_t *r){
  if (!r) return NULL;
  //if (DEBUG_NOW==DEBUG_NOW) return &r->filetypedata_dummy;
  //log_entered_function("vp:'%s' r=%p",vp,r);
  ASSERT_LOCKED_FHANDLE();
  if(!r->filetypedata_initialized){
    assert(FILETYPEDATA_FREQUENT_NUM>_FILE_EXT_TO-_FILE_EXT_FROM);
    r->filetypedata_initialized=true;
    ht_sset(&r->ht_filetypedata, (r->filetypedata_all.ext=".*"), &r->filetypedata_all);
    ht_sset(&r->ht_filetypedata, (r->filetypedata_dummy.ext="*"),&r->filetypedata_dummy);
  }
  const int vp_l=cg_strlen(vp);
  counter_rootdata_t *d=NULL;
  const char *ext;
  if (vp_l){
    static int return_i;
    if ((ext=(char*)config_some_file_path_extensions(vp,vp_l,&return_i))){
      d=r->filetypedata_frequent+return_i;
      if (!d->ext) ht_sset(&r->ht_filetypedata,d->ext=ext,d);
    }
    if(!d && (ext=(char*)fileExtensionForPath(vp,vp_l))){
      static int i;
      ht_entry_t *e=ht_get_entry(&r->ht_filetypedata,ext, strlen(ext),0,i<FILETYPEDATA_NUM);
      if (e && !(d=e->value) && (d=e->value=r->filetypedata+i++)) d->ext=e->key;
    }
  }
  return d?d:&r->filetypedata_dummy;
}
#undef P
// ht_sget_entry


////////////////////////////////////////////////////////////
static int log_fuse_function_fd(){
  static int fd,count;
  const char *outpath=SFILE_REAL_PATHS[SFILE_LOG_FUNCTION_CALLS];
  char outpath_old[PATH_MAX+1];
  if (!count++){
    stpcpy(stpcpy(outpath_old,outpath)-3,"1.txt");
    unlink(outpath_old);
    rename(outpath,outpath_old);
  }
  static bool er;
  if (er) return 0;
  if (fd>2 && !(++count&(0xFFF))){
    struct stat st={0};
    if (!fstat(fd,&st) && st.st_size>(1<<24)){
      close(fd);
      fd=0;
      unlink(outpath_old);
      cg_rename(outpath,outpath_old);
    }
  }
  if (fd<3){

    //	argument must be supplied if O_CREAT or O_TMPFILE is 0644
    fd=open(outpath,O_WRONLY|O_CREAT,0644);
    if (fd>2){
#define  H "Seconds\tPath\tFunction\tNum\n"
      write(fd,H,sizeof(H)-1);
#undef H
    }else{
      warning(WARN_OPEN|WARN_FLAG_ERRNO,outpath," open(fd,O_WRONLY|O_CREAT,0644)");
      er=true;
    }
  }
  return fd>2?fd:0;
}
static void log_fuse_function(const char *func, const virtualpath_t *vipa,int num){
  IF_LOG_FLAG(LOG_FUSE_METHODS_ENTER)log_exited_function("%s res:%d",vipa->vp,num);
  if (vipa->dir!=DIR_LOGGED  || vipa->special_file_id || !vipa->vp_l) return;

  if (*func=='_') func++;
  if (memcmp(func,"xmp_",4)) func+=4;
  if (*func=='_') func++;
  static const char *func_1;
  static char vp_1[MAX_PATHLEN+1];
  if (vipa->vp_l>MAX_PATHLEN) return;
  static int num_1;
  lock(mutex_log);
  const int fd=log_fuse_function_fd();

  if (fd){
    const bool vp_eq=!strcmp(vipa->vp,vp_1);
    const bool func_eq=func_1 && !strcmp(func,func_1);
#define C(...) n+=snprintf(buf+n,sizeof(buf)-1-n,__VA_ARGS__)
    char buf[PATH_MAX+1];
    int n=0;
    C("%ld\t",time(NULL)-_whenStarted);
    //if (!(func_eq && vp_eq && num_1==num))
      C("%s\t%s\t%d",(vp_eq?"":vipa->vp),(func_eq?"":func), num);
#undef C
    buf[n++]='\n';
    cg_fd_write(fd,buf,n);
    //buf[n]=0;log_debug_now("fd=%d  buf: %s",fd,buf);
    strcpy(vp_1,vipa->vp);
    num_1=num;
    func_1=(char*)func;
  }
  unlock(mutex_log);
}
