/***********************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                 */
/* D download to the local disk                                        */
/* Downloaded files get the sticky bit S_ISVTX to facilitate cleanup.  */
/*  find $M -type f  -perm -1000  */
/***********************************************************************/
_Static_assert(WITH_PRELOADDISK,"");



/***********************************************************/
/* Given a virtual file within DIR_PRELOADDISK_XXX.    */
/* Construct the path of the preloaded file                    */
/* Return true if this file exists.                        */
/***********************************************************/
static bool preloaddisk_writable_realpath(const zpath_t *zpath, char dst[MAX_PATHLEN+1]){
  //log_entered_function("%s  %s",VP(),success_or_fail(is_preloaddisk_zpath(zpath)));
  if (!is_preloaddisk_zpath(zpath)) return false;
  stpcpy(stpcpy(dst,_writable_path),VP());
  //log_exited_function("dst: %s",dst);;
  return true;
}
static bool _preloaddisk_now(const char *dst,zpath_t *zpath,fHandle_t *d){
  //log_entered_function("RP:%s  %'lld bytes  dst: %s   %'lld bytes   decomp:%s",RP(),(LLD)cg_file_size(RP()), dst,(LLD)cg_file_size(dst),cg_compression_file_ext(zpath->is_decompressed,NULL));
  bool ok=false;
  if (ZPF(ZP_IS_ZIP)){ // USED_TO_BE_ZP_TRY_ZIP
    zip_t *za=NULL;
    zip_file_t *zip=(za=my_zip_open(RP()))?my_zip_fopen(za,EP(),ZIP_RDONLY,RP()):NULL;
    if (zip){
      TMP_FOR_FILE(tmp,dst);
      cg_recursive_mk_parentdir(dst);
      fputc('r',stderr);
      //log_verbose("Going to write %s",tmp);
      const int fo=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
      if (fo){
        char buf[1<<20];
        for(int n; (n=zip_fread(zip,buf,1<<20))>0;){
          PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
          if (write(fo,buf,n)>0) ok=true;
          PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
        }
        close(fo);
      }else{
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,tmp,"open()");
      }
      if (!ok){
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,RP(),"Read error -> %s %lld bytes ",tmp,(LLD)cg_file_size(tmp));
        unlink(tmp);
      }else if (!(ok=cg_rename_tmp_outfile(tmp,dst))){
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"rename");
      }
    }
    if (zip) zip_fclose(zip);
    if (za) zip_close(za);
    cg_vmtouch_e(RP());
  }else{
    ok=cg_copy_url_or_file(zpath->is_decompressed,RP(),dst,&root_loading_active,(void*)zpath->root);
  }
  if (ok){
    struct stat st;
    if (stat(dst, &st)==-1) log_errno("fstat  %s",dst);   // !!!!!!!!!!!
    else if (chmod(dst,st.st_mode|S_ISVTX)==-1) log_errno("fchmod  %s",dst);
    cg_file_set_mtime(dst,zpath->stat_vp.st_mtime); /* Note using same fd for futimes() fails*/
    IF1(WITH_ZIPFLATCACHE, LOCK(mutex_dircache, zipflatcache_drop(zpath))); // root
  }
  //log_exited_function("%s  %s",dst,success_or_fail(ok));
  return ok;
}
static bool preloaddisk_now(const char *dst,zpath_t *zpath,fHandle_t *d){
  assert(_writable_path_l);
  zpath_t zp;
  if (!zpath){ if (!d) return false;  LOCK(mutex_fhandle, zp=d->zpath; zpath=&zp); }
  //assert(zpath->root!=_root_writable);
  LOCK(mutex_fhandle, if (d) d->flags=(d->flags&~FHANDLE_PRELOADFILE_QUEUE)|FHANDLE_PRELOADFILE_RUN);
  const bool success=_preloaddisk_now(dst,zpath,d);
  LOCK(mutex_fhandle, if (d) d->flags&=~(FHANDLE_PRELOADFILE_RUN|FHANDLE_PRELOADFILE_QUEUE));
  return success;
}

static bool fHandle_preloadfile_now(fHandle_t *d){
  char dst[MAX_PATHLEN+1];
  return d && preloaddisk_writable_realpath(&d->zpath,dst) &&  preloaddisk_now(dst,NULL,d);
}

/*******************************************************************************/
/* Any fHandle_t instance with the same path which is already queued or loaded.  */
/* Avoid loading same path several times                                       */
/*******************************************************************************/
static fHandle_t *preloaddisk_fhandle(const fHandle_t *d){
  const char *vp=D_VP(d);
  const int vp_l=D_VP_L(d);
  foreach_fhandle(id,e){
    if (e!=d && (e->flags&(FHANDLE_PRELOADFILE_QUEUE|FHANDLE_PRELOADFILE_RUN)) && D_VP_L(e)==vp_l && !memcmp(D_VP(e),vp,vp_l)) return e;
  }
  return NULL;
}
/**********************************************************************************************************************************/
/* Given a virtual file, this function finds the preloaded local file.                                                            */
/* The virtual file must be contained in specific places like DIR_PRELOADDISK_R.                                              */
/* Alternatively it can be located in a root  with the root_t->prefetch==true.                                                  */
/* If the preloaded file does not yet exist, the fHandle_t is marked such that the preload is performed from another thread. */
/**********************************************************************************************************************************/
static bool is_preloaddisk_zpath(const zpath_t *zpath){
  if (zpath->dir==DIR_PLAIN) return false;
  const root_t *r=zpath->root;
  if (r && _writable_path_l){
    if (zpath->is_decompressed) return true;
    ASSERT(zpath->flags|ZP_CHECKED_EXISTENCE_COMPRESSED);
    /* Note:  (!zpath->is_decompressed) means uninitialized. Once initialized at least (1<<COMPRESSION_NIL) is set. */
    //if (r->decompress_mask && (!(zpath->flags|ZP_CHECKED_EXISTENCE_COMPRESSED) || r->preload || (r->decompress_mask&((1<<zpath->is_decompressed)))))    return true;
#define x zpath->preloadpfx
    if (x){
      cg_log_file_stat("",&zpath->stat_rp);
      if ((x==DIR_PRELOADDISK_R || x==DIR_PRELOADDISK_RC || x==DIR_PRELOADDISK_RZ) && r->remote) return true;
      if (x==DIR_PRELOADDISK_RZ && ZPF(ZP_IS_ZIP)) return true;
      if (x==DIR_PRELOADDISK_RC && (ZPF(ZP_IS_ZIP|ZP_IS_COMPRESSEDZIPENTRY)==(ZP_IS_ZIP|ZP_IS_COMPRESSEDZIPENTRY) || (r->decompress_mask&~(1<<COMPRESSION_NIL)))) return true;
    }
#undef x
  }
  return false;
}

static int preloaddisk(fHandle_t *d){
  zpath_t *zpath=&d->zpath;
  root_t *r=zpath->root;
  if (!r || r==_root_writable) return EACCES;
  //  LOCK_N(mutex_fhandle, bool ok=zpath->preloadpfx || zpath->root && zpath->root->preload && (VP_L()+_writable_path_l+50<MAX_PATHLEN));
  LOCK_N(mutex_fhandle, bool ok=is_preloaddisk_zpath(zpath) && (VP_L()+_writable_path_l+50<MAX_PATHLEN));
  //log_entered_function("%s  r:%s ok:%d",D_VP(d),rootpath(r),ok);
  if (!ok) return ENOENT;
  char dst[MAX_PATHLEN+1];
  struct stat st={0};
  ok=preloaddisk_writable_realpath(zpath,dst) && !stat(dst,&st);
  if (!ok){
    root_start_thread(zpath->root,PTHREAD_PRELOAD,false);
  again_with_other_root:
    while(true){
      LOCK_N(mutex_fhandle,fHandle_t *g=preloaddisk_fhandle(d);if (!g && !(d->flags&(FHANDLE_PRELOADFILE_QUEUE|FHANDLE_PRELOADFILE_RUN))) d->flags|=FHANDLE_PRELOADFILE_QUEUE);
      PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,false);
      for(int i=0;;i++){
        LOCK(mutex_fhandle, ok=!((g?g:d)->flags&(FHANDLE_PRELOADFILE_QUEUE|FHANDLE_PRELOADFILE_RUN)));
        if (ok) break;
        usleep(1000*100);
      }
      PRELOADFILE_ROOT_UPDATE_TIME(d,r,false);
      ok=ok && !stat(dst,&st);
      if (!g && find_realpath_other_root(zpath)){
        LOCK(mutex_fhandle, d->flags&=~FHANDLE_PRELOADFILE_RUN);
        goto again_with_other_root;
      }
      break;
    }
    ok=st.st_ino!=0;
  }
  if (ok){
    lock(mutex_fhandle);
    zpath_reset_keep_VP(zpath);
    ZPATH_NEWSTR(realpath);
    ZPATH_STRCAT(dst);
    zpath->stat_rp=zpath->stat_vp=st;
    ZPATH_COMMIT(realpath);
    zpath->root=_root_writable;
    unlock(mutex_fhandle);
    ok=(d->fd_real=open(RP(),O_RDONLY))>0;
  }
  //log_exited_function("%s %s %p   RP:%s  %'ld bytes",success_or_fail(ok), D_VP(d),d,D_RP(d),cg_file_size(RP()));
  return ok?0:ENOENT;
}
/*****************************************************/
/* virtual files should also exist if file.gz exists */
/*****************************************************/
static void _path_with_compress_sfx_exists(zpath_t *zpath){
  //log_entered_function("'%s' preloadpfx:'%s'",VP(),zpath->preloadpfx);
  const int decompress_mask=(zpath->preloadpfx)?-1:zpath->root->decompress_mask;
  if (decompress_mask){
    char gz[RP_L()+(COMPRESSION_EXT_MAX_LEN+1)], *e=stpcpy(gz,RP());
    zpath->is_decompressed=COMPRESSION_NIL;
    FOR(i,1,COMPRESSION_NUM){
      if (decompress_mask&(1<<i)){
        stpcpy(e,cg_compression_file_ext(i,NULL));
        if (!stat(gz,&zpath->stat_rp)){
          zpath->is_decompressed=i;
          zpath->stat_vp=zpath->stat_rp;
          zpath->stat_vp.st_size=closest_with_identical_digits(100*zpath->stat_rp.st_size);
          break;
        }
      }
    }
  }
}
static bool path_with_compress_sfx_exists(zpath_t *zpath){
  if (!(zpath->flags&ZP_CHECKED_EXISTENCE_COMPRESSED)){
    if (zpath->root && _writable_path_l) _path_with_compress_sfx_exists(zpath);
    zpath->flags|=ZP_CHECKED_EXISTENCE_COMPRESSED;
  }
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
  //log_entered_function("%s",D_VP(d));
  IF1(WITH_PRELOADRAM,if (d->preloadram && d->preloadram->txtbuf) return);

  const int preloadpfx_l=cg_strlen(d->zpath.preloadpfx);
  NEW_VIRTUALPATH(D_VP(d)+preloadpfx_l);
  NEW_ZIPPATH(&vipa);
  const bool found=find_realpath_in_roots(0,zpath,~1L);
  yes_zero_no_t updateSuccess=(found && zpath->stat_rp.st_mtime==d->zpath.stat_rp.st_mtime)?ZERO:  preloaddisk_now(D_RP(d),zpath,NULL)?YES:NO;
  IF1(WITH_PRELOADRAM,html_is_uptodate(d,updateSuccess, &d->zpath, found?RP():"No source file found", &zpath->stat_rp,VP()));
}
