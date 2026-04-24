/*******************************************************************************/
/* COMPILE_MAIN=ZIPsFS                                                         */
/* ZIP-entries flat in the parent directory  directory                         */
/* In contrast to show the zip file as a folder with suffix .Content           */
/*******************************************************************************/


_Static_assert(WITH_ZIPFLAT,"");



/************************************************************/
/* PARAMETER:                                               */
/* zpath: With VP which has been set.                       */
/*        The RP will be set on success.                    */
/* rule:  The caller will let rule run from 0 to ... until  */
/* RETURN:                                                  */
/* NO:No match but there are more rules to be tested.       */
/* ZERO: No match. No more rules                            */
/* YES: Match                                               */
/************************************************************/
static yes_zero_no_t find_realpath_try_zipflat_rule(zpath_t *zpath, root_t *r, const int rule){
  char *append=NULL;
  const int len=config_containing_zipfile_of_virtual_file(rule,VP(),VP_L(),&append);
  if (len<0) return ZERO;
  zpath_reset_keep_VP(zpath);
  ZPATH_NEWSTR(virtualpath_without_entry);
  if (!ZPATH_STRCAT_N(VP(),len) || !ZPATH_STRCAT(append)) return ZERO;
  ZPATH_COMMIT_HASH(virtualpath_without_entry);
  ZPATH_NEWSTR(entry_path);
  if (!ZPATH_STRCAT(VP()+cg_last_slash(VP())+1)) return ZERO;
  //EP_L()=zpath_commit(zpath);
  ZPATH_COMMIT(entry_path);
  const bool ok=test_realpath(0,ZP_TRY_ZIP|ZP_RESET_IF_NEXISTS,zpath,r);
  IF_LOG_FLAG(LOG_ZIPFLAT) if (cg_is_regular_file(RP()))log_exited_function("rp: %s vp: %s ep: %s ok: %s",RP(),VP(),EP(),yes_no(ok));
  return ok?YES:NO;
}



static yes_zero_no_t find_realpath_try_zipflat_rules(zpath_t *zpath, root_t *r){
  //log_entered_function(" VP:%s  root:%s",VP(),rootpath(r));
  yes_zero_no_t ret=ZERO;
  FOR(rule,0,99){ /* ZIP-entries inlined. ZIP-file itself not shown in listing. Occurs for Sciex mass-spec */
    const yes_zero_no_t ok=find_realpath_try_zipflat_rule(zpath,r,rule);
    if (ok==ZERO) break;
    if (ok==YES){ ret=YES;break;}
    if (zpath->flags&ZP_OVERFLOW){ ret=NO;break;}
  }
  return ret;
}


/*****************************************************************************/
/* Asynchronous reading to avoid long responds time when reading directories */
/* Initially folders .Zip.Content are shown.                                 */
/* With the time these folders get replaced by the inlined zipentries.       */
/*****************************************************************************/
static bool readdir_zipflat_from_cache(const int opt, const zpath_t *zpath_parentdir, const char *zipfilename, void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  directory_t dir={0};
  directory_init_zpath(&dir,NULL);
  zpath_init_vp(&dir.dir_zpath,ZP_VP(zpath_parentdir),zpath_parentdir->virtualpath_l, zipfilename);
  bool ok=test_realpath(0,0,&dir.dir_zpath,zpath_parentdir->root);
  if (ok){ LOCK(mutex_dircache,  ok=dircache_directory_from_cache(&dir));}
  if (!ok){
    directory_to_queue(&dir);
  }else{
    struct stat st;
    FOR(j,0,dir.core.files_l){/* Embedded zip file */
      const char *n2=dir.core.fname[j];
      if (n2 && !strchr(n2,'/')){
        stat_init(&st,(Nth0(dir.core.fflags,j)&DIRENT_ISDIR)?-1:Nth0(dir.core.fsize,j),&zpath_parentdir->stat_rp);
        //st.st_ino=make_inode(zpath_parentdir->stat_rp.st_ino,zpath_parentdir->root,Nth(dir.core.finode,j,j),RP());
        st.st_ino=zpath_make_inode(zpath_parentdir,Nth(dir.core.finode,j,j));
        char n[MAX_PATHLEN+1];
        const int n_l=zipentry_placeholder_expand(n,n2,zipfilename,NULL);
        if (config_containing_zipfile_of_virtual_file(0,n,n_l,NULL)>=0) filler_add(opt,filler,buf,n,n_l,ZPATH_FILLDIR_SFX(zpath_parentdir),&st,no_dups);
      }
    }
    directory_destroy(&dir);
  }
  return ok;
}
#if WITH_ZIPFLATCACHE
/********************************************************/
/* Example for match ext=".wiff" and ext_rp=".wiff.Zip" */
/********************************************************/
static int _what_rule_matches_zipfile_name(const char *ext, const char *ext_rp, const int ext_rp_l){
  const int ext_l=strlen(ext);
  char *append;
  FOR(rule,0,99){
    const int len=config_containing_zipfile_of_virtual_file(rule,ext,ext_l,&append);
    if (len<0) break;
    const int append_l=strlen(append);
    if (len+append_l!=ext_rp_l ||
        len && strncmp(ext,ext_rp,len) ||  /* Usually len is 0 */
        strncmp(ext_rp+ext_rp_l-append_l,append,append_l)) continue;
    //log_debug_now("rule:%d  ext:%s ext_rp:%s   len:%d append:%s",rule,ext,ext_rp,len,append);
    return rule;
  }
  return -1;
}
/**********************************************************************************/
/* Given a virtual path - what could be the containtaining Zip file?              */
/* Usually several possibilities (rule number) of Zip files to be probed.         */
/* The goal is to reduce calls to lstat() for possible Zip files.                 */
/* We are storing the (rule + one) in a hashmap                                   */
/* The key is formed from hash code of the virtual path. We accept key collision. */
/**********************************************************************************/
static void zipflatcache_store_allentries_of_dir(directory_t *dir){
  if (dir->cached_vp_to_zip++) return; /* Only once */
  const zpath_t *zpath=&dir->dir_zpath;
  if (VP_L() && zpath->root && DIR_IS_TRY_ZIP() && config_skip_zipfile_show_zipentries_instead(RP(),RP_L())){
    cg_thread_assert_locked(mutex_dircache);
    const char *ext_rp=strchr(RP()+cg_last_slash(RP())+1,'.'); /* For example ".wiff.Zip" */
    if (!ext_rp) return;
    const int ext_rp_l=strlen(ext_rp);
    //log_debug_now(" RP:%s dot:%s",RP(),rp_ext);
    static char u[PATH_MAX+1], vp[PATH_MAX+1];
    const int vp0_l=VP_L()-EP_L(); /* Virtual length without Zipentry */
    //log_debug_now("VP: %s %d EP: %s %d   vp0_l: %d    ",VP(),VP_L(), EP(),EP_L(),  vp0_l);
    ASSERT(vp0_l>0);
    memcpy(vp,VP(),vp0_l); /* Virtual path without zipentry is common prefix */
    RLOOP(i,dir->core.files_l){
      const char *n=dir->core.fname[i];
      if (!n || *n!=PLACEHOLDER_NAME || strchr(n,'/')) continue; /* Only ZIP entries in root directory */
      const long rule=_what_rule_matches_zipfile_name(n+1,ext_rp,ext_rp_l);
      if (rule<0) continue;
      const int u_l=zipentry_placeholder_expand(u,n,RP(),dir), vp_l=vp0_l+u_l;
      memcpy(vp+vp0_l,u,u_l);  vp[vp_l]=0;
      //log_debug_now(ANSI_FG_MAGENTA"u=%s   vp: %s rule:%ld"ANSI_RESET,u,vp,rule);
      ht_numkey_set(&zpath->root->ht_zipflatcache_vpath_to_rule,hash32(vp,vp_l),vp_l,(const void*)(rule+1));
    } /* Note: We are not storing vp. We rely on its hash value and accept collisions. */
  }
}

/**********************************************************************************/
/* We retrieve the rule number from the hashmap and probe the resulting zip path. */
/**********************************************************************************/
static bool zipflatcache_find_realpath(zpath_t *zpath,const long which_roots){
  if (!VP_L() || config_containing_zipfile_of_virtual_file(0,VP(),VP_L(),NULL)<0) return false;
  foreach_root(r){
    if (!(which_roots&(1<<rootindex(r)))) continue;
    LOCK_N(mutex_dircache,const long rule=((long)ht_numkey_get(&r->ht_zipflatcache_vpath_to_rule,hash32(VP(),VP_L()),VP_L()))-1);
    //if (rule>=0 && find_realpath_try_zipflat_rule(zpath,r,rule)==YES)log_debug_now(GREEN_SUCCESS"%s rule:%ld",VP(),rule);
    if (rule>=0 && find_realpath_try_zipflat_rule(zpath,r,rule)==YES) return true;
  }
  return false;
}


static void zipflatcache_drop(zpath_t *zpath){
  if (zpath->root) ht_numkey_set(&zpath->root->ht_zipflatcache_vpath_to_rule,hash32(VP(),VP_L()),VP_L(),NULL);
}
#endif //WITH_ZIPFLATCACHE
