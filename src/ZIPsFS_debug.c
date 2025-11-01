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
  ASSERT_LOCKED_FHANDLE();
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
#define assert_validchars(...) _assert_validchars(__VA_ARGS__,__func__)

static void _assert_validchars(enum enum_validchars t,const char *s,int s_l,const char *fn){
  if (t==VALIDCHARS_PATH||t==VALIDCHARS_FILE) cg_validchars(t)[PLACEHOLDER_NAME]=true;
  const int pos=cg_find_invalidchar(t,s,s_l);
  if (pos<0) return;
  lock(mutex_validchars);
  if (!ht_numkey_set(&_ht_valid_chars,hash32(s,s_l),s_l,"X")){
    char encoded[PATH_MAX];
    url_encode(encoded,PATH_MAX,s);
    warning(WARN_CHARS|WARN_FLAG_ONCE_PER_PATH,encoded,ANSI_FG_BLUE"%s()"ANSI_RESET": position: %d",fn,pos);
  }
  unlock(mutex_validchars);
}
#define  assert_validchars_direntries(...) _assert_validchars_direntries(__VA_ARGS__,__func__)
static void _assert_validchars_direntries(const struct directory *dir,const char *fn){
  if (dir){
    RLOOP(i,dir->core.files_l){
      const char *s=dir->core.fname[i];
      if (s) assert_validchars(VALIDCHARS_PATH,s,strlen(s));
    }
  }
}

/* #define debug_directory_print(dir) _debug_directory_print(dir,__func__,__LINE__); */
/*   static void _debug_directory_print(const struct directory *dir,const char *fn,const int line){ */
/*     if (dir){ */
/*       const struct directory_core *d=&dir->core; */
/*       log_msg("%s():%d "ANSI_INVERSE"Directory"ANSI_RESET" rp: %s files_l: %d  destroyed: %d debug: %d\n",fn,line,DIR_RP(dir), d->files_l, dir->dir_is_destroyed,dir->debug); */
/*       log_msg("d->fname: %p directory_is_stack: %d   \n",d->fname, d->fname==dir->_stack_fname); */
/*       RLOOP(i,d->files_l){ */
/*         const char *s=d->fname[i]; */
/*         log_msg(" (%d) %p '%s'  size: %'lld\n",i,s,snull(s),(LLD)(!d->fsize?-1:d->fsize[i])); */
/*       } */
/*     } */
/*   } */

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

#define directory_print(dir,maxNum) _directory_print(__func__,__LINE__,dir,maxNum)
static void _directory_print(const char *func,const int line,const struct directory *dir,const int maxNum){
  const struct directory_core *d=&dir->core;
  ASSERT(d->fname);
  const struct ht *hti=IF01(WITH_TIMEOUT_READDIR,NULL,dir->ht_intern_names);
  fprintf(stderr,"\n"ANSI_INVERSE"%s:%d  Directory %p '%s'   files_l: %d/%d  destroyed: %s success: %s  ht-intern: %s\n"ANSI_RESET,func,line,dir,DIR_RP(dir),d->files_l,  dir->files_capacity, yes_no(dir->dir_is_destroyed), yes_no(dir->dir_is_success),yes_no(NULL!=hti));
  assert(d->files_l<=dir->files_capacity);
  FOR(i,0,d->files_l){
    const char *n=d->fname[i];
    if (!n) continue;
    const int len=strnlen(n,MAX_PATHLEN);
    if (len>=MAX_PATHLEN){ log_error("%s %s: strnlen d->fname[%d] is %d\n",__func__,func,i,len);}
    if (i<maxNum){
          char encoded[PATH_MAX];
          const bool invalid=len!=url_encode(encoded,PATH_MAX,n);
      //        fprintf(stderr,"%s (%d)\t%"PRIu64"\t%'zu\t%s\t%u\n"ANSI_RESET,invalid?ANSI_FG_RED:"",i,Nth0(d->finode,i), Nth0(d->fsize,i),encoded,hash_value_strg(n));
      fprintf(stderr,"%s:%d ",func,line);
      fprintf(stderr,"%s (%d)\t%"PRIu64"\t%'lld\t%s\n"ANSI_RESET,invalid?ANSI_FG_RED:"",i,Nth0(d->finode,i), (LLD)Nth0(d->fsize,i),encoded);
    }
  }
}



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
#if DEBUG_DIRCACHE_COMPARE_CACHED
#define dde_print(f,...) fprintf(stderr,ANSI_YELLOW"DDE "ANSI_RESET f,__VA_ARGS__)
static void debug_compare_directory_a_b(struct directory *A,struct directory *B){
  bool diff=false;
#define print_realpath() dde_print("dir_realpath  '%s'  '%s'\n",DIR_RP(A),DIR_RP(B))
  if (DIR_RP(A) && DIR_RP(B) &&  strcmp(DIR_RP(A),DIR_RP(B))){
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
  } //else dde_print(GREEN_SUCCESS"%s\n",DIR_RP(B));
}
#endif //DEBUG_DIRCACHE_COMPARE_CACHED




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
