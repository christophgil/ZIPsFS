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
  if (!is_preloaddisk_zpath(zpath)) return false;
  //stpcpy(stpcpy(stpcpy(dst,_root_writable->rootpath), zpath->preloadpfx?DIR_PRELOADDISK_R:""),VP());
  stpcpy(stpcpy(dst,_root_writable->rootpath),VP());
    log_exited_function("dst: %s",dst);;
  return true;
}
static bool _preloaddisk_now(const char *dst,zpath_t *zpath,fHandle_t *d){
  log_entered_function("RP:%s dst: %s  decomp:%s",RP(),dst,cg_compression_file_ext(zpath->is_decompressed,NULL));
  bool ok=false;
  if (ZPF(ZP_TRY_ZIP)){
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
      }else if (!cg_rename_tmp_outfile(tmp,dst)){
        warning(WARN_PRELOADDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"rename");
        ok=false;
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
    if (stat(dst, &st)==-1) log_errno("fstat  %s",dst);
    else if (chmod(dst,st.st_mode|S_ISVTX)==-1) log_errno("fchmod  %s",dst);
    cg_file_set_mtime(dst,zpath->stat_vp.st_mtime); /* Note using same fd for futimes() fails*/
    IF1(WITH_ZIPINLINE_CACHE, LOCK(mutex_dircache, zipinline_cache_drop_vp(VP(),VP_L())));
  }
  //log_exited_function("%s  %s",dst,success_or_fail(ok));
  return ok;
}
static bool preloaddisk_now(const char *dst,zpath_t *zpath,fHandle_t *d){
  assert(_root_writable);
  zpath_t zp;
  if (!zpath){ if (!d) return false;  LOCK(mutex_fhandle, zp=d->zpath; zpath=&zp); }
  assert(zpath->root!=_root_writable);
  LOCK(mutex_fhandle, if (d) d->flags=(d->flags&~FHANDLE_PRELOADFILE_QUEUE)|FHANDLE_PRELOADFILE_RUN);
  const bool success=_preloaddisk_now(dst,zpath,d);
  LOCK(mutex_fhandle, if (d) d->flags&=~(FHANDLE_PRELOADFILE_RUN|FHANDLE_PRELOADFILE_QUEUE));
  return success;
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


#define   D_IS_PRELOADDISK_UPDATE(d)      (_root_writable &&  d->zpath.root==_root_writable && d->zpath.dir==DIR_PRELOADED_UPDATE)

static bool is_preloaddisk_zpath(const zpath_t *zpath){
  if (zpath->dir==DIR_PLAIN) return false;
  const root_t *r=zpath->root;
  if (r && _root_writable){
    ASSERT(zpath->flags|ZP_CHECKED_EXISTENCE_COMPRESSED);
    /* Note:  (!zpath->is_decompressed) means uninitialized. Once initialized at least (1<<COMPRESSION_NIL) is set. */
    if (r->preload_flags && (!(zpath->flags|ZP_CHECKED_EXISTENCE_COMPRESSED) || (r->preload_flags&((1<<zpath->is_decompressed)|PRELOAD_YES)))){
      return true;
    }
    if (zpath->preloadpfx){
      if ((zpath->preloadpfx==DIR_PRELOADDISK_R || zpath->preloadpfx==DIR_PRELOADDISK_RC || zpath->preloadpfx==DIR_PRELOADDISK_RZ) && r->remote) return true;
      if (zpath->preloadpfx==DIR_PRELOADDISK_RZ && ZPF(ZP_IS_ZIP)) return true;
      if (zpath->preloadpfx==DIR_PRELOADDISK_RC && (ZPF(ZP_IS_ZIP|ZP_IS_COMPRESSEDZIPENTRY)==(ZP_IS_ZIP|ZP_IS_COMPRESSEDZIPENTRY) || (r->preload_flags&~(1<<COMPRESSION_NIL)))) return true;

    }
  }
  return false;
}

static int preloaddisk(fHandle_t *d){
  zpath_t *zpath=&d->zpath;
  root_t *r=zpath->root;
  if (!r || r==_root_writable) return EACCES;
  //  LOCK_N(mutex_fhandle, bool ok=zpath->preloadpfx || zpath->root && zpath->root->preload && (VP_L()+_root_writable->rootpath_l+50<MAX_PATHLEN));
  LOCK_N(mutex_fhandle, bool ok=is_preloaddisk_zpath(zpath) && (VP_L()+_root_writable->rootpath_l+50<MAX_PATHLEN));
  //log_entered_function("%s  r:%s ok:%d",D_VP(d),rootpath(r),ok);
  if (!ok) return ENOENT;
  char dst[MAX_PATHLEN+1];
  struct stat st={0};
  ok=preloaddisk_writable_realpath(zpath,dst) && !stat(dst,&st);
  if (!ok){
    root_start_thread(zpath->root,PTHREAD_PRELOAD,false);
  again_with_other_root:
    while(true){
      LOCK_N(mutex_fhandle,  fHandle_t *g=preloaddisk_fhandle(d); if (!g && !(d->flags&(FHANDLE_PRELOADFILE_QUEUE|FHANDLE_PRELOADFILE_RUN))) d->flags|=FHANDLE_PRELOADFILE_QUEUE);
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
    LOCK(mutex_fhandle, zpath_copy_rp(zpath,dst,&st); zpath->root=_root_writable);
    ok=(d->fd_real=open(RP(),O_RDONLY))>0;
  }
  //log_exited_function("%s %s %p   RP:%s  %'ld bytes",success_or_fail(ok), D_VP(d),d,D_RP(d),cg_file_size(RP()));
  return ok?0:ENOENT;
}
static void zpath_copy_rp(zpath_t *zpath, const char *rp, const struct stat *st){
  zpath_reset_keep_VP(zpath);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath, rp);
  zpath->stat_rp=zpath->stat_vp=*st;
}
/*****************************************************/
/* virtual files should also exist if file.gz exists */
/*****************************************************/
static bool path_with_gz_exists(zpath_t *zpath, const root_t *r){
  //log_entered_function("'%s' preloadpfx:'%s'",VP(),zpath->preloadpfx);
  const int decompress_flags=zpath->preloadpfx || !r?-1:r->decompress_flags;
  if (!decompress_flags) return false;
  char gz[RP_L()+4], *e=stpcpy(gz,RP());
  if (!(zpath->flags&ZP_CHECKED_EXISTENCE_COMPRESSED)){
    zpath->is_decompressed=COMPRESSION_NIL;
    FOR(iCompress,1,COMPRESSION_NUM){
      if (decompress_flags&(1<<iCompress)){
        stpcpy(e,cg_compression_file_ext(iCompress,NULL));
        if (!stat(gz,&zpath->stat_rp)) zpath->is_decompressed=iCompress;
      }
    }
    zpath->flags|=ZP_CHECKED_EXISTENCE_COMPRESSED;
  }
  if (zpath->is_decompressed==COMPRESSION_NIL) return false;
  zpath->stat_vp=zpath->stat_rp;
  zpath->stat_vp.st_size=closest_with_identical_digits(100*zpath->stat_rp.st_size);
  return true;
}
/*****************************************************************************************************************/
/*  Users trigger updating  pre-loaded files by reading corresponding                                            */
/*  virtual files located in mnt/ZIPsFS/lrz/DIRNAME_PRELOADED_UPDATE/                                            */
/*  The preloaded file resides in <root1>/ZIPsFS/lr/                                                             */
/*  If the original exists and has a different mtime or size then the function tries to make a local copy .      */
/*  If this failes, the old local file will be kept.                                                             */
/*****************************************************************************************************************/
static void preloaddisk_uptodate_or_update(fHandle_t *d){
  //log_entered_function("%s",D_VP(d));
  IF1(WITH_PRELOADRAM,if (d->preloadram && d->preloadram->txtbuf) return);
  const int preloadpfx_l=cg_strlen(d->zpath.preloadpfx);
  NEW_VIRTUALPATH(D_VP(d)+preloadpfx_l);
  NEW_ZIPPATH(&vipa);
  const bool found=find_realpath_roots_by_mask(0,zpath,~1L);
  const char *dst=D_RP(d);
  const struct stat *copy=&d->zpath.stat_rp;
  const char *status=found?ANSI_FG_MAGENTA"found-original"ANSI_RESET:ANSI_FG_RED"not-found-original"ANSI_RESET;
  char buf[222+2*MAX_PATHLEN];
  int n=sprintf(buf,"Preloaded\t%s\t%'ld\t%s\t%s\n",ST_MTIME(copy),copy->st_size,dst,status);
  if (found){
    const struct stat *orig=&zpath->stat_vp;
    status=ANSI_FG_BLUE"up-to-date"ANSI_RESET;
    if ((orig->st_mtime!=copy->st_mtime)){
      char bak[MAX_PATHLEN+60];
      sprintf(bak,"%s.%d.bak",dst,getpid());
      cg_rename(dst,bak);
      preloaddisk_now(dst,zpath,NULL);
      if (cg_file_size(dst)>=0){
        cg_unlink(bak);
        status=ANSI_FG_GREEN"Updated"ANSI_RESET;
      }else{
        status=RED_FAIL;
        cg_rename(bak,dst);
      }
    }
    n+=sprintf(buf+n,"Original\t%s\t%'ld\t%s%s\t%s\n",ST_MTIME(orig),orig->st_size,RP(),cg_compression_file_ext(zpath->is_decompressed,NULL),status);
  }
  buf[n++]='\n';
#undef T
#if WITH_PRELOADRAM
  textbuffer_t *b=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  textbuffer_add_segment(TXTBUFSGMT_DUP,b,buf,n);
  lock(mutex_fhandle);
  if (!fhandle_set_text(d,b)){
    FREE_NULL_MALLOC_ID(b);
  }else{
    IF1(WITH_PRELOADRAM,preloadram_set_status(d,preloadram_done));
    d->flags|=FHANDLE_PRELOADRAM_COMPLETE;
  }
  unlock(mutex_fhandle);
#else
  write(STDERR_FILENO,buf,n);
#endif //WITH_PRELOADRAM
}
