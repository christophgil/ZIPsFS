///////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                     ///
/// Optional caches to improve performance  ///
///////////////////////////////////////////////


#if WITH_ZIPINLINE_CACHE
static const char *zinline_cache_vpath_to_zippath(const char *vp,const int vp_l){
  cg_thread_assert_locked(mutex_dircache);
  const char *zip=ht_numkey_get(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l);
  //static int count; if (zip && !count++)log_debug_now("%s vp: %s zip: %s",zip?GREEN_SUCCESS:RED_FAIL,vp,zip);
  if (!zip) return NULL;
  /* validation because we rely on hash only */
  const char *vp_name=vp+cg_last_slash(vp)+1;
  const int len=config_containing_zipfile_of_virtual_file(0,vp_name,strlen(vp_name),NULL);
  return strncmp(vp_name,zip+cg_last_slash(zip)+1,len)?NULL:zip;
}
#endif //WITH_ZIPINLINE_CACHE


//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
#define CLEAR_ALL_CACHES 0
#define CLEAR_DIRCACHE 1
#define CLEAR_ZIPINLINE_CACHE 2
#define CLEAR_STATCACHE 3
#define CLEAR_CACHE_LEN 4
#if WITH_CLEAR_CACHE
static void dircache_clear_if_reached_limit_all(const bool always,const int mask){
#define C(m) 0==(mask&(1<<m))?"":#m
  if (always) log_verbose("%s %s %s\n",C(CLEAR_DIRCACHE),C(CLEAR_STATCACHE),C(CLEAR_ZIPINLINE_CACHE));
#undef C
  LOCK(mutex_dircache,foreach_root(r) dircache_clear_if_reached_limit(always,mask,r,0));
}
static bool _clear_ht(struct ht *ht){
  /* There is a version clear_ht_unless_observed() in the Archive */
  if (!ht) return false;
  cg_thread_assert_locked(mutex_dircache);
  ht_clear(ht);
  warning(WARN_DIRCACHE|WARN_FLAG_SUCCESS,ht->name,"Cache cleared");
  return true;
}
static void dircache_clear_if_reached_limit(const bool always,const int mask,struct rootdata *r,const off_t limit){
  if (!WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT && !always || !r) return;
  cg_thread_assert_locked(mutex_dircache);
  IF1(WITH_DIRCACHE,const off_t ss=mstore_count_blocks(&r->dircache_mstore));
#define M(x) (0!=(mask&(1<<x)))
  if (always IF1(WITH_DIRCACHE,|| ss>=limit)){
    IF1(WITH_DIRCACHE,warning(WARN_DIRCACHE,r->rootpath,"Clearing directory cache. Cached segments: %zu (%zu) bytes: %zu. %s",ss,limit,mstore_usage(&r->dircache_mstore),!limit?"":"Consider to increase NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE"));
    if M(CLEAR_DIRCACHE){
        _clear_ht(&r->dircache_ht);
        IF0(WITH_TIMEOUT_READDIR, IF1(WITH_DIRCACHE, if (_clear_ht(&r->dircache_ht_fname)) mstore_clear(&r->dircache_mstore)));
      }
    if (!rootindex(r)){
      IF1(WITH_ZIPINLINE_CACHE, if M(CLEAR_ZIPINLINE_CACHE) _clear_ht(&ht_zinline_cache_vpath_to_zippath));
      IF1(WITH_STAT_CACHE,      if M(CLEAR_STATCACHE)       _clear_ht(&stat_ht));
    }
  }
#undef M
}
#endif //WITH_CLEAR_CACHE
#if 0
static void debug_assert_crc32_not_null(const struct directory *dir){
  if (!dir || !DIR_IS_ZIP(dir)) return;
  RLOOP(i,dir->core.files_l){
    if (tdf_or_tdf_bin(dir->core.fname[i])){
      assert(dir->core.fcrc!=NULL);
      assert(dir->core.fcrc[i]!=0);
    }
  }
}
#endif//0
/*
  Not the entire struct directory only the directory_core is stored
  RAM: &r->dircache_mstore
*/



#if WITH_DIRCACHE
static void dircache_directory_to_cache(struct directory *dir){
  directory_remove_unused_fields(dir);
  //log_entered_function("%s",DIR_RP(dir));
  cg_thread_assert_locked(mutex_dircache);
  //debug_assert_crc32_not_null(dir);
  struct rootdata *r=DIR_ROOT(dir);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE));
  assert_validchars_direntries(dir);
  struct directory_core src=dir->core, *d=mstore_add(&r->dircache_mstore,&src,sizeof(struct directory_core),SIZEOF_POINTER);
  if (!d->dir_mtim.tv_sec){
    directory_rp_stat(dir);
    d->dir_mtim=dir->dir_zpath.stat_rp.ST_MTIMESPEC;
  }
  //ASSERT_PRINT(src.dir_mtim.tv_sec!=0);
  if (src.files_l){
    RLOOP(i,src.files_l){
      d->fname[i]=(char*)ht_sinternalize(&r->dircache_ht_fname,src.fname[i]); /* All  filenames internalized */
    }
#define C(F,type) if (src.F) d->F=mstore_add(&r->dircache_mstore,src.F,src.files_l*sizeof(type),sizeof(type))
    C_FILE_DATA_WITHOUT_NAME();
#undef C    /*  Due to zipentry_placeholder_insert()  and internalization, the array of filenames will often be the same and needs to be stored only once - hence ht_intern.*/
    d->fname=(char**)ht_intern(&r->dircache_ht_fnamearray,src.fname,src.files_l*SIZE_POINTER,0,SIZE_POINTER);
  }
  const ht_hash_t rp_hash=hash32(DIR_RP(dir),DIR_RP_L(dir));
  const char *rp=ht_intern(&_root->dircache_ht_fname,DIR_RP(dir),DIR_RP_L(dir),rp_hash,HT_MEMALIGN_FOR_STRG); /* The real path of the zip file */
  ht_set(&r->dircache_ht,rp,DIR_RP_L(dir),rp_hash,d);
  to_cache_vpath_to_zippath(dir);
  //log_exited_function("%s",DIR_RP(dir));
}



/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// All virtual paths are formed by concatenating the virtual path without ZIP Entry and each ZIP entry  ///
/// Each  resulting path is linked with the absolute ZIP-file path in a look-up-table                    ///
/// This accelerates the search for the corresponding ZIP file for inline ZIP entries.                   ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////
static void to_cache_vpath_to_zippath(struct directory *dir){
#if WITH_ZIPINLINE_CACHE
  if (!DIR_VP_L(dir) || dir->cached_vp_to_zip++) return;
  const struct zippath *zpath=&dir->dir_zpath;
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
      const int u_l=zipentry_placeholder_expand(u,n,RP());
      const int vp_l=vp0_l+u_l;
      memcpy(vp+vp0_l,u,u_l);  vp[vp_l]=0;
      //log_debug_now(ANSI_FG_MAGENTA"u=%s   vp: %s"ANSI_RESET,u,vp);
      ht_numkey_set(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l,RP());
    } /* Note: We are not storing vp. We rely on its hash value and accept collisions. */
  }
#endif // WITH_ZIPINLINE_CACHE
}

static bool dircache_directory_from_cache(struct directory *dir){
  cg_thread_assert_locked(mutex_dircache);
  struct ht_entry *e=ht_get_entry(&DIR_ROOT(dir)->dircache_ht,DIR_RP(dir),DIR_RP_L(dir),0,false);
  const struct directory_core *dc=e?e->value:NULL;
  //log_entered_function(ANSI_FG_MAGENTA"%s  e: %p  dc: %p "ANSI_RESET,DIR_RP(dir),e,dc);
  if (dc){
    //ASSERT_PRINT(dc->dir_mtim.tv_sec!=0);
    //log_debug_now("DIR_RP: %s", cg_ctime(cg_file_mtime(DIR_RP(dir))) );
    //log_debug_now(" zpath: %s %ld", ZP_RP(&dir->dir_zpath),   dir->dir_zpath.stat_rp.st_ino);
    //dir->dir_zpath.stat_rp.st_ino=0;
    directory_rp_stat(dir);
    if (!CG_TIMESPEC_EQ(dc->dir_mtim,dir->dir_zpath.stat_rp.ST_MTIMESPEC)){/* Cached data not valid any more. */
      //log_debug_now(RED_FAIL"invalid");
      e->value=NULL;
    }else{
      //log_debug_now(GREEN_SUCCESS"valid");
      dir->core=*dc;
      assert_validchars_direntries(dir);
      dir->dir_is_dircache=true;
      return true;
    }
  }
  return false;
}
#endif //WITH_DIRCACHE


///////////////////////////
///// file i/o cache  /////
///////////////////////////
#if WITH_EVICT_FROM_PAGECACHE && (!defined(HAS_POSIX_FADVISE) || HAS_POSIX_FADVISE)
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l, const char *zipentry, const int zipentry_l){
  config_advise_evict_from_filecache(realpath,realpath_l, zipentry, zipentry_l);
  const int fd=fdOrZero?fdOrZero:realpath?open(realpath,O_RDONLY):0;
  if (fd>0){
    posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
    if (!fdOrZero) close(fd);
    IF_LOG_FLAG(LOG_EVICT_FROM_CACHE) log_verbose(ANSI_MAGENTA"Evicted %d %s"ANSI_RESET, fdOrZero, snull(realpath));
  }
}
#else
#define maybe_evict_from_filecache(fdOrZero,realpath,realpath_l,zipentry,zipentry_l) {}
#endif //WITH_EVICT_FROM_PAGECACHE
