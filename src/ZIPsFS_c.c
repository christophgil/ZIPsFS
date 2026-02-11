/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
_Static_assert(WITH_CCODE,"");
#define FSIZE_FROM_HASHTABLE(st,vp,vp_l,default)  if (0>=(st->st_size=fsize_from_hashtable((st->st_ino=inode_from_virtualpath(vp,vp_l))))) st->st_size=default
#define FSIZE_TO_HASHTABLE(bb,vp,vp_l)  fsize_to_hashtable(vp,vp_l, bb[0]?textbuffer_length(bb[0]):0)
#define ZIPSFS_C_IS_DIR_A (1<<0)
#define ZIPSFS_C_MMAP     (1<<1)
typedef textbuffer_t** c_read_handle_t;






#define X(...)  c_from_exec_output(__VA_ARGS__)
#define H(b,txt,size) textbuffer_add_segment(TXTBUFSGMT_NO_COUNT,                    _zipsfs_c_init_tb(b),txt,size);
#define M(b,txt,size) textbuffer_add_segment(TXTBUFSGMT_NO_COUNT|TXTBUFSGMT_MUNMAP,  _zipsfs_c_init_tb(b),txt,size);
#define C(b,txt,size) textbuffer_add_segment(TXTBUFSGMT_NO_FREE,                     _zipsfs_c_init_tb(b),txt,size);
#include "ZIPsFS_configuration_c.c"
#undef H
#undef M
#undef C
#undef X

#define C_FLAGS_FROM_ZPATH()   (ZPATH_IS_FILECONVERSION()?ZIPSFS_C_IS_DIR_A:0)
static int c_from_exec_output(textbuffer_t **bb,const uint8_t flags,const char *cmd[],const char *env[]){
  return textbuffer_from_exec_output((flags&ZIPSFS_C_MMAP)?TXTBUFSGMT_MUNMAP:0,
                                     _zipsfs_c_init_tb(bb),
                                     cmd,
                                     env,NULL);
}



static textbuffer_t *_zipsfs_c_init_tb(textbuffer_t **bb){
  if (!bb[0]){
    cg_thread_assert_not_locked(mutex_fhandle);
    bb[0]=textbuffer_new(COUNT_MALLOC_PRELOADRAM_TXTBUF);
  }
  return bb[0];
}
//static fHandle_t *c_fuse_open(const zpath_t *zpath, uint64_t *fh){  return  config_c_open(f(),VP(),VP_L())?fhandle_create(FHANDLE_IS_CCODE,fh,zpath):NULL;}


static bool c_getattr(struct stat *st, const virtualpath_t *vipa){
  stat_init(st,0,NULL);
  st->st_mtime=time(NULL);
  if (!config_c_getattr((vipa->dir==DIR_FILECONVERSION)?ZIPSFS_C_IS_DIR_A:0,vipa->vp,vipa->vp_l,st)){
    return false;
  }
  if (st->st_mode&S_IFDIR) stat_set_dir(st);
  if (!st->st_ino) st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
  return true;
}

static bool c_file_content_to_fhandle(fHandle_t *d){
  if (!(d->flags&FHANDLE_IS_CCODE)) return false;
  const zpath_t *zpath=&d->zpath;
  textbuffer_t *bb[1]={0};
  if (!config_c_read(bb,C_FLAGS_FROM_ZPATH(),VP(), VP_L())) return false;
  lock(mutex_fhandle);
  if (!fhandle_set_text(d,bb[0])){
    FREE_NULL_MALLOC_ID(bb[0]);
  }else{
    IF1(WITH_PRELOADRAM,d->preloadram->txtbuf=bb[0];preloadram_set_status(d,preloadram_done));
    d->flags|=FHANDLE_PRELOADRAM_COMPLETE;
  }
  unlock(mutex_fhandle);
  return true;
}
static bool c_readdir(const zpath_t *zpath,void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  char fname[MAX_PATHLEN+1];
  bool isDirectory[1], ok=false;
  FOR(i,0,ZIPSFS_C_MAX_NUM){
    *fname=0;
    if (!config_c_readdir(C_FLAGS_FROM_ZPATH(),VP(),VP_L(),i,fname,MAX_PATHLEN,isDirectory)) break;
    if (*fname){
      ok=true;
      struct stat st={0};
      stat_init(&st,*isDirectory?-1:0,NULL);
      st.st_ino=inode_from_virtualpath(VP(),VP_L());
      filler_add(0,filler,buf, fname,0, &st,NULL /*no_dups*/);
    }
  }
  return ok;
}

#undef f
