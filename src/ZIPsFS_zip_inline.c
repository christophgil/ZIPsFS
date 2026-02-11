/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// ZIP-entries flat not int directory                        ///
/////////////////////////////////////////////////////////////////
_Static_assert(WITH_ZIPINLINE,"");


static bool find_realpath_try_inline(zpath_t *zpath, const char *vp, root_t *r){
  zpath->entry_path=zpath_newstr(zpath);
  if (!zpath_strcat(zpath,vp+cg_last_slash(vp)+1)) return false;
  EP_L()=zpath_commit(zpath);
  const bool ok=test_realpath(0,ZP_TRY_ZIP|ZP_RESET_IF_NEXISTS,zpath,r);
  IF_LOG_FLAG(LOG_ZIP_INLINE) if (cg_is_regular_file(RP()))log_exited_function("rp: %s vp: %s ep: %s ok: %s",RP(),vp,EP(),yes_no(ok));
  return ok;
}
static yes_zero_no_t find_realpath_try_inline_rules(zpath_t *zpath,char *append, root_t *r){
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





static bool readdir_inline_from_cache(const int opt, const zpath_t *zpath, const char *u, void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  directory_t dir={0};
  zpath_t *zp2=directory_init_zpath(&dir,NULL);
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
        const int n_l=zipentry_placeholder_expand(n,n2,u,NULL);
        if (config_containing_zipfile_of_virtual_file(0,n,n_l,NULL)) filler_add(opt,filler,buf,n,n_l,&st,no_dups);
      }
    }
    directory_destroy(&dir);
  }
  return ok;
}



#if WITH_ZIPINLINE_CACHE
static const char *zinline_cache_vpath_to_zippath(const char *vp,const int vp_l){
  cg_thread_assert_locked(mutex_dircache);
  const char *zip=ht_numkey_get(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l);
  //static int count; if (zip && !count++)log_debug_now("%s vp: %s zip: %s",success_or_fail(zip),vp,zip);
  if (!zip) return NULL;
  /* validation because we rely on hash only */
  const char *vp_name=vp+cg_last_slash(vp)+1;
  const int len=config_containing_zipfile_of_virtual_file(0,vp_name,strlen(vp_name),NULL);
  return strncmp(vp_name,zip+cg_last_slash(zip)+1,len)?NULL:zip;
}
static yes_zero_no_t zipinline_find_realpath_any_root(zpath_t *zpath,const long which_roots){
  const char *vp=VP();
  const int vp_l=VP_L();
  if (!vp_l) return 0;
  foreach_root(r) if (which_roots&(1<<rootindex(r))){
    //log_entered_function("%s  root:%s",VP(), rootpath(r));
    char zip[MAX_PATHLEN+1]; *zip=0;
    LOCK(mutex_dircache,const char *z=zinline_cache_vpath_to_zippath(vp,vp_l); if (z) strcpy(zip,z));

    //log_debug_now("%s  root:%s zip:'%s'",VP(), rootpath(r),zip);

    if (*zip && !strncmp(zip,r->rootpath,r->rootpath_l) && zip[r->rootpath_l]=='/'  && wait_for_root_timeout(r)){
      zpath_reset_keep_VP(zpath);
      zpath->virtualpath_without_entry=zpath_newstr(zpath);
      if (!zpath_strcat(zpath,zip+r->rootpath_l)) return NO;
      ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
      //log_entered_function("%s  root:%s   find_realpath_try_inline:%d",VP(), rootpath(r),find_realpath_try_inline(zpath,vp,r));
      if (find_realpath_try_inline(zpath,vp,r)) return YES;
    }
  }
  return ZERO;
}
static void to_cache_vpath_to_zippath(directory_t *dir){
  if (!DIR_VP_L(dir) || dir->cached_vp_to_zip++) return;
  const zpath_t *zpath=&dir->dir_zpath;
  cg_thread_assert_locked(mutex_dircache);
  if (DIR_IS_ZIP(dir) && config_skip_zipfile_show_zipentries_instead(RP(),RP_L())){
    struct directory_core *dc=&dir->core;
    static char u[PATH_MAX+1], vp[PATH_MAX+1];
    const int vp0_l=VP_L()-EP_L(); /* Virtual length without Zipentry */
    //log_debug_now("VP: %s %d EP: %s %d   vp0_l: %d    ",VP(),VP_L(), EP(),EP_L(),  vp0_l);
    ASSERT(vp0_l>0);
    memcpy(vp,VP(),vp0_l); /* Virtual path without zipentry is common prefix */
    RLOOP(i,dc->files_l){
      const char *n=dc->fname[i];
      if (!n || strchr(n,'/')) continue; /* Only ZIP entries in root directory */
      const int u_l=zipentry_placeholder_expand(u,n,RP(),dir);
      const int vp_l=vp0_l+u_l;
      memcpy(vp+vp0_l,u,u_l);  vp[vp_l]=0;
      //log_debug_now(ANSI_FG_MAGENTA"u=%s   vp: %s"ANSI_RESET,u,vp);
      ht_numkey_set(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l,RP());
    } /* Note: We are not storing vp. We rely on its hash value and accept collisions. */
  }
}

static void zipinline_cache_drop_vp(const char *vp, const int vp_l){
  ht_numkey_set(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l,NULL);
}
#endif //WITH_ZIPINLINE_CACHE
