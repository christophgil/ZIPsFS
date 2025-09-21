//////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                        ///
/// Cache file attributes of remote branches   ///
/// For read-only files                        ///
//////////////////////////////////////////////////


struct cached_stat{
  time_t when_read;
  ino_t st_ino;
  struct timespec ST_MTIMESPEC;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};

#define STAT_CACHE_OPT_FOR_ROOT(r) (!r?0: ((r->remote?STAT_CACHE_ROOT_IS_REMOTE:0)|(r==_root_writable?STAT_CACHE_ROOT_IS_WRITABLE:0)))
static bool _stat_from_cache(struct stat *stbuf, const struct strg *path, const struct rootdata *r){
  const int opt=STAT_CACHE_OPT_FOR_ROOT(r);
  if (0<config_file_attribute_valid_seconds(opt,path->s,path->l)){
    const time_t now=time(NULL);
    struct cached_stat st={0};
    LOCK(mutex_dircache,const struct cached_stat *c=ht_get(&stat_ht,path->s,path->l,path->hash);if (c) st=*c);
    if (st.st_ino){
      if (now-st.when_read<config_file_attribute_valid_seconds((IS_STAT_READONLY(st)?STAT_CACHE_FILE_IS_READONLY:0)|opt, path->s,path->l)){
#define C(f) stbuf->f=st.f
        C(st_ino);C(st_mode);C(st_uid);C(st_gid); C(ST_MTIMESPEC);
#undef C
        COUNTER_INC(COUNT_STAT_FROM_CACHE);
        return true;
      }
    }
  }
  return false;
}


static bool stat_from_cache(struct stat *stbuf, const struct strg *path, const struct rootdata *r){
  //if (path)log_entered_function("%s",path->s);
  bool ok=_stat_from_cache(stbuf, path,r);
  //  if (path)    log_exited_function("%s",path->s);
  return ok;
}
static void stat_to_cache(const struct stat *stbuf, const struct strg *path){
  //log_entered_function("'%s'",path->s);
  assert(path); assert(path->s);assert(*path->s);
#define C(f) st->f=stbuf->f
  lock(mutex_dircache);
  struct cached_stat *st=ht_get(&stat_ht,path->s,path->l,path->hash);
  if (!st) ht_set(&stat_ht,  ht_intern(&_root->dircache_ht_fname,path->s,path->l,path->hash,HT_MEMALIGN_FOR_STRG),  path->l,path->hash, st=mstore_malloc(&_root->dircache_mstore,sizeof(struct cached_stat),8));
  C(st_ino);C(st_mode);C(st_uid);C(st_gid);
  C(ST_MTIMESPEC);
  st->when_read=time(NULL);
  unlock(mutex_dircache);
#undef C
}
