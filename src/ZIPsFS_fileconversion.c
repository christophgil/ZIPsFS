///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////
_Static_assert(WITH_FILECONVERSION,"");
/* These text patterns contained in the command line arguments will be replaced */

#define PLACEHOLDER_INFILE "PLACEHOLDER_INFILEx"
#define PLACEHOLDER_TMP_OUTFILE "PLACEHOLDER_TMP_OUTFILEx"           /* The real path of the generated file. It has a temporary name with will be renamed to PLACEHOLDER_OUTFILE on success. */
#define PLACEHOLDER_OUTFILE "PLACEHOLDER_OUTFILEx"                   /* The real path of the generated file */
#define PLACEHOLDER_MNT "PLACEHOLDER_MNTx"                           /* The mount point where ZIPsFS is mounted */
#define PLACEHOLDER_TMP_DIR "PLACEHOLDER_TMP_DIRx"                   /* The real path of a directory to be used for temporary file */
#define PLACEHOLDER_INFILE_NAME "PLACEHOLDER_INFILE_NAMEx"           /* The last path component of the input file */
#define PLACEHOLDER_TMP_OUTFILE_NAME "PLACEHOLDER_TMP_OUTFILE_NAMEx"
#define PLACEHOLDER_OUTFILE_NAME "PLACEHOLDER_OUTFILE_NAME"
#define PLACEHOLDER_INFILE_PARENT "PLACEHOLDER_INFILE_PARENTx"
#define PLACEHOLDER_OUTFILE_PARENT "PLACEHOLDER_OUTFILE_PARENTx"
#define PLACEHOLDER_EXTERNAL_QUEUE  "PLACEHOLDER_EXTERNAL_QUEUEx"   /* See ZIPsFS_fileconversion_queue.sh */

static char *_realpath_fileconversion;
const char *PLACEHOLDERS[]={
  PLACEHOLDER_EXTERNAL_QUEUE,
  PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,PLACEHOLDER_MNT,PLACEHOLDER_INFILE_NAME,PLACEHOLDER_TMP_OUTFILE_NAME,PLACEHOLDER_OUTFILE,PLACEHOLDER_OUTFILE_NAME,PLACEHOLDER_INFILE_PARENT,PLACEHOLDER_OUTFILE_PARENT,PLACEHOLDER_TMP_DIR,NULL};




static void fileconversion_mv_tmp_to_rp(const char *tmp, const char *rp){
  struct stat st;
  if (stat(tmp,&st)){ log_errno("stat '%s'",tmp); return;}

  if (!st.st_size){
    warning(WARN_FILECONVERSION,tmp,"File is empty");
    if (unlink(tmp))  log_errno("unlink '%s'",tmp);
    return;
  }
  if (st.st_uid){
    if (rename(tmp,rp))  log_errno("rename '%s' -> '%s'",tmp,rp);
    return;
  }
  log_verbose("The file '%s' is owned by root. Was it created by a docker image?  Going to copy instead of rename.",tmp);
  if (strlen(rp)>MAX_PATHLEN){ warning(WARN_FILECONVERSION,rp,"Filepath too long"); return;}
  char tmp2[MAX_PATHLEN+99];stpcpy(stpcpy(tmp2,tmp),".tmp");
  if (cg_copy_file_content(tmp,tmp2)<0){log_errno("cg_copy_file_content '%s' -> '%s'",tmp,tmp2); return;}
  if (rename(tmp2,rp)) log_errno("rename '%s' -> '%s'",tmp2,rp);
  cg_vmtouch_e(tmp);
  if (unlink(tmp)) log_errno("unlink '%s'",tmp);

}

static void fileconversion_run(fHandle_t *d){
  IF_LOG_FLAG(LOG_FILECONVERSION) log_entered_function("%s",D_VP(d));
  const char *rp=D_RP(d);
  assert(strlen(rp)<MAX_PATHLEN-10);
  struct fileconversion_files ff={0};
  struct_fileconversion_files_init(&ff,D_VP(d),D_VP_L(d));
  stpcpy(stpcpy(ff.log,rp),".log");
  stpcpy(stpcpy(ff.fail,rp),".fail.txt");
  cg_recursive_mk_parentdir(ff.log);
  ff.grealpath=rp;
  {
    const int slash=cg_last_slash(rp);
    snprintf(stpncpy(ff.tmpout,rp,slash+1),MAX_PATHLEN,"fileconversion_tmp_%d_%llu_%s",getpid(),(LLU)currentTimeMillis(),rp+slash+1);
  }
  struct stat st={0};
  if (fileconversion_realinfiles(&ff)<=0){
    d->fileconversion_state=FILECONVERSION_FAIL;
    IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("FILECONVERSION_FAIL fileconversion_realinfiles(ff)");
  }else if ((d->errorno=-fc_run(&ff))){
    d->fileconversion_state=FILECONVERSION_FAIL;
    IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("FILECONVERSION_FAIL fc_run");
  }else if (ff.af_txtbuf){ /* Result is in RAM */
    LOCK(mutex_fhandle, htentry_fsize(D_VP(d),D_VP_L(d),true)->value=(char*)textbuffer_length(ff.af_txtbuf);  fhandle_set_text(d,ff.af_txtbuf); ff.af_txtbuf=NULL;  d->fileconversion_state=FILECONVERSION_SUCCESS);
  }else{ /*Result is in file tmpout  */
    if (stat(ff.tmpout,&st)){ /* Fail */
      warning(WARN_FILECONVERSION|WARN_FLAG_ERRNO,ff.tmpout," size=%ld ino: %ld",st.st_size, st.st_ino);
      d->fileconversion_state=FILECONVERSION_FAIL;
    }else{/* tmpout success */
      IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("Size: %lld ino: %llu, Going to rename(%s,%s)\n",(LLD)st.st_size,(LLU)st.st_ino,ff.tmpout,rp);

      fileconversion_mv_tmp_to_rp(ff.tmpout,rp);
      zpath_stat(0,&d->zpath); //,_root_writable
      ASSERT(_root_writable==d->zpath.root);
      d->fileconversion_state=FILECONVERSION_SUCCESS;
    }
  }
  struct_fileconversion_files_destroy(&ff);
}
static bool fileconversion_up_to_date(struct timespec t,const char *vp,const int vp_l){
  struct stat stats[FILECONVERSION_MAX_INFILES];
  struct fileconversion_files ff={0};
  struct_fileconversion_files_init(&ff,vp,vp_l);
  bool ok=true;
  RLOOP(i,fileconversion_realinfiles(&ff)){
    if (!cg_timespec_b_before_a(t, ff.infiles_stat[i].ST_MTIMESPEC)){ ok=false;break;}
  }
  struct_fileconversion_files_destroy(&ff);
  return ok;
}
static long fileconversion_estimate_filesize(const char *vp,const int vp_l){
  LOCK_N(mutex_fhandle,ht_entry_t *ht=htentry_fsize(vp,vp_l,true);long size=(long)ht->value);
  if (!size){
    struct fileconversion_files ff={0};
    struct_fileconversion_files_init(&ff,vp,vp_l);
    if (fileconversion_realinfiles(&ff)){
      bool rememberFileSize=false;
      size=config_fileconversion_estimate_filesize(&ff,&rememberFileSize);
      if (rememberFileSize){ LOCK(mutex_fhandle,ht->value=(void*)size);}
      struct_fileconversion_files_destroy(&ff);
    }
  }
  return size;
}

static bool fileconversion_remove_if_not_up_to_date(zpath_t *zpath){
  if (!(_realpath_fileconversion && ZPATH_IS_FILECONVERSION())) return false;
  if (!fileconversion_up_to_date(zpath->stat_rp.ST_MTIMESPEC,VP(),VP_L()) || config_fileconversion_file_is_invalid(RP(),RP_L(),&zpath->stat_rp, _root_writable->rootpath)){
    IF_LOG_FLAG(LOG_FILECONVERSION) log_verbose("Not up-to-date. Deleting %s",RP());
    unlink(RP());
    return true;
  }
#if ! defined(HAS_NO_ATIME) || HAS_NO_ATIME
  static int noAtime=-1;
  if (noAtime<0) {
    struct statvfs statvfsbuf;
    statvfs(_realpath_fileconversion,&statvfsbuf);
    noAtime=0!=(statvfsbuf.f_flag&ST_NOATIME);
  }
  if (noAtime)
#endif
    cg_file_set_atime(RP(),&zpath->stat_rp,0);
  return false;
}

static bool fileconversion_filldir(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,ht_t *no_dups){
  if (!_realpath_fileconversion) return false;
  //log_entered_function("%s",name);
  const int name_l=strlen(name);
  if (!ENDSWITH(name,name_l,EXT_CONTENT)){
    char generated[MAX_PATHLEN+1];
    const FOREACH_FILECONVERSION_RULE(iac,ac){
      fc_vgenerated_from_vinfile(generated,name,name_l,ac);
      if (*generated) filler_add(0,filler,buf,generated,0,stbuf,no_dups);
    }
  }
  //log_exited_function("%s",name);
  return true;
}


static char *fileconversion_apply_replacements_for_argv(char *dst_or_NULL,const char *orig,const struct fileconversion_files *ff){ /* NOT_TO_HEADER */
  if (!orig) return NULL;
  const char *rin=ff->rinfiles[0];
  char *c=(char*)orig;
  for(int len=strlen(c),capacity=len,i=0;PLACEHOLDERS[i];i++){
    const char *placeholder=PLACEHOLDERS[i];
    assert(placeholder!=NULL);
    if (!strstr(c,placeholder)) continue;
    const int placeholder_l=strlen(placeholder);
    int replace_l=0;
    const char *replace=NULL;
    if (placeholder==PLACEHOLDER_EXTERNAL_QUEUE){
      static char q[PATH_MAX+1];
      LOCK(mutex_fileconversion_init, if (!*q) strcpy(strcpy(q,_self_exe)+cg_last_slash(_self_exe)+1,"ZIPsFS_fileconversion_queue.sh"));
      replace=q;
    }else if (placeholder==PLACEHOLDER_INFILE){
      replace=rin;
    }else if (placeholder==PLACEHOLDER_OUTFILE){
      replace=ff->grealpath;
    }else if (placeholder==PLACEHOLDER_TMP_OUTFILE){
      replace=ff->tmpout;
    }else if (placeholder==PLACEHOLDER_MNT){
      replace=_mnt;
    }else if (placeholder==PLACEHOLDER_TMP_DIR){
      static char t[MAX_PATHLEN+1];
      LOCK(mutex_fileconversion_init,if (!*t) snprintf(t,MAX_PATHLEN,"%s"DIR_FILECONVERSION"/.tmp",_root_writable->rootpath);cg_recursive_mkdir(replace=t));
    }else if (placeholder==PLACEHOLDER_INFILE_NAME){
      replace=rin+cg_last_slash(rin)+1;
    }else if (placeholder==PLACEHOLDER_TMP_OUTFILE_NAME){
      replace=ff->tmpout+cg_last_slash(ff->tmpout)+1;
    }else if (placeholder==PLACEHOLDER_OUTFILE_NAME){
      replace=ff->grealpath+cg_last_slash(ff->grealpath)+1;
    }else if (placeholder==PLACEHOLDER_INFILE_PARENT){
      replace=rin;
      replace_l=MAX_int(0,cg_last_slash(rin));
    }else if (placeholder==PLACEHOLDER_OUTFILE_PARENT){
      replace=ff->grealpath;
      replace_l=MAX_int(0,cg_last_slash(ff->grealpath));
    }
    if (replace && strlen(c)==placeholder_l){
      c=(char*)replace;
    }else if (replace){
      const int len2=cg_str_replace(OPT_STR_REPLACE_DRYRUN,c,len,placeholder,placeholder_l,replace,replace_l);
      if (len2>capacity || c==orig){
        capacity=len2+333;
        char *c2=strcpy(dst_or_NULL?dst_or_NULL:cg_malloc(COUNT_FILECONVERSION_MALLOC_argv,capacity),c);
        if (c!=orig) cg_free(COUNT_FILECONVERSION_MALLOC_argv,c);
        c=c2;
      }
      len=cg_str_replace(0,c,len,placeholder,placeholder_l,replace,replace_l);
    }
  }
  return c;
}
static bool _fileconversion_is_placeholder(const char *c){
  for(const char **r=PLACEHOLDERS;*r;r++){
    if (*r<=c && c<*r+strlen(*r)) return true; /* if c is a pointer to the beginning or within string *r.  For 'within' see PLACEHOLDER_OUTFILE_NAME  */
  }
  return false;
}


static void fileconversion_free_argv(char const * const * cmd,const char * const * cmd_orig){
  for(int i=0;cmd_orig[i];i++){
    if (cmd[i]!=cmd_orig[i] && !_fileconversion_is_placeholder(cmd_orig[i])) cg_free(COUNT_FILECONVERSION_MALLOC_argv,cmd[i]);
  }
}


static void fileconversion_zpath_init(zpath_t *zpath,const virtualpath_t  *vipa){
  zpath_init(zpath,vipa);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath,rootpath(zpath->root=_root_writable));
  zpath_strcat(zpath,DIR_FILECONVERSION);
  zpath_strcat(zpath,vipa->vp);
  ZPATH_COMMIT_HASH(zpath,realpath);
}


static bool fileconversion_getattr(struct stat *stbuf,const zpath_t *zpath,const virtualpath_t *vipa){
  long size;
  if(_realpath_fileconversion && (size=fileconversion_estimate_filesize(vipa->vp,vipa->vp_l))>0){
    stat_init(stbuf,size,&zpath->stat_rp);
    stbuf->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    return true;
  }
  return false;
}
