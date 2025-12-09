////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// D download to the local disk ///
////////////////////////////////////




/*****************************************************************************************/
/* Depending on the virtual file path return the special folder path.                    */
/* Cut, length of the additional path components before the normal virtual path starts.  */
/* negative values of cut: Folder for update preloaded files                             */
/*****************************************************************************************/


static const char *preloadfiledisk_type(int *cut, const char *vp, const int vp_l){
  *cut=0;
  if (!VP_STARTS(DIR_ZIPsFS)) return NULL;
  const char *dir=NULL;

  //#define C(d) if (VP_EQ_Z(d) || VP_STARTS_Z(d)){ dir=d; *cut=sizeof(d)-1;} else
#define C(d) if (PATH_STARTS_WITH_OR_EQ(vp,vp_l,d)){ dir=d; *cut=sizeof(d)-1;} else
  C(DIR_PRELOADFILEDISK_R) C(DIR_PRELOADFILEDISK_RC) C(DIR_PRELOADFILEDISK_RZ) C(DIR_ZIPsFS){};
#undef C
  if (dir){
    if (PATH_STARTS_WITH_OR_EQ(vp+*cut+1,vp_l-*cut-1,DIRNAME_PRELOADED_UPDATE)){
      *cut=-*cut-sizeof(DIRNAME_PRELOADED_UPDATE);
    }
    if (dir!=DIR_ZIPsFS || *cut) return dir;
  }
  *cut=0;
  return NULL;
}



static int preloadfiledisk_zpath_prepare_realpath(const bool fst_root_try_preload, struct zippath *zpath, const struct rootdata *r){

  const char *vp=VP();
  const int vp_l=VP_L();
  int cut=0;
  const char *dir=preloadfiledisk_type(&cut,vp,vp_l);
  const bool update=cut<0;
  //log_entered_function("%s %s",r->rootpath, vp);
  if (dir){
    if (update){
      if (r!=_root_writable) return INT_MAX;
      cut=-cut; /* ZIPsFS/lrc/_UPDATE_PRELOADED_FILES/xyz -> ZIPsFS/lr/ */
      //log_debug_now("vp:%s dir:%s cut:%d update:%d",vp, dir,cut,update);
      zpath->flags|=ZP_PRELOADFILEDISK_UPDATE;  /* Reading the file willt trigger preloadfiledisk() */
      zpath_strcat(zpath,dir==DIR_ZIPsFS?DIR_PRELOADED_BY_ROOT:DIR_PRELOADED_BY_PATH);
    }else if (dir==DIR_PRELOADED_BY_ROOT){
      return r!=_root_writable?INT_MAX:0;
    }else if (r==_root_writable){ /* All different virtual DIR_PRELOADFILEDISK_XXX  use the same DIR_PRELOADFILEDISK_R for storage. */
      zpath_strcat(zpath,DIR_PRELOADED_BY_PATH);   /* ZIPsFS/lrc/xyz -> ZIPsFS/lr/ */
    }else{ /* Files in DIR_PRELOADFILEDISK_XXX need to be copied to _root_writable under certain conditions */
#define C(t,f) (dir==t && (zpath->flags&(f)) || r->remote)
      if (C(DIR_PRELOADFILEDISK_R,0) || C(DIR_PRELOADFILEDISK_RC,ZP_ZIP|ZP_IS_COMPRESSED) || C(DIR_PRELOADFILEDISK_RZ,ZP_ZIP))  zpath->flags|=ZP_PRELOADFILEDISK;
#undef C
    }
    return cut;
  }

  //log_debug_now("%s %s   r->preload:%d "ANSI_RESET,r->preload?ANSI_MAGENTA:"",vp,r->preload);
  if (r->preload && r!=_root_writable){
    zpath->flags|=ZP_PRELOADFILEDISK;
  }

  if (fst_root_try_preload){  /* mnt/xyz  -> mnt/ZIPsFS/preload/ */
  /* The realpath in _root_writable has not been found previously. Now look in DIR_PRELOADED_BY_ROOT */
  assert(r==_root_writable);
  zpath_strcat(zpath,DIR_PRELOADED_BY_ROOT);
 }
return 0;
}

/***********************************************************/
/* Given a virtual file within DIR_PRELOADFILEDISK_XXX.    */
/* Construct the path of the preloaded file                    */
/* Return true if this file exists.                        */
/***********************************************************/
static bool preloadfiledisk_realpath(const struct zippath *zpath, char dst[MAX_PATHLEN+1]){
  int cut;
  preloadfiledisk_type(&cut,VP(), VP_L());
  if (cut<0) return false;
  stpcpy(stpcpy(stpcpy(dst,_root_writable->rootpath),zpath->root->preload? DIR_PRELOADED_BY_ROOT:DIR_PRELOADED_BY_PATH),cut+VP());
  return true;
}




/*******************************************************************************/
/* Unless the local file dst exists, it will be downloaded. */
/* dst will contain the downloaded path.                                       */
/* If parameter zpath is NULL, it will be obtained from parameter d.           */
/*******************************************************************************/
static bool preloadfiledisk_now(const char *dst,struct zippath *zpath,struct fHandle *d){
  assert(_root_writable);
  struct zippath zp;
  if (!zpath){ LOCK(mutex_fhandle, zp=d->zpath; zpath=&zp); }
  assert(zpath->root!=_root_writable);
  bool success=false;
  {
    //log_entered_function("'%s'",VP());
    char tmp[MAX_PATHLEN+1];
    snprintf(tmp,MAX_PATHLEN,"%s.preload.%d.%ld.tmp",dst,getpid(),time(NULL)-_whenStarted);
    unlink(tmp);
    off_t n=-4;
    {
      int fi=0;
      zip_t *za=NULL;
      zip_file_t *zip=NULL;
      if (ZPF(ZP_ZIP)) zip=(za=my_zip_open(RP()))?my_zip_fopen(za,EP(),ZIP_RDONLY,RP()):NULL;  else fi=open(RP(),O_RDONLY);
      //log_debug_now("'%s'  ZP_ZIP: %d  za: %p, zip: %p  fi: %d",RP(), ZPF(),za,zip,fi);
      if (zip || fi>0){
        n=-3;
        cg_recursive_mk_parentdir(dst);
        fputc('r',stderr);
        log_verbose("Going to write %s",tmp);
        const int fo=open(tmp,O_WRONLY|O_CREAT|O_TRUNC,S_IRUSR|S_IWUSR);
        if (fo){
          char buf[1<<20];
          while((n= zip?zip_fread(zip,buf,1<<20):read(fi,buf,1<<20))>0){
            //log_debug_now("n=%ld",n);
            PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
            write(fo,buf,n);
            PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,true);
          }
          close(fo);
          posix_fadvise(fi,0,0,POSIX_FADV_DONTNEED);
        }else{
          warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,tmp,"open()");
        }
      }
      if (zip) zip_fclose(zip);
      if (za) zip_close(za);
      if (fi<=0) fi=open(RP(),O_RDONLY);
      if (fi>0){ posix_fadvise(fi,0,0,POSIX_FADV_DONTNEED); close(fi);}
    }
    if (n>0 && strstr(_mnt,"/cgille/") && cg_file_size(dst)>=0) DIE_DEBUG_NOW(RED_WARNING" File dst: '%s' exists",dst);
    struct stat st;
    if (n<0){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,RP(),"Read error -> %s  n: %d",tmp,n);
      unlink(tmp);
      return false;
    }else if (stat(tmp,&st)){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,tmp,"%s",RP());
      unlink(tmp);
      return false;
    }else if (cg_rename(tmp,dst)){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"rename");
      unlink(tmp);
      return false;
    }else{
      log_verbose("Going cg_file_set_mtime %s %s",dst, ST_MTIME(&zpath->stat_vp));
      cg_file_set_mtime(dst,zpath->stat_vp.st_mtime);
      success=true;
    }
  }
  LOCK(mutex_fhandle,if (d) d->flags&=~(FHANDLE_FLAG_LCOPY_RUN|FHANDLE_FLAG_LCOPY_QUEUE));
  //log_exited_function("%s %s",RP(),success?GREEN_SUCCESS:RED_FAIL);
  return success;
}




/*******************************************************************************/
/* Any fHandle instance with the same path which is already queued or loaded.  */
/* Avoid loading same path several times                                       */
/*******************************************************************************/
static struct fHandle *preloadfiledisk_fhandle(const struct fHandle *d){
  const char *vp=D_VP(d);
  const int vp_l=D_VP_L(d);
  foreach_fhandle(id,e){
    if (e!=d && (e->flags&(FHANDLE_FLAG_LCOPY_QUEUE|FHANDLE_FLAG_LCOPY_RUN)) && D_VP_L(e)==vp_l && !memcmp(D_VP(e),vp,vp_l)){
      return e;
    }
  }
  return NULL;
}


/**********************************************************************************************************************************/
/* Given a virtual file, this function finds the preloaded local file.                                                            */
/* The virtual file must be contained in specific places like DIR_PRELOADFILEDISK_R.                                              */
/* Alternatively it can be located in a root  with the rootdata->prefetch==true.                                                  */
/* If the preloaded file does not yet exist, the struct fHandle is marked such that the preload is performed from another thread. */
/**********************************************************************************************************************************/

static bool preloadfiledisk(struct fHandle *d){
  if (!d || !_root_writable) return false;
  struct zippath *zpath=&d->zpath;
  struct rootdata *r=zpath->root;
  if (!r) return false;
  LOCK_N(mutex_fhandle, bool ok=(ZPF(ZP_PRELOADFILEDISK)) && zpath->root && (VP_L()+_root_writable->rootpath_l+50<MAX_PATHLEN));
  if (!ok) return false;
  //log_entered_function("%s", D_VP(d));
  char dst[MAX_PATHLEN+1];
  struct stat st={0};
  ok=preloadfiledisk_realpath(zpath,dst) && !stat(dst,&st);
  if (!ok){
    root_start_thread(zpath->root,PTHREAD_PRELOAD,false);
  again_with_other_root:
    for(int i=0;;i++){
      LOCK_N(mutex_fhandle,  struct fHandle *g=preloadfiledisk_fhandle(d); if (!g && !(d->flags&(FHANDLE_FLAG_LCOPY_QUEUE|FHANDLE_FLAG_LCOPY_RUN)))  d->flags|=FHANDLE_FLAG_LCOPY_QUEUE);
      PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,false);
      while(true){
        LOCK(mutex_fhandle, ok=!((g?g:d)->flags&(FHANDLE_FLAG_LCOPY_QUEUE|FHANDLE_FLAG_LCOPY_RUN)));
        if (ok && stat(dst,&st)) ok=false;
        if (ok) break;
        //if(preloadfile_time_exceeded(__func__,g?g:d,COUNT_LCOPY_WAITFOR_TIMEOUT)) break;
        //if (i%10==0) log_debug_now("Waiting for %s",dst);
        if (i%10==0) fputc('P',stderr);
        usleep(1000*100);
      }
      PRELOADFILE_ROOT_UPDATE_TIME(d,r,false);
      if (ok) break;
      if (!g){
        if (find_realpath_other_root(zpath)){
          LOCK(mutex_fhandle, d->flags&=~FHANDLE_FLAG_LCOPY_RUN);
          goto again_with_other_root;
        }
      }
    }
    ok=st.st_ino;
    if (!ok) log_error("Not exists %s",dst);
  }
  if (ok){
    LOCK(mutex_fhandle, zpath_copy_rp(zpath,dst,&st); zpath->root=_root_writable);
  }else{
    log_failed("%s RP:%s", VP(),RP());
  }
  //log_exited_function("%s %s %p   RP:%s",ok?GREEN_SUCCESS:RED_FAIL, D_VP(d),d,D_RP(d));
  return ok;
}

static void zpath_copy_rp(struct zippath *zpath, const char *rp, const struct stat *st){
  zpath_reset_keep_VP(zpath);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath, rp);
  zpath->stat_rp=zpath->stat_vp=*st;
}




/*****************************************************************************************************************/
/*  Users trigger updating  pre-loaded files by reading corresponding                                            */
/*  virtual files located in mnt/ZIPsFS/lrz/DIRNAME_PRELOADED_UPDATE/                                            */
/*  The preloaded file resides in <root1>/ZIPsFS/lr/                                                             */
/*  If the original exists and has a different mtime or size then the function tries to make a local copy .      */
/*  If this failes, the old local file will be kept.                                                             */
/*****************************************************************************************************************/
static void preloadfiledisk_uptodate_or_update(struct fHandle *d){
  IF1(WITH_PRELOADFILERAM,if (d->preloadfileram && d->preloadfileram->txtbuf) return);
  char buf[222+2*MAX_PATHLEN]; int n=0;
  int cut=0;
  const char *pfxdir=preloadfiledisk_type(&cut,D_VP(d),D_VP_L(d));
  assert(cut<0);
  NEW_ZIPPATH(D_VP(d)-cut);
  const int found=find_realpath_roots_by_mask(zpath,pfxdir==DIR_ZIPsFS?_rootmask_preload:~1L);
  const char *dst=D_RP(d);
  zpath_stat(zpath,NULL);
  struct stat *copy=&d->zpath.stat_rp;
  //ZPATH_LOG_FILE_STAT(&d->zpath); DIE_DEBUG_NOW("");
  const char *status=found?ANSI_FG_MAGENTA"found-original"ANSI_RESET:ANSI_FG_RED"not-found-original"ANSI_RESET;
  n+=sprintf(buf+n,"Preloaded\t%s\t%'ld\t%s\t%s\n",ST_MTIME(copy),copy->st_size,dst,status);
  log_debug_now("Preloaded %s",ST_MTIME(copy));
  if (found){
    const struct stat *orig=&zpath->stat_vp;
    status=ANSI_FG_BLUE"up-to-date"ANSI_RESET;
    log_debug_now("orig: %s  copy: %s ",ST_MTIME(orig),ST_MTIME(copy));
    if ((orig->st_mtime!=copy->st_mtime  ||  orig->st_size!=copy->st_size)){
      log_debug_now("Not up-to-date");
      char bak[MAX_PATHLEN+60];
      sprintf(bak,"%s.%d.bak",dst,getpid());
      cg_rename(dst,bak);
      preloadfiledisk_now(dst,zpath,NULL);
      if (cg_file_size(dst)>=0){
        cg_unlink(bak);
        status=GREEN_SUCCESS"xxx";
      }else{
        status=RED_FAIL;
        cg_rename(bak,dst);
      }
    }
    n+=sprintf(buf+n,"Original\t%s\t%'ld\t%s\t%s\n",ST_MTIME(orig),orig->st_size,RP(),status);
  }
  buf[n++]='\n';
#undef T
#if WITH_PRELOADFILERAM
  struct textbuffer *b=textbuffer_new(COUNT_MALLOC_PRELOADFILERAM_TXTBUF);
  textbuffer_add_segment(TXTBUFSGMT_DUP,b,buf,n);
  lock(mutex_fhandle);
  if (!fhandle_set_text(d,b)){
    FREE_NULL_MALLOC_ID(b);
  }else{
    IF1(WITH_PRELOADFILERAM,preloadfileram_set_status(d,preloadfileram_done));
    d->flags|=FHANDLE_FLAG_PRELOADFILERAM_COMPLETE;
  }
  unlock(mutex_fhandle);
#else
  write(STDERR_FILENO,buf,n);
#endif //WITH_PRELOADFILERAM
}
