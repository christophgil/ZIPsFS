///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////

#define AUTOGEN_FILESIZE_ORIG_SIZE -100
#define PLACEHOLDER_INPUTFILE "PLACEHOLDER_INPUTFILEx"
#define PLACEHOLDER_TMP_OUTPUTFILE "PLACEHOLDER_TMP_OUTPUTFILEx"
#define PLACEHOLDER_OUTPUTFILE "PLACEHOLDER_OUTPUTFILEx" // e.g /slow3/Users/x/ZIPsFS/modifications/ZIPsFS/a/6600-tof3/Data/50-0086/20240229S_TOF3_FA_060_50-0086_OxoScan-MSBatch04_P08_H01.wiff.scan
#define PLACEHOLDER_MNT "PLACEHOLDER_MNTx"
#define PLACEHOLDER_TMP_DIR "PLACEHOLDER_TMP_DIRx"
#define PLACEHOLDER_INPUTFILE_NAME "PLACEHOLDER_INPUTFILE_NAMEx"
#define PLACEHOLDER_TMP_OUTPUTFILE_NAME "PLACEHOLDER_TMP_OUTPUTFILE_NAMEx"
#define PLACEHOLDER_OUTPUTFILE_NAME "PLACEHOLDER_OUTPUTFILE_NAME"
#define PLACEHOLDER_INPUTFILE_PARENT "PLACEHOLDER_INPUTFILE_PARENTx"
#define PLACEHOLDER_EXTERNAL_QUEUE  "PLACEHOLDER_EXTERNAL_QUEUEx"

static char *_realpath_autogen;

const char *PLACEHOLDERS[]={
  PLACEHOLDER_EXTERNAL_QUEUE,
  PLACEHOLDER_INPUTFILE,PLACEHOLDER_TMP_OUTPUTFILE,PLACEHOLDER_MNT,PLACEHOLDER_INPUTFILE_NAME,PLACEHOLDER_TMP_OUTPUTFILE_NAME,PLACEHOLDER_OUTPUTFILE,PLACEHOLDER_OUTPUTFILE_NAME,PLACEHOLDER_INPUTFILE_PARENT,PLACEHOLDER_TMP_DIR,NULL};



////////////////////////////////////////////////////////////
/// To be implemented in ZIPsFS_configuration_autogen.c  ///
////////////////////////////////////////////////////////////
static void config_autogen_cleanup(const char *root);
static bool config_autogen_from_virtualpath(const int iGen,char *generated,const char *vp,const int vp_l);
static bool config_autogen_dependencies(const int counter,const char *generated,const int autogen_l,char *return_vp, uint64_t *return_filesize_limit);
static int config_autogen_run(const char *virtual_outfile,const char *outfile, const char *tmpoutfile,const char *errfile, struct textbuffer *buf[]);
static void autogen_run(struct fhdata *d){
  const char *rp=D_RP(d);
  log_entered_function("%s D_RP=%s\n",D_VP(d),rp);
  if (d->autogen_state) return;
  cg_recursive_mk_parentdir(rp);
  char tmp[MAX_PATHLEN];
  char err[MAX_PATHLEN];
  //snprintf(tmp,MAX_PATHLEN,"%s.%d.%ld.autogen.tmp",rp,getpid(),currentTimeMillis());
  snprintf(tmp,MAX_PATHLEN,"autogen_tmp_%d_%ld_%s",getpid(),currentTimeMillis(),rp);
  snprintf(err,MAX_PATHLEN,"%s.log",rp);
  struct textbuffer *buf=NULL;
  struct stat st={0};
  if (!stat(err,&st) && st.st_ino>0) unlink(err);
  int err_gf=config_autogen_run(D_VP(d),D_RP(d),tmp,err,&buf);
  if (buf){
    LOCK(mutex_fhdata, d->memcache2=buf;d->memcache_l=d->memcache_already=textbuffer_length(buf));
    d->autogen_state=AUTOGEN_SUCCESS;
  }else{
    if (stat(tmp,&st)){
      warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,tmp,"size=%ld ino: %ld",st.st_size, st.st_ino);
      d->autogen_state=AUTOGEN_FAIL;
    }else{
      log_verbose("Size: %ld ino: %lu, Going to rename(%s,%s)\n",st.st_size,st.st_ino,tmp,rp);
      if (DEBUG_NOW==DEBUG_NOW) usleep(200*1000*1000);
      if (rename(tmp,rp)) log_errno("rename(%s,%s)",tmp,rp);
      else if (chmod(rp,0777)) log_errno("chmod(%s,0777)",rp);
      zpath_stat(&d->zpath,_root_writable);
      d->autogen_state=AUTOGEN_SUCCESS;
    }
  }
}
static bool autogen_not_up_to_date(struct timespec st_mtim,const char *vp,const int vp_l){
  struct stat st={0};
  char autogen_from[MAX_PATHLEN];
  uint64_t size;
  FOR(i,0,AUTOGEN_MAX_DEPENDENCIES){
    if (!config_autogen_dependencies(i,vp,vp_l,autogen_from,&size)) break;

    if (!stat(autogen_from,&st) && cg_timespec_b_before_a(st_mtim,st.st_mtim)) return true;
  }
  return false;
}
static void autogen_remove_if_not_up_to_date(const char *vp,const int vp_l){
  char rp[MAX_PATHLEN];
  snprintf(rp,MAX_PATHLEN,"%s%s/%s",_root_writable->rootpath,DIR_AUTOGEN,vp);
  struct stat st={0};
  if (!stat(rp,&st)){
    if (autogen_not_up_to_date(st.st_mtim,vp,vp_l)) unlink(rp);
    else{
      static int noAtime=-1;
      if (noAtime<0) {
        struct statfs statfsbuf;
        statfs(_realpath_autogen,&statfsbuf);
        noAtime=0!=(statfsbuf.f_flags&ST_NOATIME);
      }
      if (noAtime) cg_file_set_atime(rp,&st,0);
    }
  }
}
static bool virtualpath_startswith_autogen(const char *vp, const int vp_l){
  return (vp_l==DIR_AUTOGEN_L || vp_l>DIR_AUTOGEN_L && vp[DIR_AUTOGEN_L]=='/') && !memcmp(vp,DIR_AUTOGEN,DIR_AUTOGEN_L);
}

static long autogen_size_of_not_existing_file(const char *vp,const int vp_l){
  if (!virtualpath_startswith_autogen(vp,vp_l)) return -1;
  char autogen_from[MAX_PATHLEN];
  uint64_t size=0,computed_size=-1;
  struct zippath zpath={0};
  FOR(i,0,AUTOGEN_MAX_DEPENDENCIES){
    if (!config_autogen_dependencies(i,vp,vp_l,autogen_from,&size)){
      if (!i) return -1;
      break;
    }


    if ((i || size==AUTOGEN_FILESIZE_ORIG_SIZE) && *autogen_from && !(zpath_init(&zpath,autogen_from),find_realpath_any_root(0,&zpath,NULL))) return -1;
    if (!i){
      switch(size){
      case AUTOGEN_FILESIZE_ORIG_SIZE: computed_size=zpath.stat_rp.st_size;break;
      }
    }
  }
  return computed_size>0?computed_size:size>=0?size:1234;
}
static void autogen_filldir(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
  const int name_l=strlen(name);
  if (ENDSWITH(name,name_l,EXT_CONTENT)) return;
  char generated[MAX_PATHLEN];
  for(int i=0; config_autogen_from_virtualpath(i,generated,name,name_l);i++){
    //    if (ht_only_once(no_dups,generated,0)) filler(buf,generated,stbuf,0,fill_dir_plus);
    filldir(0,filler,buf,generated,stbuf,no_dups);
  }
  return;
  //ht_only_once(no_dups,name,0);  filler(buf,name,stbuf,0,fill_dir_plus);
}

static bool autogen_cleanup_running;
static void *_autogen_cleanup_runnable(void *arg){
  if(_realpath_autogen){
    static int count;
    autogen_cleanup_running=true;
    assert(strstr(_realpath_autogen,DIR_AUTOGEN));
    if (strstr(_realpath_autogen,DIR_AUTOGEN)) config_autogen_cleanup(_realpath_autogen);
    autogen_cleanup_running=false;
  }
  return NULL;
}
static void autogen_cleanup(){
  static time_t when=0;
  static pthread_t thread_cleanup;
  if (!autogen_cleanup_running){
    pthread_create(&thread_cleanup,NULL,&_autogen_cleanup_runnable,NULL);
  }
}
static char *autogen_apply_replacements_for_cmd(const char *orig,const char *infile,const char *outfile, const char *tmpoutfile){
  char *c=(char*)orig;
  for(int len=strlen(c),capacity=len,i=0;;i++){
    const char *placeholder=PLACEHOLDERS[i];
    if (!placeholder) break;
    if (!strstr(c,placeholder)) continue;
    const int placeholder_l=strlen(placeholder);
    int replace_l=0;
    const char *replace=NULL;
    if (placeholder==PLACEHOLDER_EXTERNAL_QUEUE){
      static char q[PATH_MAX+1];
      LOCK(mutex_autogen_init, if (!*q) strcpy(strcpy(q,_self_exe)+cg_last_slash(_self_exe)+1,"ZIPsFS_autogen_queue.sh"));
      replace=q;
    }else if (placeholder==PLACEHOLDER_INPUTFILE){
      replace=infile;
    }else if (placeholder==PLACEHOLDER_OUTPUTFILE){
      replace=outfile;
    }else if (placeholder==PLACEHOLDER_TMP_OUTPUTFILE){
      replace=tmpoutfile;
    }else if (placeholder==PLACEHOLDER_MNT){
      replace=_mnt;
    }else if (placeholder==PLACEHOLDER_TMP_DIR){
      static char t[MAX_PATHLEN+1];
      LOCK(mutex_autogen_init,if (!*t) snprintf(t,MAX_PATHLEN,"%s"DIR_AUTOGEN"/.tmp",_root_writable->rootpath);cg_recursive_mkdir(replace=t));
    }else if (placeholder==PLACEHOLDER_INPUTFILE_NAME){
      replace=infile+cg_last_slash(infile)+1;
    }else if (placeholder==PLACEHOLDER_TMP_OUTPUTFILE_NAME){
      replace=tmpoutfile+cg_last_slash(tmpoutfile)+1;
    }else if (placeholder==PLACEHOLDER_OUTPUTFILE_NAME){
      replace=outfile+cg_last_slash(outfile)+1;
    }else if (placeholder==PLACEHOLDER_INPUTFILE_PARENT){
      replace=infile;
      if ((replace_l=cg_last_slash(infile))<0) replace_l=0;
    }else assert(false);
    if (replace && strlen(c)==placeholder_l){
      c=(char*)replace;
    }else{
      const int len2=cg_str_replace(OPT_STR_REPLACE_DRYRUN,c,len,placeholder,placeholder_l,replace,replace_l);
      if (len2>capacity){
        capacity=len2+333;
        char *c2=strcpy(malloc(capacity),c);
        if (c!=orig) free(c);
        c=c2;
      }
      len=cg_str_replace(0,c,len,placeholder,placeholder_l,replace,replace_l);
    }
  }
  return c;
}
static bool _autogen_is_placeholder(const char *c){
  for(const char **r=PLACEHOLDERS;*r;r++){
    if (*r<=c && c<*r+strlen(*r)) return true; /* if c is a pointer to the beginning or within string *r.  For 'within' see PLACEHOLDER_OUTPUTFILE_NAME  */
  }
  return false;
}

static void autogen_apply_replacements(char *cmd[],char *cmd_orig[], const char *infile, const char *outfile, const char *tmpoutfile){
  for(int i=0;cmd_orig[i];i++){
    cmd[i]=autogen_apply_replacements_for_cmd(cmd_orig[i],infile,outfile,tmpoutfile);
  }
}


static void autogen_free_cmd(char *cmd[],char *cmd_orig[]){
  for(int i=0;cmd_orig[i];i++){
    if (cmd[i]!=cmd_orig[i] && !_autogen_is_placeholder(cmd_orig[i])){
      FREE(cmd[i]);
    }
  }
}




/* static bool autogen_queue_is_running(){ */
/*     const pid_t pid=fork(); */
/*     static char *cmd[]={"pgrep","-f",ZIPSFS_QUEUE,(char *)0}; */
/*     if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);  return false;} */
/*     if (!pid) cg_exec(NULL,cmd,-1,-1); */
/*     return 0==cg_waitpid_logtofile_return_exitcode_f(pid,stderr); */
/* } */


/* static bool autogen_queue_and_wait(const char *infile, const char *outfile, const char *err){ */
/*     const pid_t pid=fork(); */
/*     static char *cmd[]={ZIPSFS_QUEUE,infile,outfile,(char *)0}; */
/*     if (pid<0){ log_errno("fork() waitpid 1 returned %d",pid);  return false;} */
/*     if (!pid) cg_exec(NULL,cmd,-1,-1); */
/*     return 0==cg_waitpid_logtofile_return_exitcode(pid,err); */


/* } */
