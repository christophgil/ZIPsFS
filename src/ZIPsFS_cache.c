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
  struct timespec ST_MTIMESPEC;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};

static bool stat_from_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
  if (0<config_file_attribute_valid_seconds(true,path,path_l)){
    const int now=deciSecondsSinceStart();
    struct cached_stat st={0};
    LOCK(mutex_dircache,struct cached_stat *c=ht_get(&stat_ht,path,path_l,hash);if (c) st=*c);
    if (st.st_ino){
      if (now-st.when_read_decisec<10L*config_file_attribute_valid_seconds(IS_STAT_READONLY(st),path,path_l)){
#define C(f) stbuf->f=st.f
        C(st_ino);C(st_mode);C(st_uid);C(st_gid); C(ST_MTIMESPEC);
#undef C
        _count_stat_from_cache++;
        return true;
      }
    }
  }
  return false;
}

static void stat_to_cache(struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
#define C(f) st->f=stbuf->f
  LOCK(mutex_dircache,
       struct cached_stat *st=ht_get(&stat_ht,path,path_l,hash);
       if (!st) ht_set(&stat_ht,ht_intern(&_root->dircache_ht_fname,path,path_l,hash,HT_MEMALIGN_FOR_STRG),path_l,hash,st=mstore_malloc(DIRCACHE(_root),sizeof(struct cached_stat),8, MSTOREID_stat_to_cache,path));
       C(st_ino);C(st_mode);C(st_uid);C(st_gid);
       C(ST_MTIMESPEC);
       st->when_read_decisec=deciSecondsSinceStart());
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
  LOCK(mutex_dircache,foreach_root(i,r) dircache_clear_if_reached_limit(always,mask,r,0));
}
static void dircache_clear_if_reached_limit(const bool always,const int mask,struct rootdata *r,off_t limit){
  if (!DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT && !always || !r) return;
  cg_thread_assert_locked(mutex_dircache);
  IF1(WITH_DIRCACHE,const off_t ss=mstore_count_blocks(DIRCACHE(r)));
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
  // log_entered_function("%s ",dir->dir_realpath);
  struct rootdata *r=dir->root;
  IF1(DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,dircache_clear_if_reached_limit(false,0xFFFF,r,DIRECTORY_CACHE_SEGMENTS));
  assert_validchars_direntries(VALIDCHARS_PATH,dir);
  struct directory_core src=dir->core, *d=mstore_add(DIRCACHE(r),&src,sizeof(struct directory_core),SIZEOF_POINTER, MSTOREID(dircache_directory_to_cache),"");
  if (src.files_l){
    RLOOP(i,src.files_l){
      d->fname[i]=(char*)ht_sinternalize(&r->dircache_ht_fname,src.fname[i]); /* All  filenames internalized */
    }
#define C(F,type) if (src.F) d->F=mstore_add(DIRCACHE(r),src.F,src.files_l*sizeof(type),sizeof(type), MSTOREID(dircache_directory_to_cache),"")
    C_FILE_DATA_WITHOUT_NAME();
#undef C    /*  Due to simplify_name and internalization, the array of filenames will often be the same and needs to be stored only once - hence ht_intern.*/
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
  //log_debug_now("dir_realpath: %s   s: %p",dir->dir_realpath,s);
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
