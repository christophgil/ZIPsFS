///////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                     ///
/// Optional caches to improve performance  ///
///////////////////////////////////////////////


#if WITH_ZIPINLINE_CACHE
static const char *zinline_cache_vpath_to_zippath(const char *vp,const int vp_l){
  cg_thread_assert_locked(mutex_dircache);
  const char *zip=ht_numkey_get(&ht_zinline_cache_vpath_to_zippath,hash32(vp,vp_l),vp_l);
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
  LOCK(mutex_dircache,foreach_root1(r) dircache_clear_if_reached_limit(always,mask,r,0));
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
        IF1(WITH_DIRCACHE, if (_clear_ht(&r->dircache_ht_fname)) mstore_clear(&r->dircache_mstore));
      }
    if (!rootindex(r)){
      IF1(WITH_ZIPINLINE_CACHE, if M(CLEAR_ZIPINLINE_CACHE) _clear_ht(&ht_zinline_cache_vpath_to_zippath));
      IF1(WITH_STAT_CACHE,      if M(CLEAR_STATCACHE)       _clear_ht(&stat_ht));
    }
  }
#undef M
}
#endif //WITH_CLEAR_CACHE
static void debug_assert_crc32_not_null(const struct directory *dir){
  if (!dir || !(dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE)) return;
  RLOOP(i,dir->core.files_l){
    if (tdf_or_tdf_bin(dir->core.fname[i])){
      assert(dir->core.fcrc!=NULL);
      assert(dir->core.fcrc[i]!=0);
    }
  }
}
/*
  Not the entire struct directory only the directory_core is stored
  RAM: &r->dircache_mstore
*/
#if WITH_DIRCACHE
static void dircache_directory_to_cache(const struct directory *dir){
  cg_thread_assert_locked(mutex_dircache);
  debug_assert_crc32_not_null(dir);
  struct rootdata *r=dir->root;
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE));
  assert_validchars_direntries(VALIDCHARS_PATH,dir);
  struct directory_core src=dir->core, *d=mstore_add(&r->dircache_mstore,&src,sizeof(struct directory_core),SIZEOF_POINTER);
  if (src.files_l){
    RLOOP(i,src.files_l){
      d->fname[i]=(char*)ht_sinternalize(&r->dircache_ht_fname,src.fname[i]); /* All  filenames internalized */
    }
#define C(F,type) if (src.F) d->F=mstore_add(&r->dircache_mstore,src.F,src.files_l*sizeof(type),sizeof(type))
    C_FILE_DATA_WITHOUT_NAME();
#undef C    /*  Due to zipentry_placeholder_insert()  and internalization, the array of filenames will often be the same and needs to be stored only once - hence ht_intern.*/
    d->fname=(char**)ht_intern(&r->dircache_ht_fnamearray,src.fname,src.files_l*SIZE_POINTER,0,SIZE_POINTER);
  }
  const ht_hash_t rp_hash=hash32(dir->dir_realpath,dir->dir_realpath_l);
  const char *rp=ht_intern(&_root->dircache_ht_fname,dir->dir_realpath,dir->dir_realpath_l,rp_hash,HT_MEMALIGN_FOR_STRG); /* The real path of the zip file */
  ht_set(&r->dircache_ht,rp,dir->dir_realpath_l,rp_hash,d);
#if WITH_ZIPINLINE_CACHE
  if (dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
    static char u[PATH_MAX+1], vp_entry[PATH_MAX+1];
    const char *vp=rp+r->rootpath_l;
    const int slash1=cg_last_slash(vp)+1;//,vp_l=dir->dir_realpath_l-r->rootpath_l;
    stpncpy(vp_entry,vp,slash1);/* First copy parent dir */
    RLOOP(i,src.files_l){
      if (!d->fname[i]) continue;
      strcpy(u,d->fname[i]);
      IF1(WITH_ZIPENTRY_PLACEHOLDER,zipentry_placeholder_expand(u,rp));
      const int vp_entry_l=strlen(strcpy(vp_entry+slash1,u)); /* Append file name to parent dir */
      ht_numkey_set(&ht_zinline_cache_vpath_to_zippath,hash32(vp_entry,vp_entry_l),vp_entry_l,rp);
    } /* Note: We are not storing vp_entry. We rely on its hash value and accept collisions. */
  }
#endif // WITH_ZIPINLINE_CACHE
}
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim){
  cg_thread_assert_locked(mutex_dircache);
  struct ht_entry *e=ht_get_entry(&dir->root->dircache_ht,dir->dir_realpath,dir->dir_realpath_l,0,false);
  struct directory_core *s=e?e->value:NULL;
  //log_entered_function(ANSI_FG_MAGENTA"%s %d/%lu  e: %p  s: %p ht: %p root:%d"ANSI_RESET,dir->dir_realpath,dir->dir_realpath_l, strlen(dir->dir_realpath),e,s,&dir->root->dircache_ht,rootindex(dir->root));
  if (s){
    assert(mtim.tv_sec!=0);
    assert(s->mtim.tv_sec!=0);
    if (!CG_TIMESPEC_EQ(s->mtim,mtim)){/* Cached data not valid any more. */
      e->value=NULL;
    }else{
      dir->core=*s;
      assert_validchars_direntries(VALIDCHARS_PATH,dir);
      dir->dir_flags|=DIRECTORY_IS_DIRCACHE;
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
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l){
  const int fd=fdOrZero?fdOrZero:realpath?open(realpath,O_RDONLY):0;
  if (fd>0){
    posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
    if (!fdOrZero) close(fd);
  }
}
#else
#define maybe_evict_from_filecache(fdOrZero,realpath,realpath_l)
#endif //WITH_EVICT_FROM_PAGECACHE
