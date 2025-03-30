///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////

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
#define PLACEHOLDER_EXTERNAL_QUEUE  "PLACEHOLDER_EXTERNAL_QUEUEx"   /* See ZIPsFS_autogen_queue.sh */

static char *_realpath_autogen;
const char *PLACEHOLDERS[]={
  PLACEHOLDER_EXTERNAL_QUEUE,
  PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,PLACEHOLDER_MNT,PLACEHOLDER_INFILE_NAME,PLACEHOLDER_TMP_OUTFILE_NAME,PLACEHOLDER_OUTFILE,PLACEHOLDER_OUTFILE_NAME,PLACEHOLDER_INFILE_PARENT,PLACEHOLDER_OUTFILE_PARENT,PLACEHOLDER_TMP_DIR,NULL};



/////////////////////
/// Size of file  ///
/////////////////////

//static struct ht_entry *autogen_ht_fsize(const char *vp,const int vp_l){return ht_numkey_get_entry(&ht_fsize,inode_from_virtualpath(vp,vp_l),0,true);}
#define autogen_ht_fsize(vp,vp_l) ht_numkey_get_entry(&ht_fsize,inode_from_virtualpath(vp,vp_l),0,true)

static void autogen_run(struct fhandle *d){
  log_verbose("%s",D_VP(d));
  const char *rp=D_RP(d);
  assert(strlen(rp)<MAX_PATHLEN-10);
  INIT_STRUCT_AUTOGEN_FILES(ff,D_VP(d),D_VP_L(d));
  stpcpy(stpcpy(ff.log,rp),".log");
  stpcpy(stpcpy(ff.fail,rp),".fail.txt");
  cg_recursive_mk_parentdir(ff.log);
  ff.grealpath=rp;
  {
    const int slash=cg_last_slash(rp);
    snprintf(stpncpy(ff.tmpout,rp,slash+1),MAX_PATHLEN,"autogen_tmp_%d_%llu_%s",getpid(),(LLU)currentTimeMillis(),rp+slash+1);
  }
  struct stat st={0};
  if (autogen_realinfiles(&ff)<0){
    d->autogen_state=AUTOGEN_FAIL;
     log_verbose("AUTOGEN_FAIL autogen_realinfiles(ff)");
  }else if ((d->autogen_error=-aimpl_run(&ff))){
    d->autogen_state=AUTOGEN_FAIL;
    log_verbose("AUTOGEN_FAIL aimpl_run");
  }else if (ff.buf){ /* Result is in RAM */
    //log_debug_now("Result in ff.buf");
    LOCK(mutex_fhandle, fhandle_set_text(d,ff.buf); autogen_ht_fsize(D_VP(d),D_VP_L(d))->value=(char*)textbuffer_length(ff.buf);d->autogen_state=AUTOGEN_SUCCESS);
  }else{ /*Result is in file tmpout  */
    //log_debug_now("ff.tmpout=%s  %d ",ff.tmpout,cg_file_exists(ff.tmpout));
    if (stat(ff.tmpout,&st)){ /* Fail */
      warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,ff.tmpout," size=%ld ino: %ld",st.st_size, st.st_ino);
      d->autogen_state=AUTOGEN_FAIL;
    }else{/* tmpout success */
      log_verbose("Size: %lld ino: %llu, Going to rename(%s,%s)\n",(LLD)st.st_size,(LLU)st.st_ino,ff.tmpout,rp);
      if (rename(ff.tmpout,rp)) log_errno("rename(%s,%s)",ff.tmpout,rp); /* Atomic file creation */
      else if (chmod(rp,0777)) log_errno("chmod(%s,0777)",rp);
      zpath_stat(&d->zpath,_root_writable);
      d->autogen_state=AUTOGEN_SUCCESS;
    }
  }
}
static bool virtualpath_startswith_autogen(const char *vp, const int vp_l_or_zero){
  if (!vp) return false;
  const int vp_l=vp_l_or_zero?vp_l_or_zero:strlen(vp);
  return (vp_l==DIR_AUTOGEN_L || vp_l>DIR_AUTOGEN_L && vp[DIR_AUTOGEN_L]=='/') && !memcmp(vp,DIR_AUTOGEN,DIR_AUTOGEN_L);
}
static const char *vp_without_pfx_autogen(const char *vp,const int vp_l_or_zero){
  return !vp?NULL:vp IF1(WITH_AUTOGEN, +(virtualpath_startswith_autogen(vp,vp_l_or_zero)?DIR_AUTOGEN_L:0));
}
/*
  rinfiles: speak real-input-files     vgenfile: speak virtual generated file
  Results are set to ff or stats
*/
static bool autogen_not_up_to_date(struct timespec st_mtim,const char *vp,const int vp_l){
  struct stat stats[AUTOGEN_MAX_INFILES];
  INIT_STRUCT_AUTOGEN_FILES(ff,vp,vp_l);
  RLOOP(i,autogen_realinfiles(&ff)){
    if (!cg_timespec_b_before_a(st_mtim, ff.infiles_stat[i].ST_MTIMESPEC)) return true;
  }
  return false;
}
static long autogen_estimate_filesize(const char *vp,const int vp_l){
  LOCK_N(mutex_fhandle,struct ht_entry *ht=autogen_ht_fsize(vp,vp_l);long size=(long)ht->value);
  if (!size){
    INIT_STRUCT_AUTOGEN_FILES(ff,vp,vp_l);
    if (!autogen_realinfiles(&ff)) return 0;
    bool rememberFileSize=false;
    size=config_autogen_estimate_filesize(&ff,&rememberFileSize);
    if (rememberFileSize){ LOCK_N(mutex_fhandle,ht->value=(void*)size);}
  }
  return size;
}
static bool autogen_remove_if_not_up_to_date(struct zippath *zpath){
  if (autogen_not_up_to_date(zpath->stat_rp.ST_MTIMESPEC,VP(),VP_L()) || config_autogen_file_is_invalid(VP(),VP_L(),&zpath->stat_rp, _root_writable->rootpath)){
    log_verbose("Not up-to-date. Deleting %s",RP());
    unlink(RP());
    return true;
  }
#if ! defined(HAS_NO_ATIME) || HAS_NO_ATIME
  static int noAtime=-1;
  if (noAtime<0) {
    struct statvfs statvfsbuf;
    statvfs(_realpath_autogen,&statvfsbuf);
    noAtime=0!=(statvfsbuf.f_flag&ST_NOATIME);
  }
  if (noAtime)
#endif
    cg_file_set_atime(RP(),&zpath->stat_rp,0);
  return false;
}


static void autogen_filldir(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
  const int name_l=strlen(name);
  if (ENDSWITH(name,name_l,EXT_CONTENT)) return;
  char generated[MAX_PATHLEN+1];
  const FOREACH_AUTOGEN_RULE(iac,ac){
    aimpl_vgenerated_from_vinfile(generated,name,name_l,ac);
    if (*generated) filldir(0,filler,buf,generated,stbuf,no_dups);
  }
  return;
}

static bool autogen_cleanup_running;
static void *_autogen_cleanup_runnable(void *arg){
  if(_realpath_autogen){
    static int count;
    autogen_cleanup_running=true;
    if (strstr(_realpath_autogen,DIR_AUTOGEN)) aimpl_cleanup(_realpath_autogen);
    autogen_cleanup_running=false;
  }
  return NULL;
}
static void autogen_cleanup(void){
  static pthread_t thread_cleanup;
  if (!autogen_cleanup_running){
    pthread_create(&thread_cleanup,NULL,&_autogen_cleanup_runnable,NULL);
  }
}

static char *autogen_apply_replacements_for_argv(char *dst_or_NULL,const char *orig,const struct autogen_files *ff){ /* NOT_TO_GENERATED_HEADER */
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
    // cppcheck-suppress-begin literalWithCharPtrCompare
    if (placeholder==PLACEHOLDER_EXTERNAL_QUEUE){ // cppcheck-suppress literalWithCharPtrCompare
      static char q[PATH_MAX+1];
      LOCK(mutex_autogen_init, if (!*q) strcpy(strcpy(q,_self_exe)+cg_last_slash(_self_exe)+1,"ZIPsFS_autogen_queue.sh"));
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
      LOCK(mutex_autogen_init,if (!*t) snprintf(t,MAX_PATHLEN,"%s"DIR_AUTOGEN"/.tmp",_root_writable->rootpath);cg_recursive_mkdir(replace=t));
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
    // cppcheck-suppress-end literalWithCharPtrCompare
    if (replace && strlen(c)==placeholder_l){
      c=(char*)replace;
    }else if (replace){
      const int len2=cg_str_replace(OPT_STR_REPLACE_DRYRUN,c,len,placeholder,placeholder_l,replace,replace_l);
      if (len2>capacity || c==orig){
        capacity=len2+333;
        char *c2=strcpy(dst_or_NULL?dst_or_NULL:cg_malloc(MALLOC_autogen_argv,capacity),c);
        if (c!=orig) cg_free(MALLOC_autogen_argv,c);
        c=c2;
      }
      len=cg_str_replace(0,c,len,placeholder,placeholder_l,replace,replace_l);
    }
  }
  return c;
}
static bool _autogen_is_placeholder(const char *c){
  for(const char **r=PLACEHOLDERS;*r;r++){
    if (*r<=c && c<*r+strlen(*r)) return true; /* if c is a pointer to the beginning or within string *r.  For 'within' see PLACEHOLDER_OUTFILE_NAME  */
  }
  return false;
}



static void autogen_free_argv(char const * const * cmd,const char * const * cmd_orig){
  for(int i=0;cmd_orig[i];i++){
    if (cmd[i]!=cmd_orig[i] && !_autogen_is_placeholder(cmd_orig[i])) cg_free(MALLOC_autogen_argv,cmd[i]);
  }
}
