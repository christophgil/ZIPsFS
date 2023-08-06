#define FILE_DEBUG_KILL       "/ZIPsFS_KILL_878546a9e4d4b2f375a1f72b8c913a01"
#define FILE_DEBUG_CLEARCACHE "/ZIPsFS_CLEARCACHE"
static void debug_triggerd_by_magic_files(const char *path){
  //log_debug_now("ddddddddddddd debug_triggerd_by_magic_files %s \n",path);
  if (!strcmp(path,FILE_DEBUG_KILL)){
    warning(WARN_MISC,path,"Triggered exit of ZIPsFS");
    while(_fhdata_n){
      log_msg("Waiting for _fhdata_n  %d to be zero\n",_fhdata_n);
      usleep(300);
    }
    foreach_root(i,r){
      mstore_clear(&r->storedir);
      ht_destroy(&r->storedir_ht);
      }
                       //                       /local/filesystem/fuse-3.14.0/lib/helper.c fuse_main
    //          fuse_session_exit();
    //fuse_session_exit(fuse_get_session(m_filesystem));
    exit(0);
  }
  if (!strcmp(path,FILE_DEBUG_CLEARCACHE)){
    warning(WARN_MISC,FILE_DEBUG_CLEARCACHE,"");
    roots_clear_cache();
  }
}
static bool debug_fhdata(const struct fhdata *d){ return d && !d->n_read && tdf_or_tdf_bin(d->path);}
