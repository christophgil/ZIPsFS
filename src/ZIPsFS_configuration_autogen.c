///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////
//    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {


#define AUTOGEN_CMD_MAX 100
#define N_PATTERNS 9
enum _autogen_capture_output{STDOUT_DROP,STDOUT_TO_OUTFILE,STDOUT_TO_MALLOC,STDOUT_TO_MMAP,STDOUT_MERGE_TO_STDERR};
struct _autogen_config{ /* Note: those starting with underscore are initialized later */
  int id;
  bool also_search_in_other_roots;
  char *patterns[N_PATTERNS];char **_patterns[N_PATTERNS];int *_patterns_l[N_PATTERNS];
  /*Note:  patterns[0],patterns[1]  are ANDed and the colon separated within patterns[0] are ORed */
  char *ends;char **_ends;int *_ends_l;
  const char *ext;
  int _ext_l;
  enum _autogen_capture_output stdout;
  uint64_t filesize_limit;
  char **env;
  char *cmd[];
}
// #define C(limit) .tail=".raw", .ends={".raw",NULL},.match="_30-0033_",.stderr=true,.filesize_limit=limit

#define C(limit)  .ends=".raw",.patterns={"_30-0033_:_30-0037_",NULL},.filesize_limit=limit
#define ID_AUTOGEN_HALLO 1
  _test_outputfile=  {C(9999),.ext=".test1.txt",.stdout=STDOUT_TO_OUTFILE,.cmd={"touch",PLACEHOLDER_OUTPUTFILE,NULL}},
  _test_stdout=      {C(  99),.ext=".test2.txt",.stdout=STDOUT_TO_OUTFILE,.cmd={"hexdump","-C","-n","10",PLACEHOLDER_INPUTFILE,NULL}},
  _test_malloc_ok=   {C(  99),.ext=".test3.txt",.stdout=STDOUT_TO_MMAP,.cmd={"hexdump","-C","-n","10",PLACEHOLDER_INPUTFILE,NULL},},
  _test_malloc_fail= {C(  99),.ext=".test4.txt",.stdout=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},
  _test_cmd_notexist={C(  99),.ext=".test5.txt",.stdout=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},  /* Yields  Operation not permitted */
  _test_textbuf=     {C(  99),.ext=".test6.txt",.id=ID_AUTOGEN_HALLO,.cmd={NULL}},
  _wiff_strings={.ends=".wiff",.filesize_limit=99999,.ext=".strings",.stdout=true,.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INPUTFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_OUTPUTFILE" "PLACEHOLDER_OUTPUTFILE,NULL}},
#undef C
#define DOCKER_MSCONVERT "docker","run","-v",PLACEHOLDER_INPUTFILE_PARENT":/data","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INPUTFILE_NAME,\
    "--ext",".tmp","--outfile",PLACEHOLDER_OUTPUTFILE_NAME_NO_TMP
  _msconvert_mzML={.ext=".mzML",.ends=".raw:.wiff",.filesize_limit=99999999999,.stdout=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,"--mzML",NULL}},
  _msconvert_mgf={.ext=".mgf",.ends=".raw:.wiff",.filesize_limit=99999999999,.stdout=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,"--mgf",NULL}},
  *_autogen_array[]={&_test_outputfile,
                     &_test_stdout,
                     &_test_malloc_ok,&_test_malloc_fail,
                     &_test_cmd_notexist,
                     &_test_textbuf,
                     &_wiff_strings,
                     &_msconvert_mzML,&_msconvert_mgf,
                     NULL};
static struct _autogen_config *autogen_config(const int i){
  return i<0?NULL:_autogen_array[i];
}
#define FOREACH_AUTOGEN(i,s)  struct _autogen_config *s; for(int i=0;(s=autogen_config(i));i++)
//////////////////////////////////////////////////////////////////////
/// This is run once when ZIPsFS is started.
//////////////////////////////////////////////////////////////////////
static void autogen_cleanup();
__attribute__((__no_sanitize__("address")))
static void config_autogen_init(){
  FOREACH_AUTOGEN(is,s){
    if (s->ext && !s->_ext_l) s->_ext_l=strlen(s->ext);
    if (!s->_ext_l) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"#%d s->ext must not be NULL \n",is);
    for(int i=0;s->cmd[i];i++) if (i==AUTOGEN_CMD_MAX) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"AUTOGEN_CMD_MAX exceeded");
    FOR(i,0,N_PATTERNS){
      if (s->patterns[i]){
        const int n=cg_strsplit(':',s->patterns[i],0,NULL,NULL);
        cg_strsplit(':',s->patterns[i],0,s->_patterns[i]=calloc(n+1,sizeof(char*)),s->_patterns_l[i]=calloc(n+1,sizeof(int)));
      }
    }
    if (s->ends){
      const int n=cg_strsplit(':',s->ends,0,NULL,NULL);
      cg_strsplit(':',s->ends,0,s->_ends=calloc(n+1,sizeof(char*)),s->_ends_l=calloc(n+1,sizeof(int)));
    }
  }
  autogen_cleanup();
}

static void config_autogen_cleanup_before_exit(){
  FOREACH_AUTOGEN(is,s){
    free(s->_ends);
    FOR(i,0,N_PATTERNS) free(s->_patterns[i]);
  }
}

//_autogen_from_virtualpath(const int i,char *generated,const char *vp,const int vp_l){
//////////////////////////////////////////////////////////////////////
/// What files can be computed from given virtual path
/// return values:  0: failed   1: OK   2: OK and has more. I.e. will be run again with iDependency+1.
//////////////////////////////////////////////////////////////////////
static bool _config_matches(const char *vp, const int vp_l,struct _autogen_config *s){
  FOR(i,0,N_PATTERNS){
    if (s->patterns[i]){
      bool ok=false;
      for(int j=0;s->_patterns[i][j];j++){
        const int l=s->_patterns_l[i][j];
        if ((ok=  (vp_l>=l && 0!=memmem(vp,vp_l,s->_patterns[i][j],l)))) break;
      }
      if (!ok) return false;
    }
  }
  if (s->ends){
    bool ok=false;
    for(int i=0;s->_ends[i];i++){
      const int e_l=s->_ends_l[i];
      if ((ok=  (vp_l>e_l && !memcmp(vp+vp_l-e_l, s->_ends[i],e_l)))) break;
    }
    if (!ok) return false;
  }
  return true;
}


static bool config_autogen_from_virtualpath(const int iGen,char *generated,const char *vp,const int vp_l){
  int count=0;
  FOREACH_AUTOGEN(i,s){
    if (_config_matches(vp,vp_l,s)){
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
/// For computing the file what other files are needed?
/// return values:  0: failed   1: OK   2: OK and has more. I.e. will be run again with iDependency+1.
//////////////////////////////////////////////////////////////////////
static struct _autogen_config  *_config_autogen_for_path(const char *generated,const int autogen_l){
  FOREACH_AUTOGEN(i,s){
    const int vp_l=autogen_l-s->_ext_l;
    if (vp_l>0 && !memcmp(generated+vp_l,s->ext,s->_ext_l) &&  _config_matches(generated,vp_l,s)){
      return s;
    }
  }
  return NULL;
}
static bool config_autogen_dependencies(const int counter,const char *generated,const int autogen_l,char *return_vp, uint64_t *return_filesize_limit){
  if (counter>0) return false; // Assume that it depends only on one file.
  struct _autogen_config *s=_config_autogen_for_path(generated,autogen_l);
  if (!s) return false;
  const int vp_l=autogen_l-s->_ext_l;
  memcpy(return_vp,generated,vp_l);
  return_vp[vp_l]=0;
  //log_entered_function("generated=%s  %p  %s",generated,s,return_vp);
  *return_filesize_limit=s->filesize_limit;
  return true;
}
/* This function generates the outfile or instanciates a buffer with the file content */


/*
  tmpoutfile=/home/cgille/tmp/ZIPsFS/modifications/autogen/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff.mzML.3996984.tmp  lenPidTmp=12
  outfilevp=/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff.mzML
  infile=/home/cgille/tmp/ZIPsFS/mnt/autogen/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff
*/
static int config_autogen_run(char *outfilevp,char *tmpoutfile,int lenPidTmpSfx,const char *errfile, struct textbuffer *buf[]){
  log_debug_now("tmpoutfile=%s\n",tmpoutfile);

  if (!_realpath_autogen) return EACCES;
  { /* Free Space */
    struct statfs st;
    if (statfs(_realpath_autogen,&st)) warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,outfilevp,"");
    const int64_t bytes_free=st.f_bsize*st.f_bfree;
    if (bytes_free<1L*1024*1024*1024){
      warning(WARN_AUTOGEN|WARN_FLAG_ONCE_PER_PATH,_realpath_autogen,"%s Free: %'ld bytes",strerror(ENOSPC),bytes_free);
      return ENOSPC;
    }
  }
  /* Obtain s. With s->_ext_l, determin infile */
  struct _autogen_config *s=_config_autogen_for_path(outfilevp,strlen(outfilevp));
  if (!s) return ENOENT;
  char infile[MAX_PATHLEN];
  sprintf(infile,"%s%s",_mnt,outfilevp);
  infile[strlen(infile)-s->_ext_l]=0;
  log_entered_function("tmpoutfile=%s  lenPidTmp=%d  outfilevp=%s  infile=%s\n",tmpoutfile,lenPidTmpSfx,outfilevp,infile);
  switch(s->id){   /* Setting virtual file content into textbuffer */
  case ID_AUTOGEN_HALLO:
    buf[0]=textbuffer_new();
    if (s->stdout==STDOUT_TO_MMAP) buf[0]->flags|=TEXTBUFFER_MMAP;
    textbuffer_add_segment(buf[0],strdup("How is it going?"),sizeof("How is it going?"));
    return 0;
  }
  log_list_filedescriptors(STDERR_FILENO); // DEBUG_NOW
  int fd_err=-1,fd_out=-1;
#define F(path,fd)  if ((fd=open(path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0){ warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,path,"open failed");\
    assert(ENDSWITH(tmpoutfile,strlen(tmpoutfile),".tmp"));\
    return EIO;}
  if (s->stdout==STDOUT_TO_OUTFILE) F(tmpoutfile,fd_out);
  F(errfile,fd_err);
#undef F

  char *cmd[AUTOGEN_CMD_MAX+1]={0};
  autogen_apply_replacements_for_cmd_or_free(true,cmd,s->cmd,infile,tmpoutfile);
  //    cg_log_exec_fd(STDERR_FILENO,NULL,s->cmd);
  cg_log_exec_fd(STDERR_FILENO,NULL,cmd);

  if (s->stdout==STDOUT_TO_MALLOC || s->stdout==STDOUT_TO_MMAP){   /* Make virtual file by putting output of external prg  into textbuffer */
    *buf=textbuffer_new();
    if (s->stdout==STDOUT_TO_MMAP) buf[0]->flags|=TEXTBUFFER_MMAP;
    const int res=textbuffer_from_exec_output(buf[0],cmd,s->env,errfile);
    if (res){
      textbuffer_destroy(buf[0]);
      FREE(buf[0]);
    }
    return res;
  }
  const pid_t pid=fork(); /* Make real file by running external prg */
  if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);  cg_fd_write_str(fd_err,"fork() failed.\n");  return EIO;}
  if (!pid) cg_exec(s->env,cmd,s->stdout==STDOUT_MERGE_TO_STDERR?fd_err:fd_out,fd_err);
  return cg_waitpid_logtofile_return_exitcode(pid,errfile);
}
static void config_autogen_cleanup(const char *root){
  static time_t when=0;
  struct stat st={0};
  if (time(NULL)-when>60*60*4 && !stat(root,&st) && st.st_ino){
    static int count;
    when=time(NULL);
    const pid_t pid=fork(); /* Make real file by running external prg */
    if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);return;}
    if (!pid){
      execlp("find","find",root,"-type","f","(","-executable","-atime","+"AUTOGEN_DELETE_FILES_AFTER_DAYS,  "-or", "-name","*.autogen.tmp", "-mmin","+90",")","-delete",(char*)NULL);
    }else{
      int status;
      waitpid(pid,&status,0);
      cg_log_waitpid_status(stderr,status,__func__);
    }
  }
}
////////////////////////////////////////////////////////////////////
/// Restrict search for certain files to improve performance.    ///
////////////////////////////////////////////////////////////////////
static bool _config_ends_like_a_generated_file(const char *virtualpath,const int virtualpath_l){
  FOREACH_AUTOGEN(is,s){
    if (cg_endsWith(virtualpath,virtualpath_l,s->ext,s->_ext_l)){
      char *e;
      for(int ie=0;(e=s->_ends[ie]);ie++){
        if (cg_endsWith(virtualpath,virtualpath_l-s->_ext_l,e,s->_ends_l[ie])){
         return true;
        }
      }
    }
  }
  return false;
}
