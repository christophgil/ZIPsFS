#define FILE_DEBUG_KILL       "/ZIPsFS_KILL_878546a9e4d4b2f375a1f72b8c913a01"
#define FILE_DEBUG_BLOCK       "/ZIPsFS_BLOCK_878546a9e4d4b2f375a1f72b8c913a01"
#define FILE_DEBUG_CLEARCACHE "/ZIPsFS_CLEARCACHE"
static void debug_triggerd_by_magic_files(const char *path){
  //log_debug_now("ddddddddddddd debug_triggerd_by_magic_files %s \n",path);
  {
    const int t=
      (!strcmp(path,"/ZIPsFS_BLOCK_PTHREAD_S"))?PTHREAD_STATQUEUE:
      (!strcmp(path,"/ZIPsFS_BLOCK_PTHREAD_M"))?PTHREAD_MEMCACHE:
      (!strcmp(path,"/ZIPsFS_BLOCK_PTHREAD_D"))?PTHREAD_DIRCACHE:
      -1;
    if (t>=0){
      warning(WARN_DEBUG,path,"Triggered blocking of threads %s",PTHREAD_S[t]);
      foreach_root(i,r) r->debug_pretend_blocked[t]=true;
    }
  }
  {
    const int t=
      (!strcmp(path,"/ZIPsFS_CANCEL_PTHREAD_S"))?PTHREAD_STATQUEUE:
      (!strcmp(path,"/ZIPsFS_CANCEL_PTHREAD_M"))?PTHREAD_MEMCACHE:
      (!strcmp(path,"/ZIPsFS_CANCEL_PTHREAD_D"))?PTHREAD_DIRCACHE:
      -1;
    if(t>=0){
      warning(WARN_DEBUG,path,"Triggered canceling of threads %s",PTHREAD_S[t]);
      foreach_root(i,r) pthread_cancel(r->pthread[t]);
    }
  }
  if (!strcmp(path,FILE_DEBUG_KILL)){
    warning(WARN_MISC,path,"Triggered exit of ZIPsFS");
    while(_fhdata_n){
      log_msg("Waiting for _fhdata_n  %d to be zero\n",_fhdata_n);
      usleep(300);
    }
    foreach_root(i,r){
      mstore_clear(&r->dircache);
      ht_destroy(&r->dircache_ht);
    }
    //                       /local/filesystem/fuse-3.14.0/lib/helper.c fuse_main
    //          fuse_session_exit();
    //fuse_session_exit(fuse_get_session(m_filesystem));
    exit(0);
  }
  if (!strcmp(path,FILE_DEBUG_CLEARCACHE)){
    warning(WARN_MISC,FILE_DEBUG_CLEARCACHE,"");
    dircache_clear_if_reached_limit_all();
  }
}
static bool debug_fhdata(const struct fhdata *d){ return d && !d->n_read && tdf_or_tdf_bin(d->path);}
