///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS         ///
/// Dynamically generated files ///
///////////////////////////////////
#define PLACEHOLDER_INFILE "PLACEHOLDER_INFILEx"
#define PLACEHOLDER_TMP_OUTFILE "PLACEHOLDER_TMP_OUTFILEx"
#define PLACEHOLDER_OUTFILE "PLACEHOLDER_OUTFILEx" // e.g /slow3/Users/x/ZIPsFS/modifications/ZIPsFS/a/6600-tof3/Data/50-0086/20240229S_TOF3_FA_060_50-0086_OxoScan-MSBatch04_P08_H01.wiff.scan
#define PLACEHOLDER_MNT "PLACEHOLDER_MNTx"
#define PLACEHOLDER_TMP_DIR "PLACEHOLDER_TMP_DIRx"
#define PLACEHOLDER_INFILE_NAME "PLACEHOLDER_INFILE_NAMEx"
#define PLACEHOLDER_TMP_OUTFILE_NAME "PLACEHOLDER_TMP_OUTFILE_NAMEx"
#define PLACEHOLDER_OUTFILE_NAME "PLACEHOLDER_OUTFILE_NAME"
#define PLACEHOLDER_INFILE_PARENT "PLACEHOLDER_INFILE_PARENTx"
#define PLACEHOLDER_OUTFILE_PARENT "PLACEHOLDER_OUTFILE_PARENTx"
#define PLACEHOLDER_EXTERNAL_QUEUE  "PLACEHOLDER_EXTERNAL_QUEUEx"

static char *_realpath_autogen;
const char *PLACEHOLDERS[]={
  PLACEHOLDER_EXTERNAL_QUEUE,
  PLACEHOLDER_INFILE,PLACEHOLDER_TMP_OUTFILE,PLACEHOLDER_MNT,PLACEHOLDER_INFILE_NAME,PLACEHOLDER_TMP_OUTFILE_NAME,PLACEHOLDER_OUTFILE,PLACEHOLDER_OUTFILE_NAME,PLACEHOLDER_INFILE_PARENT,PLACEHOLDER_OUTFILE_PARENT,PLACEHOLDER_TMP_DIR,NULL};



////////////////////////////////////////////////////////////
/// To be implemented in ZIPsFS_configuration_autogen.c  ///
////////////////////////////////////////////////////////////
static struct ht_entry *ht_entry_fsize(const char *vp,const int vp_l,const bool create){
   struct ht_entry *e=ht_numkey_get_entry(&ht_fsize,inode_from_virtualpath(vp,vp_l),0,create);
  return e;
}

static void autogen_run(struct fhdata *d){
  const char *rp=D_RP(d);
  log_entered_function("%s D_RP=%s\n",D_VP(d),rp);
  if (d->autogen_state) return;
  cg_recursive_mk_parentdir(rp);
  char tmp[MAX_PATHLEN+1];
  char err[MAX_PATHLEN+1];
  //snprintf(tmp,MAX_PATHLEN,"%s.%d.%ld.autogen.tmp",rp,getpid(),currentTimeMillis());
  {
    const int slash=cg_last_slash(rp);
    memcpy(tmp,rp,slash+1);
    snprintf(tmp+slash+1,MAX_PATHLEN,"autogen_tmp_%d_%lld_%s",getpid(),(LLU)currentTimeMillis(),rp+slash+1);
  }
  snprintf(err,MAX_PATHLEN,"%s.log",rp);
  struct textbuffer *buf=NULL;
  struct stat st={0};

  const int run_err=aimpl_run(D_VP(d),rp,tmp,err,&buf);
  if (buf){
    LOCK_N(mutex_fhdata, struct memcache *m=memcache_new(d);m->txtbuf=buf;m->memcache_already=m->memcache_l=textbuffer_length(buf));
    d->memcache=m;
    LOCK(mutex_fhdata,ht_entry_fsize(D_VP(d),D_VP_L(d),true)->value=(char*)textbuffer_length(buf));
    d->autogen_state=run_err?AUTOGEN_FAIL:AUTOGEN_SUCCESS;
  }else{
    if (PROFILED(stat)(tmp,&st)){
      warning(WARN_AUTOGEN|WARN_FLAG_ERRNO,tmp," size=%ld ino: %ld",st.st_size, st.st_ino);
      d->autogen_state=AUTOGEN_FAIL;
    }else{
      log_verbose("Size: %lld ino: %llu, Going to rename(%s,%s)\n",(LLD)st.st_size,(LLU)st.st_ino,tmp,rp);
      if (rename(tmp,rp)) log_errno("rename(%s,%s)",tmp,rp);
      else if (chmod(rp,0777)) log_errno("chmod(%s,0777)",rp);
      zpath_stat(false,&d->zpath,_root_writable);
      d->autogen_state=AUTOGEN_SUCCESS;
    }
  }
  //log_exited_function("%s AUTOGEN_SUCCESS=%d  m=%p m->txtbuf=%p",path,d->autogen_state==AUTOGEN_SUCCESS,m,m?m->txtbuf:NULL);
}
static bool virtualpath_startswith_autogen(const char *vp, const int vp_l){
  return (vp_l==DIR_AUTOGEN_L || vp_l>DIR_AUTOGEN_L && vp[DIR_AUTOGEN_L]=='/') && !memcmp(vp,DIR_AUTOGEN,DIR_AUTOGEN_L);
}

static int autogen_find_sourcefiles(struct path_stat path_stat[AUTOGEN_MAX_DEPENDENCIES],const char *vp,const int vp_l/*, const bool filter_remember_size*/){ /*NOT_TO_HEADER*/
  if (!virtualpath_startswith_autogen(vp,vp_l)) return 0;
  //  log_entered_function("%s",vp); cg_print_stacktrace(0);
  char autogen_from[MAX_PATHLEN+1];
  struct zippath zp,*zpath=&zp;
  int i=0;
  for(;i<AUTOGEN_MAX_DEPENDENCIES;i++){
    static struct stat empty_stat={0};
    path_stat[i].path[0]=0;
    path_stat[i].stat=empty_stat;

    if (!aimpl_for_genfile_iterate_sourcefiles(i,vp,vp_l,autogen_from)) break;
    if (*autogen_from && (zpath_init(zpath,autogen_from),find_realpath_any_root(0,zpath,NULL))){
      strcpy(path_stat[i].path, RP());
      path_stat[i].stat=zp.stat_rp;
    }
  }
  return i;
}
static bool autogen_not_up_to_date(struct timespec st_mtim,const char *vp,const int vp_l){
  struct path_stat path_stat[AUTOGEN_MAX_DEPENDENCIES];
  const int n=autogen_find_sourcefiles(path_stat,vp,vp_l);
  FOR(i,0,n){
    if (cg_timespec_b_before_a(st_mtim, path_stat[i].stat.ST_MTIMESPEC)) return true;
  }
  return false;
}


static long autogen_size_of_not_existing_file(const char *vp,const int vp_l){
  LOCK_N(mutex_fhdata,struct ht_entry *ht=ht_entry_fsize(vp,vp_l,false);long size=(long)ht->value);
  if (!size){
    bool cache_size=false;
    size=config_autogen_size_of_not_existing_file(vp,vp_l,&cache_size);
    if (cache_size){
      LOCK_N(mutex_fhdata,ht->value=(void*)size);
    }
  }
  return size;
}
static void autogen_remove_if_not_up_to_date(const char *vp,const int vp_l){
  char rp[MAX_PATHLEN+1];
  snprintf(rp,MAX_PATHLEN,"%s/%s",_root_writable->rootpath,vp);
  struct stat st={0};
  //log_entered_function("vp:%s rp:%s  %d ",vp, rp,cg_is_regular_file(rp));
  if (!PROFILED(stat)(rp,&st)){
    if (autogen_not_up_to_date(st.ST_MTIMESPEC,vp,vp_l)){
      unlink(rp);
    }else{
#if ! defined(HAS_NO_ATIME) || HAS_NO_ATIME
      static int noAtime=-1;
      if (noAtime<0) {
        struct statvfs statvfsbuf;
        statvfs(_realpath_autogen,&statvfsbuf);
        noAtime=0!=(statvfsbuf.f_flag&ST_NOATIME);
      }
      if (noAtime)
#endif
        cg_file_set_atime(rp,&st,0);
    }
  }
}






static void autogen_filldir(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
  const int name_l=strlen(name);
  if (ENDSWITH(name,name_l,EXT_CONTENT)) return;
  char generated[MAX_PATHLEN+1];
  for(int i=0; aimpl_virtualpath_from_virtualpath(i,generated,name,name_l);i++){
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
    if (strstr(_realpath_autogen,DIR_AUTOGEN)) aimpl_cleanup(_realpath_autogen);
    autogen_cleanup_running=false;
  }
  return NULL;
}
static void autogen_cleanup(void){
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
    }else if (placeholder==PLACEHOLDER_INFILE){
      replace=infile;
    }else if (placeholder==PLACEHOLDER_OUTFILE){
      replace=outfile;
    }else if (placeholder==PLACEHOLDER_TMP_OUTFILE){
      replace=tmpoutfile;
    }else if (placeholder==PLACEHOLDER_MNT){
      replace=_mnt;
    }else if (placeholder==PLACEHOLDER_TMP_DIR){
      static char t[MAX_PATHLEN+1];
      LOCK(mutex_autogen_init,if (!*t) snprintf(t,MAX_PATHLEN,"%s"DIR_AUTOGEN"/.tmp",_root_writable->rootpath);cg_recursive_mkdir(replace=t));
    }else if (placeholder==PLACEHOLDER_INFILE_NAME){
      replace=infile+cg_last_slash(infile)+1;
    }else if (placeholder==PLACEHOLDER_TMP_OUTFILE_NAME){
      replace=tmpoutfile+cg_last_slash(tmpoutfile)+1;
    }else if (placeholder==PLACEHOLDER_OUTFILE_NAME){
      replace=outfile+cg_last_slash(outfile)+1;
    }else if (placeholder==PLACEHOLDER_INFILE_PARENT){
      replace=infile;
      replace_l=MAX_int(0,cg_last_slash(infile));
    }else if (placeholder==PLACEHOLDER_OUTFILE_PARENT){
      replace=outfile;
      replace_l=MAX_int(0,cg_last_slash(outfile));
    }else assert(false);
    if (replace && strlen(c)==placeholder_l){
      c=(char*)replace;
    }else{
      const int len2=cg_str_replace(OPT_STR_REPLACE_DRYRUN,c,len,placeholder,placeholder_l,replace,replace_l);
      if (len2>capacity){
        capacity=len2+333;
        char *c2=strcpy(cg_malloc(MALLOC_autogen_cmd,capacity),c);
        if (c!=orig) cg_free(MALLOC_autogen_cmd,c);
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

static void autogen_apply_replacements(char *cmd[],char *cmd_orig[], const char *infile, const char *outfile, const char *tmpoutfile){
  for(int i=0;cmd_orig[i];i++){
    cmd[i]=autogen_apply_replacements_for_cmd(cmd_orig[i],infile,outfile,tmpoutfile);
  }
}


static void autogen_free_cmd(char *cmd[],char *cmd_orig[]){
  for(int i=0;cmd_orig[i];i++){
    if (cmd[i]!=cmd_orig[i] && !_autogen_is_placeholder(cmd_orig[i])) cg_free_null(MALLOC_autogen_cmd,cmd[i]);
  }
}




/* static bool autogen_queue_is_running(void){ */
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
