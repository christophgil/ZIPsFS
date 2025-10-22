/////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                   ///
/// Dynamically generated files                           ///
/// This file can be customized by the user               ///
/////////////////////////////////////////////////////////////
#define FSIZE_FROM_HASHTABLE(st,vp,vp_l,default)  if (0>=(st->st_size=fsize_from_hashtable((st->st_ino=inode_from_virtualpath(vp,vp_l))))) st->st_size=default
#define FSIZE_TO_HASHTABLE(bb,vp,vp_l)  fsize_to_hashtable(vp,vp_l, bb[0]?textbuffer_length(bb[0]):0)
#define ZIPSFS_C_IS_DIR_A (1<<0)
#define ZIPSFS_C_MMAP (1<<1)
typedef struct textbuffer** c_read_handle_t;






#define X(...)  c_from_exec_output(__VA_ARGS__)
#define H(b,txt,size) textbuffer_add_segment(0,                  _zipsfs_c_init_tb(b),txt,size);
#define M(b,txt,size) textbuffer_add_segment(TXTBUFSGMT_MUNMAP,  _zipsfs_c_init_tb(b),txt,size);
#define C(b,txt,size) textbuffer_add_segment(TXTBUFSGMT_NO_FREE, _zipsfs_c_init_tb(b),txt,size);
#include "ZIPsFS_configuration_c.c"
#undef H
#undef M
#undef C
#undef X

#define a(vp,vp_l)   const bool inDirA=IS_IN_DIR_AUTOGEN(vp,vp_l)
#define o()   (inDirA?DIR_AUTOGEN_L:0)
#define f()   inDirA?ZIPSFS_C_IS_DIR_A:0


static int c_from_exec_output(struct textbuffer **bb,const uint8_t flags,char *cmd[],char *env[]){

    return textbuffer_from_exec_output((flags&ZIPSFS_C_MMAP)?TXTBUFSGMT_MUNMAP:0,
                                       _zipsfs_c_init_tb(bb),
                                       (const char*const*)cmd,(const char*const*)env,NULL);
}

static struct textbuffer *_zipsfs_c_init_tb(struct textbuffer **bb){
  if (!bb[0]){
    cg_thread_assert_not_locked(mutex_fhandle);
    bb[0]=textbuffer_new(COUNT_MALLOC_MEMCACHE_TXTBUF);
  }
  return bb[0];
}


static  int64_t c_fuse_open(const struct zippath *zpath){
  a(VP(),VP_L());
  if (!config_c_open(f(),VP()+o(),VP_L()-o())) return 0;
  int64_t fh;
  LOCK(mutex_fhandle,fhandle_create(FHANDLE_FLAG_IS_CCODE,(fh=next_fh()),zpath));
  return fh;
}
static bool c_getattr(struct stat *st, const char *vp,const int vp_l){
  a(vp,vp_l);
  stat_init(st,0,NULL);
  st->st_mtime=time(NULL);
  if (!config_c_getattr(f(),vp+o(),vp_l-o(),st)){
    return false;
  }
  if (st->st_mode&S_IFDIR) stat_set_dir(st);
  if (!st->st_ino) st->st_ino=inode_from_virtualpath(vp+o(),vp_l-o());
  return true;
}

static void c_file_content_to_fhandle(struct fHandle *d){
  if (!(d->flags&FHANDLE_FLAG_IS_CCODE)) return;
  //    if (d->memcache && d->memcache->txtbuf){ d->flags|=FHANDLE_FLAG_NOT_CCODE; return;}
  const char *vp=D_VP(d);
  const int vp_l=D_VP_L(d);
  a(vp,vp_l);
  struct textbuffer *bb[1]={0};
  if (config_c_read(bb,f(),vp+o(), vp_l-o())){
    assert(bb[0]);
    lock(mutex_fhandle);
    if (!fhandle_set_text(d,bb[0])){
      FREE_NULL_MALLOC_ID(bb[0]);
    }else{
      d->memcache->txtbuf=bb[0];
      memcache_set_status(d,memcache_done);
      d->flags|=FHANDLE_FLAG_MEMCACHE_COMPLETE;
    }
    unlock(mutex_fhandle);
  }
}
static void c_readdir(const struct zippath *zpath,void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  a(VP(),VP_L());
  //log_entered_function("%s inDirA: %d",VP(),inDirA);
  char fname[MAX_PATHLEN+1];
  bool isDirectory[1];
  FOR(i,0,ZIPSFS_C_MAX_NUM){
    *fname=0;
    if (!config_c_readdir(f(),VP()+o(),VP_L()-o(),i,fname,MAX_PATHLEN,isDirectory)) break;
    if (*fname){
      struct stat st={0};
      stat_init(&st,*isDirectory?-1:0,NULL);
      st.st_ino=inode_from_virtualpath(VP()+o(),VP_L()-o());
      filler_add(0,filler,buf, fname,0, &st,NULL /*no_dups*/);
    }

  }
}





#undef f
#undef a
#undef o
