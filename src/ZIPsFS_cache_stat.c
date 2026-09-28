//////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                        ///
/// Cache file attributes of remote branches   ///
/// For read-only files                        ///
//////////////////////////////////////////////////
_Static_assert(WITH_STATCACHE,"");



// find  /s-mcpb-ms03/CHA-CHA-RALSER-RAW/store  -type f -not -name '*.md5' -printf '/%P\t%i\t%s\t%A@\t%U\t%G\n'
// /home/cgille/test/CHA-CHA-RALSER-RAW.content.gz


static int  _statcache_opt_for_root(const int opt_filldir_findrp, const root_t *r,const virtualfolder_t *vfolder){
  return ((opt_filldir_findrp&FINDRP_IS_PFXPLAIN)?STATCACHE_IS_PFXPLAIN:0)|
    (r->worm          ?STATCACHE_ROOT_IS_WORM:0)|
    (r->immutable     ?STATCACHE_ROOT_IS_IMMUTABLE:0)|
    (r==_root_writable?STATCACHE_ROOT_IS_WRITABLE:0)|
    (r->remote        ?STATCACHE_ROOT_IS_REMOTE:0)|
    IF1(WITH_TIMEOUT_STAT, (r->stat_timeout_seconds?STATCACHE_ROOT_WITH_TIMEOUT:0)|)
   ((r->preload||  (vfolder->flags&ID_FLAG(PRELOADDISK)))?STATCACHE_IS_PRELOADDISK:0);
}




static bool _stat_from_cache(const int opt_filldir_findrp,struct stat *stbuf, const char *vp, const int vp_l, ht_hash_t vp_hash,const zpath_t *zpath){

  if (vp&&*vp&&vp_l){
    const root_t *r=ZPR();
    if (vp_hash) ASSERT(vp_hash==hash32(vp,vp_l));
    const int opt_cache=_statcache_opt_for_root(opt_filldir_findrp,r,zpath->vfolder);
    if (!config_file_attribute_cache_TTL(opt_cache, rootpath(r), vp,vp_l,NULL,r->cache_TTL)) return false;
    stbuf->st_ino=0;
    LOCK_N(mutex_dircache,const struct stat *c=ht_get(&r->ht_stat,vp,vp_l,vp_hash);if (c) *stbuf=*c);
    //if (stbuf->st_mode&S_IFDIR) stat_set_dir(stbuf); /* Sonst sind in Samba die Verzeichnisse wie regular Files */
    time_t valid=config_file_attribute_cache_TTL(opt_cache, rootpath(r), vp,vp_l,stbuf,r->cache_TTL);
    if (opt_filldir_findrp&FINDRP_CACHE_NOT)  valid=MIN(valid,CACHE_TAKES_PRECEDENCE_TTL);
    if (time(NULL)-stbuf->st_atime<=valid){
      COUNTER1_INC(COUNT_STAT_FROM_CACHE);
      return true;
    }
    stbuf->st_ino=0;
  }
  return false;
}

static bool zpath_stat_from_cache(const int opt_filldir_findrp, zpath_t *zpath){
  root_t *r=ZPR();
  ASSERT(r); if (!r || !VP0_L()) return false;
  struct stat *st=&zpath->stat_rp;
  st->st_ino=0;
  bool ok=_stat_from_cache(opt_filldir_findrp,st,VP0(),VP0_L(),0,zpath);
#if WITH_PRELOADDISK_DECOMPRESS
  if (!ok && r->decompress_mask){
    char gz[VP_L()+COMPRESS_EXT_MAXLEN+1];
    FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){
      if (!(r->decompress_mask&(1<<iCompress))) continue;
      int ext_l=0;
      stpcpy(gz+VP_L(),cg_compression_file_ext(iCompress,&ext_l));
      ok=_stat_from_cache(opt_filldir_findrp,st,gz,VP_L()+ext_l,0,zpath);
      if (ok){
        st->st_size=nextRepdigitFileSize(st->st_size*64);
        st->st_ino=make_inode(st->st_ino,r,PSEUDO_ENTRYIDX_FOR_DECOMPRESS,RP());
      }
    }
  }
#endif //WITH_PRELOADDISK_DECOMPRESS
  if (!ok) st->st_ino=0;
  //if (ok) cg_log_file_stat(RP(),st);
  return ok && st->st_ino;
}

static void stat_to_cache(const int opt_filldir_findrp,const struct stat *stbuf, const char *vp,  const int vp_l, root_t *r, const virtualfolder_t *vfolder,time_t time_or_null){
  if (!stbuf || !stbuf->st_ino
      //|| !(stbuf->st_mode&S_IFREG)
      ) return;
  if (!(opt_filldir_findrp&FINDRP_STAT_TOCACHE_ALWAYS)){
    const int opt_cache=_statcache_opt_for_root(opt_filldir_findrp,r,vfolder);
    const time_t valid=config_file_attribute_cache_TTL(opt_cache,rootpath(r),vp,vp_l,stbuf,r->cache_TTL);
    //	log_debug_now("vp:%s valid:%ld  FINDRP_IS_WORM:%s valid:%s",vp,valid,yes_no(FINDRP_IS_WORM&opt_filldir_findrp),success_or_fail(valid!=0));
    if (!valid) return;
  }
  {
    lock(mutex_dircache);
    ht_entry_t *e=ht_get_entry(&r->ht_stat,vp,vp_l,0,true);
    if (!e->value)  e->value=mstore_malloc(&_root->dircache_mstore,sizeof(struct stat),8);
    struct stat *st=e->value;
    *st=*stbuf;
    st->st_atime=time_or_null?time_or_null:time(NULL);
    unlock(mutex_dircache);
  }
}
