/***********************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                 */
/* D download to the local disk                                        */
/* Downloaded files get the sticky bit S_ISVTX to facilitate cleanup.  */
/*  find $M -type f  -perm -1000  */
/***********************************************************************/
_Static_assert(WITH_PRELOADDISK,"");
#define  NOT_PRELOADDISK_ZPATH_FOR_DIR(zpath) (zpath->root==_root_writable && !zpath->is_decompressed || !_writable_path_l || VFOLDER_HAS_FLAG2(zpath,PRELOADDISK_NOT,PRELOAD_UPDATE))

/***********************************************************/
/* Given a virtual file within DIR_PRELOADDISK_XXX.    */
/* Construct the path of the preloaded file                    */
/* Return true if this file exists.                        */
/***********************************************************/

static bool preloaddisk_test_realpath_preloaded_strcat_rp(const bool isUpdate, zpath_t *zpath){
  //log_entered_function("RP:%s",RP());
  char rp[PATH_MAX]; *rp=0;
  struct stat st={0};
  if(isUpdate){
    int rp_l=snprintf(rp,PATH_MAX,"%s%s%s",_writable_path,DIR_PRELOADED,VFOLDER_PATH_L(zpath)+VP());
    if (ENDSWITH(rp,rp_l,SFX_UPDATE)) rp[(rp_l-=SFX_UPDATE_L)]=0;
    //log_debug_now("isUpdate %s rp:%s  stat:%s",VP(),rp,success_or_fail(!stat(rp,&st)));
    if (stat(rp,&st)) return false;
  }else{
    if (NOT_PRELOADDISK_ZPATH_FOR_DIR(zpath)) return false;
    char dst[PATH_MAX];
    realpath_writable_folder(dst,zpath,DIR_PRELOADED);
    if (stat(rp,&st) || !(st.st_mode&S_IFREG)) return false;
  }
  zpath_set_realpath(zpath,rp,NULL,NULL);
  if (isUpdate)    zpath->stat_rp=st;
  zpath_stat(0,zpath);
  zpath->root=_root_writable;
  //log_exited_function("RP:%s",RP());
  return true;
}

static bool _preloaddisk_now(const char *dst,fHandle_t *d){
  zpath_t *zpath=&d->zpath;
  //log_debug_now("RP:%s preloadram_advise=%d",RP(),preloadram_advise(zpath,0));
  //log_entered_function("RP:%s  %'jd bytes  dst: %s   %'jd bytes   decomp:%s",RP(),IM(cg_file_size(RP())), dst,IM(cg_file_size(dst)),cg_compression_file_ext(zpath->is_decompressed,NULL));
  //log_debug_now(ANSI_MAGENTA"%s is_decompressed %d"ANSI_RESET,cg_compression_file_ext(zpath->is_decompressed,NULL), zpath->is_decompressed,NULL);
  bool ok=false;
  if (ZPF(ZP_IS_ZIPENTRY)){ // USED_TO_BE_ZP_TRY_ZIP
    if (!VFOLDER_HAS_FLAG(zpath,PRELOADDISK_ENTIRE_ZIP_NOT) && (zpath->root->preload_entire_zip||VFOLDER_HAS_FLAG(zpath,PRELOADDISK_ENTIRE_ZIP))){
      /**************************************************************************************************************************/
      /* Consider mnt/db/pride/2022/12/PXD036786/20220121_Z1_ZW_001_30-0043_K562_SWATH_3.91ng_3.wiff2                           */
      /* It is part of a Zip file. The Zip file will be entirely loaded.                                                        */
      /* Unless preloadram_advise(zpath,0), the the ZIP entry will be extracted and stored as a file in the writeable root.     */
      /* The follwoing zipentry will not be extracted to file because it is preloaded to RAM                                    */
      /*      mnt/db/pride/2026/07/PXD062492/20231031_PRO3_KTT_174_50-0121_AstraZeneca-PROTAC_P07_B09.d/analysis.tdf            */
      /**************************************************************************************************************************/
      const int vfolder_l=VFOLDER_PATH_L(zpath); // z_l=VP0_L()+_writable_path_l+sizeof(DIR_PRELOADED)-1-vfolder_l;
      char z[VP0_L()+_writable_path_l+sizeof(DIR_PRELOADED)]; cg_stpncpy0(stpcpy(stpcpy(z,_writable_path),DIR_PRELOADED),VP0()+vfolder_l,VP0_L()-vfolder_l);
      const ssize_t z_s=cg_file_size(z),rp_s=zpath->stat_rp.st_size;
      if (z_s<=0 || z_s!=rp_s){
        if (z_s>=0) warning(WARN_PRELOADDISK,z," %s size %'zd differs from %s %'zd",z, z_s,RP(),rp_s);
        if (!cg_copy_url_or_file(0,RP(),z,&root_loading_active,(void*)ZPR())) return false;
      }
      zpath_set_realpath(zpath,z,NULL,NULL);
      zpath->root=_root_writable;
      IF1(WITH_PRELOADRAM, if (preloadram_advise(zpath,0)){d->zpath.flags|=ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY; d->flags|=FHANDLE_WITH_PRELOADRAM;
          //log_debug_now(ANSI_MAGENTA"ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY for %s  ZP_IS_ZIPENTRY:%d "ANSI_RESET,RP(), ZPF(ZP_IS_ZIPENTRY));
          return true;});
    }
    zip_t *za=NULL;
    zip_file_t *zip=(za=my_zip_open(RP()))?my_zip_fopen(za,EP(),ZIP_RDONLY,RP()):NULL;
    if (zip){
      cg_recursive_mk_parentdir(dst);
      TMP_FOR_FILE(tmp,dst);
      const int fo=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
      if (fo){
        char buf[1<<20];
        for(int n; (n=my_zip_fread(zip,buf,1<<20,RP()))>0;){
          PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
          if (write(fo,buf,n)>0) ok=true;
          PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
        }
        close(fo);
      }else{
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,tmp,"open()");
      }
      if (!ok){
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,RP(),"Read error -> %s %jd bytes ",tmp,IM(cg_file_size(tmp)));
        unlink(tmp);
      }else if (!(ok=cg_rename_tmp_outfile(tmp,dst))){
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"rename");
      }
    }
    my_zip_fclose(zip,RP());
    my_zip_close(za,RP());
    cg_vmtouch_e(RP());
  }else{
    ok=cg_copy_url_or_file(zpath->is_decompressed,RP(),dst,&root_loading_active,(void*)ZPR());
  }
  if (ok){
    struct stat st;
    if (stat(dst,&st)) log_errno("fstat  %s",dst);   // !!!!!!!!!!!
    else if (chmod(dst,st.st_mode|S_ISVTX)==-1) log_errno("fchmod  %s",dst);
    cg_file_set_mtime(dst,zpath->stat_vp.st_mtime); /* Note using same fd for futimes() fails*/
    zpath_set_realpath(zpath,dst,NULL,NULL);
    //DIE_DEBUG_NOW("zip  VP:%s RP:%s dst=%s",VP(),RP(),dst);
    zpath->root=_root_writable;

    IF1(WITH_ZIPFLATCACHE, LOCK(mutex_dircache, zipflatcache_drop(zpath))); // root
  }
  //log_exited_function("%s  %s",dst,success_or_fail(ok));
  return ok;
} /* _preloaddisk_now */
static bool preloaddisk_now(const char *dst,fHandle_t *d){
  const zpath_t *zpath=&d->zpath;
  log_entered_function("VP:%s \n    %s "ANSI_YELLOW"==>"ANSI_RESET"  \n    %s",VP(),RP(),dst);
  assert(_writable_path_l);
  LOCK(mutex_fhandle, d->flags=(d->flags&~FHANDLE_PRELOADDISK_QUEUE)|FHANDLE_PRELOADDISK_RUN);
  const bool success=_preloaddisk_now(dst,d);
  LOCK(mutex_fhandle, d->flags&=~(FHANDLE_PRELOADDISK_RUN|FHANDLE_PRELOADDISK_QUEUE));
  log_exited_function(ANSI_YELLOW"==>"ANSI_RESET" %s %s",RP(),success_or_fail(success));
  return success;
}


static bool fHandle_preloadfile_now(fHandle_t *d){
  //log_entered_function("fHandle_preloadfile_now %s %s   %d %d ",D_VP(d),D_RP(d), d!=NULL,  PRELOADDISK_WRITABLE_REALPATH(dst, &d->zpath));
  const zpath_t *zpath=&d->zpath;
  if (d && !NOT_PRELOADDISK_ZPATH_FOR_DIR(zpath)){
    char dst[MAX_PATHLEN+1];
    realpath_writable_folder(dst,zpath,DIR_PRELOADED);
    if (preloaddisk_now(dst,d)){
      D_ROOT(d)=_root_writable;
      return true;
    }
  }
  return false;
}



/*******************************************************************************/
/* Any fHandle_t instance with the same path which is already queued or loaded.  */
/* Avoid loading same path several times                                       */
/*******************************************************************************/
static fHandle_t *preloaddisk_fhandle(const fHandle_t *d){
  const char *vp=D_VP(d);
  const int vp_l=D_VP_L(d);
  FOREACH_FHANDLE(id,e){
    if (e!=d && (e->flags&(FHANDLE_PRELOADDISK_QUEUE|FHANDLE_PRELOADDISK_RUN)) && D_VP_L(e)==vp_l && !memcmp(D_VP(e),vp,vp_l)) return e;
  }
  return NULL;
}
/**********************************************************************************************************************************/
/* Given a virtual file, this function finds the preloaded local file.                                                            */
/* The virtual file must be contained in specific places like DIR_PRELOADDISK_R.                                              */
/* Alternatively it can be located in a root  with the root_t->prefetch==true.                                                  */
/* If the preloaded file does not yet exist, the fHandle_t is marked such that the preload is performed from another thread. */
/* Note:  (!zpath->is_decompressed) means uninitialized. Once initialized in path_with_compress_sfx_exists(zpath) at least (1<<COMPRESSION_NIL) is set. */
/**********************************************************************************************************************************/
static bool is_preloaddisk_zpath(zpath_t *zpath){
  if (NOT_PRELOADDISK_ZPATH_FOR_DIR(zpath)) return false;
  assert(ZPR());
  const bool yes=(ZPR()->preload ||
                  zpath->is_decompressed ||
                  ((ZPR()->decompress_mask&(1<<zpath->is_decompressed)) || VFOLDER_HAS_FLAG(zpath,VIEWMOD_DECOMPRESS)) &&  path_with_compress_sfx_exists(zpath) ||
                  VFOLDER_HAS_FLAG(zpath,PRELOADDISK) && is_preload_by_selectors(zpath));
  //log_exited_function("VP:%s RP:%s  root:%s vfolder:%s  %s    ",VP(),RP(),ZPRP(),VFOLDER_PATH(zpath),success_or_fail(yes));
  return yes;
}

static int preloaddisk(fHandle_t *d){
  zpath_t *zpath=&d->zpath;
  root_t *r=ZPR();
  struct stat st={0};
  bool ok=!NOT_PRELOADDISK_ZPATH_FOR_DIR(zpath);
  if (ok){
    char dst[MAX_PATHLEN+1];
    realpath_writable_folder(dst,zpath,DIR_PRELOADED);
    ok=cg_is_regular_file(dst);
  }
  //log_entered_function("%s   root:%s already:%d",D_VP(d),rootpath(r),ok);
  if (!ok){
    root_start_thread(ZPR(),PTHREAD_PRELOAD,false);
  again_with_other_root:
    while(true){
#define FHQ_FHAQ FHANDLE_PRELOADDISK_QUEUE|FHANDLE_PRELOAD_ALREADY_QUEUED
      LOCK_N(mutex_fhandle,fHandle_t *g=preloaddisk_fhandle(d);if (!g && !(d->flags&(FHQ_FHAQ|FHANDLE_PRELOADDISK_RUN))) d->flags|=FHQ_FHAQ);
      PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,false);
      for(int i=0;;i++){
        LOCK(mutex_fhandle, ok=!((g?g:d)->flags&(FHANDLE_PRELOADDISK_QUEUE|FHANDLE_PRELOADDISK_RUN)));
        if (ok) break;
        usleep(1000*100);
      }
      PRELOADFILE_ROOT_UPDATE_TIME(d,r,false);
      //ok=ok && !stat(D_ZPF(ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY)?D_RP(d):  dst,&st);
      ok=ok && !stat(D_RP(d),&st);
      if (!g && find_realpath_other_root(zpath)){ // Via find_realpath_other_root() ->  test_realpath_pfx() -> strgs_l wird immer laenger.
        LOCK(mutex_fhandle, d->flags&=~FHANDLE_PRELOADDISK_RUN);
        goto again_with_other_root;
      }
      break;
    }
    ok=st.st_ino!=0;
  }
  if (ok){
        lock(mutex_fhandle);
        //if (!ZPF(ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY)) zpath_set_realpath(zpath,dst,NULL,NULL);
        zpath->stat_rp=zpath->stat_vp=st;
        ZPR()=_root_writable;
        unlock(mutex_fhandle);
  }
  //log_exited_function("%s %s %p   RP:%s  %'ld bytes",success_or_fail(ok), VP(),d,RP(),cg_file_size(RP()));
  return ok?0:ENOENT;
}
/*****************************************************/
/* virtual files should also exist if file.gz exists */
/*****************************************************/
// DIR_PRELOADED
static int zpath_decompress_mask(const zpath_t *zpath){
  //  return VFOLDER_HAS_FLAG3(zpath,PRELOAD_UPDATE,VIEWMOD_INTERNET,VIEWMOD_DECOMPRESS) || ZPF(ZP_TRY_DECOMPRESS)? COMPRESSION_MASK:  ZPR()?ZPR()->decompress_mask: 0;
  return VFOLDER_HAS_FLAG2(zpath,VIEWMOD_INTERNET,VIEWMOD_DECOMPRESS) || ZPF(ZP_TRY_DECOMPRESS)? COMPRESSION_MASK:  ZPR()?ZPR()->decompress_mask: 0;
}

static void _path_with_compress_sfx_exists(zpath_t *zpath){
  assert(zpath);
  //  const int decompress_mask=ZPF(ZP_TRY_DECOMPRESS)||VFOLDER_HAS_FLAG(zpath,VIEWMOD_DECOMPRESS)?COMPRESSION_MASK:ZPR()->decompress_mask;
  const int decompress_mask=zpath_decompress_mask(zpath);//ZPF(ZP_TRY_DECOMPRESS)||VFOLDER_HAS_FLAG(zpath,VIEWMOD_DECOMPRESS)?COMPRESSION_MASK:ZPR()->decompress_mask;
  if (decompress_mask){
    char gz[RP_L()+(COMPRESSION_EXT_MAX_LEN+1)], *e=stpcpy(gz,RP());
    zpath->is_decompressed=COMPRESSION_NIL;
    FOR(i,1,COMPRESSION_NUM){
      if (decompress_mask&(1<<i)){
        stpcpy(e,cg_compression_file_ext(i,NULL));
        if (!stat(gz,&zpath->stat_rp)){
          zpath->is_decompressed=i;
          zpath->stat_vp=zpath->stat_rp;
          zpath->stat_vp.st_size=nextRepdigitFileSize(64*zpath->stat_rp.st_size);
          break;
        }
      }
    }
  }
}
static bool path_with_compress_sfx_exists(zpath_t *zpath){
  if (!ZPF2(ZP_FLAG2_CHECKED_EXISTENCE_COMPRESSED)){
    if (ZPR() && _writable_path_l) _path_with_compress_sfx_exists(zpath);
    zpath->flags2|=ZP_FLAG2_CHECKED_EXISTENCE_COMPRESSED;
  }
  //if (ENDSWITH(VP(),VP_L(),"wiff") && ZPR() && strstr(rootpath(ZPR()),"CHA-CHA"))    log_exited_function("Root:%s %s  %s  %x",rootpath(ZPR()), VP(),success_or_fail(zpath->is_decompressed), zpath_decompress_mask(zpath));
  return zpath->is_decompressed;
}
/*****************************************************************************************************************/
/*  Users trigger updating  pre-loaded files by reading corresponding                                            */
/*  virtual files located in mnt/zipsfs/lrz/DIRNAME_PRELOADED_UPDATE/                                            */
/*  The preloaded file resides in <root1>/zipsfs/lr/                                                             */
/*  If the original exists and has a different mtime or size then the function tries to make a local copy .      */
/*  If this failes, the old local file will be kept.                                                             */
/*****************************************************************************************************************/
static void preloaddisk_uptodate_or_update(fHandle_t *d){
  //log_entered_function("vp:%s rp:%s",D_VP(d), D_RP(d));
  const int vdir_l=VFOLDER_PATH_L(&d->zpath);
  IF1(WITH_PRELOADRAM,if (d->preloadram && d->preloadram->txtbuf) return);
  const int vp_l=D_VP_L(d)-vdir_l-SFX_UPDATE_L;
  char vp[vp_l+1];
  cg_stpncpy0(vp,D_VP(d)+vdir_l,vp_l);
  NEW_VIRTUALPATH(vp);
  NEW_ZIPPATH(&vipa);
  zpath->flags|=ZP_TRY_DECOMPRESS;
  const bool found=find_realpath_in_roots(0,zpath,~1L);
  assert(zpath->root!=_root);
  fHandle_t d2={0}; d2.zpath=*zpath;
  yes_zero_no_t updateSuccess=(found && zpath->stat_rp.st_mtime==d->zpath.stat_rp.st_mtime)?ZERO:  preloaddisk_now(D_RP(d),&d2)?YES:NO;
  //  IF1(WITH_PRELOADRAM,html_is_uptodate(d,updateSuccess, &d->zpath, found?RP():"No source file found", &zpath->stat_rp,VP()));
  IF1(WITH_PRELOADRAM,html_is_uptodate(d,updateSuccess, D_RP(d), &d->zpath.stat_rp, found?RP():"No source file found", &zpath->stat_rp,VP()));
}
/*
static int preloaddisk_filename_for_updatedir(char *u, int u_l, const char *vparent){
  //log_entered_function(ANSI_MAGENTA"u:%s vparent:%s"ANSI_RESET,u,vparent);
  const char *pp[]={_writable_path,DIR_PRELOADED,vparent,"/",u,NULL};
  struct stat st;
  cg_stat_concat_path(&st,pp);
  if (st.st_mode&(S_IFDIR)) return u_l;
  strcpy(u+u_l,SFX_UPDATE);
  return (S_IFREG==(st.st_mode&(S_ISVTX|S_IFREG)))?0:SFX_UPDATE_L+u_l;
}
*/
