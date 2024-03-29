///////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                   ///
/// Optional caches to improve performance  ///
///////////////////////////////////////////////


/////////////////////////////////////////////////////////
// Zip inline cache                                    //
// Faster /bin/ls with many inlined ZIPs               //
// Can be deaktivated by setting  WITH_ZIPINLINE_CACHE to 0 //
/////////////////////////////////////////////////////////
//   time ls -l  $TZ/Z1/Data/30-0072 |nl
//   time ls -l  $TZ/Z1/Data/30-0051 |nl

#if WITH_STAT_CACHE
struct cached_stat{
  int when_read_decisec;
  ino_t st_ino;
  struct timespec st_mtim;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};


static bool stat_from_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
  if (0<config_file_attribute_valid_seconds(true,path,path_l)){
    const int now=deciSecondsSinceStart();
    struct cached_stat st={0};
    {
      LOCK(mutex_dircache,struct cached_stat *pointer=ht_get(&stat_ht,path,path_l,hash));
      if (pointer) st=*pointer;
    }
    if (st.st_ino){
      if (now-st.when_read_decisec<10L*config_file_attribute_valid_seconds(IS_STAT_READONLY(st),path,path_l)){
#define C(f) stbuf->st_##f=st.st_##f
        C(ino);C(mtim);C(mode);C(uid);C(gid);
#undef C
        _count_stat_from_cache++;
        return true;
      }
    }
  }
  return false;
}

static void stat_to_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
#define C(f) st->st_##f=stbuf->st_##f
  LOCK(mutex_dircache,
       struct cached_stat *st=ht_get(&stat_ht,path,path_l,hash);
       if (!st) ht_set(&stat_ht,ht_internalize(&_root->dircache_ht_fname,path,path_l,hash,HT_MEMALIGN_FOR_STRG),path_l,hash,st=mstore_malloc(DIRCACHE(_root),sizeof(struct cached_stat),8));
       C(ino);C(mtim);C(mode);C(uid);C(gid);
       st->when_read_decisec=deciSecondsSinceStart());
  // assert(st->st_mtim.tv_nsec==stbuf->st_mtim.tv_nsec));
#undef C
}


#endif
#if WITH_ZIPINLINE_CACHE
static const char *zipinline_cache_virtualpath_to_zippath(const char *vp,const int vp_l){
  cg_thread_assert_locked(mutex_dircache);
  const char *zip=ht_numkey_get(&zipinline_cache_virtualpath_to_zippath_ht,hash32(vp,vp_l),vp_l);
  if (!zip) return NULL;
  /* validation because we rely on hash only */
  const char *vp_name=vp+cg_last_slash(vp)+1;
  const int len=config_containing_zipfile_of_virtual_file(0,vp_name,strlen(vp_name),NULL);
  return strncmp(vp_name,zip+cg_last_slash(zip)+1,len)?NULL:zip;
}
#endif //WITH_ZIPINLINE_CACHE
// static void zipinline_cache_set_zip(

//////////////////////////////////////////////////////////////////////
/// Directory from and to cache
//////////////////////////////////////////////////////////////////////
#define CLEAR_ALL_CACHES 0
#define CLEAR_DIRCACHE 1
#define CLEAR_ZIPINLINE_CACHE 2
#define CLEAR_STATCACHE 3
  #if DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
static void dircache_clear_if_reached_limit_all(const bool always,const int mask){
#define C(m) 0==(mask&(1<<m))?"":#m
  if (always) log_verbose("%s %s %s\n",C(CLEAR_DIRCACHE),C(CLEAR_STATCACHE),C(CLEAR_ZIPINLINE_CACHE));
#undef C
  LOCK(mutex_dircache,foreach_root(i,r)  dircache_clear_if_reached_limit(always,mask,r,0));
}
static void dircache_clear_if_reached_limit(const bool always,const int mask,struct rootdata *r,size_t limit){
  if (!DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT && !always || !r) return;
  cg_thread_assert_locked(mutex_dircache);
  IF1(WITH_DIRCACHE,const size_t ss=mstore_count_blocks(DIRCACHE(r)));
#define M(x) (0!=(mask&(1<<x)))
  if (always IF1(WITH_DIRCACHE,|| ss>=limit)){
    IF1(WITH_DIRCACHE,if (!always)  warning(WARN_DIRCACHE,r->rootpath,"Clearing directory cache. Cached segments: %zu (%zu) bytes: %zu. %s",ss,limit,mstore_usage(DIRCACHE(r)),always?"":"Consider to increase DIRECTORY_CACHE_SEGMENTS"));
    if M(CLEAR_DIRCACHE){
        ht_clear(&r->dircache_ht);
        IF1(WITH_DIRCACHE,  ht_clear(&r->dircache_ht_fname);ht_clear(&r->dircache_ht_dir));
      }
    if (r==_root){
      IF1(WITH_ZIPINLINE_CACHE, if M(CLEAR_ZIPINLINE_CACHE) ht_clear(&zipinline_cache_virtualpath_to_zippath_ht));
      IF1(WITH_STAT_CACHE, if M(CLEAR_STATCACHE) ht_clear(&stat_ht));
      //ht_clear(&ht_internalize_mutex_dircache);
    }
  }
#undef M
}
#endif //DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
static void debug_assert_crc32_not_null(const struct directory *dir){
  if (!dir || !(dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE)) return;
  //  log_debug_now("core.files_l:  %d\n",dir->core.files_l);
  RLOOP(i,dir->core.files_l){
    if (tdf_or_tdf_bin(dir->core.fname[i])){
      assert(dir->core.fcrc!=NULL);
      assert(dir->core.fcrc[i]!=0);
    }
  }
}
/*
Not the entire struct directory only the directory_core is stored
RAM: DIRCACHE(r)



 */

#if WITH_DIRCACHE
static void dircache_directory_to_cache(const struct directory *dir){
  cg_thread_assert_locked(mutex_dircache);
  debug_assert_crc32_not_null(dir);
  struct rootdata *r=dir->root;
  IF1(DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,DIRECTORY_CACHE_SEGMENTS));
  assert_validchars_direntries(VALIDCHARS_PATH,dir);
  struct directory_core src=dir->core, *d=mstore_add(DIRCACHE(r),&src,sizeof(struct directory_core),SIZEOF_POINTER);
  if (src.files_l){
    RLOOP(i,src.files_l) d->fname[i]=(char*)ht_sinternalize(&r->dircache_ht_fname,src.fname[i]); /* All  filenames internalized */
#define C(F,type) if (src.F) d->F=mstore_add(DIRCACHE(r),src.F,src.files_l*sizeof(type),sizeof(type))
    C_FILE_DATA_WITHOUT_NAME();
#undef C    /*  Due to simplify_name and internalization, the array of filenames will often be the same and needs to be stored only once.*/
    d->fname=(char**)ht_internalize(&r->dircache_ht_fnamearray,src.fname,src.files_l*SIZE_POINTER,0,SIZE_POINTER);
  }
  const ht_hash_t rp_hash=hash32(dir->dir_realpath,dir->dir_realpath_l);
  const char *rp=ht_internalize(&_root->dircache_ht_fname,dir->dir_realpath,dir->dir_realpath_l,rp_hash,HT_MEMALIGN_FOR_STRG); /* The real path of the zip file */
  ht_set(&r->dircache_ht,rp,dir->dir_realpath_l,rp_hash,d);
#if WITH_ZIPINLINE_CACHE
  if (dir->dir_flags&DIRECTORY_IS_ZIPARCHIVE){
    static char u[PATH_MAX+1], vp_entry[PATH_MAX+1];
    const char *vp=rp+r->rootpath_l;
    const int slash1=cg_last_slash(vp)+1;//,vp_l=dir->dir_realpath_l-r->rootpath_l;
    strncpy(vp_entry,vp,slash1);/* First copy parent dir */
    RLOOP(i,src.files_l){
      unsimplify_fname(strcpy(u,d->fname[i]),rp);
      const int vp_entry_l=strlen(strcpy(vp_entry+slash1,u)); /* Append file name to parent dir */
      ht_numkey_set(&zipinline_cache_virtualpath_to_zippath_ht,hash32(vp_entry,vp_entry_l),vp_entry_l,rp);
    } /* Note: We are not storing vp_entry. We rely on its hash value and accept collisions. */
  }
#endif // WITH_ZIPINLINE_CACHE
}
static bool dircache_directory_from_cache(struct directory *dir,const struct timespec mtim){
  cg_thread_assert_locked(mutex_dircache);
  struct ht_entry *e=ht_sget_entry(&dir->root->dircache_ht,dir->dir_realpath,false);
  struct directory_core *s=e?e->value:NULL;
  if (s){
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

/////////////////////////////////////////////////////////////////////////////////////
/// Temporary cache for file attributs (struct stat).
/// The hashtable is a member of struct fhdata.
/// The cache lives as long as the fhdata instance representing a virtual file stored as a ZIP entry.
/// MOTIVATION:
///     We are using software which is sending lots of requests to the FS while the large data files are loaded from the ZIP file.
///
/////////////////////////////////////////////////////////////////////////////////////
#if WITH_TRANSIENT_ZIPENTRY_CACHES
static struct ht* zipentry_cache_get_ht(struct fhdata *d){
  struct ht *ht=d->ht_zipentry_cache;
  if (!ht){
    ht_set_mutex(mutex_fhdata,ht_init(ht=d->ht_zipentry_cache=calloc(1,sizeof(struct ht)),HT_FLAG_NUMKEY|5));
    mstore_set_mutex(mutex_fhdata,mstore_init(ht->value_store=calloc(1,sizeof(struct mstore)),(SIZEOF_ZIPPATH*16)|MSTORE_OPT_MALLOC));
    foreach_fhdata_same_path(d,d_loop){
      if (!d_loop->ht_zipentry_cache) d_loop->ht_zipentry_cache=ht;
    }
  }
  return ht;
}
/* Get/create a zippath object for a given virtual path.
   The hash table is associated to an instance of  struct fhdata.
   Therefore the list of fhdata instances are iterated to find an entry in its hash table.
   If no zpath entry  found, one is created and associated to the cache of one of the fhdata instances.
   The cache lives as long as the fhdata object.
*/
static struct zippath *zipentry_cache_get_or_create_zpath_for_virtualpath(bool verbose,const char *path,const int path_l, struct rootdata *r, struct ht *ht_reuse[1]){
  ASSERT_LOCKED_FHDATA();
  //verbose=verbose && cg_endsWithDotD(path,path_l);
  const ht_hash_t hash=hash32(path,path_l);
  if (verbose) log_entered_function("path: %s hash:%u path_l=%d\n",path,hash,path_l);
  foreach_fhdata(id,d){
    if (!(d->flags&FHDATA_FLAGS_WITH_TRANSIENT_ZIPENTRY_CACHES)) continue;
    const char *vp=D_VP(d);
    if (!vp || !*vp || !d->zpath.stat_rp.st_ino) continue;
    const int vp_l=D_VP_L(d);
    ASSERT(ROOTd(d)!=NULL);
    assert(d->zpath.flags&ZP_ZIP);
    const bool maybe_same_zip=cg_path_equals_or_is_parent(vp,vp_l-D_EP_L(d)-1,path,path_l); /* Left is the part of vp that describes the ZIP file */
    const bool or_path_is_parent_dir=!maybe_same_zip && vp_l>0 && cg_path_equals_or_is_parent(path,path_l,vp,vp_l);
    if (maybe_same_zip || or_path_is_parent_dir){
      if (verbose) log_verbose("path: %s vp=%s\n",path,vp);
      if (!*ht_reuse) *ht_reuse=zipentry_cache_get_ht(d);
      const int key2=(path_l|(rootindex(r)<<12));
      struct ht_entry *e=ht_numkey_get_entry(*ht_reuse,hash,key2);
      struct zippath *zpath=e->value;
      if (zpath && strcmp(path,VP())){ zpath=NULL; warning(WARN_DIRCACHE,path,"hash_collision");}
      if (zpath && zpath->root && zpath->root!=r){ warning(WARN_DIRCACHE|WARN_FLAG_ERROR,path,"rootindex(zpath->root)=%d rootpath: %s",rootindex(zpath->root),r->rootpath); zpath=NULL;}
      if (zpath && (zpath->realpath||(zpath->flags&ZP_DOES_NOT_EXIST))){  /* Mind possibility of hash collision */
        if (verbose) log_verbose0("OK\n");
      }else{
        HT_ENTRY_SET_NUMKEY(e,hash,key2);
        if (!zpath) zpath=e->value=mstore_malloc((*ht_reuse)->value_store,SIZEOF_ZIPPATH,8);
        zpath_init(zpath,path);
        if (verbose) log_verbose("zpath_init zpath %p\n",zpath);
      }
      return zpath;
    }
  }
  if (verbose) log_verbose0("return NULL;\n");
  return NULL;
}
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES

///////////////////////////
///// file i/o cache  /////
///////////////////////////
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l,const char *zipentryOrNull,const int zipentry_l){
  if (realpath && config_advise_evict_from_filecache(realpath,realpath_l,zipentryOrNull,zipentry_l)){
    const int fd=fdOrZero?fdOrZero:open(realpath,O_RDONLY);
    if (fd>0){
      posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
      if (!fdOrZero) close(fd);
    }
  }
}
