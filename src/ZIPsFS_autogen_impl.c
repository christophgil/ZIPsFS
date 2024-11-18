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
    if (s->ext) s->_ext_l=strlen(s->ext);
    if (!s->concurrent_computations) s->concurrent_computations=*s->cmd==PLACEHOLDER_EXTERNAL_QUEUE?32:1;
    if (!s->_ext_l) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"#%d s->ext must not be NULL \n",is);
    for(int i=0;s->cmd[i];i++) if (i==AUTOGEN_CMD_MAX) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"AUTOGEN_CMD_MAX exceeded");
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
      cg_strsplit(':',s->e,0,s->_##e=calloc(n+1,sizeof(char*)),s->_##e##_l=calloc(n+1,sizeof(int)));\
    }
    C(ends);
    C(ends_ic);
#undef C
  }
  autogen_cleanup();
}

////////////////////////////////////////////////////////////////////////////////
/// Iterate through all allowed file extensions of the given configuration.  ///
/// On match return the length of the file extension. Or return 0.           ///
////////////////////////////////////////////////////////////////////////////////
static const int fileextlen_of_infile(const char *vp, const int vp_l, const struct _autogen_config *s){
  RLOOP(ic,2){
    const int *ll=ic?s->_ends_ic_l:s->_ends_l;
    const int i=cg_find_suffix(0,vp,vp_l,ic?s->_ends_ic:s->_ends,ll);
    if (i>=0) return ll[i];
  }
  return 0;
}
////////////////////////////////////////////////
/// Does the virtual path match the config?  ///
////////////////////////////////////////////////
static int _aimpl_matches(const char *vp, const int vp_l,const struct _autogen_config *s){ /*NOT_TO_HEADER*/
  FOR(i,0,N_PATTERNS){
    if (s->patterns[i]){
      bool ok=false;
      for(int j=0,l;(l=s->_patterns_l[i][j]);j++){
        if ((ok=  (vp_l>=l && 0!=cg_memmem(vp,vp_l,s->_patterns[i][j],l)))) break;
      }
      if (!ok) return 0;
    }
  }
  return fileextlen_of_infile(vp,vp_l,s);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// What files can be computed from given virtual path                                                 ///
/// return values:  0: failed   1: OK   2: OK and has more. I.e. will be run again with iDependency+1. ///
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool aimpl_virtualpath_from_virtualpath(const int iGen,char *generated,const char *vp,const int vp_l){
  int count=0;
  FOREACH_AUTOGEN(i,s){
    if (_aimpl_matches(vp,vp_l,s)){
      if (iGen==count++){
        memcpy(generated,vp,vp_l);
        memcpy(generated+vp_l,s->ext,s->_ext_l);
        generated[vp_l+s->_ext_l]=0;
        return true;
      }
    }
  }
  return false;
}
//////////////////////////////////////////////////////////////////////
/// For a given generated file find the matching   configuration.  ///
//////////////////////////////////////////////////////////////////////
static struct _autogen_config *aimpl_struct_for_genfile(const char *generated,const int generated_l){
  FOREACH_AUTOGEN(i,s){
    const int x_l=s->_ext_l;
    if (generated_l>x_l && !memcmp(generated+generated_l-x_l,s->ext,x_l) && _aimpl_matches(generated,generated_l-x_l,s)) return s;
  }
  return NULL;
}

////////////////////////////////////////////////////////////////////////
/// For a generated file iterate through the list of source files    ///
/// Return: true if there are further                                ///
///         false: no further. The caller should  stop iteration     ///
////////////////////////////////////////////////////////////////////////
static bool aimpl_for_genfile_iterate_sourcefiles(const int iTh,const char *generated,const int generated_l,char *iTh_dependency){
  if (iTh>0) return false; /* For this implementation, we assume that it depends only on one file. */
  struct _autogen_config *s=aimpl_struct_for_genfile(generated,generated_l);
  if (!s) return false;
  const int vp_l=generated_l-s->_ext_l;
  memcpy(iTh_dependency,generated,vp_l);
  iTh_dependency[vp_l]=0;
  return true;
}
static int autogen_fd_open(const char *path, int *fd){
  if (path && (*fd=open(path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0){ warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,path,"open failed"); return EIO;}
  return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Generate the file.  Either write tmpoutfile or fill buf.                                           ///
/// The calling function will rename tmpoutfile to virtual_outfile accroding to  atomic file creation. ///
/// outfile is not writen here. It will be created atomically from tmpoutfile elsewhere.               ///
//////////////////////////////////////////////////////////////////////////////////////////////////////////
static int aimpl_run(const char *virtual_outfile,const char *outfile,const char *tmpoutfile,const char *logfile, struct textbuffer *buf[]){
  if (!_realpath_autogen) return -EACCES;
  { /* Free Space */
    struct statvfs st;
    if (statvfs(_realpath_autogen,&st)) warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,virtual_outfile,"");
    const int64_t bytes_free=st.f_bsize*st.f_bfree;
    if (bytes_free<1L*1024*1024*1024){
      warning(WARN_AUTOGEN|WARN_FLAG_ONCE_PER_PATH,_realpath_autogen,"%s Free: %'ld bytes",strerror(ENOSPC),bytes_free);
      return -ENOSPC;
    }
  }
  /* Obtain s. With s->_ext_l, determine infile */
  struct _autogen_config *s=aimpl_struct_for_genfile(virtual_outfile,strlen(virtual_outfile));
  if (!s) return -ENOENT;
  char infile[MAX_PATHLEN+1], virtual_infile[MAX_PATHLEN+1];
  strcpy(virtual_infile,virtual_outfile);
  virtual_infile[strlen(virtual_outfile)-s->_ext_l]=0;

  if (0!=(s->flags&_CA_FLAG_WITH_GENERATED_FILES_AS_INPUT_FILES)){  /* recursion */
    bool found=false;FIND_REALPATH(virtual_infile);
    if (!found) return -ENOENT;
    strcpy(infile,RP());
  }else{
    sprintf(infile,"%s%s",_mnt,virtual_infile);
  }
  int res=0, fd_err=-1,fd_out=-1;
  bool ok=false;
  char errfile[MAX_PATHLEN+1];  strcat(strcpy(errfile,outfile),".fail.txt");
  struct stat st_fail,st_out,st_in;
  //  if (s->concurrent_computations<2){    lock(mutex_autogen);
  if (stat(infile,&st_in)){
    perror(infile); /* No infile */
    res=errno;
    warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,infile,"aimpl_run");
  } else if (0==(s->flags&_CA_FLAG_IGNORE_ERRFILE) && !stat(errfile,&st_fail)){
    if (st_out.st_ino && cg_timespec_b_before_a(st_fail.ST_MTIMESPEC,st_out.ST_MTIMESPEC) || cg_timespec_b_before_a(st_fail.ST_MTIMESPEC,st_in.ST_MTIMESPEC)){
      res=EPIPE; /*  Previously failed */
    }else{
      unlink(errfile);
    }
  }
  //}
  if (!res && !ok){
    while(s->_count_concurrent>=s->concurrent_computations){
      log_verbose("while(s->_count_concurrent (%d) >=s->concurrent_computations (%d))",s->_count_concurrent,s->concurrent_computations);
      usleep(1000*1000);
    }
    { /* No return between atomic inc and dec. !!*/
      atomic_fetch_add(&s->_count_concurrent,1);
      if (config_autogen_run(buf,s,outfile,tmpoutfile,logfile)){
      }else{
        if (s->out==STDOUT_TO_OUTFILE) res=autogen_fd_open(tmpoutfile,&fd_out);
        if (!res && !s->no_redirect) res=autogen_fd_open(logfile,&fd_err);
        if (!res){
          if (fd_err && s->info){ cg_fd_write_str(fd_err,s->info);cg_fd_write_str(fd_err,"\n");}
          char *cmd[AUTOGEN_CMD_MAX+1]={0};
          autogen_apply_replacements(cmd,s->cmd,infile,fileextlen_of_infile(infile,strlen(infile),s), outfile,tmpoutfile);
          cg_log_exec_fd(STDERR_FILENO,NULL,cmd);
          if (s->out==STDOUT_TO_MALLOC || s->out==STDOUT_TO_MMAP){   /* Make virtual file by putting output of external prg  into textbuffer */
            *buf=textbuffer_new(MALLOC_autogen_textbuffer);
            if (s->out==STDOUT_TO_MMAP) buf[0]->flags|=TEXTBUFFER_MMAP;
            if ((res=textbuffer_from_exec_output(buf[0],cmd,s->env,logfile))){ textbuffer_destroy(buf[0]); cg_free_null(MALLOC_autogen_textbuffer,buf[0]); log_failed("textbuffer_from_exec_output  %s ",infile);}
          }else{
            const pid_t pid=fork(); /* Make real file by running external prg */
            if (pid<0){
              log_errno("fork() waitpid 1 returned %d",pid);
              cg_fd_write_str(fd_err,"fork() failed.\n");
              res=EIO;
            }else{
              if (!pid) cg_exec(s->env,cmd,s->no_redirect?-1:s->out==STDOUT_MERGE_TO_STDERR?fd_err:fd_out,s->no_redirect?-1:fd_err);
              res=cg_waitpid_logtofile_return_exitcode(pid,logfile);
            }
            if (!cg_is_regular_file(tmpoutfile)){
              unlink(errfile);
              if (rename(logfile,errfile)) log_errno("rename(%s,%s)",logfile,errfile);
            }
          }
          autogen_free_cmd(cmd,s->cmd);
        }
      }
      atomic_fetch_sub(&s->_count_concurrent,1);
    }
  }/* if (!res) */
  //  if (s->concurrent_computations<2) unlock(mutex_autogen);
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
        else execlp("find","find",root,"-name","*.autogen.tmp", "-mtime","+7","-delete",(char*)0);
      }else{
        int status;
        waitpid(pid,&status,0);
        cg_log_waitpid_status(stderr,status,__func__);
      }
    }
  }
}
////////////////////////////////////////////////////////////////////
/// Restrict search for certain files to improve performance.    ///
////////////////////////////////////////////////////////////////////
static const int aimpl_ends_like_a_generated_file(const char *vp,const int vp_l){
  FOREACH_AUTOGEN(is,s){
    int ext=0;
    if (cg_endsWith(vp,vp_l,s->ext,s->_ext_l) && (ext=_aimpl_matches(vp,vp_l-s->_ext_l,s))) return ext;
  }
  return 0;
}
