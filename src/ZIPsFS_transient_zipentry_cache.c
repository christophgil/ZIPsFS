////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                                                                                          ///
/// Temporary cache for file attributs (struct stat).
/// The hashtable is a member of struct fhandle.
/// The cache lives as long as the fhandle instance representing a virtual file stored as a ZIP entry.
/// MOTIVATION:
///     We are using software which is sending lots of requests to the FS while the large data files are loaded from the ZIP file.
///
/////////////////////////////////////////////////////////////////////////////////////

#define FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(d1,d2) (d1->zpath.virtualpath_without_entry_hash==d2->zpath.virtualpath_without_entry_hash && !strcmp(D_VP0(d1),D_VP0(d2)))
static struct ht* transient_cache_get_ht(struct fhandle *d){
  struct ht *ht=d->ht_transient_cache;
  if (!ht){
    ht_set_mutex(mutex_fhandle,ht_init(ht=d->ht_transient_cache=calloc_untracked(1,sizeof(struct ht)),"transient_cache",HT_FLAG_NUMKEY|HT_FLAG_COUNTMALLOC|5));
    ht_set_id(HT_MALLOC_transient_cache,ht);
    mstore_set_mutex(mutex_fhandle,mstore_init(ht->valuestore=calloc_untracked(1,sizeof(struct mstore)),NULL,(SIZEOF_ZIPPATH*16)|MSTORE_OPT_MALLOC));
    //const ht_hash_t hash=d->zpath.virtualpath_without_entry_hash;
    foreach_fhandle(ie,e){
      if (!e->ht_transient_cache && FHANDLE_BOTH_SHARE_TRANSIENT_CACHE(e,d)) e->ht_transient_cache=ht;
    }
  }
  return ht;
}
/* Get/create a zippath object for a given virtual path.
   The hash table is stored in  struct fhandle.
   Therefore the list of fhandle instances are iterated to find an entry in its hash table.
   If no zpath entry  found, one is created and associated to the cache of one of the fhandle instances.
   The cache lives as long as the fhandle object.
   This function is not searching for realpath of path. It is just retrieving or creating struct zippath.
*/
static struct zippath *transient_cache_get_or_create_zpath(const bool create,const char *virtualpath,const int virtualpath_l){
  ASSERT_LOCKED_FHANDLE();
  const ht_hash_t hash=hash32(virtualpath,virtualpath_l);
  if (*virtualpath) assert(virtualpath_l>0);
  foreach_fhandle(id,d){
    if (!(d->flags&FHANDLE_FLAGS_WITH_TRANSIENT_ZIPENTRY_CACHES)) continue;
    const char *vp=D_VP(d);
    if (!vp || !*vp || !zpath_exists(&d->zpath) || !(d->zpath.flags&ZP_ZIP)) continue;
    const bool maybe_same_zip=cg_path_equals_or_is_parent(vp,D_VP_L(d)-D_EP_L(d)-1,virtualpath,virtualpath_l); /* (VP_L-EP_L) is the part of vp that specifies the ZIP file */
    if (maybe_same_zip || D_VP_L(d)>0 && cg_path_equals_or_is_parent(virtualpath,virtualpath_l,vp,D_VP_L(d))){  /* || virtualpath is a parent of vp */
      struct ht *ht=transient_cache_get_ht(d);
      struct ht_entry *e=ht_numkey_get_entry(ht,hash,virtualpath_l,create);
      if (e){
        const bool no_value=e->value==NULL;
        if (no_value) e->value=mstore_malloc(ht->valuestore,SIZEOF_ZIPPATH,8);
        struct zippath *zpath=e->value;
        if (no_value || !zpath->virtualpath || strcmp(virtualpath,VP())){ /* Accept hash_collision */
          zpath_init(zpath,virtualpath);
        }else if (!create){
          // DEBUG_NOW  Used to be  (!maybe_same_zip which is  wrong AFAIK
          if (maybe_same_zip && (zpath->flags&ZP_DOES_NOT_EXIST)){
            return NULL; /* Did not exist before. This  is taken as evidence that it is still absent. This is because  according to path is part of same zip */
          }
          ht->client_value_int[zpath_exists(zpath)]++;
        }
        return zpath;
      }
    }
  }
  return NULL;
}
