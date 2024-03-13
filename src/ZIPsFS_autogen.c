///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS.c       ///
/// Dynamically generated files ///
///////////////////////////////////


static bool generated_run(struct fhdata *d){
  if (!(d->flags&FHDATA_FLAGS_IS_GENERATED)) return true;
  if (!_root_generated) return false;
  if (d->generated_state) return d->generated_state==generated_success;
  bool ok;
  LOCK(mutex_generated,
       static char mnt_path[MAX_PATHLEN];
       if (!d->generated_state){
         strcpy(mnt_path,_mnt);
         strncat(mnt_path,d->path,MAX_PATHLEN);
         recursive_mk_parentdir(D_RP(d));
         char tmp[MAX_PATHLEN];
         sprintf(tmp,"%s.%d.tmp",D_RP(d),getpid());
         if ((ok=config_generate_file(tmp,mnt_path))){
           int err=rename(tmp,D_RP(d));
           if (err){ ok=false;log_error("%s\n",strerror(err));}
         }
         d->generated_state=ok?generated_success:generated_fail;
       }
       );
  if (ok) zpath_stat(&d->zpath,_root_generated);
  return ok;
}


static long size_of_not_yet_generated_file(const char *vp){
  char generated_from[MAX_PATHLEN];
  uint64_t size=0;
  struct zippath zpath={0};
  const int vp_l=strlen(vp);
  for(int i=0;;i++){
    if (!config_generated_depends_on(i,vp,vp_l,generated_from,&size)){
      if (!i) return 0;
      break;
    }
    if (*generated_from && !(zpath_init(&zpath,generated_from),find_realpath_any_root(&zpath,NULL))) return 0;
  }
  return size;
}

#define GENERATED_PATH_INIT(path) int path##_l=strlen(virtual_##path);const char *path=generated_remove_pfx(virtual_##path,&path##_l)
static const char *generated_remove_pfx(const char *vp,int *len){
  if (path_equals_or_is_parent(PATH_GENERATED,PATH_GENERATED_L,vp,*len)){
    *len-=PATH_GENERATED_L;
    return vp+PATH_GENERATED_L;
  }
  return vp;
}

static void filler_for_generated(fuse_fill_dir_t filler,void *buf, const char *name, const struct stat *stbuf,struct ht *no_dups){
  const int name_l=strlen(name);
  char generated[MAX_PATHLEN];
  for(int i=0; config_generated_from_virtualpath(i,generated,name,name_l);i++){
    if (only_once(no_dups,generated,0)) filler(buf,generated,stbuf,0,fill_dir_plus);
  }
  return;
  only_once(no_dups,name,0);
  filler(buf,name,stbuf,0,fill_dir_plus);
}
