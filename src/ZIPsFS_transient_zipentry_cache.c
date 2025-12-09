////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                                                                          ///
/// Temporary cache for file attributs (struct stat).
/// MOTIVATION:
///     We are using software which is sending lots of requests to the FS.
///     At the same time a large zip entry  is accessed.
///     The goal is to reduce calls to stat_direct()
/// DESIGN:
///     The struct fHandle of that file carries a hash map which acts as a cache.
///     The cache lives as long as the fHandle instance
///
/////////////////////////////////////////////////////////////////////////////////////

#define FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(d1,d2) (d1->zpath.virtualpath_without_entry_hash==d2->zpath.virtualpath_without_entry_hash && !strcmp(D_VP0(d1),D_VP0(d2)))


static bool zpath_has_inode(struct zippath *zpath){
  if (!zpath) return false;
  const ino_t ino=zpath->stat_rp.st_ino;
  if ((ino!=0) != (zpath->realpath_l!=0)) warning(WARN_FLAG_ONCE|WARN_FLAG_ERROR,RP(),"Inconsistence ino: %ld  realpath_l: %d",ino,zpath->realpath_l);
  return ino!=0;
}

static struct ht* transient_cache_make_ht(struct fHandle *d){
  struct ht *ht=d->ht_transient_cache;
  if (!ht){
    //log_debug_now(ANSI_RED"%p %s",d,D_RP(d));
    ht_set_mutex(mutex_fhandle,ht_init(ht=d->ht_transient_cache=cg_calloc(COUNT_FHANDLE_TRANSIENT_CACHE,1,sizeof(struct ht)),"transient_cache",HT_FLAG_NUMKEY|5));
    assert(0==(ht->flags&HT_FLAG_KEYS_ARE_STORED_EXTERN));
    ht->ht_counter_malloc=COUNT_HT_MALLOC_TRANSIENT_CACHE;
    ht->key_malloc_id=COUNT_MALLOC_KEY_TRANSIENT_CACHE;
    ht_set_id(HT_MALLOC_transient_cache,ht);
    mstore_set_mutex(mutex_fhandle,mstore_init(ht->valuestore=calloc_untracked(1,sizeof(struct mstore)),NULL,(sizeof(struct zippath)*16)|MSTORE_OPT_MALLOC));
    //const ht_hash_t hash=d->zpath.virtualpath_without_entry_hash;
    ht->valuestore->mstore_counter_mmap=COUNT_MSTORE_MMAP_TRANSIENT_CACHE_VALUES;
    foreach_fhandle(ie,e){
      if (!e->ht_transient_cache && FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(e,d)) e->ht_transient_cache=ht;
    }
  }
  return ht;
}
/* Get/create a zippath object for a given virtual path.
   The hash table is stored in  struct fHandle.
   Therefore the list of fHandle instances are iterated to find an entry in its hash table.
   If no zpath entry  found, one is created and associated to the cache of one of the fHandle instances.
   The cache lives as long as the fHandle object.
   This function is not searching for realpath of path. It is just retrieving or creating struct zippath.
*/
static struct zippath *transient_cache_get_or_create_zpath(const bool create,const bool must_be_same_zip, const char *virtualpath,const int virtualpath_l){
  ASSERT_LOCKED_FHANDLE();
  const ht_hash_t hash=hash32(virtualpath,virtualpath_l);
  if (*virtualpath) assert(virtualpath_l>0);
  struct ht *ht1=NULL;
  struct fHandle *d1=NULL;
  foreach_fhandle(id,d){ /* Look for fHandle with attached hash map */
    if (create){
      if (!(d->flags&FHANDLE_FLAG_WITH_TRANSIENT_ZIPENTRY_CACHES)) continue;
    }else{
      if (!d->ht_transient_cache) continue;
    }
    const int vp_l=D_VP_L(d);
    if (!vp_l || !(d->zpath.flags&ZP_ZIP) || !zpath_has_inode(&d->zpath) ) continue;
        const char *vp=D_VP(d);
        const int path_without_entry_path_l=vp_l-D_EP_L(d)-1;
    const bool maybe_same_zip=path_without_entry_path_l && cg_path_equals_or_is_parent(vp,path_without_entry_path_l,virtualpath,virtualpath_l);
    if (must_be_same_zip && !maybe_same_zip) continue;
    /* maybe_same_zip: path_without_entry_path is part of virtualpath indicating that virtualpath is likely to be in the same ZIP file. */
    const bool ok=(maybe_same_zip || vp_l>0 && cg_path_equals_or_is_parent(virtualpath,virtualpath_l,vp,vp_l));  /* || virtualpath is a parent of vp */
    //log_debug_now("Virtualpath: %s vp: %s ok: %d",virtualpath,vp,ok);
    if (!ok) continue;
    if (!d1) d1=d;
    struct ht *ht=d->ht_transient_cache;
    if (!ht) continue;
    if (!ht1) ht1=ht;
    struct ht_entry *e=ht_numkey_get_entry(ht,hash,virtualpath_l,false);
    if (!e || !e->key || !e->value) continue;
    struct zippath *zpath=e->value;
    if (!zpath->virtualpath
        || strcmp(virtualpath,VP())){ /* Accept hash_collision */
      zpath_init(zpath,virtualpath);
    }
    if (maybe_same_zip && ZPF(ZP_DOES_NOT_EXIST)){
      //DIE_DEBUG_NOW("ZP_DOES_NOT_EXIST %s",VP());
      return NULL;
    }
    ht->client_value_int[zpath_has_inode(zpath)]++;
    if (maybe_same_zip) zpath->flags|=ZP_FROM_TRANSIENT_CACHE;
    return zpath;
  }
  if (create && d1){
    if (!ht1) ht1=transient_cache_make_ht(d1);
    struct ht_entry *e=ht_numkey_get_entry(ht1,hash,virtualpath_l,true);
    e->value=mstore_malloc(ht1->valuestore,sizeof(struct zippath),8);
    memset(e->value,0,sizeof(struct zippath));
    return e->value;
  }
  return NULL;
}

static int transient_cache_find_realpath(const int opt, struct zippath *zpath, const char *vp,const int vp_l){
  //log_entered_function("vp: %s opt: %d ",VP(),(opt&FINDRP_NOT_TRANSIENT_CACHE));
  if ((opt&FINDRP_NOT_TRANSIENT_CACHE)) return 0;
  struct zippath cached={0};
  LOCK(mutex_fhandle,const struct zippath *zp=transient_cache_get_or_create_zpath(false,false,vp,vp_l); if (zp) cached=*zp);
  const int f=cached.flags;
  if (!(f&ZP_DOES_NOT_EXIST) && zpath_has_inode(&cached)){
    assert(cached.virtualpath && !strcmp(ZP_VP(&cached),vp));
    *zpath=cached;
    IF_LOG_FLAG(LOG_TRANSIENT_ZIPENTRY_CACHE) log_verbose(ANSI_FG_GREEN"%s %d"ANSI_RESET,vp,vp_l);
    //log_debug_now("%s %d opt: %d"GREEN_SUCCESS,vp,vp_l,opt);
    return 1;
  }
  if (f&ZP_DOES_NOT_EXIST){
    IF_LOG_FLAG(LOG_TRANSIENT_ZIPENTRY_CACHE) log_verbose(ANSI_FG_RED"%s"ANSI_RESET,vp);
    //log_debug_now(ANSI_MAGENTA"%s %d"ANSI_RESET,vp,vp_l);
    return -1;
  }
  //log_debug_now("%s %d"RED_FAIL" cached:'%s'  inode: %d",vp,vp_l,ZP_VP(&cached),zpath_has_inode(&cached));
  return 0;
}

static void transient_cache_store(const struct zippath *zpath, const char *vp,const int vp_l){
  struct zippath *zp=transient_cache_get_or_create_zpath(true,!zpath,vp,vp_l);
  //log_debug_now("%s %d %s  zpath: %s",vp,vp_l,zp?GREEN_SUCCESS:RED_FAIL,zpath?VP():NULL);
  if (zp){
    if (!zpath){
      zp->flags|=ZP_DOES_NOT_EXIST; /* Not found in any root. */
    }else if (!zp->virtualpath){
      *zp=*zpath;
#if 0 //WITH_EXTRA_ASSERT
      const struct zippath *wiedergefunden=transient_cache_get_or_create_zpath(false,false,vp,vp_l);
      assert(NULL!=wiedergefunden);
      //log_debug_now("Wiederfinden: %p %s",wiedergefunden,wiedergefunden?ZP_VP(wiedergefunden):NULL);
#endif //WITH_EXTRA_ASSERT
    }
  }
}

static void transient_cache_destroy(struct fHandle *d){
  struct ht *ht=d->ht_transient_cache;
  if (ht){
    {
      foreach_fhandle(ie,e) if (d!=e && ht==e->ht_transient_cache) return;
    }
    ht_destroy(ht);
    cg_free(COUNT_FHANDLE_TRANSIENT_CACHE,ht);
    {
      foreach_fhandle(ie,e) if (ht==e->ht_transient_cache) e->ht_transient_cache=NULL;
    }
  }
}

static void transient_cache_activate(struct fHandle *d){
  if ((d->zpath.flags&ZP_ZIP) && config_advise_transient_cache_for_zipentries(D_VP(d),D_VP_L(d))){
    d->flags|=FHANDLE_FLAG_WITH_TRANSIENT_ZIPENTRY_CACHES;
    foreach_fhandle(ie,e){
      if (d!=e && FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(d,e) && NULL!=(d->ht_transient_cache=e->ht_transient_cache)) break;
    }
  }
}
