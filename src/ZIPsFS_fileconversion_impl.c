//////////////////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                            ///
/// Dynamically generated files                                                    ///
/// This file is less likely to be customized by the user                          ///
/// It is an interface between ZIPsFS_fileconversion.c and ZIPsFS_configuration_fileconversion.c ///
/// ZIPsFS_configuration_fileconversion.c  is customized by the user.                     ///
//////////////////////////////////////////////////////////////////////////////////////
_Static_assert(WITH_FILECONVERSION,"");
//  setgid (Group Inheritance): Use chmod g+s directoryname or chmod 2755
//////////////////////////////////////////////////////////////////////
/// Lock                                                           ///
/// At a time not more than N computations for a given rule.     ///
//  See ac->concurrent_computations.                               ///
//////////////////////////////////////////////////////////////////////
static pthread_mutex_t _fileconversion_mutex[FILECONVERSION_MAX_RULES];
#define FCLOCK(ac) _fileconversion_mutex[ac->_seqnum]
#define R(strg) cg_fd_write_str(fd_err,strg)

//#define STRUCT_FILECONVERSION_FILES_INIT(ff,vp,vp_l)  struct fileconversion_files ff={0};
static void struct_fileconversion_files_init(struct fileconversion_files *ff,const char *vp,const int vp_l){
  cg_stpncpy0(ff->virtualpath,vp,ff->virtualpath_l=vp_l);
}



static void struct_fileconversion_files_destroy_txtbuf(struct fileconversion_files *ff){
  textbuffer_destroy(ff->af_txtbuf);
  FREE_NULL_MALLOC_ID(ff->af_txtbuf);
}

static void struct_fileconversion_files_destroy(struct fileconversion_files *ff){
  struct_fileconversion_files_destroy_txtbuf(ff);
}


static void fc_wait_concurrent(const struct fileconversion_rule *ac, const int inc){
  assert(-1<=inc && inc<=1);
  if (ac->concurrent_computations>1){
#define A(inc) atomic_fetch_add(count+ac->_seqnum,inc)
    static atomic_int count[FILECONVERSION_MAX_RULES];
    int running;
    while(inc==1 && (running=A(0))>=ac->concurrent_computations){
      IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("Waiting: Rule: %d %s  Running: %d >= Max running (%d)",ac->_seqnum,ac->ext,running,ac->concurrent_computations);
      usleep(1000*1000);
    }
    assert(A(0)>=0);
    const int c=A(inc);
    assert(c>=0);
#undef A
  }else{
    if (inc==1) pthread_mutex_lock(&FCLOCK(ac));
    else pthread_mutex_unlock(&FCLOCK(ac));
  }
}

#define fc_wait_concurrent_begin(ac)  fc_wait_concurrent(ac, 1)   IF1(IS_CHECKING_CODE,;FILE *_wait_concurrent=fopen("abc","r"))
#define fc_wait_concurrent_end(ac)    fc_wait_concurrent(ac,-1)   IF1(IS_CHECKING_CODE,,close(_wait_concurrent))
//////////////////////////////////////////////////////////////////////
/// This is run once when ZIPsFS is started.                       ///
//////////////////////////////////////////////////////////////////////
static void fc_init(void){
  static char realpath_fileconversion_heap[MAX_PATHLEN+1]; // cppcheck-suppress unassignedVariable
  if (!_root_writable) return;
  stpcpy(stpcpy(_realpath_fileconversion=realpath_fileconversion_heap,_root_writable->rootpath),DIR_FILECONVERSION);
  FOREACH_FILECONVERSION_RULE(idx,ac){
    assert(idx<FILECONVERSION_MAX_RULES);
    ac->_seqnum=idx;
    assert(ac->cmd!=NULL);
    assert(cg_idx_of_NULL((void*)ac->cmd,0xFFFF)<FILECONVERSION_MAX_RULES);
    if (ac->ext) ac->_ext_l=strlen(ac->ext);
    if (!ac->concurrent_computations) ac->concurrent_computations=*ac->cmd==PLACEHOLDER_EXTERNAL_QUEUE?32:1;
    if (ac->concurrent_computations<2)   pthread_mutex_init(&FCLOCK(ac),NULL);
    assert(!(ac->cmd[0]=="bash" && ac->cmd[1]=="-c" && !ac->security_check_filename));
    FOR(i,0,FILECONVERSION_FILENAME_PATTERNS){
#define S(s,ss,ll) if (s){ const int n=cg_strsplit(':',s,0,NULL,NULL); cg_strsplit(':',s,0,ss=calloc_untracked(n+1,sizeof(char*)),ll=calloc_untracked(n+1,sizeof(int))); }
      S(ac->patterns[i],ac->_patterns[i],ac->_patterns_l[i]);
      S(ac->exclude_patterns[i],ac->_xpatterns[i],ac->_xpatterns_l[i]);
#undef S
    }

#define C(e)\
    if (ac->e){\
      if (strchr(ac->e,' ')) warning(WARN_FILECONVERSION,ac->e,"Please use colon ':' and not space to separate file extensions");\
      const int n=cg_strsplit(':',ac->e,0,NULL,NULL);\
      cg_strsplit(':',ac->e,0,ac->_##e=calloc_untracked(n+1,sizeof(char*)),ac->_##e##_ll=calloc_untracked(n+1,sizeof(int)));\
    }
    C(ends);
    C(ends_ic);
    if (ac->generated_file_hasnot_infile_ext && ac->generated_file_inherits_infile_ext)  DIE("fileconversion_rule %d: generated_file_hasnot_infile_ext and generated_file_inherits_infile_ext are mutual exclusive",idx);
#undef C
    if (ac->generated_file_hasnot_infile_ext){
      if (ac->ends_ic) DIE("fileconversion_rule %d:   With option generated_file_hasnot_infile_ext, use field ends and not ends_ic.",idx);
      const int n=cg_array_length(ac->_ends);
      if (n!=1) DIE("fileconversion_rule %d: There are %d file endings. However, with option generated_file_hasnot_infile_ext, there must be exactly one.",idx,n);
    }
  }
}

///////////////////////////////////////////////////////////////////////
/// Iterate through all allowed file extensions of the given rule.  ///
/// On match return the length of the file extension. Or return 0.  ///
///////////////////////////////////////////////////////////////////////
static const char *fc_fileext(const char *vp, const int vp_l,const struct fileconversion_rule *ac){
  if (ac){
    RLOOP(ic,2){
      const int *ll=ic?ac->_ends_ic_ll:ac->_ends_ll;
      const char **ee=ic?ac->_ends_ic:ac->_ends;
      const int i=cg_find_suffix(0,vp,vp_l,ee,ll);
      if (i>=0) return ee[i];
    }
  }
  return NULL;
}
////////////////////////////////////////////////
/// Does the virtual path match the rule?  ///
/// Applied to infile and genfile paths      ///
////////////////////////////////////////////////
static bool _fc_patterns_match(const char *vp, const int vp_l,const char ** const patterns[], int * const patterns_l[], int *count){ // cppcheck-suppress [constParameterPointer,constParameter]
  for(int i=0;i<FILECONVERSION_FILENAME_PATTERNS && patterns[i]; i++){
    bool ok=true;
    for(int j=0,l; (l=patterns_l[i][j]);j++){ /* These are separated by colon : and must match all */
      count++;
      if (!(ok=(vp_l>=l && 0!=cg_memmem(vp,vp_l,patterns[i][j],l)))) break;
    }
    if (ok) return true;
  }
  return false;

}

static bool _fc_matches(const char *vp, const int vp_l,const struct fileconversion_rule *ac){
  int count=0;
  const bool match=_fc_patterns_match(vp,vp_l,ac->_patterns, ac->_patterns_l,&count);
  return (!count||match) && !_fc_patterns_match(vp,vp_l,ac->_xpatterns,ac->_xpatterns_l,&count)
    && !config_fileconversion_exclude(vp,vp_l,ac); // cppcheck-suppress knownConditionTrueFalse
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// What files can be computed from given virtual path                                                 ///
/// return values:  0: failed   1: OK   2: OK and has more. I.e. will be run again with iDependency+1. ///
/// Used by  fileconversion_filldir()                                                                         ///
/// Opposite of config_fileconversion_estimate_filesize(), fileconversion_realinfiles()                              ///
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static void fc_vgenerated_from_vinfile(char *generated,const char *vp,const int vp_l, const struct fileconversion_rule *ac){
  *generated=0;
  assert(ac!=NULL);
  const int infile_ext_l=_fc_matches(vp,vp_l,ac)?cg_strlen(fc_fileext(vp,vp_l,ac)):0;
  if (infile_ext_l){
    const int vp_l_maybe_no_ext=vp_l-((ac->generated_file_hasnot_infile_ext||ac->generated_file_inherits_infile_ext)?infile_ext_l:0);
    assert(vp_l_maybe_no_ext>0);
    char *e=cg_stpncpy0(stpncpy(generated,vp,vp_l_maybe_no_ext),  ac->ext,ac->_ext_l);
    if (ac->generated_file_inherits_infile_ext) strcpy(e,vp+vp_l-infile_ext_l);
  }
}
/////////////////////////////////////////////////////////////
/// For a given generated file find the matching rule.    ///
/// This is the only method to load struct fileconversion_files  ///
/// a may be NULL                                         ///
/////////////////////////////////////////////////////////////

static bool _fileconversion_rule_matches(struct fileconversion_files *ff){
  //bool debug=strstr(ff->virtualpath,"img_2.scale")  && ac &&  ac->generated_file_inherits_infile_ext;
  const struct fileconversion_rule *ac=ff->rule;
  const int x_l=ac->_ext_l;
  int g_l=ff->virtualpath_l;
  const char *ext_inherit=NULL;
  if (ac->generated_file_inherits_infile_ext){
    if (!(ext_inherit=fc_fileext(ff->virtualpath,ff->virtualpath_l,ac))) return false;
    g_l-=strlen(ext_inherit);
  }
  if (g_l>x_l && !memcmp(ff->virtualpath+g_l-x_l,ac->ext,x_l) && _fc_matches(ff->virtualpath,ff->virtualpath_l-x_l,ac) &&
      (ac->generated_file_hasnot_infile_ext||ac->generated_file_inherits_infile_ext || fc_fileext(ff->virtualpath,g_l-x_l,ac))){
    char *e=cg_stpncpy0(ff->vinfiles[0],ff->virtualpath,g_l-ac->_ext_l);
    if (ac->generated_file_hasnot_infile_ext) strcpy(e,ac->ends);
    if (ext_inherit) strcpy(e,ext_inherit);
    ff->infiles_n=config_fileconversion_add_virtual_infiles(ff);
    *ff->vinfiles[ff->infiles_n]=0;
    return true;
  }
  return false;
}
static int _fileconversion_realinfiles(struct fileconversion_files *ff,const struct fileconversion_rule *ac){

  //log_entered_function("%s",ff->virtualpath);
  ff->rule=ac;
  const int n=_fileconversion_rule_matches(ff);
  if (n){
    ff->infiles_size_sum=0;
    FOR(i,0,n){
      const char *vin=ff->vinfiles[i];
      if (ac->security_check_filename && (strchr(vin,'\\')||strchr(vin,'\'')||strchr(vin,'"'))){
        FILE *f=fopen(ff->fail,"w"); if (f) fputs("Illegal file name\n",f),fclose(f);
        goto end;
      }
      NEW_VIRTUALPATH(vin);
      NEW_ZIPPATH(&vipa);
      //if (find_realpath_any_root(0,zpath,NULL) || (zpath_init(zpath,vin+DIR_FILECONVERSION_L),find_realpath_any_root(0,zpath,NULL))){
      if (!find_realpath_any_root(0,zpath,NULL)) goto end;
      //if ((ZPF(ZP_TRY_ZIP))) strcpy(stpcpy(ff->rinfiles[i],_mnt) ,VP());     /* TODO entry size */
      //      else strcpy(ff->rinfiles[i],RP());                                                                       /* TODO recursion? */
      strcpy(stpcpy(ff->rinfiles[i],_mnt),VP());
      ff->infiles_size_sum+=(ff->infiles_stat[i]=zpath->stat_vp).st_size;
    }
  }
 end:
  return (ff->infiles_n=n);
}
/////////////////////////////////////////////////////////////////////////////
/// Given a virtual path.                                                 ///
/// Check if it could be a file that can be  automatically                ///
/// Return the dependent files for generating this not yet existing file. ///
/////////////////////////////////////////////////////////////////////////////
static int fileconversion_realinfiles(struct fileconversion_files *ff){
  int num_infiles=0;
  const FOREACH_FILECONVERSION_RULE(iac,ac){
    if ((num_infiles=_fileconversion_realinfiles(ff,ac))) break;
  }
  return num_infiles;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Setting  atime into the future is a method to protect generated files from being  automatically deleted ///
/// When these files are read, their atime  should not be set to now.                                       ///
/// O_NOATIME can not be used.  It is Linux specific                                                        ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void fc_maybe_reset_atime_in_future(const fHandle_t *d){
  //if (d && d->zpath.flags&ZP_FILECONVERSION){
  if (d && d->zpath.dir==DIR_FILECONVERSION){
    const struct stat *st=&d->zpath.stat_rp;
    if (st->st_atime>currentTimeMillis()/1000+(3600*24)){ /* future + one day */
      IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("Reset atime for %s atime",D_RP(d));
      struct utimbuf new_times={.actime=st->st_atime,.modtime=st->st_mtime};
      if (utime(D_RP(d),&new_times)) warning(WARN_FILECONVERSION|WARN_FLAG_ERRNO,D_RP(d),"Setting atime");
    }
  }
}
//////////////////////////////////////////////////////////
static int _fc_fd_open(const char *path, int *fd){
  if (path && (*fd=open(path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0){
    warning(WARN_FILECONVERSION|WARN_FLAG_ERRNO,path,"open failed");
    return EIO;
  }
  return 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Generate the file.  Either write tmpoutfile or fill buf.                                           ///
/// The calling function will rename tmpoutfile to virtual_outfile accroding to  atomic file creation. ///
/// ff->grealpath is not writen here. It will be created atomically from tmpoutfile elsewhere.               ///
/// Called from  fileconversion_run()
//////////////////////////////////////////////////////////////////////////////////////////////////////////

static int fc_run(struct fileconversion_files *ff){
  if (!_realpath_fileconversion) return -EACCES;
  const struct fileconversion_rule *ac=ff->rule;
  if (!ac || !ff->infiles_n) return -EIO;
  int res=0;
  ff->out=ac->out;
#define isMMAP (ff->out==STDOUT_TO_MMAP)
#define isRAM  (ff->out==STDOUT_TO_MALLOC||isMMAP)
#define isOUTF (ff->out==STDOUT_TO_OUTFILE)
  if (isRAM &&  ac->max_infilesize_for_RAM && ac->max_infilesize_for_RAM<ff->infiles_stat[0].st_size || ramUsageForFilecontent()>_preloadram_bytes_limit) ff->out=STDOUT_TO_OUTFILE;
  {
    struct stat st_fail={0},st_out={0};
    if (!ac->ignore_errfile && !stat(ff->fail,&st_fail) && CG_STAT_B_BEFORE_A(st_fail,_thisPrgStat)){ /* Previously failed */
      if (isOUTF && !stat(ff->grealpath,&st_out) && CG_STAT_B_BEFORE_A(st_fail,st_out)  || ff->rinfiles[0] && CG_STAT_B_BEFORE_A(st_fail,ff->infiles_stat[0])) return EPIPE;
    }
  }
  IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("ff->rinfiles[0]: %s size: %lld  ac->out:%d ff->out:%d",snull(ff->rinfiles[0]), (LLD)ff->infiles_stat[0].st_size,ac->out,ff->out);
  unlink(ff->log);
  unlink(ff->fail);
  fc_wait_concurrent_begin(ac);
  Assert(FILECONVERSION_RUN_SUCCESS==0); // cppcheck-suppress knownConditionTrueFalse
  if (!res &&  (res=config_fileconversion_run(ff))==FILECONVERSION_RUN_NOT_APPLIED){
    res=isOUTF?mk_parentdir_if_sufficient_storage_space(ff->grealpath):0;
    int fd_err=-1;
    if (!res && !ac->no_redirect) res=_fc_fd_open(ff->log,&fd_err);
    if (!res){
      if (fd_err>=0 && ac->info){ R(ac->info);R("\n");}
      const char *cmd[FILECONVERSION_ARGV+1]={0};
      FOR(i,0,FILECONVERSION_ARGV){
        if (!(cmd[i]=fileconversion_apply_replacements_for_argv(NULL,ff->rule->cmd[i],ff))) break;
      }
      config_fileconversion_modify_exec_args(cmd,ff);
      cg_log_exec_fd(STDERR_FILENO, cmd,NULL);
      if (isRAM){   /* Output of external prg  into textbuffer_t */
        (ff->af_txtbuf=textbuffer_new(COUNT_FILECONVERSION_MALLOC_TXTBUF))->max_length=isMMAP?FILECONVERSION_MMAP_MAX_BYTES:FILECONVERSION_MALLOC_MAX_BYTES;
        if ((res=textbuffer_from_exec_output(isMMAP?TXTBUFSGMT_MUNMAP:0,ff->af_txtbuf,cmd,ac->env,ff->log))){
          log_failed("textbuffer_from_exec_output  %s ",ff->rinfiles[0]);
          if (res==ENOMEM && mk_parentdir_if_sufficient_storage_space(ff->grealpath)){
            ff->out=STDOUT_TO_OUTFILE;
            errno=ENOMEM;
            warning(WARN_FILECONVERSION|WARN_FLAG_ERRNO,ff->grealpath,"Workaround: Going to write output file");
            errno=res=0;
          }
          struct_fileconversion_files_destroy_txtbuf(ff);
        }
      }
      int fd_out=-1;
      if (!isRAM && !(isOUTF && (res=_fc_fd_open(ff->tmpout,&fd_out)))){
        const pid_t pid=fork(); /* Make real file by running external prg */
        if (pid<0){
          log_errno("fork() waitpid 1 returned %d",pid);
          R("fork() failed.\n");
          res=EIO;
        }else{
          if (!pid){
            exit(cg_exec(cmd,ac->env,0,ac->no_redirect?0:ff->out==STDOUT_MERGE_WITH_STDERR?fd_err:fd_out,ac->no_redirect?0:fd_err));
          }else{
            int status; waitpid(pid,&status,0);
            if (!(res=cg_log_waitpid(pid,status,ff->log,true,cmd,ac->env)) && !cg_is_regular_file(ff->tmpout))  res=EIO;
            if (ff->out!=STDOUT_MERGE_WITH_STDERR && fd_err>0 && ff->grealpath){ R("File: "); R(ff->grealpath); R("\n"); }
          }
        }
      }
      if (fd_out>0) close(fd_out);
      fileconversion_free_argv((char const*const*)cmd,ac->cmd);
    }
    if (fd_err>0) close(fd_err);
  }/* if (!res) */
  fc_wait_concurrent_end(ac);
  if (res && cg_file_size(ff->log)>=0) cg_rename(ff->log,ff->fail);
  IF_LOG_FLAG(LOG_FILECONVERSION)log_exited_function("%s res: %d %s",ff->grealpath,res,success_or_fail(!res));
  return -res;
#undef isRAM
#undef isMMAP
#undef isOUTF
}

static int _fileconversion_filecontent_append(const int flags, struct fileconversion_files *ff, const char *s,const long s_l){
  if (!ff->af_txtbuf) ff->af_txtbuf=textbuffer_new(COUNT_FILECONVERSION_MALLOC_TXTBUF);
  return textbuffer_add_segment(flags,ff->af_txtbuf,s,s_l);
}
#undef ALOCK
#undef R
// COUNT_FILECONVERSION_MALLOC_TXTBUF  af_txtbuf
