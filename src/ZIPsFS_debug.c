/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                     ///
/// Debugging ZIPsFS                                          ///
/////////////////////////////////////////////////////////////////
// cppcheck-suppress-file unusedFunction
static int countFhandleWithMemcache(const char *path, int len,int h){
  if (!len) len=strlen(path);
  if (!h) h=hash32(path,len);
  int count=0;
  IF1(WITH_MEMCACHE,foreach_fhandle(id,d)     if (D_VP_HASH(d)==h && d->memcache && d->memcache->txtbuf && !strcmp(path,D_VP(d))) count++);
  return count;
}

#define fhandleWithMemcachePrint(...) _fhandleWithMemcachePrint(__func__,__LINE__,__VA_ARGS__)
static void _fhandleWithMemcachePrint(const char *func,int line,const char *path, int len,int h){
#if WITH_MEMCACHE
  if (!len) len=strlen(path);
  if (!h) h=hash32(path,len);
  foreach_fhandle(id,d){
    if (D_VP_HASH(d)==h){
      const struct memcache *m=d->memcache;
      if (m && (m->txtbuf||m->memcache_status) && !strcmp(path,D_VP(d))){
        log_msg("%s:%d fhandleWithMemcachePrint: %d %s  memcache_status: %s memcache_l: %lld\n",func,line,id,path,MEMCACHE_STATUS_S[m->memcache_status],(LLD)m->memcache_l);
      }
    }
  }
#endif //WITH_MEMCACHE
}


#define debugSpecificPath(path,path_l) _debugSpecificPath(0,path,path_l)
#define debugSpecificPath_tdf(path,path_l) _debugSpecificPath(1,path,path_l)
#define debugSpecificPath_tdf_bin(path,path_l) _debugSpecificPath(2,path,path_l)
#define debugSpecificPath_d(path,path_l) _debugSpecificPath(3,path,path_l)

static bool _debugSpecificPath(int mode, const char *path, int path_l){
  if (!path) return false;
  if (!path_l) path_l=strlen(path);
  bool b=false;
  switch(mode){
  case 0: b=NULL!=strstr(path,"20230126_PRO1_KTT_017_30-0046_LisaKahl_P01_VNATSerAuxgM1evoM2Glycine5mM_dia_BF4_1_12110.d");break;
  case 1: b=ENDSWITH(path,path_l,"20230126_PRO1_KTT_017_30-0046_LisaKahl_P01_VNATSerAuxgM1evoM2Glycine5mM_dia_BF4_1_12110.d/analysis.tdf");break;
  case 2: b=ENDSWITH(path,path_l,"20230126_PRO1_KTT_017_30-0046_LisaKahl_P01_VNATSerAuxgM1evoM2Glycine5mM_dia_BF4_1_12110.d/analysis.tdf_bin");break;
      case 3: b=ENDSWITH(path,path_l,"20230126_PRO1_KTT_017_30-0046_LisaKahl_P01_VNATSerAuxgM1evoM2Glycine5mM_dia_BF4_1_12110.d");break;
  }
  if (b){
    const int n=countFhandleWithMemcache(path,path_l,0);
    if (n>1){
      log_error("path=%s   countFhandleWithMemcache=%d\n",path,n);
      fhandleWithMemcachePrint(path,path_l,0);
    }
    return true;
  }
  return false;
}




////////////////////////
/// Check File names ///
////////////////////////
#define assert_validchars(t,s,len,msg) _assert_validchars(t,s,len,msg,__func__)

static void _assert_validchars(enum enum_validchars t,const char *s,int len,const char *msg,const char *fn){
  static bool initialized;
  // cppcheck-suppress duplicateConditionalAssign
  if (!initialized) initialized=cg_validchars(VALIDCHARS_PATH)[PLACEHOLDER_NAME]=cg_validchars(VALIDCHARS_FILE)[PLACEHOLDER_NAME]=true;
  const int pos=cg_find_invalidchar(t,s,len);
  if (pos>=0){
    LOCK_NCANCEL(mutex_validchars,
                 if (!ht_numkey_set(&_ht_valid_chars,hash32(s,len),len,"X")) warning(WARN_CHARS|WARN_FLAG_ONCE_PER_PATH,s,ANSI_FG_BLUE"%s()"ANSI_RESET" %s: position: %d",fn,msg?msg:"",pos));
  }
}
#define  assert_validchars_direntries(t,dir) _assert_validchars_direntries(t,dir,__func__)
static void _assert_validchars_direntries(enum enum_validchars t,const struct directory *dir,const char *fn){
  if (dir){
    RLOOP(i,dir->core.files_l){
      const char *s=dir->core.fname[i];
      if (s) _assert_validchars(VALIDCHARS_PATH,s,cg_strlen(s),dir->dir_realpath,fn);
    }
  }
}

#define debug_directory_print(dir) _debug_directory_print(dir,__func__,__LINE__);
static void _debug_directory_print(const struct directory *dir,const char *fn,const int line){
  if (dir){
    const struct directory_core *d=&dir->core;
    log_msg(ANSI_INVERSE"%s():%d Directory rp: %s files_l: %d\n"ANSI_RESET,fn,line,dir->dir_realpath, d->files_l);
    RLOOP(i,d->files_l){
      const char *s=d->fname[i];
      if(s)log_msg(" (%d) '%s'\n",i,snull(s));
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
  LOCK(mutex_fhandle,
       const char *e=fileExtension(ss[i],cg_strlen(ss[i]));
       fprintf(stderr,"Testing  fileExtension()%40s %10s   is .wiff: %s\n",ss[i],e,yes_no(((uint64_t)e)==wiff));
       );
 }
EXIT(0);
#endif //0

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////

#if 0
static void directory_debug_filenames(const char *func,const char *msg,const struct directory_core *d){
  if (!d->fname){ log_error("%s %s %s: d->fname is NULL\n",__func__,func,msg);EXIT(9);}
  const bool print=(strchr(msg,'/')!=NULL);
  if (print) fprintf(stderr,"\n"ANSI_INVERSE"%s Directory %s   files_l=%d\n"ANSI_RESET,func,msg,d->files_l);
  FOR(i,0,d->files_l){
    const char *n=d->fname[i];
    if (!n) continue;
    const int len=strnlen(n,MAX_PATHLEN);
    if (len>=MAX_PATHLEN){ log_error("%s %s %s: strnlen d->fname[%d] is %d\n",__func__,func,msg,i,len);EXIT(9);}
    const char *s=Nth0(d->fname,i);
    if (print) fprintf(stderr," (%d)\t%"PRIu64"\t%'zu\t%s\t%p\t%lu\n",i,Nth0(d->finode,i), Nth0(d->fsize,i),s,s,hash_value_strg(s));
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
      log_verbose("%s #%d path: %s\n",msg,i,path);
    }
  }
}
#define DEBUG_NANOSEC(i,path,t) _debug_nanosec(__func__,i,path,t)
#endif


//////////////////////////////////
/// Trigger by magic file name ///
//////////////////////////////////



static bool debug_fhandle(const struct fHandle *d){
  return d && !d->n_read && tdf_or_tdf_bin(D_VP(d));
}
static void debug_fhandle_listall(void){
  log_msg(ANSI_INVERSE"%s"ANSI_RESET"\n",__func__);
  foreach_fhandle(id,d){
    log_msg("d %p path: %s fh: %llu\n",d,D_VP(d),(LLU)d->fh);
  }
}





/*
  WITH_DIRCACHE leads to errors  ls: cannot access '/home/cgille/tmp/ZIPsFS/mnt/Z1/Data/30-0089/20231124_Z1_LRS_079_30-0089_AHSG_5k2kV_300C_OxoScan_rep02.raw
  .
  Clearing the cache resolves.
*/
#define DEBUG_DIRCACHE_COMPARE_CACHED 0 /*TO_HEADER*/
#if DEBUG_DIRCACHE_COMPARE_CACHED
#define dde_print(f,...) fprintf(stderr,ANSI_YELLOW"DDE "ANSI_RESET f,__VA_ARGS__)
static void debug_compare_directory_a_b(struct directory *A,struct directory *B){
  bool diff=false;
#define print_realpath() dde_print("dir_realpath  '%s'  '%s'\n",A->dir_realpath,B->dir_realpath)
  if (A->dir_realpath && B->dir_realpath &&  strcmp(A->dir_realpath,B->dir_realpath)){
    print_realpath();
    diff=true;
  }
  struct directory_core a=A->core,b=B->core;
  if (a.files_l!=b.files_l){
    dde_print("files_l  %d  %d\n",a.files_l,b.files_l);
    diff=true;
  }else{
    FOR(i,0,b.files_l){
      if ((!a.fname[i])!=(!b.fname[i]) || a.fname[i] && strcmp(a.fname[i],b.fname[i])){
        dde_print("fname[%d]  %s %s\n",i,a.fname[i],b.fname[i]);
        diff=true;
      }
#define D(f,F) if (a.f && a.f[i]!=b.f[i]) { dde_print(#f " [%d]  " F " " F " \n",i,a.f[i],b.f[i]); diff=true;}
      D(fsize,"%zu"); D(finode,"%lu"); D(fmtime,"%lu"); D(fcrc,"%u"); D(fflags,"%d");
#undef D
    }
  }
  if (diff){
    print_realpath();
    exit_ZIPsFS();
  } //else dde_print(GREEN_SUCCESS"%s\n",B->dir_realpath);
}
static void debug_dircache_compare_cached(struct directory *mydir,const struct stat *rp_stat){
  bool dde_result;
  struct directory dde_dir={0};
  directory_init(DIRECTORY_IS_ZIPARCHIVE,&dde_dir,mydir->dir_realpath,mydir->root);
  LOCK_NCANCEL(mutex_dircache, dde_result=dircache_directory_from_cache(&dde_dir,rp_stat->ST_MTIMESPEC)?1:0);
  if (dde_result) debug_compare_directory_a_b(&dde_dir,mydir);
}
#endif //DEBUG_DIRCACHE_COMPARE_CACHED



#define DEBUG_TRACK_FALSE_GETATTR_ERRORS 0
#if DEBUG_TRACK_FALSE_GETATTR_ERRORS
static void debug_track_false_getattr_errors(const char *vp,const int vp_l){
  if ((ENDSWITH(vp,vp_l,".SSMetaData") || ENDSWITH(vp,vp_l,".raw")  )){
    log_verbose("vp=%s",vp);
    NEW_ZIPPATH(vp);
    const bool found=find_realpath_any_root(0,zpath,NULL);
    log_zpath("",zpath);
    exit_ZIPsFS();
    usleep(1000*500);
  }
}
#endif //DEBUG_TRACK_FALSE_GETATTR_ERRORS

///////////////////////////////////////////////////////////////////
#if WITH_EXTRA_ASSERT
static bool debug_trigger_vp(const char *vp,const int vp_l){
  return  !strcmp("/PRO2/Data/50-0139",vp) ||
    !strcmp("/PRO2/Data",vp) ||
    !strcmp("/PRO2",vp) ||
    ENDSWITH(vp,vp_l,".d") ||
    ENDSWITH(vp,vp_l,".tdf") ||
    ENDSWITH(vp,vp_l,".tdf_bin");
}
#else
#define debug_trigger_vp(...) false
#endif
