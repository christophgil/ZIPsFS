//////////////////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                            ///
/// Dynamically generated files                                                    ///
/// This file is less likely to be customized by the user                          ///
/// It is an interface between ZIPsFS_autogen.c and ZIPsFS_configuration_autogen.c ///
/// ZIPsFS_configuration_autogen.c  is customized by the user.                     ///
//////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////
/// This is run once when ZIPsFS is started.                       ///
//////////////////////////////////////////////////////////////////////
static void aimpl_init(void){
  FOREACH_AUTOGEN(is,s){
    s->_idx=is+1;
    if (s->ext) s->_ext_l=strlen(s->ext);
    if (!s->concurrent_computations) s->concurrent_computations=*s->cmd==PLACEHOLDER_EXTERNAL_QUEUE?32:1;
    if (s->concurrent_computations<2)      pthread_mutex_init(&s->mutex,NULL);
    if (!s->_ext_l) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"#%d s->ext must not be NULL \n",is);
    for(int i=0;s->cmd[i];i++) if (i==AUTOGEN_ARGV_MAX) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"AUTOGEN_ARGV_MAX exceeded");
    FOR(i,0,N_PATTERNS){
      if (s->patterns[i]){
        const int n=cg_strsplit(':',s->patterns[i],0,NULL,NULL);
        cg_strsplit(':',s->patterns[i],0,s->_patterns[i]=calloc_untracked(n+1,sizeof(char*)),s->_patterns_l[i]=calloc_untracked(n+1,sizeof(int)));
      }
    }
#define C(e)\
    if (s->e){\
      if (strchr(s->e,' ')) warning(WARN_AUTOGEN,s->e,"Please use colon ':' and not space to separate file extensions");\
      const int n=cg_strsplit(':',s->e,0,NULL,NULL);\
      cg_strsplit(':',s->e,0,s->_##e=calloc(n+1,sizeof(char*)),s->_##e##_ll=calloc(n+1,sizeof(int)));\
    }
    C(ends);
    C(ends_ic);
    if ((s->flags&(CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT))==(CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT)){
      DIE("autogen_config %d: CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT and CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT are mutual exclusive !",is);
    }
    if ((s->flags&CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT)){
      if (s->ends_ic) DIE("autogen_config %d:   With option CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT, use .ends and not .ends_ic.",is);
      const int n=cg_array_length(s->_ends);
      if (n!=1) DIE("autogen_config %d:   There are %d file endings. However, with option CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT, there must be exactly one file ending.",is,n);
    }
#undef C
  }
  autogen_cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// Iterate through all allowed file extensions of the given configuration.  ///
/// On match return the length of the file extension. Or return 0.           ///
////////////////////////////////////////////////////////////////////////////////
static const char *aimpl_fileext(const char *vp, const int vp_l,const struct autogen_config *s){
  if (s){
    RLOOP(ic,2){
      const int *ll=ic?s->_ends_ic_ll:s->_ends_ll;
      const char **ee=ic?s->_ends_ic:s->_ends;
      const int i=cg_find_suffix(0,vp,vp_l,ee,ll);
      if (i>=0){
        return ee[i];
      }
    }
  }
  return NULL;
}
////////////////////////////////////////////////
/// Does the virtual path match the config?  ///
/// Applied to infile and genfile paths      ///
////////////////////////////////////////////////
static bool _aimpl_matches(const char *vp, const int vp_l,const struct autogen_config *s){ /*NOT_TO_HEADER*/
  if (!s) return false;
  for(int i=0;i<N_PATTERNS && s->patterns[i]; i++){ /* Logical AND */
    bool ok=false;
    for(int j=0,l;(l=s->_patterns_l[i][j]);j++){ /* Logical or. One match is enough */
      if ((ok=(vp_l>=l && 0!=cg_memmem(vp,vp_l,s->_patterns[i][j],l)))) break;
    }
    if (!ok) return false;
  }
  return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// What files can be computed from given virtual path                                                 ///
/// return values:  0: failed   1: OK   2: OK and has more. I.e. will be run again with iDependency+1. ///
/// Used by  autogen_filldir()                                                                         ///
/// Opposite of config_autogen_estimate_filesize(), autogen_rinfiles_for_vgenfile()                            ///
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static void aimpl_vgenerated_from_vinfile(char *generated,const char *vp,const int vp_l, struct autogen_config *s){
  *generated=0;
  //bool debug=strstr(vp,"img_2");
  const int infile_ext_l=_aimpl_matches(vp,vp_l,s)?cg_strlen(aimpl_fileext(vp,vp_l,s)):0;
  if (infile_ext_l){
    const int vp_l_maybe_no_ext=vp_l-((s->flags&(CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT))?infile_ext_l:0);
    assert(vp_l_maybe_no_ext>0);
    char *e=stpncpy(stpncpy(generated,vp,vp_l_maybe_no_ext),  s->ext,s->_ext_l);
    *e=0;
    if ((s->flags&CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT)) strcpy(e,vp+vp_l-infile_ext_l);
  }
}
//////////////////////////////////////////////////////////////////////
/// For a given generated file find the matching   configuration.  ///
/// This is the only method to load struct autogen_for_vgenfile    ///
/// a may be NULL                                                  ///
//////////////////////////////////////////////////////////////////////
static struct autogen_config *autogen_for_vgenfile(struct autogen_for_vgenfile *a, const char *generated,const int generated_l){
  FOREACH_AUTOGEN(i,s){
    //bool debug=strstr(generated,"img_2.scale")  && s &&  (s->flags&CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT);
    const int x_l=s->_ext_l;
    int g_l=generated_l;
    const char *ext_inherit=NULL;
    if ((s->flags&CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT)){
      if (!(ext_inherit=aimpl_fileext(generated,generated_l,s))) continue;
      g_l-=strlen(ext_inherit);
    }
    if (g_l>x_l && !memcmp(generated+g_l-x_l,s->ext,x_l) && _aimpl_matches(generated,generated_l-x_l,s) &&
        ((s->flags&(CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT|CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT)) ||  aimpl_fileext(generated,g_l-x_l,s))){
      if (a){
        a->config=s;
        char *e=stpncpy(a->vinfiles[0],generated,g_l-s->_ext_l);
        *e=0;
        if (s->flags&CA_FLAG_GENERATED_FILE_HASNOT_INFILE_EXT) strcpy(e,s->ends);
        if (ext_inherit) strcpy(e,ext_inherit);
        a->vinfiles_n=config_autogen_vinfiles_for_vgenfile(a->vinfiles,generated,generated_l,s);
        *a->vinfiles[a->vinfiles_n]=0;
      }
      return s;
    }
  }
  return NULL;
}
static int _aimpl_fd_open(const char *path, int *fd){
  if (path && (*fd=open(path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0){ warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,path,"open failed"); return EIO;}
  return 0;
}
static void aimpl_wait_concurrent(struct autogen_config *s, const int inc){
  while(s->_count_concurrent>=s->concurrent_computations){
    log_verbose("while(s->_count_concurrent (%d) >=s->concurrent_computations (%d))",s->_count_concurrent,s->concurrent_computations);
    usleep(1000*1000);
  }
  if (s->concurrent_computations>1){
    atomic_fetch_sub(&s->_count_concurrent,inc);
  }else{
    if (inc==1) pthread_mutex_lock(&s->mutex);
    else pthread_mutex_unlock(&s->mutex);
  }
}
static bool aimpl_sufficient_diskspace(const struct autogen_config *s, const char *infile, const char *logfile){
  struct statvfs st;
  if (statvfs(_realpath_autogen,&st)) warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,infile,"");
  const int64_t bytes_free=st.f_bsize*st.f_bfree;
  if (bytes_free<(2L<<30)*(s->min_free_diskcapacity_gb?s->min_free_diskcapacity_gb:DEFAULT_MIN_FREE_DISKCAPACITY_GB)){
    FILE *f=fopen(logfile,"w");
    if (!f) perror(logfile);
    else{
      fprintf(f,"ENOSPC - %s Free: %'lld GB",strerror(ENOSPC),(LLD)(bytes_free/(2L<<30)));
      fclose(f);
    }
    return false;
  }
  return true;
}


////////////////////////////////////////////////////////////////
/// Replace the placeholders int the command line parameters ///
////////////////////////////////////////////////////////////////

static void aimpl_apply_replacements_for_argv(char *cmd[], struct autogen_files *ff,struct autogen_config *s){
  const char *rin=ff->rinfiles[0];
  const int infile_ext_len=cg_strlen(aimpl_fileext(rin,strlen(rin),s));
  for(int i=0;s->cmd[i];i++) cmd[i]=autogen_apply_replacements_for_argv(NULL,s->cmd[i],ff);
  config_autogen_modify_exec_args(cmd,ff,s);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Generate the file.  Either write tmpoutfile or fill buf.                                           ///
/// The calling function will rename tmpoutfile to virtual_outfile accroding to  atomic file creation. ///
/// ff->out is not writen here. It will be created atomically from tmpoutfile elsewhere.               ///
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static int aimpl_run(struct autogen_files *ff){
  if (!_realpath_autogen) return -EACCES;
  //  struct autogen_for_vgenfile *a;
  struct autogen_config *s=ff->config;
  bool debug=strstr(ff->virtual_out,"img_2.scale")  && s && (s->flags&CA_FLAG_GENERATED_FILE_INHERITS_INFILE_EXT);
  const char *rin=*ff->rinfiles;
  // TODO consider recursion ???
  int res=0;
  struct stat st_fail={0},st_in={0};
  if (rin) stat(rin,&st_in);
  const bool stdout_to_ram=(s->out==STDOUT_TO_MALLOC||s->out==STDOUT_TO_MMAP);
  const bool is_outfile=s->out==STDOUT_TO_OUTFILE ||  stdout_to_ram && s->max_infilesize_for_stdout_in_RAM<st_in.st_size;
  {
    aimpl_wait_concurrent(s,1); /* No return statement up from here. !!*/
    if (rin && !st_in.st_ino){ /* No rin */
      res=errno;
      warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,rin,"aimpl_run");
    }else if (0==(s->flags&CA_FLAG_IGNORE_ERRFILE) && !stat(ff->fail,&st_fail) && CG_STAT_B_BEFORE_A(st_fail,_thisPrgStat)){ /* Previously failed */
      struct stat st_out={0};
      if (is_outfile && !stat(ff->out,&st_out) && CG_STAT_B_BEFORE_A(st_fail,st_out)  || rin && CG_STAT_B_BEFORE_A(st_fail,st_in)) res=EPIPE;
    }
  }
  if (!res &&  (res=config_autogen_run(s,ff))==-1){  /* 0: success  -1: not applied    other: errno */
    res=0;
    int fd_err=-1,fd_out=-1;
    if (!res && !stdout_to_ram && !aimpl_sufficient_diskspace(s,rin,ff->log)) res=ENOSPC;
    if (!res && is_outfile) res=_aimpl_fd_open(ff->tmpout,&fd_out);
    if (!res && !s->no_redirect) res=_aimpl_fd_open(ff->log,&fd_err);
    if (!res){
      if (fd_err && s->info){ cg_fd_write_str(fd_err,s->info);cg_fd_write_str(fd_err,"\n");}
      char *cmd[AUTOGEN_ARGV_MAX+1]={0};
      aimpl_apply_replacements_for_argv(cmd,ff,s);
      cg_log_exec_fd(STDERR_FILENO,NULL,cmd);
      if (stdout_to_ram && !is_outfile){   /* Make virtual file by putting output of external prg  into textbuffer */
        ff->buf=textbuffer_new(MALLOC_autogen_textbuffer);
        if ((res=textbuffer_from_exec_output(s->out==STDOUT_TO_MMAP?TEXTBUFFER_MUNMAP:0,ff->buf,cmd,s->env,ff->log))){
          textbuffer_destroy(ff->buf);
          cg_free_null(MALLOC_autogen_textbuffer,ff->buf);
          log_failed("textbuffer_from_exec_output  %s ",rin);
        }
      }else{
        const pid_t pid=fork(); /* Make real file by running external prg */
        if (pid<0){
          log_errno("fork() waitpid 1 returned %d",pid);
          cg_fd_write_str(fd_err,"fork() failed.\n");
          res=EIO;
        }else{
          if (!pid) cg_exec(s->env,cmd,s->no_redirect?-1:s->out==STDOUT_MERGE_WITH_STDERR?fd_err:fd_out,s->no_redirect?-1:fd_err);
          else if (!(res=cg_waitpid_logtofile_return_exitcode(pid,ff->log)) && !cg_is_regular_file(ff->tmpout))  res=EIO;
        }
      }
      autogen_free_argv(cmd,s->cmd);
    }
  }/* if (!res) */
  aimpl_wait_concurrent(s,-1);
  if (res){
    unlink(ff->fail);
    if (rename(ff->log,ff->fail)) log_errno("rename(%s,%s)",ff->log,ff->fail);
  }else{
    unlink(ff->fail);
  }
  return -res;
}
static void aimpl_cleanup(const char *root){
  static time_t when=0;
  struct stat st={0};
  RLOOP(pass,2){
    if (time(NULL)-when>60*60*4 && !stat(root,&st) && st.st_ino){
      static int count;
      when=time(NULL);
      const pid_t pid=fork(); /* Make real file by running external prg */
      if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);return;}
      if (!pid){
        if (pass) execlp("find","find",root,"-type","f","-executable","-atime","+"AUTOGEN_DELETE_FILES_AFTER_DAYS,"-delete",(char*)0);
        else execlp("find","find",root,"-name","*.autogen.tmp","-mtime","+7","-delete",(char*)0);
      }else{
        int status;
        waitpid(pid,&status,0);
        cg_log_waitpid_status(stderr,status,__func__);
      }
    }
  }
}

static void _autogen_filecontent_append(const int flags, struct autogen_files *ff, const char *s,const long s_l){
  if (!ff->buf) ff->buf=textbuffer_new(MALLOC_autogen_textbuffer);
  textbuffer_add_segment(flags,ff->buf,s,s_l);
}
