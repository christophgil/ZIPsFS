/////////////////////////////////////////////////////////////////////////////////////
/// Temporary cache for file attributs (struct stat).
/// The hashtable is a member of struct fhdata.
/// The cache lives as long as the fhdata instance representing a virtual file stored as a ZIP entry.
/// MOTIVATION:
///     We are using software which is sending lots of requests to the FS while the large data files are loaded from the ZIP file.
///
/////////////////////////////////////////////////////////////////////////////////////

#define FHDATA_BOTH_SHARE_TRANSIENT_CACHE(d1,d2) (d1->zpath.virtualpath_without_entry_hash==d2->zpath.virtualpath_without_entry_hash && !strcmp(D_VP0(d1),D_VP0(d2)))
static struct ht* transient_cache_get_ht(struct fhdata *d){
  struct ht *ht=d->ht_transient_cache;
  if (!ht){
    ht_set_mutex(mutex_fhdata,ht_init(ht=d->ht_transient_cache=calloc(1,sizeof(struct ht)),HT_FLAG_NUMKEY|5));
    mstore_set_mutex(mutex_fhdata,mstore_init(ht->value_store=calloc(1,sizeof(struct mstore)),NULL,(SIZEOF_ZIPPATH*16)|MSTORE_OPT_MALLOC));
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
        const bool no_value=e->value==NULL;
        if (no_value) e->value=mstore_malloc(ht->value_store,SIZEOF_ZIPPATH,8, MSTOREID(transient_cache_get_or_create_zpath),virtualpath);
        struct zippath *zpath=e->value;
        if (no_value || !zpath->virtualpath || strcmp(virtualpath,VP())){ /* Accept hash_collision */
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
