///////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                     ///
/// Optional caches to improve performance  ///
///////////////////////////////////////////////




//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
#if WITH_CLEAR_CACHE

static void dircache_clear_if_reached_limit_all(const bool always,const int mask){
#define C(m) 0==(mask&(1<<m))?"":#m
  if (always) log_verbose("%s %s %s\n",C(CLEAR_DIRCACHE),C(CLEAR_STATCACHE),C(CLEAR_ZIPFLATCACHE));
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
        _clear_ht(&r->ht_dircache);
        _clear_ht(&_ht_intern_vp);
        IF1(WITH_DIRCACHE, if (_clear_ht(&r->ht_int_fname)) mstore_clear(&r->dircache_mstore));
        IF1(WITH_STATCACHE, if M(CLEAR_STATCACHE) _clear_ht(&r->ht_stat));
      }
    if (!rootindex(r)){
      IF1(WITH_ZIPFLATCACHE, if M(CLEAR_ZIPFLATCACHE) _clear_ht(&r->ht_zipflatcache_vpath_to_rule));
    }
  }
#undef M
}
#endif //WITH_CLEAR_CACHE
#if 0
static void debug_assert_crc32_not_null(const directory_t *dir){
  if (!dir || !DIR_IS_TRY_ZIP()) return;
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
  cg_thread_assert_locked(mutex_dircache);
  if (DEBUG_NOW!=DEBUG_NOW){
    //if (DIR_VP_L()!=DIR_VP0_L() || strcmp(DIR_VP0(),DIR_VP())) log_entered_function("VP:'%s' %d VP0:'%s' %d",DIR_VP(),DIR_VP_L(),DIR_VP0(),DIR_VP0_L());
    //if (tdf_or_tdf_bin(DIR_VP0())) DIE_DEBUG_NOW("VP:'%s' %d VP0:'%s' %d",DIR_VP(),DIR_VP_L(),DIR_VP0(),DIR_VP0_L());

  }
  //debug_assert_crc32_not_null(dir);
  root_t *r=DIR_ROOT();
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE));
  assert_validchars_direntries(dir);
  directory_core_t src=dir->core;
  if (DIR_VP_L() && DIR_IS_TRY_ZIP()){src.finode=NULL; src.fflags=NULL;}
  directory_core_t *dc=mstore_add(&r->dircache_mstore,&src,sizeof(directory_core_t),SIZEOF_POINTER);
    if (!dc->dir_mtim.tv_sec){
      directory_rp_stat(dir);
      dc->dir_mtim=dir->dir_zpath.stat_rp.ST_MTIMESPEC;
    }
  if (src.files_l){
    RLOOP(i,src.files_l) dc->fname[i]=(char*)ht_sintern(&r->ht_int_fname,src.fname[i]); /* All  filenames internalized */
#define X(F,type) if (src.F) dc->F=mstore_add(&r->dircache_mstore,src.F,src.files_l*sizeof(type),sizeof(type))
    XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME();
#undef X
    /*  Due to zipentry_placeholder_insert()  and internalization, the array of filenames will often be the same and needs to be stored only once - hence ht_intern.*/
    dc->fname=(char**)ht_intern(&r->ht_int_fnamearray,src.fname,src.files_l*sizeof(char*),0,sizeof(char*));
  }
  //log_debug_now("DIR_VP0:%s   DIR_VP:%s  DIR_RP:%s",DIR_VP0(), DIR_VP(),  DIR_RP());
  //if (cg_uid_is_developer() && ENDSWITH(DIR_VP(),DIR_VP_L(), "analysis.tdf")) cg_print_stacktrace_test(0);

  ht_set(&r->ht_dircache,DIR_VP(),DIR_VP_L(),0,dc);
  IF1(WITH_ZIPFLATCACHE,zipflatcache_store_allentries_of_dir(dir));

}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// All virtual paths are formed by concatenating the virtual path without ZIP Entry and each ZIP entry  ///
/// Each  resulting path is linked with the absolute ZIP-file path in a look-up-table                    ///
/// This accelerates the search for the corresponding ZIP file for inline ZIP entries.                   ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////
static bool dircache_directory_from_cache(directory_t *dir){
  cg_thread_assert_locked(mutex_dircache);
  ht_entry_t *e=ht_get_entry(&DIR_ROOT()->ht_dircache,DIR_VP(),DIR_VP_L(),0,false);
  const directory_core_t *dc=e?e->value:NULL;
  if (dc){
    directory_rp_stat(dir);
    if (DIR_ROOT()->immutable || CG_TIMESPEC_EQ(dc->dir_mtim,dir->dir_zpath.stat_rp.ST_MTIMESPEC)){/* Cached data still valid. */
      dir->core=*dc;
      assert_validchars_direntries(dir);
      dir->dir_is_dircache=true;
      dir->files_capacity=dc->files_l;
      return true;
    }else{
      struct stat st={0};
      lstat(DIR_RP(),&st);
      log_verbose("Cache invalid DIR_VP:%s DIR_VP0:%s    cache:%'20ld  dir %'20ld    ",DIR_VP(),DIR_VP0(),dc->dir_mtim.tv_sec, dir->dir_zpath.stat_rp.ST_MTIMESPEC.tv_sec);
    }
    e->value=NULL;
  }
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
