//////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                        ///
/// Cache file attributes of remote branches   ///
/// For read-only files                        ///
//////////////////////////////////////////////////
_Static_assert(WITH_STAT_CACHE,"");

// find  /s-mcpb-ms03/CHA-CHA-RALSER-RAW/store  -type f -not -name '*.md5' -printf '/%P\t%i\t%s\t%A@\t%U\t%G\n'
// /home/cgille/test/CHA-CHA-RALSER-RAW.content.gz
struct cached_stat{
  time_t when_read;
  ino_t st_ino;
  off_t st_size;
  struct timespec ST_MTIMESPEC;
  mode_t st_mode;
  uid_t st_uid;
  gid_t st_gid;
};
static bool _stat_from_cache(const int opt_filldir_findrp,struct stat *stbuf, const char *rp, const root_t *r){
  int key_l[1]={0};
  ht_hash_t key_hash[1]={0};
  const int rp_l=strlen(rp);
  const char *key=key_from_rp(false,rp,rp_l,key_l,key_hash,(root_t*)r);
  if (!*key_l) return false;
  const bool debug=WITH_EXTRA_ASSERT && ENDSWITH(key,*key_l, ".d.Zip");
  if (debug){
    //log_entered_function("%s FINDRP_IS_WORM:%d",key,FINDRP_IS_WORM&opt_filldir_findrp);
    if (!(FINDRP_IS_WORM&opt_filldir_findrp)) cg_print_stacktrace(0);
  }

  const int opt_cache=STAT_CACHE_OPT_FOR_ROOT(opt_filldir_findrp,r);
  if (!config_file_attribute_valid_seconds(opt_cache,rp,rp_l,NULL)) return false;
  struct cached_stat st={0};
  LOCK_N(mutex_dircache,const struct cached_stat *c=ht_get(&r->stat_ht,key,*key_l,*key_hash);if (c) st=*c);
  //if (debug)log_debug_now("c: %p ino: %ld,  sze: %ld  rp:'%s' l: %d",c,st.st_ino,st.st_size,key,*key_l);
  if (st.st_ino){
#define C(f) stbuf->f=st.f
    C(st_ino);C(st_size);C(st_mode);C(st_uid);C(st_gid);C(ST_MTIMESPEC);
#undef C
    const time_t valid=config_file_attribute_valid_seconds(opt_cache, key,*key_l,stbuf);
    const bool ok=time(NULL)-st.when_read<=valid;
    if (debug){
      //log_debug_now("key:'%s' FINDRP_IS_PFXPLAIN:%d FINDRP_IS_WORM:%d age:%ld valid: %ld  %s",key, !!(opt_filldir_findrp&FINDRP_IS_PFXPLAIN),  !!(opt_filldir_findrp&FINDRP_IS_WORM),  time(NULL)-st.when_read, valid, success_or_fail(ok)  );
      //if ((opt_filldir_findrp&FINDRP_IS_WORM)) DIE_DEBUG_NOW("FINDRP_IS_WORM"); else cg_print_stacktrace(0);
    }
    if (ok){
      COUNTER1_INC(COUNT_STAT_FROM_CACHE);
      //if (debug)log_exited_function(GREEN_SUCCESS"%s",rp);
      return true;
    }else{
      stbuf->st_ino=0;
    }
  }
  //if (debug)log_exited_function(RED_FAIL"%s",rp);

  return false;
}




static bool stat_from_cache(const int opt_filldir_findrp,struct stat *st, const char *rp,  root_t *r){
  ASSERT(r);   ASSERT(rp); if (!rp || !r) return false;
  //log_entered_function("%s",rp);
  bool ok=_stat_from_cache(opt_filldir_findrp,st,rp,r);
  //if (rp && strstr(rp,"txt"))log_exited_function("%s %s",rp,success_or_fail(ok));
#if WITH_PRELOADDISK_DECOMPRESS
  if (!ok && r->decompress_flags){
    FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){
      if (!(r->decompress_flags&(1<<iCompress))) continue;
      char gz[strlen(rp)+4]; stpcpy(stpcpy(gz,rp),cg_compression_file_ext(iCompress,NULL));
      if ((ok=_stat_from_cache(opt_filldir_findrp,st,gz,r)) && st->st_size && (st->st_mode&S_IFDIR)){
        st->st_size=closest_with_identical_digits(st->st_size*100);
        st->st_ino=make_inode(st->st_ino,r,1,rp);
        //log_debug_now("stat_from_cache %s   %ld",path, st->st_ino);
      }
    }
  }
#endif //WITH_PRELOADDISK_DECOMPRESS
  return ok;
}

static void stat_to_cache(const int opt_filldir_findrp,const struct stat *stbuf, const char *rp,  const int rp_l, root_t *r, time_t time_or_null){
  const bool debug=WITH_EXTRA_ASSERT && ENDSWITH(rp,rp_l, ".d.Zip");

  if (!(opt_filldir_findrp&FINDRP_STAT_TOCACHE_ALWAYS)){
    const int opt_cache=STAT_CACHE_OPT_FOR_ROOT(opt_filldir_findrp,r);
    const time_t valid=config_file_attribute_valid_seconds(opt_cache,rp,rp_l,stbuf);
    if (debug){
      //log_debug_now("rp:%s valid:%ld  FINDRP_IS_WORM:%s valid:%s",rp,valid,yes_no(FINDRP_IS_WORM&opt_filldir_findrp),success_or_fail(valid!=0));
    }
    if (!valid) return;
  }
  //static int stack; if (!stack++) cg_print_stacktrace(0);
  int key_l[1]={0};
  ht_hash_t key_hash[1]={0};
  {
    lock(mutex_dircache);
    const char *key=key_from_rp(true,rp,strlen(rp),key_l,key_hash,r);
    ht_entry_t *e=ht_get_entry(&r->stat_ht,key,*key_l,*key_hash,true);

    //if (debug)log_debug_now(ANSI_RED"Going to set ht key:'%s' FINDRP_STAT_TOCACHE_ALWAYS:%d"ANSI_RESET,key,!!(opt_filldir_findrp&FINDRP_STAT_TOCACHE_ALWAYS));
    if (!e->value)  e->value=mstore_malloc(&_root->dircache_mstore,sizeof(struct cached_stat),8);
    struct cached_stat *st=e->value;
#define C(f) st->f=stbuf->f
    C(st_ino);C(st_size);C(st_mode);C(st_uid);C(st_gid);    C(ST_MTIMESPEC);
#undef C
    st->when_read=time_or_null?time_or_null:time(NULL);
    unlock(mutex_dircache);
  }
}

/***********************************************************************************/
/* async_stat() for hundreds of  directory entries is inefficient.                 */
/* Therefore we call stat directly after reading the directory in the same thread. */
/* The struct stat is stored in the cache.                                         */
/***********************************************************************************/
static int dir_entry_to_stat_cache(const char *rp, const int rp_l, root_t *r){
  /* async_stat() is inefficient. Therefore we read the stat information now and store it in the cache. */
  int isdir=0;
  struct stat st;
  if (stat_direct(FINDRP_STAT_TOCACHE_ALWAYS, &st,rp,rp_l,r)){
    isdir=S_ISDIR(st.st_mode)?1:-1;
    root_update_time(r,PTHREAD_ASYNC);
    //log_debug_now(GREEN_SUCCESS"stat_direct %s   test:%d",rp, stat_from_cache(&st,rp,0));
  }
  return isdir;
}
static void dir_entries_to_stat_cache(directory_t *dir,root_t *r){
  char rp[MAX_PATHLEN+1], *rp1=stpcpy(rp,DIR_RP(dir)); *rp1++='/';
  RLOOP(i,dir->core.files_l)    dir_entry_to_stat_cache(rp,stpcpy(rp1,dir->core.fname[i])-rp,r);
}
