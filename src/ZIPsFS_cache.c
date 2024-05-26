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
    LOCK(mutex_dircache,struct cached_stat *c=ht_get(&stat_ht,path,path_l,hash);if (c) st=*c);
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
       if (!st) ht_set(&stat_ht,ht_intern(&_root->dircache_ht_fname,path,path_l,hash,HT_MEMALIGN_FOR_STRG),path_l,hash,st=mstore_malloc(DIRCACHE(_root),sizeof(struct cached_stat),8));
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
  struct directory_core src=dir->core, *d=mstore_add(DIRCACHE(r),&src,sizeof(struct directory_core),SIZEOF_POINTER);
  if (src.files_l){
    RLOOP(i,src.files_l){
      d->fname[i]=(char*)ht_sinternalize(&r->dircache_ht_fname,src.fname[i]); /* All  filenames internalized */
    }
#define C(F,type) if (src.F) d->F=mstore_add(DIRCACHE(r),src.F,src.files_l*sizeof(type),sizeof(type))
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
      log_debug_now0("Not valid any more");
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

/////////////////////////////////////////////////////////////////////////////////////
/// Temporary cache for file attributs (struct stat).
/// The hashtable is a member of struct fhdata.
/// The cache lives as long as the fhdata instance representing a virtual file stored as a ZIP entry.
/// MOTIVATION:
///     We are using software which is sending lots of requests to the FS while the large data files are loaded from the ZIP file.
///
/////////////////////////////////////////////////////////////////////////////////////
#if WITH_TRANSIENT_ZIPENTRY_CACHES
#define FHDATA_BOTH_SHARE_TRANSIENT_CACHE(d1,d2) (d1->zpath.virtualpath_without_entry_hash==d2->zpath.virtualpath_without_entry_hash && !strcmp(D_VP0(d1),D_VP0(d2)))
static struct ht* transient_cache_get_ht(struct fhdata *d){
  struct ht *ht=d->ht_transient_cache;
  if (!ht){
    ht_set_mutex(mutex_fhdata,ht_init(ht=d->ht_transient_cache=calloc(1,sizeof(struct ht)),HT_FLAG_NUMKEY|5));
    mstore_set_mutex(mutex_fhdata,mstore_init(ht->value_store=calloc(1,sizeof(struct mstore)),(SIZEOF_ZIPPATH*16)|MSTORE_OPT_MALLOC));
    //const ht_hash_t hash=d->zpath.virtualpath_without_entry_hash;
    foreach_fhdata(ie,e){
      if (!e->ht_transient_cache && FHDATA_BOTH_SHARE_TRANSIENT_CACHE(e,d)) e->ht_transient_cache=ht;
    }
  }
  return ht;
}
/* Get/create a zippath object for a given virtual path.
   The hash table is stored in  struct fhdata.
   Therefore the list of fhdata instances are iterated to find an entry in its hash table.
   If no zpath entry  found, one is created and associated to the cache of one of the fhdata instances.
   The cache lives as long as the fhdata object.
   This function is not searching for realpath of path. It is just retrieving or creating struct zippath.
   //  old: zipentry_cache_get_or_create_zpath_for_virtualpath DEBUG_NOW
   */
static struct zippath *transient_cache_get_or_create_zpath(const bool create,const char *virtualpath,const int virtualpath_l){
  ASSERT_LOCKED_FHDATA();
  const ht_hash_t hash=hash32(virtualpath,virtualpath_l);
  if (*virtualpath) assert(virtualpath_l>0);
  foreach_fhdata(id,d){
    if (!(d->flags&FHDATA_FLAGS_WITH_TRANSIENT_ZIPENTRY_CACHES)) continue;
    const char *vp=D_VP(d);
    if (!vp || !*vp || !zpath_exists(&d->zpath) || !(d->zpath.flags&ZP_ZIP)) continue;
    const bool maybe_same_zip=cg_path_equals_or_is_parent(vp,D_VP_L(d)-D_EP_L(d)-1,virtualpath,virtualpath_l); /* (VP_L-EP_L) is the part of vp that specifies the ZIP file */
    if (maybe_same_zip || D_VP_L(d)>0 && cg_path_equals_or_is_parent(virtualpath,virtualpath_l,vp,D_VP_L(d))){  /* || virtualpath is a parent of vp */
      struct ht *ht=transient_cache_get_ht(d);
      struct ht_entry *e=ht_numkey_get_entry(ht,hash,virtualpath_l,create);
      if (e){
        if (!e->value) e->value=mstore_malloc(ht->value_store,SIZEOF_ZIPPATH,8);
        struct zippath *zpath=e->value;
        if (strcmp(virtualpath,VP())){ /* Accept hash_collision */
          zpath_init(zpath,virtualpath);
        }else if (!create){
          if (!maybe_same_zip && (zpath->flags&ZP_DOES_NOT_EXIST)) return NULL; /* Did not exist before. This  is taken as evidence that it is still absent. This is because  according to path is part of same zip */
          if (zpath_exists(zpath) && (zpath->flags&ZP_DOES_NOT_EXIST)!=0){ log_zpath("ZP_DOES_NOT_EXIST???",zpath); DIE_DEBUG_NOW0("ZZZ");}
          //          if (!zpath->realpath && (zpath->flags&ZP_DOES_NOT_EXIST)==0){ log_zpath("!ZP_DOES_NOT_EXIST???",zpath); DIE_DEBUG_NOW0("! ZZZ");}
          ht->client_value_int[zpath_exists(zpath)]++;
        }
        return zpath;
      }
    }
  }
  return NULL;
}
#endif //WITH_TRANSIENT_ZIPENTRY_CACHES

///////////////////////////
///// file i/o cache  /////
///////////////////////////
#if WITH_EVICT_FROM_PAGECACHE
static void maybe_evict_from_filecache(const int fdOrZero,const char *realpath,const int realpath_l){
  const int fd=fdOrZero?fdOrZero:realpath?open(realpath,O_RDONLY):0;
    if (fd>0){
      posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);
      if (!fdOrZero) close(fd);
    }
}
#endif //WITH_EVICT_FROM_PAGECACHE
