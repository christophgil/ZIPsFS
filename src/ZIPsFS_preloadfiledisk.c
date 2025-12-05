////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS          ///
/// D download to the local disk ///
////////////////////////////////////


static bool preloadfiledisk_zpath_up_to_date_invalid(const char *rp,const int rp_l){
  const ht_hash_t hash=hash32(rp,rp_l);
  lock(mutex_dircache);
  static struct ht ht;  if (!ht.capacity)  ht_init(&ht,"preload_time",HT_FLAG_KEYS_ARE_STORED_EXTERN|12);
  struct ht_entry *e=ht_get_entry(&ht,internalize_realpath(rp,rp_l,hash),rp_l,hash,true);
  const time_t now=time(NULL), diff=now-(time_t)e->value;
  const bool invalid=diff>PRELOADFILEDISK_CACHE_MTIME_SECONDS;
  if (invalid) e->value=(void*)now;
  unlock(mutex_dircache);
  log_exited_function("%s need: %s %d diff: %ld"ANSI_RESET, rp,invalid?ANSI_RED:ANSI_GREEN,invalid,diff);
  return invalid;
}

static void preloadfiledisk_zpath_up_to_date(struct zippath *zpath){
  int prefix_l;
  if (!_root_writable || zpath->root!=_root_writable || !S_ISREG(zpath->stat_rp.st_mode) || !preloadfiledisk_type(&prefix_l, VP(), VP_L())) return;
  if (!preloadfiledisk_zpath_up_to_date_invalid(RP(),RP_L())) return;
  struct zippath zp=*zpath;
  zpath_reset_keep_VP(&zp);
  if (!find_realpath_roots_by_mask(&zp,~1)) return;
  if (zpath->stat_rp.st_mtime!=zp.stat_rp.st_mtime || zpath->stat_rp.st_size!=zp.stat_rp.st_size){
    warning(WARN_PRELOADFILERAM,RP(),"Not up to date - going to remove. Upstream: '%s' ",ZP_RP(&zp));
    log_debug_now("zpath->stat_rp.st_mtime %ld",zpath->stat_rp.st_mtime);
    log_debug_now("zp.stat_rp.st_mtime: %ld",zp.stat_rp.st_mtime);
    if (unlink(RP())){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,RP(),"");
    }else{
      *zpath=zp;
    }
  }else log_debug_now("Same mtime %ld",zpath->stat_rp.st_mtime);
}


static const char *preloadfiledisk_type(int *prefix_l, const char *vp, const int vp_l){
  *prefix_l=0;
#define C(dir)  if (IS_DIR_Z(dir,vp,vp_l)||IS_IN_DIR_Z(dir,vp,vp_l)){ *prefix_l=sizeof(dir)-1; return dir; }
  C(DIR_PRELOADFILEDISK_R);
  C(DIR_PRELOADFILEDISK_RC);
  C(DIR_PRELOADFILEDISK_RZ);
#undef C
  return NULL;
}


static bool preloadfiledisk_up_to_date(struct fHandle *d, char dst[MAX_PATHLEN+1], struct stat *st){
  if (!d) return false;
  int cut;  preloadfiledisk_type(&cut,D_VP(d),D_VP_L(d));
  stpcpy(stpcpy(stpcpy(dst,_root_writable->rootpath),d->zpath.root->preload? DIR_PRELOADFILEDISK_PRELOAD:DIR_PRELOADFILEDISK_R),cut+D_VP(d));
  DIE_DEBUG_NOW("dst=%s",dst);
  bool ok=false;
  if (!stat(dst,st)){
    if (st->st_mtime>=d->zpath.stat_rp.st_mtime){
      ok=true;
      const off_t s=d->zpath.stat_vp.st_size;
      if (st->st_size!=s){
        warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"%s Wrong size %lld != %lld",D_VP(d), (LLD)st->st_size, (LLD)s);
        unlink(dst);
        ok=false;
      }
    }
    if (ok){
      cg_file_set_atime(dst,st,0);
      log_succes("up-to-date %s",D_VP(d));
    }else{
      log_verbose("Not up-to-date %s",D_VP(d));
      if (unlink(dst)) warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"unlink");
    }
  }
  return ok;
}

static void zpath_copy_rp(struct zippath *zpath, const char *rp, const struct stat *st){
  zpath_reset_keep_VP(zpath);
  zpath->realpath=zpath_newstr(zpath);
  zpath_strcat(zpath, rp);
  zpath->stat_rp=zpath->stat_vp=*st;
}

static void preloadfiledisk_now(struct fHandle *d){
  log_entered_function("'%s'",D_VP(d));
  char dst[MAX_PATHLEN+1];
  struct stat st;
  if (!preloadfiledisk_up_to_date(d,dst,&st)){
    LOCK_N(mutex_fhandle, struct zippath zp=d->zpath; struct zippath *zpath=&zp);
    char tmp[MAX_PATHLEN+1];
    snprintf(tmp,MAX_PATHLEN,"%s.%d.tmp",dst,getpid());
    off_t n=-4;
    {
      int fi=0;
      zip_t *za=NULL;
      zip_file_t *zip=NULL;
      if (ZPATH_IS_ZIP()) zip=(za=my_zip_open(RP()))?my_zip_fopen(za,EP(),ZIP_RDONLY,RP()):NULL;  else fi=open(RP(),O_RDONLY);
      //log_debug_now("'%s'  ZPATH_IS_ZIP: %d  za: %p, zip: %p  fi: %d",RP(), ZPATH_IS_ZIP(),za,zip,fi);
      if (zip || fi>0){
        n=-3;
        cg_recursive_mk_parentdir(dst);
        //log_verbose("Going to write %s",tmp);
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
    if (n>0 && strstr(_mnt,"/cgille/") && cg_file_exists(dst)) DIE_DEBUG_NOW("File dst: '%s' exists",dst);

    if (n<0){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,RP(),"Read error -> %s  n: %d",tmp,n);
      unlink(tmp);
    }else if (stat(tmp,&st)){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,tmp,"%s",RP());
      unlink(tmp);
    }else if (unlink(dst),cg_rename(tmp,dst)){
      warning(WARN_PRELOADFILEDISK|WARN_FLAG_ERROR|WARN_FLAG_ERRNO,dst,"rename");
      unlink(tmp);
    }else{
      cg_file_set_mtimes(dst,zpath->stat_rp.st_mtim);
    }
  }
  LOCK(mutex_fhandle,d->flags&=~(FHANDLE_FLAG_LCOPY_RUN|FHANDLE_FLAG_LCOPY_QUEUE));
}


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

static bool preloadfiledisk(struct fHandle *d){
  if (!d || !_root_writable) return false;
  struct rootdata *r=d->zpath.root;
  if (!r) return false;
  LOCK_N(mutex_fhandle,
         struct zippath *zpath=&d->zpath;
         bool ok=(zpath->flags&ZP_PRELOADFILEDISK) && zpath->root && (VP_L()+_root_writable->rootpath_l+50<MAX_PATHLEN));
  if (!ok) return false;
  //log_entered_function("%s", D_VP(d));
  char dst[MAX_PATHLEN+1];
  struct stat st;
  if (!(ok=preloadfiledisk_up_to_date(d,dst, &st))){
    root_start_thread(zpath->root,PTHREAD_PRELOAD,false);
  again_with_other_root:
    for(int i=0;;i++){
      log_debug_now("%s FHANDLE_FLAG_LCOPY_QUEUE %p",D_VP(d),d);
      LOCK_N(mutex_fhandle,  struct fHandle *g=preloadfiledisk_fhandle(d); if (!g && !(d->flags&(FHANDLE_FLAG_LCOPY_QUEUE|FHANDLE_FLAG_LCOPY_RUN)))  d->flags|=FHANDLE_FLAG_LCOPY_QUEUE);
      PRELOADFILE_ROOT_UPDATE_TIME(d,NULL,false);
      while(true){
        LOCK(mutex_fhandle, ok=!((g?g:d)->flags&(FHANDLE_FLAG_LCOPY_QUEUE|FHANDLE_FLAG_LCOPY_RUN)));
        if (ok && !cg_file_exists(dst)) ok=false;
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
    //ok=preloadfiledisk_up_to_date(d,dst,&st);
    ok=cg_file_exists(dst);
    if (!ok) log_error("cg_file_exists %s",dst);
  }
  if (ok){
    LOCK(mutex_fhandle, zpath_copy_rp(&d->zpath,dst,&st); zpath->root=_root_writable);
  }else{
    log_failed("%s %p   RP:%s", D_VP(d),d,D_RP(d));
  }
  //log_exited_function("%s %s %p   RP:%s",ok?GREEN_SUCCESS:RED_FAIL, D_VP(d),d,D_RP(d));
  return ok;
}
