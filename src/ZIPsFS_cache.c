///////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                     ///
/// Optional caches to improve performance  ///
///////////////////////////////////////////////




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
static bool _clear_ht(ht_t *ht){
  /* There is a version clear_ht_unless_observed() in the Archive */
  if (!ht) return false;
  cg_thread_assert_locked(mutex_dircache);
  ht_clear(ht);
  warning(WARN_DIRCACHE|WARN_FLAG_SUCCESS,ht->name,"Cache cleared");
  return true;
}
static void dircache_clear_if_reached_limit(const bool always,const int mask,root_t *r,const off_t limit){
  if (!WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT && !always || !r) return;
  cg_thread_assert_locked(mutex_dircache);
  IF1(WITH_DIRCACHE,const off_t ss=mstore_count_blocks(&r->dircache_mstore));
#define M(x) (0!=(mask&(1<<x)))
  if (always IF1(WITH_DIRCACHE,|| ss>=limit)){
    IF1(WITH_DIRCACHE,warning(WARN_DIRCACHE,r->rootpath,"Clearing directory cache. Cached segments: %zu (%zu) bytes: %zu. %s",ss,limit,mstore_usage(&r->dircache_mstore),!limit?"":"Consider to increase NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE"));
    if M(CLEAR_DIRCACHE){
        _clear_ht(&r->dircache_ht);
        _clear_ht(&r->ht_int_rp);
        IF1(WITH_DIRCACHE, if (_clear_ht(&r->ht_int_fname)) mstore_clear(&r->dircache_mstore));
        IF1(WITH_STAT_CACHE, if M(CLEAR_STATCACHE) _clear_ht(&r->stat_ht));
      }
    if (!rootindex(r)){
      IF1(WITH_ZIPINLINE_CACHE, if M(CLEAR_ZIPINLINE_CACHE) _clear_ht(&ht_zinline_cache_vpath_to_zippath));
    }
  }
#undef M
}
#endif //WITH_CLEAR_CACHE
#if 0
static void debug_assert_crc32_not_null(const directory_t *dir){
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
  Not the entire directory_t only the directory_core is stored
  RAM: &r->dircache_mstore
*/

#if WITH_DIRCACHE
static void dircache_directory_to_cache(directory_t *dir){
  const bool debug=WITH_EXTRA_ASSERT && ENDSWITH(DIR_RP(dir),DIR_RP_L(dir), ".d.Zip");
  //if (debug)log_entered_function("%s",DIR_RP(dir));
  cg_thread_assert_locked(mutex_dircache);
  //debug_assert_crc32_not_null(dir);
  root_t *r=DIR_ROOT(dir);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE));
  assert_validchars_direntries(dir);
  struct directory_core src=dir->core;
  if (DIR_VP_L(dir) && DIR_IS_ZIP(dir)){
    src.finode=NULL; src.fflags=NULL; // DEBUG_NOW
  }
  struct directory_core *dc=mstore_add(&r->dircache_mstore,&src,sizeof(struct directory_core),SIZEOF_POINTER);
  if (!dc->dir_mtim.tv_sec){
    directory_rp_stat(dir);
    dc->dir_mtim=dir->dir_zpath.stat_rp.ST_MTIMESPEC;
  }
  if (src.files_l){
    RLOOP(i,src.files_l) dc->fname[i]=(char*)ht_sinternalize(&r->ht_int_fname,src.fname[i]); /* All  filenames internalized */
#define X(F,type) if (src.F) dc->F=mstore_add(&r->dircache_mstore,src.F,src.files_l*sizeof(type),sizeof(type))
    XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME();
#undef X
    /*  Due to zipentry_placeholder_insert()  and internalization, the array of filenames will often be the same and needs to be stored only once - hence ht_intern.*/
    dc->fname=(char**)ht_intern(&r->ht_int_fnamearray,src.fname,src.files_l*SIZE_POINTER,0,SIZE_POINTER);
  }
  ht_hash_t key_hash[1]={0};
  int key_l[1]={0};
  const char *key=key_from_rp(true,DIR_RP(dir),DIR_RP_L(dir),key_l,key_hash,r);
  ht_set(&r->dircache_ht,key,*key_l,*key_hash,dc);
  IF1(WITH_ZIPINLINE_CACHE,to_cache_vpath_to_zippath(dir));
  //log_exited_function("%s",DIR_RP(dir));
}


/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// All virtual paths are formed by concatenating the virtual path without ZIP Entry and each ZIP entry  ///
/// Each  resulting path is linked with the absolute ZIP-file path in a look-up-table                    ///
/// This accelerates the search for the corresponding ZIP file for inline ZIP entries.                   ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool dircache_directory_from_cache(directory_t *dir){
  const bool debug=WITH_EXTRA_ASSERT && ENDSWITH(DIR_RP(dir),DIR_RP_L(dir), ".d.Zip");
  cg_thread_assert_locked(mutex_dircache);
  ht_hash_t key_hash[1]={0};
  int key_l[1]={0};
  const char *key=key_from_rp(false,DIR_RP(dir),DIR_RP_L(dir),key_l,key_hash,DIR_ROOT(dir));
  ht_entry_t *e=ht_get_entry(&DIR_ROOT(dir)->dircache_ht,key,*key_l,*key_hash,false);
  const struct directory_core *dc=e?e->value:NULL;
  //log_entered_function(ANSI_FG_MAGENTA"%s  e: %p  dc: %p "ANSI_RESET,DIR_RP(dir),e,dc);
  if (dc){
    directory_rp_stat(dir);
    if (CG_TIMESPEC_EQ(dc->dir_mtim,dir->dir_zpath.stat_rp.ST_MTIMESPEC)){/* Cached data still valid. */
      dir->core=*dc;
      assert_validchars_direntries(dir);
      dir->dir_is_dircache=true;
      dir->files_capacity=dc->files_l;
      //if (debug) log_exited_function(GREEN_SUCCESS"%s",DIR_RP(dir));
      return true;
    }
    e->value=NULL;
  }
  //if (debug) log_exited_function(RED_FAIL"%s",DIR_RP(dir));
  return false;
}
#endif //WITH_DIRCACHE

///////////////////////////
///// file i/o cache  /////
///////////////////////////

static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l, const char *zipentry, const int zipentry_l){
#if WITH_EVICT_FROM_PAGECACHE
#if !defined(HAS_POSIX_FADVISE) || HAS_POSIX_FADVISE
  if (!config_advise_evict_from_filecache(realpath,realpath_l, zipentry, zipentry_l)) return;
  const int err=fdOrZero?posix_fadvise(fdOrZero,0,0,POSIX_FADV_DONTNEED):cg_vmtouch_e(realpath);
  IF_LOG_FLAG(LOG_EVICT_FROM_CACHE) log_verbose(ANSI_MAGENTA"Evicted %s: %s%s"ANSI_RESET, snull(realpath), err?ANSI_FG_GREEN:ANSI_FG_RED,cg_error_symbol(err));
#else
  static int i;
  if (!i++) warning(WARN_CONFIG,"","The method posix_fadvise is not available.");
#endif //WITH_EVICT_FROM_PAGECACHE
#endif // WITH_EVICT_FROM_PAGECACHE
}
