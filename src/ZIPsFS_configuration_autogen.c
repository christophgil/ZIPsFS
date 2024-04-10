///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////
//    if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1) {


#define AUTOGEN_CMD_MAX 100
#define N_PATTERNS 9

#define C(limit)  .ends=".raw",.patterns={"_30-0033_:_30-0037_",NULL},.filesize_limit=limit
#define ID_AUTOGEN_HALLO 1
#define DOCKER_MSCONVERT "docker","run","-v",PLACEHOLDER_INPUTFILE_PARENT":/data","-it","--rm","chambm/pwiz-skyline-i-agree-to-the-vendor-licenses","wine","msconvert",PLACEHOLDER_INPUTFILE_NAME,\
    "--ext",".tmp","--outfile",PLACEHOLDER_OUTPUTFILE_NAME
//static const char empty_string_array[]={NULL};
enum _autogen_capture_output{STDOUT_DROP,STDOUT_TO_OUTFILE,STDOUT_TO_MALLOC,STDOUT_TO_MMAP,STDOUT_MERGE_TO_STDERR};
struct _autogen_config{
  /* The following  start with underscore and are initialized automatically. */
  char **_patterns[N_PATTERNS];int *_patterns_l[N_PATTERNS];
  char **_ends;int *_ends_l;
  char **_ends_ic;int *_ends_ic_l;
  int _ext_l;
  atomic_int _count_concurrent;
  enum _autogen_capture_output stdout;
  uint64_t filesize_limit;
  /* The following are configured by the user: */
  int id; /* Not required. Only to implement specific rule */
  int concurrent_computations; /* How many computations */
  bool also_search_in_other_roots;
  bool no_redirect; /* Do not redirect the stdout / stderr. Was required for one Windows executable under wine. The process freezes when stdout ist redirected. */
  bool not_remember_fsize; /* If _stdout is set to STDOUT_TO_MALLOC or STDOUT_TO_MMAP, no File is generated. The file size is normally saved in a hashmap and reused. */
  char *patterns[N_PATTERNS]; /* Literal strings. Not reg-expressions. The patterns[0], patterns[1], ... are AND-ed and the colon ":" separated within patterns[i] are OR-ed */
  char *ends; /* Any ending must match */
  char *ends_ic; /* Same, ignore case */
  const char *ext; /* Extension of generated file */
  char *info; /* Will be printed into the logs */
  char **env; /* Environment. See execle(). Requires trailing NULL */
  char *cmd[]; /* Command line parameters. Requires trailing NULL */
}

  _test_malloc_fail= {C(  99),.ext=".test4.txt",.stdout=STDOUT_TO_OUTFILE,.cmd={"ls","-l","not exist",NULL},},
  _test_cmd_notexist={C(  99),.ext=".test5.txt",.stdout=STDOUT_TO_MALLOC,.cmd={"not exist",NULL},},  /* Yields  Operation not permitted */
  _test_textbuf=     {C(  99),.ext=".test6.txt",.id=ID_AUTOGEN_HALLO,.cmd={NULL}},
  _wiff_strings={.ends=".wiff",.filesize_limit=99999,.ext=".strings",.cmd={"bash","-c","tr -d '\\0' <"PLACEHOLDER_INPUTFILE"|strings|grep '\\w\\w\\w\\w' # "PLACEHOLDER_TMP_OUTPUTFILE" "PLACEHOLDER_TMP_OUTPUTFILE,NULL}},
  _msconvert_mzML={.info="Requires Docker",.ext=".mzML",.ends=".raw:.wiff",.filesize_limit=99999999999,.stdout=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,"--mzML",NULL}},
  _msconvert_mgf= {.info="Requires Docker",.ext=".mgf", .ends=".raw:.wiff",.filesize_limit=99999999999,.stdout=STDOUT_MERGE_TO_STDERR,.cmd={DOCKER_MSCONVERT,"--mgf",NULL}},
//  _wiff_scan={.ext=".scan",.ends=".wiff",.filesize_limit=99999999999,.no_redirect=true,.cmd={"bash","rawfile_mk_wiff_scan.sh",PLACEHOLDER_INPUTFILE,PLACEHOLDER_TMP_DIR,PLACEHOLDER_OUTPUTFILE,NULL}},
  _wiff_scan={.ext=".scan",.ends=".wiff",.filesize_limit=99999999999,.stdout=STDOUT_MERGE_TO_STDERR,.cmd={PLACEHOLDER_EXTERNAL_QUEUE,PLACEHOLDER_INPUTFILE,PLACEHOLDER_OUTPUTFILE,NULL}},

#define I(x)  _##x##50={.info="Requires Imagemagick\n",.ext=".scale50%."#x,.ends_ic="."#x, .filesize_limit=AUTOGEN_FILESIZE_ORIG_SIZE, .cmd={"convert",PLACEHOLDER_INPUTFILE,"-scale","50%",PLACEHOLDER_TMP_OUTPUTFILE,NULL} }
  I(jpeg),I(jpg),I(png),I(gif),
#undef I
#define I(x) _##x##25={.info="Requires Imagemagick\n",.ext=".scale25%."#x,.ends_ic="."#x,  .filesize_limit=AUTOGEN_FILESIZE_ORIG_SIZE, .cmd={"convert",PLACEHOLDER_INPUTFILE,"-scale","25%","-",NULL},.stdout=STDOUT_TO_MMAP}
  I(jpeg),I(jpg),I(png),I(gif),
#undef I
  *_autogen_array[]={
    &_test_cmd_notexist,
    &_test_textbuf,
    &_wiff_strings,
    &_wiff_scan,
    &_msconvert_mzML,&_msconvert_mgf,
    &_jpeg50,&_jpg50,&_png50,&_gif50,
    &_jpeg25,&_jpg25,&_png25,&_gif25,
    NULL};

#undef C
/* Test files:
   - _wiff_scan:  6600-tof3/Data/50-0086/20240229S_TOF3_FA_060_50-0086_OxoScan-MSBatch04_P08_H01.rawIdx.Zip 106MB
*/
//static struct _autogen_config *xautogen_config(const int i){  return i<0?NULL:_autogen_array[i];}
static bool config_autogen_file_is_invalid(const char *path,const int path_l, struct stat *st, const char *rootpath){
  if (!st) return false;
  if (ENDSWITH(path,path_l,".wiff.scan") && st->st_size==44) return true; /*The tiny wiff.scan files are placeholders*/
  return  false;
}
#define FOREACH_AUTOGEN(i,s)  struct _autogen_config *s; for(int i=0;(s=_autogen_array[i]);i++)
//////////////////////////////////////////////////////////////////////
/// This is run once when ZIPsFS is started.
//////////////////////////////////////////////////////////////////////
static void autogen_cleanup();
__attribute__((__no_sanitize__("address")))
static void config_autogen_init(){
  FOREACH_AUTOGEN(is,s){
    if (s->ext) s->_ext_l=strlen(s->ext);
    if (!s->concurrent_computations) s->concurrent_computations=*s->cmd==PLACEHOLDER_EXTERNAL_QUEUE?32:1;
    if (!s->_ext_l) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"#%d s->ext must not be NULL \n",is);
    for(int i=0;s->cmd[i];i++) if (i==AUTOGEN_CMD_MAX) warning(WARN_AUTOGEN|WARN_FLAG_EXIT,s->cmd[0],"AUTOGEN_CMD_MAX exceeded");
    FOR(i,0,N_PATTERNS){
      if (s->patterns[i]){
        const int n=cg_strsplit(':',s->patterns[i],0,NULL,NULL);
        cg_strsplit(':',s->patterns[i],0,s->_patterns[i]=calloc(n+1,sizeof(char*)),s->_patterns_l[i]=calloc(n+1,sizeof(int)));
      }
    }
#define C(e)\
    if (s->e){\
      const int n=cg_strsplit(':',s->e,0,NULL,NULL);\
      cg_strsplit(':',s->e,0,s->_##e=calloc(n+1,sizeof(char*)),s->_##e##_l=calloc(n+1,sizeof(int)));\
    }
    C(ends);
    C(ends_ic);
#undef C
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
        for(int j=0,l;(l=s->_patterns_l[i][j]);j++){
          if ((ok=  (vp_l>=l && 0!=memmem(vp,vp_l,s->_patterns[i][j],l)))) break;
        }
        if (!ok) return false;
      }
    }
#define C(cmp,e,e_l)  if (e) for(int i=0;e[i];i++) if (vp_l>e_l && !cmp(vp+vp_l-e_l,e[i],e_l)) return true;
  C(memcmp,s->_ends,s->_ends_l[i]);
  C(strncasecmp,s->_ends_ic,s->_ends_ic_l[i])
#undef C
    return false;
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
static struct _autogen_config *_config_struct_autogen_for_path(const char *generated,const int generated_l){
  FOREACH_AUTOGEN(i,s){
    const int x_l=s->_ext_l;
    if (generated_l>x_l && !memcmp(generated+generated_l-x_l,s->ext,x_l) && _config_matches(generated,generated_l-x_l,s)) return s;
  }
  return NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
/// If  'generated' is not a generated file, return false.                                     ///
/// If there are at least iTh-1 files it depends on, copy the virtual path into iTh_dependency ///
//////////////////////////////////////////////////////////////////////////////////////////////////
static bool config_autogen_dependencies(const int iTh,const char *generated,const int generated_l,char *iTh_dependency, long *return_filesize_limit, int *return_flags){
  if (iTh>0) return false; /* For this implementation, we assume that it depends only on one file. */
  struct _autogen_config *s=_config_struct_autogen_for_path(generated,generated_l);
  if (!s) return false;
  const int vp_l=generated_l-s->_ext_l;
  memcpy(iTh_dependency,generated,vp_l);
  iTh_dependency[vp_l]=0;
  *return_filesize_limit=s->filesize_limit;
  if (s->not_remember_fsize) *return_flags=AUTOGEN_FLAG_NOT_REMEMBER_FILESIZE;
  return true;
}
/* This function generates the outfile or instanciates a buffer with the file content */


/*
  tmpoutfile=/home/cgille/tmp/ZIPsFS/modifications/autogen/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff.mzML.3996984.tmp  lenPidTmp=12
  outfilevp=/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff.mzML
  infile=/home/cgille/tmp/ZIPsFS/mnt/autogen/6600-tof2/Maintenance/202208/20220819_TOF2_FA_645_MA_pepcal.wiff
*/
static int autogen_fd_open(const char *path, int *fd){
  if (path && (*fd=open(path,O_RDWR|O_CREAT,S_IRUSR|S_IWUSR))<0){ warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,path,"open failed"); return EIO;}
  return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// Generate the file.  Either write tmpoutfile or fill buf.                                                                                    ///
/// Not write virtual_outfile directly here. The calling function will rename tmpoutfile to virtual_outfile accroding to  atomic file creation. ///
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static int config_autogen_run(const char *virtual_outfile,const char *outfile,const char *tmpoutfile,const char *errfile, struct textbuffer *buf[]){
  if (!_realpath_autogen) return EACCES;
  { /* Free Space */
    struct statfs st;
    if (statfs(_realpath_autogen,&st)) warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,virtual_outfile,"");
    const int64_t bytes_free=st.f_bsize*st.f_bfree;
    if (bytes_free<1L*1024*1024*1024){
      warning(WARN_AUTOGEN|WARN_FLAG_ONCE_PER_PATH,_realpath_autogen,"%s Free: %'ld bytes",strerror(ENOSPC),bytes_free);
      return ENOSPC;
    }
  }
  /* Obtain s. With s->_ext_l, determin infile */
  struct _autogen_config *s=_config_struct_autogen_for_path(virtual_outfile,strlen(virtual_outfile));
  if (!s) return ENOENT;
  char infile[MAX_PATHLEN];
  sprintf(infile,"%s%s",_mnt,virtual_outfile);
  infile[strlen(infile)-s->_ext_l]=0;
  log_entered_function("tmpoutfile=%s   virtual_outfile=%s  infile=%s concurrent_computations: %d\n",tmpoutfile,virtual_outfile,infile,s->concurrent_computations);
  int res=0, fd_err=-1,fd_out=-1;
  if (s->concurrent_computations<2) lock(mutex_autogen);
  while(s->_count_concurrent>=s->concurrent_computations){
    log_verbose("while(s->_count_concurrent (%d) >=s->concurrent_computations (%d))",s->_count_concurrent,s->concurrent_computations);
    usleep(1000*1000);
  }
  { /* No return between atomic inc and dec. !!*/
    atomic_fetch_add(&s->_count_concurrent,1);
    switch(s->id){
    case ID_AUTOGEN_HALLO:      textbuffer_add_segment((buf[0]=textbuffer_new()),strdup("How is it going?"),sizeof("How is it going?")); break;
    default:
      if (s->stdout==STDOUT_TO_OUTFILE) res=autogen_fd_open(tmpoutfile,&fd_out);
      if (!res && !s->no_redirect) res=autogen_fd_open(errfile,&fd_err);
      if (!res){
        if (fd_err && s->info) cg_fd_write_str(fd_err,s->info);
        char *cmd[AUTOGEN_CMD_MAX+1]={0};
        autogen_apply_replacements(cmd,s->cmd,infile,outfile,tmpoutfile);
        cg_log_exec_fd(STDERR_FILENO,NULL,cmd);
        if (s->stdout==STDOUT_TO_MALLOC || s->stdout==STDOUT_TO_MMAP){   /* Make virtual file by putting output of external prg  into textbuffer */
          *buf=textbuffer_new();
          if (s->stdout==STDOUT_TO_MMAP) buf[0]->flags|=TEXTBUFFER_MMAP;
          if ((res=textbuffer_from_exec_output(buf[0],cmd,s->env,errfile))){ textbuffer_destroy(buf[0]); FREE(buf[0]);}
        }else{
          const pid_t pid=fork(); /* Make real file by running external prg */
          if (pid<0){
            log_errno("fork() waitpid 1 returned %d",pid);
            cg_fd_write_str(fd_err,"fork() failed.\n");
            res=EIO;
          }else{
            if (!pid) cg_exec(s->env,cmd,s->no_redirect?-1:s->stdout==STDOUT_MERGE_TO_STDERR?fd_err:fd_out,s->no_redirect?-1:fd_err);
            res=cg_waitpid_logtofile_return_exitcode(pid,errfile);
          }
        }
        autogen_free_cmd(cmd,s->cmd);
      }
    }
    atomic_fetch_sub(&s->_count_concurrent,1);
  }
  if (s->concurrent_computations<2) unlock(mutex_autogen);
  return res;

}
static void config_autogen_cleanup(const char *root){
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
static bool _config_ends_like_a_generated_file(const char *virtualpath,const int virtualpath_l){
  FOREACH_AUTOGEN(is,s){
    if (cg_endsWith(virtualpath,virtualpath_l,s->ext,s->_ext_l) &&_config_matches(virtualpath,virtualpath_l-s->_ext_l,s)) return true;
  }
  return false;
}
