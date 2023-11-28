////////////////////////
/// Check File names ///
////////////////////////

static void _assert_validchars(enum validchars t,const char *s,int len,const char *msg,const char *fn){
  static bool initialized;
  if (!initialized) initialized=validchars(VALIDCHARS_PATH)[PLACEHOLDER_NAME]=validchars(VALIDCHARS_FILE)[PLACEHOLDER_NAME]=true;
  const int pos=find_invalidchar(t,s,len);
  if (pos>=0){
    LOCK_NCANCEL(mutex_validchars,
                 static struct ht ht={0};
                 if (!ht.capacity) ht_init(&ht,HT_FLAG_INTKEY|12);
                 if (!ht_set_int(&ht,hash32(s,len),len,"X")) warning(WARN_CHARS|WARN_FLAG_ONCE_PER_PATH,s,ANSI_FG_BLUE"%s()"ANSI_RESET" %s: position: %d",fn,msg?msg:"",pos));
  }
}
#define  assert_validchars_direntries(t,dir) _assert_validchars_direntries(t,dir,__func__)
static void _assert_validchars_direntries(enum validchars t,const struct directory *dir,const char *fn){
  if (dir){
    RLOOP(i,dir->core.files_l){
      const char *s=dir->core.fname[i];
      _assert_validchars(VALIDCHARS_PATH,s,my_strlen(s),dir->dir_realpath,fn);
    }
  }
}

////////////////////////////
/// File name extension ///
//////////////////////////

#if 0
    const char *ss[]={"/mypath/subdir/file.txt", "file_no_path.txt", "/mypath/subdir/file_no_ext","","file.wiff.scan","file.wiff", "file.extension_is_too_long",NULL};
    const uint64_t wiff=(uint64_t)".wiff";
    for(int i=0; ss[i];i++){
      LOCK(mutex_fhdata,
           const char *e=fileExtension(ss[i],my_strlen(ss[i]));
           printf("Testing  fileExtension()%40s %10s   is .wiff: %s\n",ss[i],e,yes_no(((uint64_t)e)==wiff));
           );
    }
    exit(0);
#endif //0

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

///////////////
/// pthread ///
///////////////
#define ASSERT_LOCKED_FHDATA() assert_locked(mutex_fhdata)
#if DO_ASSERT_LOCK
/* Count recursive locks with (_mutex+mutex). Maybe inc or dec. */
static int mutex_count(int mutex,int inc){
  int8_t *locked=pthread_getspecific(_pthread_key);
  if (!locked){
    const int R=(ROOTS*THREADS_PER_ROOT+99);
    static int i=0;
    static int8_t all_locked[R][mutex_roots+ROOTS];
    pthread_mutex_lock(_mutex+mutex_idx);i++;pthread_mutex_unlock(_mutex+mutex_idx);
    ASSERT(i<R);
    pthread_setspecific(_pthread_key,locked=all_locked[i]);
    ASSERT(locked!=NULL);
  }
#undef R
  //log_debug_now("mutex_count %s %d \n",MUTEX_S[mutex],locked[mutex]);
  if (inc>0) ASSERT(locked[mutex]++<127);
  if (inc<0) ASSERT(locked[mutex]-->=0);
  return locked[mutex];
}
#define assert_locked(mutex)        assert(_assert_locked(mutex,true,false));
#define assert_not_locked(mutex)    assert(_assert_locked(mutex,false,false));
static bool _assert_locked(int mutex,bool yesno,bool log){
  const int count=mutex_count(mutex,0), ok=(yesno==(count>0));
  //if (log || !ok) log_debug_now("_assert_locked %s  %s: %d\n",yes_no(yesno),MUTEX_S[mutex],count);
  return ok;
}
#else
#define mutex_count(mutex,inc);
#define assert_locked(mutex)
#define assert_not_locked(mutex)
#endif
/////////////////////////////////////////////////////////////

#if 0
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d){
  if (!d->fname){ log_error("%s %s %s: d->fname is NULL\n",__func__,func,msg);exit(9);}
  const bool print=(strchr(msg,'/')!=NULL);
  if (print) printf("\n"ANSI_INVERSE"%s Directory %s   files_l=%d\n"ANSI_RESET,func,msg,d->files_l);
  FOR(0,i,d->files_l){
    const char *n=d->fname[i];
    if (!n){ printf("%s %s: d->fname[%d] is NULL\n",__func__,func,i);exit(9);}
    const int len=strnlen(n,MAX_PATHLEN);
    if (len>=MAX_PATHLEN){ log_error("%s %s %s: strnlen d->fname[%d] is %d\n",__func__,func,msg,i,len);exit(9);}
    const char *s=Nth0(d->fname,i);
    if (print) printf(" (%d)\t%"PRIu64"\t%'zu\t%s\t%p\t%lu\n",i,Nth0(d->finode,i), Nth0(d->fsize,i),s,s,hash_value_strg(s));
  }
}
#endif



#if 0
static bool debug_path(const char *vp){
  return vp!=NULL && NULL!=strstr(vp,
                                  //"20230116_Z1_ZW_001_30-0061_poolmix_2ug_ZenoSWATH_T600_V4000_rep01"
                                  "20230116_Z1_ZW_001_30-0061_poolmix_2ug_ZenoSWATH_T600_V4000_rep02"
                                  );
static void _debug_nanosec(const char *msg,const int i,const char *path,struct timespec *t){
  if (!t->tv_nsec){
    log_debug_now("%s #%d path: %s\n",msg,i,path);
  }
}
#define DEBUG_NANOSEC(i,path,t) _debug_nanosec(__func__,i,path,t)
#endif


//////////////////////////////////
/// Trigger by magic file name ///
//////////////////////////////////



static void debug_trigger_files(const char *path){
  {
    const int t=
      (!strcmp(path,FILE_DEBUG_BLOCK"_S"))?PTHREAD_STATQUEUE:
      (!strcmp(path,FILE_DEBUG_BLOCK"_M"))?PTHREAD_MEMCACHE:
      (!strcmp(path,FILE_DEBUG_BLOCK"_D"))?PTHREAD_DIRCACHE:
      -1;
    if (t>=0){
      warning(WARN_DEBUG,path,"Triggered blocking of threads %s",PTHREAD_S[t]);
      foreach_root(i,r) r->debug_pretend_blocked[t]=true;
    }
  }
  {
    const int t=
      (!strcmp(path,FILE_DEBUG_CANCEL"_S"))?PTHREAD_STATQUEUE:
      (!strcmp(path,FILE_DEBUG_CANCEL"_M"))?PTHREAD_MEMCACHE:
      (!strcmp(path,FILE_DEBUG_CANCEL"_D"))?PTHREAD_DIRCACHE:
      -1;
    if(t>=0){
      warning(WARN_DEBUG,path,"Triggered canceling of threads %s",PTHREAD_S[t]);
      foreach_root(i,r) pthread_cancel(r->pthread[t]);
    }
  }
  if (!strcmp(path,FILE_DEBUG_KILL)){
    warning(WARN_MISC,path,"Triggered exit of ZIPsFS");
    while(_fhdata_n){
      log_msg("Waiting for _fhdata_n  %d to be zero\n",_fhdata_n);
      usleep(300);
    }
    foreach_root(i,r){
      mstore_clear(&r->dircache);
      ht_destroy(&r->dircache_ht);
    }
    exit(0);
  }
}
static bool debug_fhdata(const struct fhdata *d){ return d && !d->n_read && tdf_or_tdf_bin(d->path);}


 static bool endsWithDotD(const char *path){
return path &&  ENDSWITH(path,strlen(path),".d");
 }
