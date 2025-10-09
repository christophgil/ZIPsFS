/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// ZIP-entries flat not int directory                        ///
/////////////////////////////////////////////////////////////////



static bool find_realpath_try_inline(struct zippath *zpath, const char *vp, struct rootdata *r){
  zpath->entry_path=zpath_newstr(zpath);
  zpath->flags|=ZP_ZIP;
  if (!zpath_strcat(zpath,vp+cg_last_slash(vp)+1)) return false;
  EP_L()=zpath_commit(zpath);
  const bool ok=test_realpath_or_reset(zpath,r);
  IF_LOG_FLAG(LOG_ZIP_INLINE) if (cg_is_regular_file(RP()))    log_exited_function("rp: %s vp: %s ep: %s ok: %s",RP(),vp,EP(),yes_no(ok));
  return ok;
}
static yes_zero_no_t find_realpath_try_inline_rules(struct zippath *zpath,char *append, struct rootdata *r){
  const char *vp=VP();
  const int vp_l=VP_L();
  for(int rule=0;;rule++){ /* ZIP-entries inlined. ZIP-file itself not shown in listing. Occurs for Sciex mass-spec */
    const int len=config_containing_zipfile_of_virtual_file(rule,vp,vp_l,&append);
    if (!len) break;
    zpath_reset_keep_VP(zpath);
    zpath->virtualpath_without_entry=zpath_newstr(zpath);
    if (!zpath_strncat(zpath,vp,len) || !zpath_strcat(zpath,append)) return NO;
    ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
    const bool ok=find_realpath_try_inline(zpath,vp,r);
    if (ok) return YES;
  }
  return ZERO;
}





static bool readdir_inline_from_cache(const struct zippath *zpath, const char *u, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  struct directory dir={0};
  struct zippath *zp2=directory_init_zpath(&dir,NULL);
  if (!zpath_strcat(zp2,RP()) || !zpath_strcat(zp2,"/") || !zpath_strcat(zp2,u)) return false;
  zp2->realpath_l=zpath_commit(zp2);
  zp2->root=zpath->root;


  LOCK_N(mutex_dircache, const bool ok=dircache_directory_from_cache(&dir));
  if (!ok){
    directory_to_queue(&dir);
  }else{
    struct stat st;
    FOR(j,0,dir.core.files_l){/* Embedded zip file */
      const char *n2=dir.core.fname[j];
      if (n2 && !strchr(n2,'/')){
        stat_init(&st,(Nth0(dir.core.fflags,j)&DIRENT_ISDIR)?-1:Nth0(dir.core.fsize,j),&zpath->stat_rp);
        st.st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,Nth(dir.core.finode,j,j),RP());
        char n[MAX_PATHLEN+1];
        const int n_l=zipentry_placeholder_expand(n,n2,u);
        if (config_containing_zipfile_of_virtual_file(0,n,n_l,NULL)) filler_add(filler,buf,n,n_l,&st,no_dups);
      }
    }
    directory_destroy(&dir);
  }
  return ok;
}


#if WITH_ZIPINLINE_CACHE
static yes_zero_no_t zipinline_find_realpath_any_root(struct zippath *zpath,const long which_roots){
  const char *vp=VP();
  const int vp_l=VP_L();
  if (!vp_l) return 0;
  foreach_root(r) if (which_roots&(1<<rootindex(r))){
    char zip[MAX_PATHLEN+1]; *zip=0;
    LOCK(mutex_dircache,const char *z=zinline_cache_vpath_to_zippath(vp,vp_l); if (z) strcpy(zip,z));
    if (*zip && !strncmp(zip,r->rootpath,r->rootpath_l) && zip[r->rootpath_l]=='/'  && wait_for_root_timeout(r)){
      zpath_reset_keep_VP(zpath);
      zpath->virtualpath_without_entry=zpath_newstr(zpath);
      if (!zpath_strcat(zpath,zip+r->rootpath_l)) return NO;
      ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
      if (find_realpath_try_inline(zpath,vp,r)) return YES;
    }
  }
  return ZERO;
}
#endif //WITH_ZIPINLINE_CACHE
