//////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                        ///
/// Cache file attributes of remote branches   ///
/// For read-only files                        ///
//////////////////////////////////////////////////


struct cached_stat{
  int when_read_decisec;
  ino_t st_ino;
  struct timespec ST_MTIMESPEC;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};


static bool stat_from_cache(const int opt,struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
  if (0<config_file_attribute_valid_mseconds(opt,path,path_l)){
    const int now=deciSecondsSinceStart();
    struct cached_stat st={0};
    LOCK(mutex_dircache,const struct cached_stat *c=ht_get(&stat_ht,path,path_l,hash);if (c) st=*c);
    if (st.st_ino){
      if (now-st.when_read_decisec*100UL<config_file_attribute_valid_mseconds((IS_STAT_READONLY(st)?STAT_CACHE_FILE_IS_READONLY:0)|opt, path,path_l)){
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
static void stat_to_cache(const struct stat *stbuf,const char *path,const int path_l,const ht_hash_t hash){
#define C(f) st->f=stbuf->f
  LOCK(mutex_dircache,
       struct cached_stat *st=ht_get(&stat_ht,path,path_l,hash);
       if (!st) ht_set(&stat_ht,  ht_intern(&_root->dircache_ht_fname,path,path_l,hash,HT_MEMALIGN_FOR_STRG),  path_l,hash, st=mstore_malloc(&_root->dircache_mstore,sizeof(struct cached_stat),8));
       C(st_ino);C(st_mode);C(st_uid);C(st_gid);
       C(ST_MTIMESPEC);
       st->when_read_decisec=deciSecondsSinceStart());
#undef C
}
