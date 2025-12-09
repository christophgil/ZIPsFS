/*
  ZIPQsFS   Copyright (C) 2023   christoph Gille
  This program can be distributed under the terms of the GNU GPLv2.
  It has been developed starting with  fuse-3.14: Filesystem in Userspace  passthrough.c
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>
  ZIPsFS_notes.org  log.c
*/
// (defun Copy_working() (interactive) (shell-command (concat  (file-name-directory (buffer-file-name) ) "Copy_working.sh")))
// (buffer-filepath)
//__asm__(".symver realpath,realpath@GLIBC_2.2.5");
#define VFILE_SFX_INFO "@SOURCE.TXT"
#define PATH_IS_FILE_INFO(vp,vp_l) ENDSWITH(vp,vp_l,VFILE_SFX_INFO)
#define HOMEPAGE "https://github.com/christophgil/ZIPsFS"
#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
#ifndef PATH_MAX // in OpenSolaris
#define PATH_MAX 1024
#endif
// ---
#include <pthread.h> /* Keep. Required by OpenBSD */
#include <sys/types.h>
#include <unistd.h> /// ???? lseek
#include <getopt.h>
// ---
#include <sys/mman.h>
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS MAP_ANON
#endif  // !MAP_ANONYMOUS
// ---
#include "config.h"
#include <dirent.h>
#include <fuse.h>
#include <zip.h>
#include <stdatomic.h>
#ifdef __USE_GNU
#include <gnu/libc-version.h>
#endif
// ---
#ifndef FUSE_MAJOR_VERSION
#define FUSE_MAJOR_VERSION 1
#define FUSE_MINOR_VERSION 0
#endif
// ---
#if FUSE_MAJOR_VERSION>2
#define WITH_FUSE_3 1
#define COMMA_FILL_DIR_PLUS ,0
#else
#define WITH_FUSE_3 0
#define COMMA_FILL_DIR_PLUS
#endif

#define HOOK_MSTORE_CLEAR(m)   {char mpath[PATH_MAX+1];mstore_file(mpath,m,-1);warning(WARN_DIRCACHE,mpath,"Clearing struct mstore %s ",m->name);}
// ---
////////////////////
/// Early Macros ///
////////////////////
#define VERSION_AT_LEAST(major,minor,MAJOR_LEAST,MINOR_LEAST)  (major>MAJOR_LEAST || (major==MAJOR_LEAST && minor>=MINOR_LEAST))
#define WITH_DEBUG_MALLOC 1
#define WITH_ZIPsFS_COUNTERS 1
#include "ZIPsFS_configuration.h"
#include "cg_utils.h"
#include "ZIPsFS_early.h"
#include "cg_log.h"
#include "ZIPsFS_version.h"

#include "cg_ht_v7.h"
#include "cg_textbuffer.h"
#include "cg_cpu_usage_pid.h"
#include "ZIPsFS.h"
// ---
#include "generated_ZIPsFS.inc"
//
#include "cg_profiler.h"
// ---
#include "cg_pthread.c"
#include "cg_debug.c"
#include "cg_stacktrace.c"
#include "cg_utils.c"
#include "cg_unicode.c"
#include "cg_ht_v7.c"
#include "cg_log.c"
#include "cg_cpu_usage_pid.c"
#include "cg_textbuffer.c"

static pid_t _pid;

//cppcheck-suppress-macro [identicalInnerCondition]
#define fhandle_busy_start(d) {if (d) d->is_busy++;}   IF1(IS_CHECKING_CODE,FILE *_fhandle_busy=fopen("abc","r"))
//cppcheck-suppress-macro [identicalInnerCondition]
#define fhandle_busy_end(d)   {if (d) d->is_busy--;}   IF1(IS_CHECKING_CODE, FCLOSE(_fhandle_busy))
//cppcheck-suppress-macro unreadVariable
#define NORMALIZE_EMPTY_PATH_L(p) if (*p=='/'&&!p[1]) p++; const int p##_l=cg_pathlen_ignore_trailing_slash(p)


#if WITH_AUTOGEN
#include "ZIPsFS_configuration_autogen.h"
#include "ZIPsFS_autogen_impl.h"
#endif //WITH_AUTOGEN
#if WITH_CCODE
#include "ZIPsFS_c.c"
#endif //WITH_CCODE
static char _self_exe[PATH_MAX+1];//,static char _initial_cwd[PATH_MAX+1];
static int _fhandle_n=0,_mnt_l=0;
static rlim_t _rlimit_vmemory=0;
IF1(WITH_PRELOADFILERAM,static enum enum_when_preloadfileram_zip _preloadfileram_policy=PRELOADFILERAM_RULE);
static char _mkSymlinkAfterStart[MAX_PATHLEN+1]={0},_mnt[PATH_MAX+1];
static struct ht *ht_set_id(const int id,struct ht *ht){
#if WITH_DEBUG_MALLOC
  if (!ht) return NULL;
  assert(id<(1<<MALLOC_TYPE_SHIFT));
  ht->id=MALLOC_TYPE_HT|id;
  if (ht->keystore)    _malloc_is_count_mstore[ht->keystore->id=MALLOC_TYPE_KEYSTORE|id]=true;
  if (ht->valuestore)  _malloc_is_count_mstore[ht->valuestore->id=MALLOC_TYPE_VALUESTORE|id]=true;
#endif //WITH_DEBUG_MALLOC
  return ht;
}
static int _lock_oldstate_unused;
static int zpath_strlen(const struct zippath *zpath,int s){
  return s==0?0:strlen(zpath->strgs+s);
}

static MAYBE_INLINE struct fHandle* fhandle_at_index(int i){
  ASSERT_LOCKED_FHANDLE();
  static struct fHandle *_fhandle[FHANDLE_BLOCKS];
#define B _fhandle[i>>FHANDLE_LOG2_BLOCK_SIZE]
  struct fHandle *block=B;
  if (!block){
    block=B=cg_calloc(COUNTm_FHANDLE_ARRAY_MALLOC,FHANDLE_BLOCK_SIZE,sizeof(struct fHandle));
    assert(block!=NULL);
  }
  return block+(i&(FHANDLE_BLOCK_SIZE-1));
#undef B
}
static struct mstore mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
static struct ht ht_intern_fileext,_ht_valid_chars, ht_inodes_vp,ht_count_by_ext
  IF1(WITH_AUTOGEN_OR_CCODE,,ht_fsize)
  IF1(WITH_STAT_CACHE,,stat_ht)
  IF1(WITH_ZIPINLINE_CACHE,,ht_zinline_cache_vpath_to_zippath);



static int _log_flags; /* The bits specifying what is logged.*/
/////////////////////////////////////////////////////////////////
/// Root paths - the source directories                       ///
/// The root directories are specified as program arguments   ///
/// The _root[0] may be read/write, and can be empty string   ///
/// The others are always  read-only                          ///
/////////////////////////////////////////////////////////////////

static int _root_n=0;
/* *** fHandle vars and defs *** */
static char _fWarningPath[2][MAX_PATHLEN+1];

static float _ucpu_usage,_scpu_usage;/* user and system */
static int64_t _preloadfileram_bytes_limit=3L*1000*1000*1000;
static int _unused_int;
static bool _thread_unblock_ignore_existing_pid, _fuse_started, _isBackground, _exists_root_with_preload;

static const char *_fuse_argv[99]={0},
  *_root_writable_path="<path-first-root>",
  *_cleanup_script="<path-first-root>"FILE_CLEANUP_SCRIPT;

static int _fuse_argc;

static struct rootdata _root[ROOTS]={0}, *_root_writable=NULL;




#include "ZIPsFS_internet.c"


static const char* rootpath(const struct rootdata *r){
  return r?r->rootpath:NULL;
}
#define  isRootRemote(r) (r && r->remote)
static const char* report_rootpath(const struct rootdata *r){
  return !r?"No root" : r->rootpath;
}
static int rootindex(const struct rootdata *r){
  return !r?-1: (int)(r-_root);
}
static int _rootdata_path_max;
static long _rootmask_preload;
static void root_init(const bool isWritable,struct rootdata *r, const char *path){
  if (isWritable){
    (_root_writable=r)->writable=true;
  }else{
    if (!*path) DIE("Command line argument for rootpath is empty.");
  }
  {
    const int s=cg_leading_slashes(path)-1;

    if ((r->remote=(s>0))) path+=s;
    r->with_timeout=(s>1);
  }
  {
    const int s=cg_last_slash(path);
    if (s>0 && path[s+1]) r->retain_dirname_l=strlen(strcpy(r->retain_dirname,path+s));
  }
  r->rootpath_l=cg_strlen(realpath(path,r->rootpath));
  _rootdata_path_max=MAX_int(_rootdata_path_max,(r->rootpath_l+r->retain_dirname_l+2));
  if (!r->rootpath_l && r==_root_writable) return;
  if (!r->rootpath_l || !cg_is_dir(r->rootpath))    DIE("Not a directory: '%s'  realpath: '%s'",path,r->rootpath);
  {
    struct statvfs st;
    if (statvfs(rootpath(r),&st)){
      perror(rootpath(r));
      exit_ZIPsFS();
    }
    r->f_fsid=st.f_fsid;
  }
  {
    int fsid=-1;
    foreach_root(s) if (s<r && r->f_fsid==s->f_fsid) fsid=s->seq_fsid;
    static int seq_fsid;
    r->seq_fsid=fsid>=0?fsid:seq_fsid++;
    if (r->seq_fsid>=(1<<LOG2_FILESYSTEMS)){
      log_error("Current number of different file systems %d Exceeds  maximum of 2^" STRINGIZE(LOG2_FILESYSTEMS)"+1. Please increase macro definition of LOG2_FILESYSTEMS.", r->seq_fsid);
      exit_ZIPsFS();
    }
  }
  RLOOP(i,ASYNC_LENGTH) pthread_mutex_init(r->async_mtx+i,NULL);
  {
    fprintf(stderr,"'%s': Calling  statvfs ... ",r->rootpath);
    if (statvfs(rootpath(r),&r->statvfs)){ perror(""); DIE("statvfs");}
    FOR(iTry,0,2){
      if (exec_on_file(EXECF_SILENT,iTry?EXECF_MOUNTPOINT_USING_DF:EXECF_MOUNTPOINT_USING_FINDMNT, r->rootpath_mountpoint, MAX_PATHLEN,r->rootpath)>0) break;
    }
    fputs(GREEN_DONE"\n",stderr);
  }
  if (r->preload){
    if (!_root_writable) warning(WARN_CONFIG,r->rootpath,"Set --preload to root, but no writable root.");
    if (_root_writable==r) DIE("Do not set --preload for  first root.");
    _rootmask_preload|=(1<<rootindex(r));
  }
}
#define my_zip_fread(...) _my_zip_fread(__VA_ARGS__,__func__)
// ---
#include "ZIPsFS_configuration.c"
#include "ZIPsFS_debug.c"
#if WITH_STAT_CACHE
#include "ZIPsFS_cache_stat.c"
#endif //WITH_STAT_CACHE
#include "ZIPsFS_cache.c"
#if WITH_TRANSIENT_ZIPENTRY_CACHES
#include "ZIPsFS_transient_zipentry_cache.c"
#endif
// ---
#if WITH_PRELOADFILEDISK
#include "ZIPsFS_preloadfiledisk.c"
#endif //WITH_PRELOADFILEDISK

// ---
#if WITH_PRELOADFILERAM
#include "ZIPsFS_filesystem_info.c"
#endif //WITH_PRELOADFILERAM
//---
#include "ZIPsFS_log.c"
#include "ZIPsFS_async.c"

// ---
#if WITH_PRELOADFILERAM
#include "ZIPsFS_preloadfileram.c"
#include "ZIPsFS_ctrl.c"
#include "ZIPsFS_special_file.c"
#endif // WITH_PRELOADFILERAM
// ---
#if WITH_ZIPINLINE
#include "ZIPsFS_zip_inline.c"
#endif //WITH_ZIPINLINE
// ---
#if WITH_ZIPENTRY_PLACEHOLDER
#include "ZIPsFS_zipentry_placeholder.c"
#endif
// ---
#if WITH_AUTOGEN
#include "ZIPsFS_autogen.c"
#define autogen_filecontent_append_nodestroy(ff,s,s_l)  _autogen_filecontent_append(TXTBUFSGMT_NO_FREE,ff,s,s_l)
#define autogen_filecontent_append(ff,s,s_l)          _autogen_filecontent_append(0,ff,s,s_l)
#define autogen_filecontent_append_munmap(ff,s,s_l)   _autogen_filecontent_append(TXTBUFSGMT_MUNMAP,ff,s,s_l)
#define C(ff,s,s_l)  _autogen_filecontent_append(TXTBUFSGMT_NO_FREE,ff,s,s_l)
#define H(ff,s,s_l)  _autogen_filecontent_append(0,ff,s,s_l)
#define M(ff,s,s_l)  _autogen_filecontent_append(TXTBUFSGMT_MUNMAP,ff,s,s_l)
#include "ZIPsFS_configuration_autogen.c"
#undef M
#undef C
#undef H


#include "ZIPsFS_autogen_impl.c"
#endif //WITH_AUTOGEN
#include "ZIPsFS_configuration_check.c"
// ---
#ifndef WITH_PROFILER
#define WITH_PROFILER 0
#endif
// ---
#if WITH_PROFILER
#include "generated_profiler.c"
#endif // WITH_PROFILER
// ---
////////////////////////////////////////
/// lock,pthread, synchronization   ///
////////////////////////////////////////
static pthread_mutex_t _mutex[NUM_MUTEX];
static void init_mutex(void){
  static pthread_mutexattr_t _mutex_attr_recursive;
  pthread_mutexattr_init(&_mutex_attr_recursive);
  pthread_mutexattr_settype(&_mutex_attr_recursive,PTHREAD_MUTEX_RECURSIVE);
  RLOOP(i,mutex_roots+_root_n)     pthread_mutex_init(_mutex+i,&_mutex_attr_recursive);
}




//////////////////////
// struct directory //
//////////////////////
static struct zippath *directory_init_zpath(struct directory *dir,const struct zippath *zpath){
  /* Note: Must not allocate on heap */
#define C(field,type) dir->core.field=dir->_stack_##field
  if (dir->files_capacity<DIRECTORY_DIM_STACK){
    dir->files_capacity=DIRECTORY_DIM_STACK;
    C_FILE_DATA();
  }
#undef C
  dir->core.files_l=0;
  if (zpath) dir->dir_zpath=*zpath; /* When called from async_periodically_dircache(), zpath is NULL  */
  STRUCT_NOT_ASSIGNABLE_INIT(dir);
  IF1(WITH_TIMEOUT_READDIR, if (!dir->ht_intern_names)){
    dir->files_capacity=DIRECTORY_DIM_STACK;
    mstore_init(&dir->filenames,"",4096|MSTORE_OPT_MALLOC);
    dir->filenames.mstore_counter_mmap=COUNT_MSTORE_MMAP_DIR_FILENAMES;
  }
  return &dir->dir_zpath;
}
static void directory_destroy(struct directory *d){
  if (d && !d->dir_is_dircache && !d->dir_is_destroyed){
    d->dir_is_destroyed=true;
#define C(field,type) if(d->core.field!=d->_stack_##field) cg_free_null(COUNT_MALLOC_dir_field,d->core.field);
    C_FILE_DATA();
#undef C
    mstore_destroy(&d->filenames);
  }
}

/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////

static void directory_ensure_capacity(struct directory *d, const int min, const int newCapacity){
  assert(DIR_RP(d));
  ASSERT_NOT_ASSIGNED(d);
  struct directory_core *dc=&d->core;
  //log_entered_function("directory_set_capacity: %d  files_l: %d fname: %p",d->files_capacity,L,&dc->fname);
  if (min>d->files_capacity || dc->fname==NULL){
    assert(min>DIRECTORY_DIM_STACK);
    // _cg_realloc_array(const int id,const int size1AndOpt,const void *pOld, const size_t nOld, const size_t nNew)
    d->files_capacity=newCapacity;
    assert(newCapacity>=min);
#define O(f) (dc->f==d->_stack_##f?REALLOC_ARRAY_NO_FREE:0)
#define C(f,type)  if (dc->f) dc->f=cg_realloc_array(COUNT_MALLOC_dir_field,sizeof(type)|O(f),dc->f,dc->files_l,newCapacity)
    C_FILE_DATA();
#undef O
#undef C
  }
  //log_exited_function("capacity: %d fname: %p",dir->files_capacity,&dc->fname);
}

static void directory_add(uint8_t flags,struct directory *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
#define L dc->files_l
  cg_thread_assert_locked(mutex_dircache); ASSERT(n0!=NULL); ASSERT(dir!=NULL);
  if (cg_empty_dot_dotdot(n0)) return;
  struct directory_core *dc=&dir->core;
  directory_ensure_capacity(dir,L+1,2*L+2);
  assert(dc->fname!=NULL);
  IF0(WITH_ZIPENTRY_PLACEHOLDER,const char *s=n0);
  IF1(WITH_ZIPENTRY_PLACEHOLDER, static char buf_for_s[MAX_PATHLEN+1];const char *s=(flags&DIRENT_DIRECT_NAME)?n0:zipentry_placeholder_insert(buf_for_s,n0,dir));
  const int s_l=cg_pathlen_ignore_trailing_slash(s);
  dc->fflags[L]=(flags&DIRENT_SAVE_MASK)|(s[s_l]=='/'?DIRENT_ISDIR:0);
  assert(dir->files_capacity>L);
#define C(name) if (dc->f##name) dc->f##name[L]=name
  C(mtime);  C(size);  C(crc);  C(inode);
#undef C
  if (crc) ASSERT(NULL!=dc->fcrc);
  ASSERT(NULL!=dc->fname);
  if (!(flags&DIRENT_DIRECT_NAME)){
#if WITH_TIMEOUT_READDIR
    if (dir->ht_intern_names){
      LOCK(mutex_dircache, s=ht_sinternalize(dir->ht_intern_names,s));
    }else
#endif //WITH_TIMEOUT_READDIR
      s=mstore_addstr(&dir->filenames,s,s_l);
  }
  //log_debug_now("s=%s  direct: %s  ht: %p",s,yes_no(flags&DIRENT_DIRECT_NAME),dir->ht_intern_names);
  dc->fname[L++]=(char*)s;
#undef L
}




////////////
/// stat ///
////////////
static void stat_set_dir(struct stat *s, const char *n){
  if (s){
#define m (s->st_mode)
    ASSERT(S_IROTH>>2==S_IXOTH);
    m&=~S_IFREG;
    s->st_size=ST_BLKSIZE;
    s->st_nlink=1;
    m=(m&~S_IFMT)|S_IFDIR|((m&(S_IRUSR|S_IRGRP|S_IROTH))>>2); /* Can read - can also execute directory */
  }
#undef m
}

static bool _stat_direct(struct stat *stbuf,const struct strg *path, const struct rootdata *r, const char *callerFunc){
  //static int count;log_entered_function(ANSI_MAGENTA"%s #%d %s"ANSI_RESET,callerFunc,count++,path->s);
  cg_thread_assert_not_locked(mutex_fhandle);
  const char *rp=path->s;
  if (!rp) return false;
  //  if (!*rp) cg_print_stacktrace(0);
  assert(*rp);
  const int res=lstat(rp,stbuf);
  inc_count_by_ext(rp,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS);
  if (res){
    *stbuf=empty_stat;
    return false;
  }
  ASSERT(stbuf->st_ino!=0);
  assert(path->s);
  IF1(WITH_STAT_CACHE, if (r && config_file_attribute_valid_seconds(STAT_CACHE_OPT_FOR_ROOT(r), path->s,path->l)) stat_to_cache(stbuf,path));
  return true;
}/*stat_direct*/

static bool stat_from_cache_or_direct_or_async(const char *rp, struct stat *stbuf,struct rootdata *r){
  cg_thread_assert_not_locked(mutex_fhandle);
  struct strg path={0};
  strg_init(&path,rp);
  if (r){
    IF1(WITH_STAT_CACHE, if (stat_from_cache(stbuf,&path,r)) return true);
    if (r->remote) return async_stat(&path,stbuf,r);
  }
  return stat_direct(stbuf,&path,r);
} /* Used to be stat_from_cache_or_syscall_or_async */


/////////////////////////////////////////////////////////////////////////////////////
/// Using the option -s, ZIPsFS can be restarted while it is in production        ///
/// The new instance  will use a different mount point.                           ///
/// Software is accessing ZIPsFS via Symlink and not directly the mountpoint      ///
/////////////////////////////////////////////////////////////////////////////////////
static void mkSymlinkAfterStartPrepare(){
  if (*_mkSymlinkAfterStart){
    _mkSymlinkAfterStart[cg_pathlen_ignore_trailing_slash(_mkSymlinkAfterStart)]=0;
    log_verbose("Command option -s:   Going to create symlink '%s' --> '%s' ...",_mkSymlinkAfterStart,_mnt);
    if (*_mkSymlinkAfterStart=='/'){
      fprintf(stderr,RED_WARNING": "ANSI_FG_BLUE"%s"ANSI_RESET" is an absolute path. You might be unable to export the file tree with NFS and Samba.\n",_mkSymlinkAfterStart);
      if (!_isBackground){ fputs("Press Enter to continue anyway!\n",stderr); cg_getc_tty();}
    }
    const int err=cg_symlink_overwrite_atomically(_mnt,_mkSymlinkAfterStart);
    char rp[PATH_MAX+1];
    if (err || !realpath(_mkSymlinkAfterStart,rp)){
      char cwd[MAX_PATHLEN+1];
      warning(WARN_MISC,_mkSymlinkAfterStart,"Working-dir: %s  cg_symlink_overwrite_atomically(%s,%s); %s",getcwd(cwd,MAX_PATHLEN),_mnt,_mkSymlinkAfterStart,strerror(err));
      EXIT(1);
    }else if(!cg_is_symlink(_mkSymlinkAfterStart)){
      warning(WARN_MISC,_mkSymlinkAfterStart," not a symlink");
      EXIT(1);
    }else{
      //log_msg(GREEN_SUCCESS"Created symlink %s -->%s\n",_mkSymlinkAfterStart,rp);
    }
    *_mkSymlinkAfterStart=0;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
/// The struct zippath is used to identify the real path from the virtual path               ///
/// All strings like virtualpath are stacked in strgs                                        ///
/// strgs is initially on stack and may later go to heap.                                    ///
/// A new one is created with zpath_newstr() and one or several calls to zpath_strncat()     ///
/// The length of the created string is finally obtained with zpath_commit()                 ///
////////////////////////////////////////////////////////////////////////////////////////////////
static const char *internalize_realpath(const char *rp,const int rp_l, ht_hash_t hash){
  if (rp==NULL) return NULL;
  if (!*rp) return "";
  cg_thread_assert_locked(mutex_dircache);
  if (!hash) hash=hash32(rp,rp_l);
  return ht_intern(&_root->dircache_ht_fname,rp,rp_l,hash,HT_MEMALIGN_FOR_STRG);
}
static int zpath_newstr(struct zippath *zpath){
  assert(zpath!=NULL);
  const int n=(zpath->current_string=++zpath->strgs_l);
  zpath->strgs[n]=0;
  return n;
}
static bool zpath_strncat(struct zippath *zpath,const char *s,int len){
  if (ZPF(ZP_OVERFLOW)) return false;
  const int l=MIN_int(cg_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      warning(WARN_LEN|WARN_FLAG_MAYBE_EXIT,"zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len);
      zpath->flags|=ZP_OVERFLOW;
      return false;
    }
    cg_strncpy0(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return true;
}
static int zpath_commit_hash(const struct zippath *zpath, ht_hash_t *hash){
  const int l=zpath->strgs_l-zpath->current_string;
  if (hash) *hash=hash32(zpath->strgs+zpath->current_string,l);
  return l;
}

static void _zpath_assert_strlen(const char *fn,const char *file,const int line,struct zippath *zpath){
  bool e=false;
#define C(a)  if (zpath_strlen(zpath,zpath->a)!=zpath->a##_l && (e=true)) log_error(#a" != "#a"_l   %d!=%d\n",zpath_strlen(zpath,zpath->a),zpath->a##_l);
  C(virtualpath);C(virtualpath_without_entry);C(entry_path);
#undef C
  if (e){
    log_zpath("Error ",zpath);
    log_warn("zpath_assert_strlen in %s at "ANSI_FG_BLUE"%s:%d\n"ANSI_RESET,fn,file,line);
    ASSERT(0);
  }
}

static void zpath_reset_keep_VP(struct zippath *zpath){
#define x_l_hash(x) x=x##_l=x##_hash
  zpath->entry_path=zpath->entry_path_l=x_l_hash(zpath->virtualpath_without_entry)=x_l_hash(zpath->realpath)=0;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_L()=EP_L()=zpath->zipcrc32=0; /* Reset. Later probe a different realpath */
  zpath->root=NULL;
  zpath->strgs_l=zpath->virtualpath+VP_L();
  zpath->flags=ZPF(ZP_STARTS_AUTOGEN);
}
static void zpath_init(struct zippath *zpath, const char *vp){
  assert(zpath!=NULL);
  assert(vp!=NULL);
  memset(zpath,0,sizeof(struct zippath));
  NORMALIZE_EMPTY_PATH_L(vp);
  zpath->virtualpath=zpath->strgs_l=1;  /* To distinguish from virtualpath==0  meaning not defined we use 1*/
  zpath->virtualpath_hash=hash_value_strg(vp);
  zpath->virtualpath_l=vp_l;
  const char *NOCSC=cg_strcasestr(vp,"$NOCSC$");/* Windows no-client-side-cache */
  int removed=0;
  if (NOCSC){
    zpath_strncat(zpath,vp,NOCSC-vp);
    zpath_strcat(zpath,NOCSC+(removed=7));
  }else{
    zpath_strcat(zpath,vp);
  }
  zpath->strgs[zpath->virtualpath+(VP_L()=vp_l-removed)]=0;
  zpath_reset_keep_VP(zpath);
  IF1(WITH_AUTOGEN,if (virtualpath_startswith_autogen(vp,vp_l)) zpath->flags|=ZP_STARTS_AUTOGEN);
}
static bool zpath_stat(struct zippath *zpath,struct rootdata *r){
  if (!zpath) return false;
#define S zpath->stat_rp
    if (!S.st_ino){
    if (r?!stat_from_cache_or_direct_or_async(RP(),&S,r):stat(RP(),&S)) return false;
    zpath->stat_vp=S;
    if (!(ZPF(ZP_ZIP))) zpath->stat_vp.st_ino=make_inode(S.st_ino,r,0,RP());
  }
#undef S
  return true;
}

#define warning_zip(path,e,txt){\
    const int ze=!e?0:zip_error_code_zip(e), se=!e?0:zip_error_code_system(e);\
    char s[1024];*s=0;  if (se) strerror_r(se,s,1023);\
    warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH,path,"%s    sys_err: %s %s zip_err: %s %s",txt,!e?"e is NULL":!se?"":error_symbol(se),s, !ze?"":error_symbol_zip(ze), !ze?"":zip_error_strerror(e));}





///////////////////////////////////////////
/// Store file size for generated files ///
///////////////////////////////////////////
#if WITH_AUTOGEN_OR_CCODE
static off_t fsize_from_hashtable(ino_t inode){
  lock(mutex_fhandle);
  const struct ht_entry *e=htentry_fsize_for_inode(inode,false);
  const off_t size=e?(off_t)e->value:-1;
  unlock(mutex_fhandle);
  return size;
}
static void fsize_to_hashtable(const char *vp, const int vp_l, const off_t size){
  lock(mutex_fhandle);
  htentry_fsize(vp,vp_l,true)->value=(void*)size;
  unlock(mutex_fhandle);
}
#endif //WITH_AUTOGEN_OR_CCODE
//statvfs
///////////////////////////////////
/// Is the virtualpath a zip entry?
///////////////////////////////////
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *shorten, char *append[]){
  IF1(WITH_AUTOGEN,if (virtualpath_startswith_autogen(vp,vp_l)) return 0);
  const char *b=(char*)vp;
  int ret=0;
  for(int i=4;i<=vp_l;i++){
    const char *e=(char*)vp+i;
    if (i==vp_l || *e=='/'){
      if ((ret=config_virtual_dirpath_to_zipfile(b,e,append))!=INT_MAX){
        if (shorten) *shorten=-ret;
        ret+=i;
        break;
      }
      if (*e=='/') b=e+1;
    }
  }
  //log_exited_function("vp: %s  %d ",vp,ret==INT_MAX?0:ret);
  return ret==INT_MAX?0:ret;
}
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add(struct directory,...)
// Returns true on success.
////////////////////////////////////////////////////////////
//static bool readdir_from_cache_zip_or_filesystem(struct directory *dir){log_entered_function("%s",DIR_RP(dir)); bool ok=_readdir_from_cache_zip_or_filesystem(dir);log_exited_function("%s ok: %d",DIR_RP(dir),ok);  return ok;}

static bool readdir_from_cache_zip_or_filesystem(struct directory *dir){
  const char *rp=DIR_RP(dir);
  if (!rp || !*rp) return false;
#if WITH_DIRCACHE
  const bool doCache=config_advise_cache_directory_listing(rp,DIR_RP_L(dir), dir->dir_zpath.stat_rp.ST_MTIMESPEC,
                                                           ((DIR_ROOT(dir) && DIR_ROOT(dir)->remote)?ADVISE_DIRCACHE_IS_REMOTE:0)|
                                                           (DIR_IS_ZIP(dir)?ADVISE_DIRCACHE_IS_ZIP:0));
  if (doCache){
    bool success=false;
    LOCK_NCANCEL(mutex_dircache,success=dircache_directory_from_cache(dir));
    if (success) return true;
  }
#endif //WITH_DIRCACHE
  if (!readdir_async(dir)) return false;
  config_exclude_files(rp,DIR_RP_L(dir),dir->core.files_l, dir->core.fname,dir->core.fsize);
  IF1(WITH_DIRCACHE,if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(dir)));
  return true;
}
#define DIRECTORY_PREAMBLE(isZip)    if (DIR_IS_ZIP(dir)!=isZip) return false;   char *rp; LOCK(mutex_dircache, rp=DIR_RP(dir); dir->core.files_l=0)

#define CONTAINS_PALCEHOLDER(n,zip) IF1(WITH_ZIPENTRY_PLACEHOLDER,dir->has_file_containing_placeholder=dir->has_file_containing_placeholder || strchr(n,PLACEHOLDER_NAME))
static bool readdir_from_zip(struct directory *dir){
  DIRECTORY_PREAMBLE(true);
  zip_t *zip=my_zip_open(rp);
  root_update_time(DIR_ROOT(dir),zip?PTHREAD_ASYNC:-PTHREAD_ASYNC);
  if (!zip) return false;
  const int SB=256,N=zip_get_num_entries(zip,0);
  struct zip_stat s[SB]; /* reduce num of pthread lock */
  for(int k=0;k<N;){
    int i=0;
    for(;i<SB && k<N;k++) if (!zip_stat_index(zip,k,0,s+i)) i++;
    lock(mutex_dircache);
    root_update_time(DIR_ROOT(dir),i>0?PTHREAD_ASYNC:-PTHREAD_ASYNC);
#define S s[j]
    FOR(j,0,i){
      CONTAINS_PALCEHOLDER(S.name,dir);
      if (!config_do_not_list_file(rp,S.name,strlen(S.name))){
        directory_add(S.comp_method?DIRENT_IS_COMPRESSED:0,dir,0,S.name,S.size,S.mtime,S.crc);
      }
    }
#undef S
    unlock(mutex_dircache);
  }
  my_zip_close(zip,rp);
  return true;
}

static bool readir_from_filesystem(struct directory *dir){
  //   DIR *fdopendir(int fd);
  DIRECTORY_PREAMBLE(false);
  DIR *d=opendir(rp);      IF_LOG_FLAG(LOG_OPENDIR) log_verbose(" opendir('%s') ",rp);
  inc_count_by_ext(rp,d?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL);
  if (!d){ log_errno("opendir: %s",rp); return false; }
  root_update_time(DIR_ROOT(dir),PTHREAD_ASYNC);
  struct dirent *de;
  while((de=readdir(d))){
    root_update_time(DIR_ROOT(dir),PTHREAD_ASYNC);
    const char *n=de->d_name;
    CONTAINS_PALCEHOLDER(n,zip);
    if (config_do_not_list_file(rp,n,strlen(n))) continue;
#if defined(HAS_DIRENT_D_TYPE) && !HAS_DIRENT_D_TYPE /* OpenSolaris */
    char fpath[PATH_MAX+1]; snprintf(fpath,PATH_MAX,"%s/%s",rp,n);
    const bool isdir=cg_is_dir(fpath);
#else /* BSD, Linux, MacOSX */
    const bool isdir=(de->d_type==DT_DIR);
#endif
    LOCK(mutex_dircache, directory_add(isdir?DIRENT_ISDIR: 0,dir,de->d_ino,n,0,0,0));
  }/* While */
  closedir(d);
  return true;
}
/////////////////////////////////////////////////////////////////////
//
// The following functions are used to search for a real path
// for a given virtual path,
//
////////////////////////////////////////////////////////////
/* Returns true on success */
// DEBUG_NOW
static bool test_realpath(const bool fst_root_try_ZIPsFS_slash_preload, struct zippath *zpath, struct rootdata *r){
  assert(r!=NULL);
  const char *vp=VP();
  const int vp_l=VP_L(), vp0_l=VP0_L()?VP0_L():vp_l;
  char vp0[1+vp0_l]; cg_strncpy0(vp0,VP0_L()?VP0():vp,vp0_l);
  if (vp0_l && r->retain_dirname_l){ /* r->retain_dirname is beginning of virtual path */
    if ((vp0_l<r->retain_dirname_l) ||
        (vp0_l>r->retain_dirname_l && vp0[r->retain_dirname_l]!='/') ||
        strncmp(vp0,r->retain_dirname,r->retain_dirname_l)) return false;
  }
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  zpath->root=r;
  zpath_strcat(zpath,rootpath(r));
  int cut=0; IF1(WITH_PRELOADFILEDISK, cut=preloadfiledisk_zpath_prepare_realpath(fst_root_try_ZIPsFS_slash_preload, zpath,r); if (cut==INT_MAX) return false; assert(cut>=0));
  zpath_strcat(zpath,vp0+cut+r->retain_dirname_l);
  ZPATH_COMMIT_HASH(zpath,realpath);
  zpath->stat_rp=empty_stat;
  const int found=(!(ZPF(ZP_OVERFLOW)) && RP_L() && zpath_stat(zpath,r));
  //  if (strstr(r->rootpath,"fulnas")){
  /* if (cut){ */
  /*   log_debug_now(ANSI_FG_BLUE"r:%s vp0:%s cut:%d vp0+cut+retain:%s   rp:'%s' "ANSI_RESET,r->rootpath,vp0,cut, vp0+cut+r->retain_dirname_l,RP()); */
  /*   log_verbose("%s -> %s   %s",vp,RP(),found?GREEN_SUCCESS:RED_FAIL); */
  /*   log_debug_now("cg_file_exists(%s) %d  cg_file_size:%ld isDir:%d",RP(),cg_file_exists(RP()),cg_file_size(RP())   ,S_ISDIR(zpath->stat_rp.st_mode)); */
  /* } */

  if (!found) return false;
  if (ZPF(ZP_ZIP)){
    if (!cg_endsWithZip(RP(),0)){ IF_LOG_FLAG(LOG_REALPATH) log_verbose("!cg_endsWithZip rp: %s\n",RP()); return false; }
    if (EP_L() && filler_readdir_zip(0,zpath,NULL,NULL,NULL)) return false; /* This sets the file stat of zip entry  */
  }else{
    IF1(WITH_PRELOADFILEDISK,if (cut<0){if (S_ISREG(zpath->stat_vp.st_mode)) zpath->stat_vp.st_size=999;  });
  }
  //cg_log_file_stat(RP(),&zpath->stat_rp);  cg_log_file_stat(VP(),&zpath->stat_vp);
  return true;
}


static bool test_realpath_or_reset(struct zippath *zpath, struct rootdata *r){
  FOR(fst_root_try_ZIPsFS_slash_preload,0, ((r==_root_writable&&_exists_root_with_preload)?2:1)){
    if (test_realpath(fst_root_try_ZIPsFS_slash_preload,zpath,r)) return true;
    zpath_reset_keep_VP(zpath);

  }
  return false;
}

/* Uses different approaches and calls test_realpath */
static bool find_realpath_for_root(struct zippath *zpath,struct rootdata *r){
  const char *vp=VP(); /* At this point, Only zpath->virtualpath is defined. */
  const int vp_l=VP_L();
  char *append="";
  if (!wait_for_root_timeout(r)) return false;
  if (vp_l){
    int shorten=0;
    const int zip_l=virtual_dirpath_to_zipfile(vp,vp_l,&shorten,&append);
    if (zip_l){ /* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name.  */
      zpath->virtualpath_without_entry=zpath_newstr(zpath);
      zpath_strncat(zpath,vp,zip_l);
      zpath_strcat(zpath,append);
      ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
      zpath->entry_path=zpath_newstr(zpath);
      {
        const int pos=VP0_L()+shorten+1-cg_strlen(append);
        if (pos<vp_l) zpath_strcat(zpath,vp+pos);
      }
      if (ZPF(ZP_OVERFLOW)) return false;
      EP_L()=zpath_commit(zpath);
      zpath->flags|=ZP_ZIP;
      zpath_assert_strlen(zpath);
      if (test_realpath_or_reset(zpath,r)){
        if (!EP_L() && ZPF(ZP_ZIP)) stat_set_dir(&zpath->stat_vp,VP()); /* ZIP file without entry path */
        return true;
      }
    }
    IF1(WITH_ZIPINLINE, const yes_zero_no_t i=find_realpath_try_inline_rules(zpath,append,r); if (i) return i==YES);
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  bool ok=test_realpath_or_reset(zpath,r);
  return ok;
} /*find_realpath_nocache*/

static long search_file_which_roots(const char *vp,const int vp_l,const bool path_starts_with_autogen){
  if (path_starts_with_autogen){
#if WITH_AUTOGEN
    struct autogen_files ff={0};
    struct_autogen_files_init(&ff,vp,vp_l-(ENDSWITH(vp,vp_l,".log")?4:0));
    const bool ok=autogen_realinfiles(&ff);
    struct_autogen_files_destroy(&ff);
    if (ok) return 1;
#endif //WITH_AUTOGEN
  }
  return config_search_file_which_roots(vp,vp_l);
}
static bool find_realpath_roots_by_mask(struct zippath *zpath,const long roots){
  zpath_reset_keep_VP(zpath);
  IF1(WITH_ZIPINLINE_CACHE, yes_zero_no_t ok=zipinline_find_realpath_any_root(zpath, roots); if (ok) return ok==YES);
  foreach_root(r) if (roots&(1<<rootindex(r))) if (find_realpath_for_root(zpath,r)) return true;
  return false;
}/*find_realpath_roots_by_mask*/
static bool find_realpath_any_root(const int opt,struct zippath *zpath,const struct rootdata *onlyThisRoot){
  const char *vp=VP();
  const int vp_l=VP_L(); /* vp_l is 0 for the virtual root */
  //if (strstr(vp,"PRO3"))log_entered_function("%s",vp);
  const bool path_starts_autogen=false IF1(WITH_AUTOGEN, || ZPF(ZP_STARTS_AUTOGEN));
  IF1(WITH_SPECIAL_FILE,if (!onlyThisRoot && find_realpath_special_file(zpath)) return true);
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,const int trans=transient_cache_find_realpath(opt,zpath,vp,vp_l); if (trans) return trans==1);
  const int virtualpath=zpath->virtualpath;
  bool found=false; /* which_roots==01 means only Zero-th root */
  FOR(cut01,0,2){
    if (cut01 && !path_starts_autogen) continue;
    if (0!=(opt&(cut01?FINDRP_AUTOGEN_CUT_NOT:FINDRP_AUTOGEN_CUT))) continue;
    const long roots=(onlyThisRoot?(1<<rootindex(onlyThisRoot)):-1) & ((path_starts_autogen&&!cut01)?01:search_file_which_roots(vp,vp_l,path_starts_autogen));
    if (!roots && !cut01) continue;
    IF1(WITH_AUTOGEN, if(cut01){ zpath->virtualpath=virtualpath+DIR_AUTOGEN_L; zpath->virtualpath_l=vp_l-DIR_AUTOGEN_L;});
    if ((found=find_realpath_roots_by_mask(zpath, roots))) goto found;
  }
  if (found) assert(zpath->realpath!=0);
 found:
  zpath->virtualpath=virtualpath;/*Restore*/
  zpath->virtualpath_l=vp_l;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, if (!onlyThisRoot||found){ LOCK(mutex_fhandle,transient_cache_store(found?zpath:NULL,vp,vp_l);}));
  if (!found && !path_starts_autogen && !onlyThisRoot && !config_not_report_stat_error(vp,vp_l) IF1(WITH_INTERNET_DOWNLOAD, && !net_is_internetfile(vp,vp_l))){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,vp,"Not found");
  }
  return found;
} /*find_realpath_any_root*/

static bool _find_realpath_other_root(struct zippath *zpath){
  assert(zpath->root);
  assert(zpath->realpath);
  //log_entered_function("%s",RP());
  const off_t size0=zpath->stat_rp.st_size;
  ASSERT(size0>0);
  const struct rootdata *prev=NULL;
  foreach_root(r){
    if (prev==zpath->root){
      zpath->strgs_l=zpath->realpath;
      zpath->realpath=0;
      if (test_realpath(false,zpath,r) && size0==zpath->stat_rp.st_size)  return true;
    }
    prev=r;
  }
  return false;
} /*find_realpath_any_root*/
static bool find_realpath_other_root(struct zippath *zpath){
  cg_thread_assert_not_locked(mutex_fhandle);
  //log_entered_function("VP: '%s'",VP());
  LOCK_N(mutex_fhandle,struct zippath zp=*zpath);
  const bool found=_find_realpath_other_root(&zp);
  if (found){ LOCK(mutex_fhandle,*zpath=zp);  }
  //log_exited_function("VP: '%s' RP: '%s' EP: '%s'   found: %d",VP(),RP(),EP(), found);
  return found;

}

/////////////////////////////////////////////////////////////////////////////////////
/// Data associated with file handle.
// Motivation: When the same file is accessed from two different programs,
/// We see different fi->fh
/// We use this as a key to obtain an instance of  struct fHandle
///
/// Conversely, fuse_get_context()->private_data cannot be used.
/// It returns always the same pointer address even for different file handles.
///////////////////////////////////////////////////////////////////////////////////
static void fhandle_init(struct fHandle *d, const struct zippath *zpath){
  *d=FHANDLE_EMPTY;
  pthread_mutex_init(&d->mutex_read,NULL);
  d->zpath=*zpath;
  d->filetypedata=filetypedata_for_ext(VP(),d->zpath.root);
  d->flags|=FHANDLE_FLAG_ACTIVE; /* Important; This must be the last assignment */
}
static struct fHandle* fhandle_create_locked(const int flags,const uint64_t fh, const struct zippath *zpath){
  ASSERT_LOCKED_FHANDLE();
  struct fHandle *d=NULL;
  { foreach_fhandle_also_emty(ie,e) if (!e->flags){ d=e; break;}} /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhandle_n>=FHANDLE_MAX){ warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_ERROR,VP(),"Excceeding FHANDLE_MAX");return NULL;}
    d=fhandle_at_index(_fhandle_n++);
  }
  fhandle_init(d,zpath);
  d->fh=fh;
  d->flags=flags;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, transient_cache_activate(d));
  IF1(WITH_SPECIAL_FILE,if (!(flags&FHANDLE_FLAG_SPECIAL_FILE)) preloadfileram_infer_from_other_handle(d));
  d->flags|=FHANDLE_FLAG_ACTIVE;
  COUNTER1_INC(COUNT_FHANDLE_CONSTRUCT);
  return d;
}
static struct fHandle* fhandle_create(const int flags,const uint64_t fh, const struct zippath *zpath){
  cg_thread_assert_not_locked(mutex_fhandle);
  while(true){
    static struct fHandle* d=NULL;
    LOCK(mutex_fhandle, d=fhandle_create_locked(flags,fh,zpath)); /* zpath is now stored in fHandle */
    if (d) return d;
    if (zpath) log_verbose("Going to sleep and retry fhandle_create %s ...",VP());
    usleep(1000*1000);
  }
}
static bool fhandle_currently_reading_writing(const struct fHandle *d){
  ASSERT_LOCKED_FHANDLE();
  if (!d) return false;
  assert(d->is_busy>=0);
  return d->is_busy>0;
}

static void fhandle_destroy(struct fHandle *d){
  ASSERT_LOCKED_FHANDLE();
  if (d){
    d->flags|=FHANDLE_FLAG_DESTROY_LATER;
    if (!fhandle_currently_reading_writing(d)) fhandle_destroy_now(d);
    else warning(WARN_FLAG_ERROR|WARN_FLAG_ONCE_PER_PATH,D_VP(d),"fhandle_currently_reading_writing()");

  }
}
static void fhandle_destroy_now(struct fHandle *d){
  ASSERT_LOCKED_FHANDLE();
#if WITH_PRELOADFILERAM
  if (atomic_load(&d->is_preloading)>0) return; /* Currently not being copied to RAM */
  if (!is_preloadfileram_shared_with_other(d) &&          /* Another fHandle could take care of struct preloadfileram instance later */
      !preloadfileram_try_destroy(d)) return;             /* struct preloadfileram is NULL or has been destroyed */
#endif //WITH_PRELOADFILERAM
  IF1(WITH_AUTOGEN,const uint64_t fhr=d->fh_real;d->fh_real=0;if (fhr) close(fhr));
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,transient_cache_destroy(d));
  fhandle_zip_fclose(d);
  my_zip_close(d->zip_archive,D_RP(d));
  pthread_mutex_destroy(&d->mutex_read);
  IF1(WITH_AUTOGEN, aimpl_maybe_reset_atime_in_future(d));
  IF1(WITH_EVICT_FROM_PAGECACHE,if (!fhandle_find_identical(d)) maybe_evict_from_filecache(0,D_RP(d),D_RP_L(d),D_EP(d),D_EP_L(d)));
  *d=FHANDLE_EMPTY;
  COUNTER2_INC(COUNT_FHANDLE_CONSTRUCT);
}
static void fhandle_destroy_those_that_are_marked(void){
  ASSERT_LOCKED_FHANDLE();
  foreach_fhandle(id,d){
    if ((d->flags&FHANDLE_FLAG_DESTROY_LATER)  && !fhandle_currently_reading_writing(d)) fhandle_destroy_now(d);
  }
}
static struct fHandle* fhandle_get(const char *path,const uint64_t fh){
  ASSERT_LOCKED_FHANDLE();
  //log_msg(ANSI_FG_GRAY" fHandle %d  %lu\n"ANSI_RESET,op,fh);
  fhandle_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(path);
  foreach_fhandle(id,d){
    if (fh==d->fh && D_VP_HASH(d)==h && !strcmp(path,D_VP(d))) return d;
  }
  return NULL;
}
static bool fhandle_find_identical(const struct fHandle *d){
  foreach_fhandle(ie,e) if (fhandle_virtualpath_equals(d,e) && !(e->flags&FHANDLE_FLAG_DESTROY_LATER)) return true;
  return false;
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t next_inode(void){
  static ino_t seq=1UL<<63;
  return seq++;
}
static ino_t make_inode(const ino_t inode0,struct rootdata *r, const int entryIdx,const char *path){
  const static int SHIFT_FSID=42,SHIFT_ENTRY=(SHIFT_FSID+LOG2_FILESYSTEMS);
  const ino_t fsid=r?r->seq_fsid:LOG2_FILESYSTEMS; /* Better than rootindex(r) */
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,path,"inode0 is zero");
  if (inode0<(1L<<SHIFT_FSID) && entryIdx<(1<<(63-SHIFT_ENTRY))){  // (- 63 46)
    const ino_t ino= inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| (fsid<<SHIFT_FSID);
    return ino;
  }else{
    struct ht *ht=&r->ht_inodes;
    const uint64_t key2=entryIdx|(fsid<<(64-LOG2_FILESYSTEMS)),key_high_variability=(inode0+1)^key2;
    /* Note: Exclusive or with keys. Otherwise no variability for entries in the same ZIP
       inode0+1:  The implementation of the hash map requires that at least one of  both keys is not 0. */
    LOCK_NCANCEL_N(mutex_inode,
                   if (!ht->capacity) ht_set_id(HT_MALLOC_inodes,ht_init(ht,"inode",HT_FLAG_NUMKEY|16));
                   ino_t inod=(ino_t)ht_numkey_get(ht,key_high_variability,key2);
                   if (!inod){ ht_numkey_set(ht,key_high_variability,key2,(void*)(inod=next_inode())); COUNTER1_INC(COUNT_SEQUENTIAL_INODE);});
    return inod;
  }
}
static ino_t inode_from_virtualpath(const char *vp,const int vp_l){
  static struct ht_entry *e;
  LOCK_NCANCEL(mutex_inode, e=ht_get_entry(&ht_inodes_vp,vp,vp_l,0,true));
  if (!e->value) e->value=(void*)next_inode();
  return (ino_t)e->value;
}
/* ******************************************************************************** */
/* *** Zip *** */
static int zipentry_placeholder_expand(char *u,const char *orig, const char *rp, const struct directory *dir){
  if (!orig) return 0;

  int len=cg_pathlen_ignore_trailing_slash(orig);
  if (len>=MAX_PATHLEN){ warning(WARN_STR|WARN_FLAG_ERROR,u,"Exceed_MAX_PATHLEN"); return 0;}
  stpcpy(u,orig)[len]=0;

  IF1(WITH_ZIPENTRY_PLACEHOLDER, if (!dir||!dir->has_file_containing_placeholder) len=zipentry_placeholder_expand2(u,rp));
  //log_debug_now("%s  %s --> %s",rp,orig,u);
  return len;
}


////////////////////////////////////////////////////////////////////////////////////////
/// List entries in ZIP file directory.                                              ///
/// Directories are not explicitely contained in dir->core                           ///
/// Consequently, path components are successively removed from the right side.      ///
/// Parameter  filler:  Not NULL If called for directory listing                     ///
///                     NULL for running stat for a specific ZIP entry               ///
////////////////////////////////////////////////////////////////////////////////////////
static void filler_add(const int opt,fuse_fill_dir_t filler,void *buf, const char *name,  int name_l,const struct stat *st, struct ht *no_dups){

  if (!name_l) name_l=strlen(name);

  IF1(WITH_AUTOGEN,if(opt&FILLDIR_AUTOGEN)autogen_filldir(filler,buf,name,st,no_dups);else)
    if (ht_only_once(no_dups,name,name_l)){
      assert_validchars(VALIDCHARS_FILE,name,name_l);
      filler(buf,name,st,0 COMMA_FILL_DIR_PLUS);
    }
}
static int filler_readdir_zip(const int opt,struct zippath *zpath,void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  char ep[MAX_PATHLEN]; /* This will be the entry path of the parent dir */
  const int ep_l=filler?EP_L():MAX_int(0,cg_last_slash(EP()));
  memcpy(ep,EP(),ep_l); ep[ep_l]=0;
  const char *lastComponent=EP()+ep_l+(ep_l>0);
  const int lastComponent_l=EP_L()-(ep_l+(ep_l>0));
  if(!filler && !EP_L()) return 0; /* When the virtual path is a Zip file then just report success */
  if (!zpath_stat(zpath,zpath->root)) return ENOENT;
  struct directory mydir={0}, *dir=&mydir; mydir.debug=true;
  directory_init_zpath(dir,zpath);
  if (!readdir_from_cache_zip_or_filesystem(dir)) return ENOENT;
  IF1(WITH_DIRCACHE,LOCK(mutex_dircache,to_cache_vpath_to_zippath(dir)));
  char u[MAX_PATHLEN+1]; /* entry path expanded placeholder */
  struct directory_core dc=dir->core;
  int idx=0;
  FOR(i,0,dc.files_l){
    if (cg_empty_dot_dotdot(dc.fname[i])) continue;
    int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),dir);
    bool isdir=false;
    //log_debug_now("fname[%d]: %s u: %s  %d %d %d",i,dc.fname[i],u   ,u_l<=ep_l , strncmp(ep,u,ep_l) , ep_l>0 && u[ep_l]!='/');
    for(int removeLast=0; u_l>0; idx++){
      if (removeLast++){ /* To get all dirs, and parent dirs successively remove last path component. */
        if ((u_l=cg_last_slash(u))<0) break;
        u[u_l]=0;
        isdir=true;
      }
      if ((u_l<=ep_l || strncmp(ep,u,ep_l) || ep_l>0 && u[ep_l]!='/')) continue; /* u must start with  zpath->entry_path */
      const char *n=u+ep_l+(ep_l>0);
      const int n_l=u_l+(u-n);
      if (!*n || (filler?NULL!=strchr(n,'/'):(n_l!=lastComponent_l || strcmp(lastComponent,n)))) continue;
      struct stat stbuf,*st=filler?&stbuf:&zpath->stat_vp;
      stat_init(st,isdir?-1:Nth0(dc.fsize,i),&zpath->stat_rp);
      st->st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,idx,RP());
      st->st_mtime=Nth0(dc.fmtime,i);
      if (!filler){  /* ---  Called from test_realpath_or_reset() to set zpath->stat_vp --- */
        zpath->stat_vp.st_uid=getuid();
        zpath->stat_vp.st_gid=getgid();
        if (Nth0(dc.fflags,i)&DIRENT_IS_COMPRESSED){
          zpath->flags|=ZP_IS_COMPRESSED;
          //cg_print_stacktrace(0);
        }
        zpath->zipcrc32=Nth0(dc.fcrc,i);
        directory_destroy(dir);
        return 0;
      }
      filler_add(opt,filler,buf,n,n_l,st,no_dups);
    }
  }
  directory_destroy(dir);
  return filler?0:ENOENT;
}/*filler_readdir_zip*/
static bool filler_readdir(const int opt,struct zippath *zpath, void *buf, fuse_fill_dir_t filler,struct ht *no_dups){
  const char *rp=RP();
  if (!rp || !*rp) return false;
  if (ZPF(ZP_ZIP)) return filler_readdir_zip(opt,zpath,buf,filler,no_dups);
  if (!zpath->stat_rp.st_ino) return true;
  ASSERT(zpath->root!=NULL);
  char dirname_from_zip[MAX_PATHLEN+1];
  ASSERT(zpath!=NULL);
  struct directory dir={0};  directory_init_zpath(&dir,zpath);
  if (readdir_from_cache_zip_or_filesystem(&dir)){
    char u[MAX_PATHLEN+1]; /*buffers for unsimplify_fname() */
    const struct directory_core dc=dir.core;
    FOR(i,0,dc.files_l){
      if (cg_empty_dot_dotdot(dc.fname[i])) continue;
      int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),&dir);
      IF1(WITH_INTERNET_DOWNLOAD, if (_root_writable && (opt&FILLDIR_STRIP_NET_HEADER)) u_l=net_filename_from_header_file(u,u_l));
      if (!u_l || 0==(opt&FILLDIR_AUTOGEN) && no_dups && ht_get(no_dups,u,u_l,0)) continue;
      IF1(WITH_ZIPINLINE,if (config_skip_zipfile_show_zipentries_instead(u,u_l) && readdir_inline_from_cache(opt,zpath,u,buf,filler,no_dups)) continue);
      struct stat st;
      stat_init(&st,(Nth0(dc.fflags,i)&DIRENT_ISDIR)?-1:Nth0(dc.fsize,i),NULL);
      st.st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,0,rp);
      if (!config_do_not_list_file(rp,u,u_l)){
        *dirname_from_zip=0;
        const bool also_show_zip_file_itself=config_zipfilename_to_virtual_dirname(dirname_from_zip,u,u_l);
        if (*dirname_from_zip){
          stat_set_dir(&st,u);
          filler_add(opt,filler,buf,dirname_from_zip,0,&st,no_dups);
          if (also_show_zip_file_itself) filler_add(opt,filler,buf,u,u_l,&st,no_dups); // cppcheck-suppress knownConditionTrueFalse
        }else{
          filler_add(opt,filler,buf,u,u_l,&st,no_dups);
        }
      }
    }
    directory_destroy(&dir);
  }
  return true;
}
static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;} // cppcheck-suppress [constParameterCallback]
static int xmp_statfs(const char *path, struct statvfs *stbuf){
  return minus_val_or_errno(statvfs(_root->rootpath,stbuf));
}
/************************************************************************************************/
static int mk_parentdir_if_sufficient_storage_space(const char *rp){
  const int slash=cg_last_slash(rp);
  if (slash<=0) return EINVAL;
  if (!cg_recursive_mk_parentdir(rp)){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,rp,"failed cg_recursive_mk_parentdir");
    return EPERM;
  }
  char parent[PATH_MAX+1]; cg_stpncpy0(parent,rp,slash);
  struct statvfs st;
  if (statvfs(parent,&st)){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,parent,"Going return EIO");
    return EIO;
  }
  const long free=st.f_frsize*st.f_bavail, total=st.f_frsize*st.f_blocks;
  if (config_has_sufficient_storage_space(rp,free,total)) return 0;
  warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH,parent,"%s: config_has_sufficient_storage_space(Available=%'ld GB, Total=%'ld GB)",strerror(ENOSPC),free>>30,total>>30);
  return ENOSPC;
}

/* *** Create parent dir for creating new files. The first root is writable, the others not *** */
static int realpath_mk_parent(char *rp,const char *vp){
  if (!_root_writable) return EACCES;/* Only first root is writable */
  NORMALIZE_EMPTY_PATH_L(vp);
  const int slash=cg_last_slash(vp);
  if (config_not_overwrite(vp,vp_l)){
    bool found;FIND_REALPATH(vp);
    if (found && zpath->root>0){ warning(WARN_OPEN|WARN_FLAG_ONCE,RP(),"It is only allowed to overwrite files in root 0");return EACCES;}
  }
  assert(_root_writable->rootpath_l+vp_l<MAX_PATHLEN);
  if (slash>0){
    char parent[MAX_PATHLEN+1];
    cg_strncpy0(parent,vp,slash);
    bool found;FIND_REALPATH(parent);
    if (!found) return ENOENT;
    strcpy(stpcpy(rp,_root_writable->rootpath),vp);
    const int res=mk_parentdir_if_sufficient_storage_space(rp);
    if (res) return res;
  }
  strcpy(stpcpy(rp,_root_writable->rootpath),vp);
  return 0;
}
/********************************************************************************/
// FUSE FUSE 3.0.0rc3 The high-level init() handler now receives an additional struct fuse_config pointer that can be used to adjust high-level API specific configuration options.
#define DO_LIBFUSE_CACHE_STAT 0
#define EVAL(a) a
#define EVAL2(a) EVAL(a)
//constParameterPointer
// XXXXXXXXXcppcheck-suppress [constParameterCallback]
static void *xmp_init(struct fuse_conn_info *conn IF1(WITH_FUSE_3,,struct fuse_config *cfg)){
  //void *x=fuse_apply_conn_info_opts;  //cfg-async_read=1;
#if WITH_FUSE_3
  cfg->use_ino=1;
  IF1(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
#endif
  log_verbose(GREEN_SUCCESS"FUSE filesystem initialized at '%s'\n",_mnt);
  if (*_mkSymlinkAfterStart){
    struct stat stbuf;
    if (lstat(_mkSymlinkAfterStart,&stbuf)) perror(_mkSymlinkAfterStart);
    if (((S_IFREG|S_IFDIR)&stbuf.st_mode) && !(S_IFLNK&stbuf.st_mode)){
      warning(WARN_MISC,""," Cannot make symlink %s =>%s  because %s is a file or dir\n",_mkSymlinkAfterStart,_mnt,_mkSymlinkAfterStart);
      DIE("");
    }
  }

  return NULL;
}



// cppcheck-suppress-macro  unreadVariable
#define FUSE_PREAMBLE(create_or_del,vpath)     int tmp_vp_l, er; char vp_buffer[MAX_PATHLEN]; const char *vp=virtualfile_transform(create_or_del,vp_buffer,vpath,&tmp_vp_l,&er); const int vp_l=tmp_vp_l;
#define FUSE_PREAMBLE_RETURN_ER(create_or_del,vpath) FUSE_PREAMBLE(create_or_del,vpath); if (er) return -er
// DEBUG_NOW
static const char *virtualfile_transform(const int create_read_del, char *buf, const char *vp, int *vp_length, int *error){
  errno=0;
  LOG_FUSE(vp);
  NORMALIZE_EMPTY_PATH_L(vp);
  *error=*buf=0;
  if (DIR_AUTOGEN_L+2+50+vp_l+_rootdata_path_max>MAX_PATHLEN){
    *error=ENAMETOOLONG;
  }else if (create_read_del){
    if (!_root_writable){
      *error=EACCES;
    }else{
      if (VP_STARTS(DIR_ZIPsFS)){
        if (!strchr(vp+DIR_ZIPsFS_L+1,'/')){ // EPERM EACCES EEXIST
          if (VP_EQ_Z(DIR_PRELOADFILEDISK_R) ||
              VP_EQ_Z(DIR_PRELOADFILEDISK_RZ) ||
              VP_EQ_Z(DIR_PRELOADFILEDISK_RC) ||
              VP_EQ_Z(DIR_AUTOGEN) ||
              VP_EQ_Z(DIR_INTERNET)){
            *error=create_read_del==1?EEXIST:EPERM;
          }else{
            *error=EACCES;
          }
        }
#define C(d,t) if (VP_STARTS_Z(d)) stpcpy(stpcpy(buf,t),vp+sizeof(d)-1);
        C(DIR_PRELOADFILEDISK_R, DIR_PRELOADED_BY_PATH);
        C(DIR_PRELOADFILEDISK_RC,DIR_PRELOADED_BY_PATH);
        C(DIR_PRELOADFILEDISK_RZ,DIR_PRELOADED_BY_PATH);
#undef C
      }
    }
  }
  *vp_length=*buf?strlen(buf):vp_l;
  return *buf?buf:vp;
}


/*  Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now  additional struct fuse_file_info pointer (which, may be NULL even if the file is currently open) */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10)
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 1
#else
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 0
#endif
static int xmp_getattr(const char *path, struct stat *st IF1(WITH_XMP_GETATTR_FUSE_FILE_INFO,,struct fuse_file_info *fi_or_null)){
  //log_entered_function("%s",path);
  LOG_FUSE(path);
  const int res=_xmp_getattr(path,st);
  LOG_FUSE_RES(path,res);
  //log_exited_function("%s res: %d",path,res);
  return res;
}

static int _xmp_getattr(const char *vpath, struct stat *st){
  FUSE_PREAMBLE_RETURN_ER(0,vpath);
  IF1(WITH_SPECIAL_FILE,if (special_file_set_statbuf(st,vp,vp_l)) return 0);
  //log_entered_function("%s",vp);
  if (!vp_l || VP_STARTS(DIR_ZIPsFS) && (VP_EQ_Z(DIR_PRELOADFILEDISK_R)||
                                         VP_EQ_Z(DIR_PRELOADFILEDISK_RC)||
                                         VP_EQ_Z(DIR_PRELOADFILEDISK_RZ)||
                                         VP_EQ_Z(DIR_PRELOADED_BY_PATH)||
                                         VP_EQ_Z(DIR_PRELOADED_BY_ROOT)||
                                         VP_EQ_Z(DIR_ZIPsFS"/"DIRNAME_PRELOADED_UPDATE)||
                                         net_which_dir_compression(vp,vp_l))){
    stat_init(st,-1,NULL);
    st->st_ino=inode_from_virtualpath(vp,vp_l);
    time(&st->st_mtime);
    return 0;
  }
  IF1(WITH_CCODE, if (c_getattr(st,vp,vp_l)) return 0);
  bool found;FIND_REALPATH(vp);
  if (found){
    *st=zpath->stat_vp;
  }else{
    IF1(WITH_INTERNET_DOWNLOAD, if (net_getattr(st,vp,vp_l))           return 0);
    IF1(WITH_AUTOGEN,           if (autogen_getattr(st,zpath,vp,vp_l)) return 0);
    er=ENOENT;
  }
  inc_count_by_ext(vp,er?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS);
  IF1(DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (er) debug_track_false_getattr_errors(vp,vp_l));
  if (config_file_is_readonly(vp,vp_l)) st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  return -er;
}/*xmp_getattr*/


#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10) /* Not sure when parameter introduced */
#define WITH_UTIMENS_FUSE_FILE_INFO 1
#else
#define WITH_UTIMENS_FUSE_FILE_INFO 0
#endif
static int xmp_utimens(const char *vpath, const struct timespec ts[2]  IF1(WITH_UTIMENS_FUSE_FILE_INFO,,struct fuse_file_info *fi_not_used)){
  FUSE_PREAMBLE(1,vpath);
  if (!er){
    bool found; FIND_REALPATH(vp);
    if (!found) return -ENOENT;
    if (zpath->root!=_root_writable) return -EPERM;
    er=utimensat(0,RP(),ts,AT_SYMLINK_NOFOLLOW);  /* don't use utime/utimes since they follow symlinks */
  }
  return minus_val_or_errno(er);
}
static int xmp_readlink(const char *vpath, char *buf, size_t size){
  FUSE_PREAMBLE_RETURN_ER(0,vpath);
  bool found;FIND_REALPATH(vpath);
  if (!found) return -ENOENT;
  const int n=readlink(RP(),buf,size-1);
  return n==-1?-errno: (buf[n]=0);
}
static int xmp_unlink(const char *vpath){
  FUSE_PREAMBLE_RETURN_ER(-1,vpath);
  bool found;FIND_REALPATH(vp);
  LOG_FUSE_RES(vp,found);
  return !found?-ENOENT: !ZPATH_ROOT_WRITABLE()?-EACCES:  minus_val_or_errno(unlink(RP()));
}
static int xmp_rmdir(const char *vpath){
  FUSE_PREAMBLE_RETURN_ER(-1,vpath);
  bool found;FIND_REALPATH(vpath);
  LOG_FUSE_RES(vpath,found);
  return !ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(rmdir(RP()));
}


#define LOG2_FD_ZIP_MIN 20
#define FD_ZIP_MIN (1<<LOG2_FD_ZIP_MIN)
static uint64_t next_fh(){
  static uint64_t next_fh=FD_ZIP_MIN;
  if (++next_fh==UINT64_MAX) next_fh=FD_ZIP_MIN;
  return next_fh;
}

static int64_t special_file_fuse_open(const struct zippath *zpath){
  IF1(WITH_PRELOADFILERAM,if (!is_special_file_preloadfileram(VP(),VP_L())) return 0);
  return fhandle_create(FHANDLE_FLAG_SPECIAL_FILE,next_fh(),zpath)->fh;
}

static int _xmp_open(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE_RETURN_ER((fi->flags&(O_WRONLY||O_RDWR|O_APPEND|O_CREAT))?1:0,vpath);
  if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))) return -create_or_open(vp,0775,fi);
  NEW_ZIPPATH(vp);
  int64_t handle=0;
  IF1(WITH_SPECIAL_FILE, handle=special_file_fuse_open(zpath));
  IF1(WITH_CCODE, if (!handle) handle=c_fuse_open(zpath));
  bool found=handle>0;
  if (!found){
    found=find_realpath_any_root(0,zpath,NULL);
    IF1(WITH_INTERNET_DOWNLOAD, if (!found) found=net_maybe_download(zpath));
    IF1(WITH_AUTOGEN, if (found && _realpath_autogen && ZPF(ZP_STARTS_AUTOGEN) && autogen_remove_if_not_up_to_date(zpath)) found=false);
    if (found){
      if (ZPF(ZP_PRELOADFILEDISK|ZP_ZIP|ZP_PRELOADFILEDISK_UPDATE) IF1(WITH_PRELOADFILERAM,||zpath_preloadfileram_advise(zpath))){
        fhandle_create(0,handle=next_fh(),zpath);
      }else{
        IF1(WITH_AUTOGEN,const int64_t fh_real=)handle=async_openfile(zpath,fi->flags);
        if (handle<=0) warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"open:  fh=%d",handle);
        if (has_proc_fs() && !cg_check_path_for_fd("my_open_fh",RP(),handle)) handle=-1;
        IF1(WITH_AUTOGEN, if (handle>0 && ZPF(ZP_STARTS_AUTOGEN)) fhandle_create(0,handle=next_fh(),zpath)->fh_real=fh_real);
      }
    }
  }
  if (!found){
#if WITH_AUTOGEN
    struct autogen_files ff={0};
    struct_autogen_files_init(&ff,vp,vp_l);
    if (_realpath_autogen && (ZPF(ZP_STARTS_AUTOGEN)) && autogen_realinfiles(&ff)>=0){
      LOCK(mutex_fhandle, autogen_zpath_init(zpath,vp));
      fhandle_create(FHANDLE_FLAG_IS_AUTOGEN,handle=next_fh(),zpath);
      found=true;
    }
    struct_autogen_files_destroy(&ff);
#endif //WITH_AUTOGEN
    if (!found && report_failure_for_tdf(vp)){ // cppcheck-suppress knownConditionTrueFalse
      log_zpath("Failed ",zpath);
      warning(WARN_OPEN|WARN_FLAG_MAYBE_EXIT,vp,"FIND_REALPATH failed");
    }
  }
  if (!found || handle==-1){
    if (!config_not_report_stat_error(vp,vp_l)) warning(WARN_GETATTR,vp,"found:%s handle:%llu",success_or_fail(found),handle);
    return !found?-ENOENT:-errno;
  }
  fi->fh=handle;
  return 0;
}/*xmp_open*/

static int xmp_open(const char *path, struct fuse_file_info *fi){
  //log_entered_function("%s",path);
  const int res=_xmp_open(path,fi);
  IF_LOG_FLAG_OR(LOG_OPEN,res!=0)log_exited_function("%s res: %d",path,res);
  return res;
}
static int xmp_truncate(const char *path, off_t size IF1(WITH_FUSE_3,,struct fuse_file_info *fi)){
  LOG_FUSE(path);
  int res;
  IF1(WITH_FUSE_3,if (fi) res=ftruncate(fi->fh,size); else)
    {
      bool found;FIND_REALPATH(path);
      res=!ZPATH_ROOT_WRITABLE()?EACCES: found?truncate(RP(),size):ENOENT;
    }
  return minus_val_or_errno(res);
}
/////////////////////////////////
// Readdir
/////////////////////////////////
/** unsigned int cache_readdir:1; FOPEN_CACHE_DIR Can be filled in by opendir. It signals the kernel to  enable caching of entries returned by readdir(). */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION,3,5)  /* FUSE 3.5 Added a new cache_readdir flag to fuse_file_info to enable caching of readdir results. */
#define WITH_XMP_READDIR_FLAGS 1
#else
#define WITH_XMP_READDIR_FLAGS 0
#endif

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi IF1(WITH_XMP_READDIR_FLAGS,,enum fuse_readdir_flags flags)){
  LOG_FUSE(path);
  const int res=_xmp_readdir(path,buf,filler,offset,fi);
  LOG_FUSE_RES(path,res);
  inc_count_by_ext(path,res?COUNTER_READDIR_FAIL:COUNTER_READDIR_SUCCESS);
  return res;
}

static bool _readdir_roots(const bool cut_autogen, const bool from_network_header, int opt, struct zippath *zpath, void *buf, fuse_fill_dir_t filler, struct ht *no_dups){
  bool ok=false;
  foreach_root(r){ /* FINDRP_AUTOGEN_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
    if (!VP_L() && r->retain_dirname_l){
      filler_add(0,filler,buf,r->retain_dirname+1,r->retain_dirname_l-1,NULL,no_dups);
      ok=true;
    }else if (find_realpath_any_root(opt|(cut_autogen?FINDRP_AUTOGEN_CUT:FINDRP_AUTOGEN_CUT_NOT),zpath,r)){
      opt|=FINDRP_NOT_TRANSIENT_CACHE; /* Transient cache only once */
      filler_readdir(from_network_header?FILLDIR_STRIP_NET_HEADER:0,zpath,buf,filler,no_dups);
      IF1(WITH_AUTOGEN, if (ZPF(ZP_STARTS_AUTOGEN)) filler_readdir(FILLDIR_AUTOGEN,zpath,buf,filler,no_dups));
      ok=true;
      IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,if (ZPF(ZP_FROM_TRANSIENT_CACHE))break); /*Performance*/
      if (config_readir_no_other_roots(RP(),RP_L())) break; /*Performance*/
    }
  }
  return ok;
}

static int _xmp_readdir(const char *vp, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
  (void)offset;(void)fi;
  struct ht no_dups={0}; ht_set_id(HT_MALLOC_without_dups,ht_init_with_keystore_dim(&no_dups,"xmp_readdir_no_dups",8,4096));
  no_dups.keystore->mstore_counter_mmap=COUNT_MSTORE_MMAP_NODUPS;
  no_dups.ht_counter_malloc=COUNT_HT_MALLOC_NODUPS;
  const int opt=strcmp(vp,DIR_ZIPsFS)?0:FILLDIR_IS_DIR_ZIPsFS;
  NEW_ZIPPATH(vp);
  const int vp_l=VP_L();
  bool ok=false;
#define A(n) filler_add(0,filler,buf,n,0,NULL,&no_dups);
#define C(cut_autogen,from_network_header) _readdir_roots(cut_autogen,from_network_header,opt,zpath,buf,filler,&no_dups);ok=true
  C(false,false);
  if (ZPF(ZP_STARTS_AUTOGEN)){
    IF1(WITH_AUTOGEN,C(true,false));
    if (VP_EQ_Z(DIR_AUTOGEN)) A(SPECIAL_FILES[SFILE_README_AUTOGEN]);
  }
  const int dirid=net_which_dir_compression(vp,vp_l);
  if (dirid){
    C(false,true);
    if (dirid==INTERNET_COMPRESSION_NONE){
      A("gz");
      A("bz2");
      A(SPECIAL_FILES[SFILE_README_INTERNET]);
    }
  }
  IF1(WITH_CCODE, c_readdir(zpath,buf,filler,NULL));
#undef C
#define N(dir) A((dir)+DIR_ZIPsFS_L+1)
  if (opt&FILLDIR_IS_DIR_ZIPsFS){ /* Childs of folder ZIPsFS */
    FOR(i,1,SFILE_NUM) if (SPECIAL_FILE_IN_DEFAULT_DIR(i)) A(SPECIAL_FILES[i]);
    N(DIR_AUTOGEN);
    N(DIR_PRELOADFILEDISK_R); N(DIR_PRELOADFILEDISK_RC);  N(DIR_PRELOADFILEDISK_RZ);
    N(DIR_PRELOADED_BY_ROOT); N(DIR_PRELOADED_BY_PATH);
    A(DIRNAME_PRELOADED_UPDATE);
    N(DIR_INTERNET);
    ok=true;
  }
#undef N
  if (!vp_l) A((DIR_ZIPsFS)+1);
#define N(d) if (VP_EQ_Z(d)){A(DIRNAME_PRELOADED_UPDATE); A(SPECIAL_FILES[SFILE_README_PRELOADFILEDISK]);}
  if (VP_STARTS(DIR_ZIPsFS)){ N(DIR_PRELOADFILEDISK_R); N(DIR_PRELOADFILEDISK_RC); N(DIR_PRELOADFILEDISK_RZ); N(DIR_PRELOADED_BY_ROOT); N(DIR_PRELOADED_BY_PATH); }

#undef N
#undef A
  ht_destroy(&no_dups);
  //log_exited_function("ok: %d",ok);
  return ok?0:-1;
}


//#define ERRNO_FOR_MINUS1(x) (x==-1?errno:0)
/////////////////////////////////////////////////////////////////////////////////
// With the following methods, new  new files,links, dirs etc.  are created  ///
/////////////////////////////////////////////////////////////////////////////////
static int xmp_mkdir(const char *vpath, mode_t mode){
  char real_path[MAX_PATHLEN+1];
  FUSE_PREAMBLE(1,vpath);
  if (!er) er=realpath_mk_parent(real_path,vp);
  if (!er) er=mkdir(real_path,mode);
  return minus_val_or_errno(er);
}

static int create_or_open(const char *vp, mode_t mode, struct fuse_file_info *fi){
  char rp[MAX_PATHLEN+1];
  {
    const int er=realpath_mk_parent(rp,vp);
    if (er) return er;
  }
  if (!(fi->fh=MAX_int(0,open(rp,fi->flags|O_CREAT,mode)))){
    log_errno("open(%s,%x,%u) returned -1\n",rp,fi->flags,mode); cg_log_file_mode(mode); cg_log_open_flags(fi->flags); log_char('\n');
    return errno;
  }
  return 0;
}


static int xmp_create(const char *vpath, mode_t mode,struct fuse_file_info *fi){ /* O_CREAT|O_RDWR goes here */
  FUSE_PREAMBLE(1,vpath);

  if (!er) er=create_or_open(vp,mode,fi);
  LOG_FUSE_RES(vp,er);
  return -er;
}
static int xmp_write(const char *vpath, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  FUSE_PREAMBLE(1,vpath);

  if (net_which_compression(vp,vp_l)) return -EACCES;
  long fd;
  if (!fi){
    char real_path[MAX_PATHLEN+1];
    if ((er=realpath_mk_parent(real_path,vp))) return -er;
    fd=MAX_int(0,open(real_path,O_WRONLY));
  }else{
    fd=fi->fh;
    LOCK_N(mutex_fhandle, struct fHandle *d=fhandle_get(vpath,fd); fhandle_busy_start(d));
    if (d){

      fd_for_fhandle(&fd,d,fi->flags);
      LOCK_N(mutex_fhandle, fhandle_busy_end(d)); // cppcheck nullPointerOutOfResources
    }
  }
  if (fd<=0) return errno?-errno:-EIO;
  long int n=pwrite(fd,buf,size,offset);
  if (n==-1) n=-errno;
  if (!fi) close(fd);
  return n;
}
///////////////////////////////
// Functions with two paths ///
///////////////////////////////
static int xmp_symlink(const char *target, const char *vpath){ // target,link
  FUSE_PREAMBLE_RETURN_ER(1,vpath);
  char real_path[MAX_PATHLEN+1];
  return (!(realpath_mk_parent(real_path,vp)) && symlink(target,real_path)==-1)? -errno :0;
}

static int xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags)){
  bool eexist=false;
  FUSE_PREAMBLE_RETURN_ER(1,neu_path);
#if WITH_GNU && WITH_FUSE_3
  if (flags&RENAME_NOREPLACE){
    bool found;FIND_REALPATH(vp);
    if (found) eexist=true;
  }else if (flags) return -EINVAL;
#endif // WITH_GNU
  bool found;FIND_REALPATH(old_path);
  if (!found) return -ENOENT;
  if (eexist) return -EEXIST;
  if (!ZPATH_ROOT_WRITABLE()) return -EACCES;
  char neu[MAX_PATHLEN+1];
  er=realpath_mk_parent(neu,vp);
  if (!er) er=cg_rename(RP(),neu);
  return minus_val_or_errno(er);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////



////////////////////////////////////////////////////////////////////
// Wrapping zip_fread, zip_ftell and zip_fseek                     //
// Because zip_ftell and zip_fseek do not work on compressed data  //
/////////////////////////////////////////////////////////////////////

static int my_zip_fclose(zip_file_t *z,const char *path){
  if (!z) return 0;
  const int ret=zip_fclose(z);
  if (ret){
    warning_zip(path,zip_file_get_error(z),"Failed zip_fclose");
  }else{
    COUNTER2_INC(COUNT_ZIP_FOPEN);
  }
  return ret;
}
static int my_zip_close(zip_t *z,const char *path){
  if (!z) return 0;
  int ret=zip_close(z);
  if (ret){
    warning_zip(path,zip_get_error(z),"Failed zip_close");
  }else{
    COUNTER2_INC(COUNT_ZIP_OPEN);
  }
  return ret;
}

static zip_t *my_zip_open(const char *orig){
  if (!orig) return NULL;
  zip_t *zip=NULL;
  if (!cg_endsWithZip(orig,0)){
    IF_LOG_FLAG(LOG_ZIP) log_verbose("Does not end with '.zip'  '%s'",orig);
    return NULL;
  }
  IF_LOG_FLAG(LOG_ZIP){ static int count; log_verbose("Going to zip_open(%s) #%d ... ",orig,count); /*cg_print_stacktrace(0);*/ }
  RLOOP(iTry,2){
    int err;
    zip=zip_open(orig,ZIP_RDONLY,&err);
    if (zip){ COUNTER1_INC(COUNT_ZIP_OPEN); }else warning_zip(orig,0,"zip_open failed");
    inc_count_by_ext(orig,zip?COUNTER_ZIPOPEN_SUCCESS:COUNTER_ZIPOPEN_FAIL);
    if (zip) break;
    warning(WARN_OPEN|WARN_FLAG_ERRNO,orig,"err=%d",err);usleep(1000);
  }
  return zip;
}

static zip_file_t *my_zip_fopen(zip_t *za, const char *entry, const zip_flags_t flags, const char *path){
  if (!za){
    warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_FAIL,path,"za is NULL");
    return NULL;
  }
  zip_file_t *zf=zip_fopen(za,entry,flags);
  if (zf){
    COUNTER1_INC(COUNT_ZIP_FOPEN);
  }else warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_FAIL,path,"zip_fopen %s",entry,zip_get_error(za));
  return zf;
}
static off_t _my_zip_fread(zip_file_t *file, void *buf, const zip_uint64_t nbytes, const char *func){
  off_t n;
  FOR(retry,0,RETRY_ZIP_FREAD){
    n=zip_fread(file,buf,nbytes);
    if (n>0) break;
    log_verbose("Retry: %d Going sleep 100 ...",retry); usleep(100*1000);
  }
  return n;
}
/* If successful, the number of bytes actually read is returned. When zip_fread() is called after reaching the end of the file, 0 is returned. In case of error, -1 is returned. */
static off_t fhandle_zip_fread(struct fHandle *d, void *buf,  zip_uint64_t nbytes, const char *errmsg){
  IF1(WITH_EXTRA_ASSERT, LOCK(mutex_fhandle,  assert(fhandle_currently_reading_writing(d))))
    const off_t read=my_zip_fread(d->zip_file,buf,nbytes);
  if (read<0){ warning_zip(D_VP(d),zip_file_get_error(d->zip_file)," fhandle_zip_fread()");return -1;}
  d->zip_fread_position+=read;
  return read;
}
/* Returns true on success. May fail for seek backward. */
static bool fhandle_zip_fseek(struct fHandle *d, const off_t offset, const char *errmsg){
  off_t skip=(offset-fhandle_zip_ftell(d));
  if (!skip) return true;
  const bool backward=(skip<0);
  IF_LOG_FLAG(LOG_ZIP) log_verbose("%p %s offset: %lld  ftell: %lld   diff: %lld  backward: %s"ANSI_RESET,d, D_VP(d),(LLD)offset,(LLD)fhandle_zip_ftell(d),(LLD)skip, backward?ANSI_FG_RED"Yes":ANSI_FG_GREEN"No");
  const int fwbw=backward?FHANDLE_FLAG_SEEK_BW_FAIL:FHANDLE_FLAG_SEEK_FW_FAIL;


#if VERSION_AT_LEAST(LIBZIP_VERSION_MAJOR,LIBZIP_VERSION_MINOR,1,9)
  if (!(d->flags&fwbw) && !zip_file_is_seekable(d->zip_file)) d->flags|=(FHANDLE_FLAG_SEEK_BW_FAIL|FHANDLE_FLAG_SEEK_FW_FAIL);
#endif
  /*  zip_file_is_seekable() was added in libzip 1.9.0.   /usr/include/zip.h */
  if (!(d->flags&fwbw) && !zip_fseek(d->zip_file,offset,SEEK_SET)){
    d->zip_fread_position=offset;
    return true;
  }
  d->flags|=fwbw;
  if (backward) return false;
  char buf[4096]; /* Read to the respective position */
  const long num=MIN(sizeof(buf),skip);
  while(0<(skip=(offset-fhandle_zip_ftell(d)))){
    if (fhandle_zip_fread(d,buf,num,"")<0){
      if (fhandle_not_yet_logged(FHANDLE_ALREADY_LOGGED_FAILED_DIFF,d)) warning(WARN_SEEK,D_VP(d),"fhandle_zip_fread returns value<0   skip=%ld bufsize=%zu",skip,num);
      return false;
    }
  }
  return true;
}
static zip_file_t *fhandle_zip_open(struct fHandle *d,const char *msg){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!d->zip_file){
    struct async_zipfile zip;
    d->zip_fread_position=0;
    async_zipfile_init(&zip,&d->zpath);
    zip.za=d->zip_archive;
    async_openzip(&zip);
    d->zip_file=zip.zf;
    d->zip_archive=zip.za;
  }
  return d->zip_file;
}
static void fhandle_zip_fclose(struct fHandle *d){
  zip_file_t *z=d->zip_file;
  d->zip_file=NULL;
  if (z){
    zip_file_error_clear(z);
    my_zip_fclose(z,D_RP(d));
  }
}
static off_t fhandle_read_zip(char *buf, const off_t size, const off_t offset,struct fHandle *d,struct fuse_file_info *fi){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!fhandle_zip_open(d,__func__)){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,D_VP(d),"xmp_read_fhandle_zip fhandle_zip_open returned -1");
    return -1;
  }
  IF1(WITH_PRELOADFILERAM,if(_preloadfileram_policy!=PRELOADFILERAM_NEVER && tdf_or_tdf_bin(D_VP(d))) warning(WARN_PRELOADFILERAM|WARN_FLAG_ERROR|WARN_FLAG_ONCE_PER_PATH,D_RP(d),"tdf or tdf_bin should be preloadfileram"));
  if (offset!=fhandle_zip_ftell(d)){   /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    if (!fhandle_zip_fseek(d,offset,"")){
#if WITH_PRELOADFILERAM
      if (_preloadfileram_policy!=PRELOADFILERAM_NEVER){ /* Worst case=seek backward - consider using cache */
        if (preloadfileram_wait(d,offset+size)){
          fi->direct_io=1;
          fhandle_zip_fclose(d);
          COUNTER1_INC(COUNT_READZIP_PRELOADFILERAM_BECAUSE_SEEK_BWD);
          fhandle_counter_inc(d,ZIP_READ_CACHE_SUCCESS);
          LOCK_N(mutex_fhandle,const off_t nread=preloadfileram_read(buf,d,offset,offset+size));
          return nread;
        }
        fhandle_counter_inc(d,ZIP_READ_CACHE_FAIL);
      }
#endif //WITH_PRELOADFILERAM
    }
  }
  const off_t pos=fhandle_zip_ftell(d);
  if (offset<pos){ /* Worst case=seek backward - need reopen zip file */
    warning(WARN_SEEK,D_VP(d),ANSI_FG_RED"fhandle_zip_ftell() - going to reopen zip"ANSI_RESET);
    fhandle_zip_fclose(d);
    fhandle_zip_open(d,"REWIND");
    if (!fhandle_zip_fseek(d,offset,"REWIND")) return -1;
  }else if (offset>pos){
    warning(WARN_SEEK,D_VP(d),"should not happen offset=%ld > pos=%ld ",offset,pos);
    return -1;
  }
  const off_t num=fhandle_zip_fread(d,buf,size,"");
  fhandle_counter_inc(d,num<0?ZIP_READ_NOCACHE_FAIL:!num?ZIP_READ_NOCACHE_ZERO:ZIP_READ_NOCACHE_SUCCESS);
  return num;
}/*xmp_read_fhandle_zip*/
static bool fd_for_fhandle(long *fd, struct fHandle *d,const int open_flags){
  if (d){
    assert(d->is_busy>0);
    if (d->flags&FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE){
      //log_debug_now("D_RP: '%s'",D_RP(d));
      d->fh_real=open(D_RP(d),open_flags);
      d->flags&=~FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE;
    }/* Auto-generated data into cg_textbuffer */
    if (d->fh_real){ *fd=d->fh_real; return true; }
  }
  return false;
}

static int xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  //log_debug_now("%s size: %'ld offset: %'ld",path,size,offset);
  IF_LOG_FLAG(LOG_READ_BLOCK)log_entered_function("%s Offset: %'ld +%'d ",path,(long)offset,(int)size);
  const int bytes=_xmp_read(path,buf,size,offset,fi);
  IF_LOG_FLAG_OR(LOG_READ_BLOCK,bytes<=0)log_exited_function("%s  %'ld +%'d Bytes: %'d %s",path,(long)offset,(int)size,bytes,success_or_fail(bytes>0));
  return bytes;
}
static int _xmp_read(const char *path, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  NORMALIZE_EMPTY_PATH_L(path);
  //if (ENDSWITH(path,path_l,"mzML"))log_entered_function("%s offset %'lld  size %'lld",path,(LLD)offset,(LLD)size);
  long fd=fi->fh;
  int nread=0;
  LOCK_N(mutex_fhandle,
         struct fHandle *d=fhandle_get(path,fd);
         if (d){ d->accesstime=time(NULL); if (d->fh_real) fd=d->fh_real;}
         fhandle_busy_start(d));
  if (d){
    IF1(WITH_PRELOADFILEDISK, if (d->zpath.flags&ZP_PRELOADFILEDISK_UPDATE) preloadfiledisk_uptodate_or_update(d));

    IF1(WITH_SPECIAL_FILE, special_file_file_content_to_fhandle(d));
    IF1(WITH_CCODE,c_file_content_to_fhandle(d));
#if WITH_AUTOGEN
    if ((d->flags&FHANDLE_FLAG_IS_AUTOGEN) && _realpath_autogen && !d->fh_real && !(d->preloadfileram && d->preloadfileram->txtbuf)){
      if (!d->autogen_state) autogen_run(d);
      if (d->autogen_state!=AUTOGEN_SUCCESS){ nread=-d->autogen_error; goto done_d;}
      LOCK(mutex_fhandle,if (!(d->preloadfileram && d->preloadfileram->txtbuf)) d->flags|=FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE);
    }
#endif //WITH_AUTOGEN

    IF1(WITH_PRELOADFILEDISK,if (preloadfiledisk(d)) d->flags|=FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE);
    if (fd_for_fhandle(&fd,d,O_RDONLY)){
      if (fd<0) nread=-EIO;
      goto done_d; /* _fhandle_busy */
    }
    IF1(WITH_PRELOADFILERAM,if (d->flags&FHANDLE_FLAG_PRELOADFILERAM_COMPLETE){ /*_fhandle_busy*/
        LOCK(mutex_fhandle,nread=preloadfileram_read(buf,d,offset,offset+size)); goto done_d; }); /*_fhandle_busy*/ // CPPCHECK-SUPPRESS [unreadVariable]
    assert(d->is_busy>=0);
    {
      nread=-1;
      IF1(WITH_PRELOADFILERAM,
          if (!(d->flags&(FHANDLE_FLAG_WITH_PRELOADFILERAM|FHANDLE_FLAG_WITHOUT_PRELOADFILERAM))) d->flags|=(fhandle_preloadfileram_advise(d)?FHANDLE_FLAG_WITH_PRELOADFILERAM:FHANDLE_FLAG_WITHOUT_PRELOADFILERAM);
          if (d->flags&FHANDLE_FLAG_WITH_PRELOADFILERAM) nread=preloadfileram_wait_and_read(buf,size,offset,d,fi));
      if (nread<0 && (d->zpath.flags&ZP_ZIP)){
        pthread_mutex_lock(&d->mutex_read); /* Comming here same/different struct fHandle instances and various pthread_self() */
        nread=fhandle_read_zip(buf,size,offset,d,fi);
        pthread_mutex_unlock(&d->mutex_read);
      }
    }
    LOCK_N(mutex_fhandle, const char *status=IF01(WITH_PRELOADFILERAM,"!WITH_PRELOADFILERAM",PRELOADFILERAM_STATUS_S[preloadfileram_get_status(d)]); if (nread>0) d->n_read+=nread);
    if (nread<0 && !config_not_report_stat_error(path,path_l)){
      warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,d?D_RP(d):path,"nread<0:  d=%p  off=%ld size=%lld  nread=%d  n_read=%llu  preloadfileram_status:%s"ANSI_RESET,d,offset,(LLD)size,nread,d->n_read,status);
    }
  }
 done_d:
  if (!nread && (!d IF1(WITH_AUTOGEN,|| d->fh_real))){
    if (!fd){
      nread=-errno;
    }else if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK_REG_FILE:"ANSI_RESET" offset: %'lld ",(LLD)offset),log_msg("Failed %s fd=%llu\n",path,(LLU)fd);
      nread=-1;
    }else if ((nread=pread(fd,buf,size,offset))==-1){
      nread=-errno;
    }
  }
  if (d) LOCK(mutex_fhandle,fhandle_busy_end(d));
  return nread;
}/*xmp_read*/
static int xmp_release(const char *vpath, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE(1,vpath);
  if (!er){
    const uint64_t fh=fi->fh;
    if (fh>=FD_ZIP_MIN){
      lock(mutex_fhandle);
      struct fHandle *d=fhandle_get(vpath,fh);
      if (d) fhandle_destroy(d); else maybe_evict_from_filecache(fh,vp,vp_l,NULL,0);
      unlock(mutex_fhandle);
    }else if (fh>2 && (er=close(fh))){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,vp,"close(fh: %llu)",fh);
      cg_print_path_for_fd(fh);
    }
  }
  return -er;
}
static int xmp_flush(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE_RETURN_ER(1,vpath);

  IF1(WITH_SPECIAL_FILE,  if (is_special_file_preloadfileram(vp,vp_l)) return 0);
  return fi->fh<FD_ZIP_MIN?fsync(fi->fh):0;
}
static void exit_ZIPsFS(void){
  log_verbose("Going to exit ...");
  /* osxfuse and netbsd needs two parameters:  void fuse_unmount(const char *mountpoint, struct fuse_chan *ch); */
  IF1(WITH_FUSE_3,if (_fuse_started && fuse_get_context() && fuse_get_context()->fuse) fuse_unmount(fuse_get_context()->fuse));
  fflush(stderr);
}



int main(const int argc,const char *argv[]){
  _pid=getpid();
  special_filename_init();
  IF1(WITH_CANCEL_BLOCKED_THREADS,assert(_pid==gettid()), assert(cg_pid_exists(_pid)));
  if (!realpath(*argv,_self_exe)) DIE("Failed realpath %s",*argv);
  init_mutex();
  init_sighandler(argv[0],(1L<<SIGSEGV)|(1L<<SIGUSR1)|(1L<<SIGABRT),stderr);
  _whenStarted=time(NULL);
  {
    _warning_color[WARN_THREAD]=ANSI_FG_RED;
    _warning_color[WARN_GETATTR]=ANSI_FG_MAGENTA;
    _warning_color[WARN_CHARS]=ANSI_YELLOW;
    _warning_color[WARN_DEBUG]=ANSI_MAGENTA;
    for(int i=0;MY_WARNING_NAME[i];i++) _warning_channel_name[i]=(char*)MY_WARNING_NAME[i];
  }
  int colon=0;
  FOR(i,1,argc) if (STR_EQ_C(argv[i],':')){ colon=i; break;}
  const char *compile_feature="";
#if defined(__has_feature)
#if defined(address_sanitizer) && __has_feature(address_sanitizer)
  compile_feature="With sanitizer ";
#endif
#endif
  fprintf(stderr,ANSI_INVERSE""ANSI_UNDERLINE"This is %s"ANSI_RESET" Version: "ZIPSFS_VERSION"  Compiled: %s "STRINGIZE(__DATE__)" "STRINGIZE(__TIME__)"  PID: "ANSI_FG_WHITE ANSI_BLUE"%d"ANSI_RESET"\n",
          path_of_this_executable(),compile_feature,_pid);
  IF1(WITH_GNU,fprintf(stderr,"gnu_ggnu_get_libc_version: %s\n",gnu_get_libc_version()));
  fprintf(stderr,"Version libfuse: "STRINGIZE(FUSE_MAJOR_VERSION)"."STRINGIZE(FUSE_MINOR_VERSION)" libzip: "STRINGIZE(LIBZIP_VERSION_MAJOR)"."STRINGIZE(LIBZIP_VERSION_MINOR)"\n"
          "MAX_PATHLEN: "STRINGIZE(MAX_PATHLEN)"\n"
          "has_proc_fs: %s\n",yes_no(has_proc_fs()));
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in fprintf */
  ASSERT(S_IXOTH==(S_IROTH>>2));
  static struct fuse_operations xmp_oper={0};
#define S(f) xmp_oper.f=xmp_##f
  S(init);
  S(getattr);
  //  S(access);
  S(utimens);
  S(readlink);
  S(readdir);
  S(symlink); S(unlink);
  S(rmdir); S(mkdir);  S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);
  /* Not needed (opendir) */
  //  IF1(WITH_FUSE_3,S(lseek));
#undef S
  static const struct option l_option[]={{"help",0,NULL,'h'}, {"version",0,NULL,'V'}, {NULL,0,NULL,0}};
  for(int c;(c=getopt_long(argc,(char**)argv,"+bqT:nkhVs:c:l:L:",l_option,NULL))!=-1;){  /* The initial + prevents permutation of argv */
    switch(c){
    case 'V': exit(0);break;
    case 'T': cg_print_stacktrace_test(atoi(optarg)); exit_ZIPsFS();break;
    case 'b': _isBackground=true; break;
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 's': cg_strncpy0(_mkSymlinkAfterStart,optarg,MAX_PATHLEN); break;
    case 'h': ZIPsFS_usage();  return 0;
    case 'l': IF1(WITH_PRELOADFILERAM,if (!preloadfileram_set_maxbytes(optarg)) return 1); break;
    case 'c': IF1(WITH_PRELOADFILERAM,if (!preloadfileram_set_policy(optarg))   return 1); break;
    case 'L': _rlimit_vmemory=cg_atol_kmgt(optarg); break;
    }
  }

  log_msg("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr");
#if ! defined(HAS_RLIMIT) || HAS_RLIMIT
  log_msg("rrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrrr 2222222222");
  static struct rlimit l={0};
  if (_rlimit_vmemory){
    l.rlim_cur=l.rlim_max=_rlimit_vmemory;
    log_msg("Setting rlimit virtual memory to %llu MB \n",(LLU)l.rlim_max>>20);
    if (setrlimit(RLIMIT_AS,&l)) warning(WARN_CONFIG|WARN_FLAG_ERRNO,"setrlimit(RLIMIT_AS,n)","");
  }
  if(MAX_NUM_OPEN_FILES){
    getrlimit(RLIMIT_NOFILE,&l);
    l.rlim_cur=MIN(l.rlim_max,MAX_NUM_OPEN_FILES);
    log_msg("Setting rlimit MAX_NUM_OPEN_FILES to %'d\n",(int)l.rlim_cur);
    if (setrlimit(RLIMIT_NOFILE,&l)) warning(WARN_CONFIG|WARN_FLAG_ERRNO,"setrlimit(RLIMIT_NOFILE,n)","");
  }
#else
  log_warn("The function setrlimit() is not supported. Option -L  and MAX_NUM_OPEN_FILES will be ignored.");
#endif



  if (!getuid() || !geteuid()){
    log_strg("Running ZIPsFS as root opens unacceptable security holes.\n");
    if (_isBackground) DIE("It is only allowed in foreground mode  with option -f.");
    fprintf(stderr,"Do you accept the risks [Enter / Ctrl-C] ?\n");cg_getc_tty();
  }
  if (!colon){ log_error("No colon ':'  found in parameter list\n"); suggest_help(); return 1;}
  if (colon==argc-1){ log_error("Expect mount point after single colon\n"); suggest_help(); return 1;}
  _mnt_l=cg_strlen(realpath(argv[argc-1],_mnt));

  if (!_mnt_l) DIE("realpath(%s): '%s'",argv[argc-1],_mnt);
  {
    struct stat st;
    if (PROFILED(stat)(_mnt,&st)){
      if (_isBackground) DIE("Directory does not exist: %s",_mnt);
      fprintf(stderr,"Going to create non-existing folder %s  [Enter / Ctrl-C] ?\n",_mnt);
      cg_getc_tty();
      cg_recursive_mkdir(_mnt);
    }else{
      if (!S_ISDIR(st.st_mode)) DIE("Not a directory: %s",_mnt);
    }
  }
  { /* dot_ZIPsFS */
    char dot_ZIPsFS[MAX_PATHLEN+1], dirOldLogs[MAX_PATHLEN+1], tmp[MAX_PATHLEN+1];
    {
      {
        char *d=dot_ZIPsFS+strlen(cg_copy_path(dot_ZIPsFS,PATH_DOT_ZIPSFS));
        strcat(d,_mnt); while(*++d) if (*d=='/') *d='_';
      }
      snprintf(dirOldLogs,MAX_PATHLEN,"%s%s",dot_ZIPsFS,"/oldLogs");
      cg_recursive_mkdir(dirOldLogs);
      strcpy(stpcpy(tmp,dot_ZIPsFS),"/PID.TXT");
      fprintf(stderr,"Writing '%s' ... ",tmp);
      FILE *f=fopen(tmp,"w");
      if (f){
        fprintf(f,"%d\n",_pid);
        fclose(f);
        fputs(GREEN_SUCCESS"\n",stderr);
      }else{
        perror(RED_FAIL);
      }
    }
    FOR(i,0,2){
      snprintf(_fWarningPath[i],MAX_PATHLEN,"%s%s",dot_ZIPsFS,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
      struct stat stbuf;
      if (!PROFILED(lstat)(_fWarningPath[i],&stbuf) && stbuf.st_size){ /* Save old logs with a mtime in file name. */
        const time_t t=stbuf.st_mtime;
        struct tm lt;
        localtime_r(&t,&lt);
        snprintf(tmp,MAX_PATHLEN,"%s%s",dirOldLogs,SPECIAL_FILES[i+SFILE_LOG_WARNINGS]);
        strftime(strrchr(tmp,'.'),22,"_%Y_%m_%d_%H:%M:%S",&lt);
        strcat(tmp,".log");
        if (cg_rename(_fWarningPath[i],tmp)) DIE("rename");
        const pid_t pid=fork();
        assert(pid>=0);
        if (pid>0){
          waitpid(pid,NULL,0);
        }else{
          assert(!execlp("gzip","gzip","-f","--best",tmp,(char*)NULL));
          exit(errno);
        }
      }
      _fWarnErr[i]=fopen(_fWarningPath[i],"w");
    }
    warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);
    IF1(WITH_SPECIAL_FILE,static char ctrl_sh[MAX_PATHLEN+1];
        snprintf(ctrl_sh,MAX_PATHLEN,"%s/%s",dot_ZIPsFS,"ZIPsFS_CTRL.sh");
        snprintf(tmp,MAX_PATHLEN,"%s/cachedir",dot_ZIPsFS); mstore_set_base_path(tmp);
        special_file_content_to_file(SFILE_DEBUG_CTRL,SPECIAL_FILES[SFILE_DEBUG_CTRL]=ctrl_sh));
  }
  bool isPreload=false;
  FOR(i,optind,colon){ /* Source roots are given at command line. Between optind and colon */
    if (!*argv[i]) continue;
    if (!strcmp("--preload",argv[i])){ _exists_root_with_preload=isPreload=true; continue;}
    if (_root_n>=ROOTS) DIE("Exceeding max number of ROOTS %d.  Increase macro  ROOTS   in configuration.h and recompile!\n",ROOTS);
    struct rootdata *r=_root+_root_n++;
      r->preload=isPreload;  isPreload=false;
    root_init(i==optind,r,argv[i]);
    if (_root_writable){
      char rp[MAX_PATHLEN];
      RLOOP(j,3){
        stpcpy(stpcpy(rp,_root_writable->rootpath),j==2?DIR_AUTOGEN:  j==1?DIR_INTERNET: DIR_PRELOADFILEDISK_R);
        cg_recursive_mkdir(rp);
        if (!cg_is_dir(rp)) DIE("Failed creating directory %s",rp);
      }
      assert(strlen(_root_writable->rootpath)+sizeof(FILE_CLEANUP_SCRIPT)<sizeof(rp));
      _root_writable_path=strdup_untracked(_root_writable->rootpath);
      _cleanup_script=strdup_untracked(strcat(strcpy(rp,_root_writable_path),FILE_CLEANUP_SCRIPT));
    }
    ht_set_mutex(mutex_dircache,ht_init(&r->dircache_ht,"dircache",HT_FLAG_KEYS_ARE_STORED_EXTERN|12));
    ht_set_mutex(mutex_dircache_queue,ht_init(&r->dircache_queue,"dircache_queue",8));
#if WITH_DIRCACHE || WITH_STAT_CACHE || WITH_TIMEOUT_READDIR
    ht_init_interner_file(&r->dircache_ht_fname,"dircache_ht_fname",16,DIRECTORY_CACHE_SIZE);
    ht_set_mutex(mutex_dircache,&r->dircache_ht_fname);
    ht_set_mutex(mutex_dircache,ht_init_interner_file(&r->dircache_ht_fnamearray,"dircache_ht_fnamearray",HT_FLAG_BINARY_KEY|12,DIRECTORY_CACHE_SIZE));
    mstore_set_mutex(mutex_dircache,mstore_init(&r->dircache_mstore,"dircache_mstore",MSTORE_OPT_MMAP_WITH_FILE|DIRECTORY_CACHE_SIZE));
#endif
  }/* Loop roots */
  log_msg("\n\nMount point: "ANSI_FG_BLUE"'%s'"ANSI_RESET"\n\n",_mnt);
  { /* Storing information per file type for the entire run time */
    mstore_set_mutex(mutex_fhandle,mstore_init(&mstore_persistent,"persistent",0x10000));
    ht_set_mutex(mutex_fhandle,ht_init_interner(&ht_intern_fileext,"ht_intern_fileext",8,4096));
    ht_set_mutex(mutex_validchars,ht_init(&_ht_valid_chars,"validchars",HT_FLAG_NUMKEY|12));
    ht_init_with_keystore_dim(&ht_inodes_vp,"inode_from_virtualpath",16,0x100000|MSTORE_OPT_MMAP_WITH_FILE);
    IF1(WITH_AUTOGEN_OR_CCODE,ht_init(&ht_fsize,"autogen-fsize",HT_FLAG_NUMKEY|9));
  }
  IF1(WITH_ZIPINLINE_CACHE, ht_set_mutex(mutex_dircache,ht_init(&ht_zinline_cache_vpath_to_zippath,"zinline_cache_vpath_to_zippath",HT_FLAG_NUMKEY|16)));
  IF1(WITH_STAT_CACHE,ht_set_mutex(mutex_dircache,ht_init(&stat_ht,"stat",16)));
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  if (check_configuration(argv[argc-1])){
    warning(WARN_CONFIG,"","Non-recommended configuration.");
    if (!strstr(_mnt,"/cgille/") && !_isBackground) fprintf(stderr,"Press enter\n"), cg_getc_tty();
  }
  mkSymlinkAfterStartPrepare();
  if (_isBackground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=_cg_is_none_interactive=true;
  IF1(WITH_AUTOGEN,aimpl_init());
  IF1(WITH_INTERNET_DOWNLOAD,net_local_dir(true));
  /* Begin  Threads */
  foreach_root(r) if (r->remote) root_start_thread(r,PTHREAD_ASYNC,false);
  root_start_thread(_root,PTHREAD_MISC,false);
  /* End  Threads */

  log_msg("Running %s with PID %d. Going to fuse_main() ...\n",argv[0],_pid);
  cg_free(COUNT_MALLOC_TESTING,cg_malloc(COUNT_MALLOC_TESTING,10));
  _fuse_argv[_fuse_argc++]="";
  if (!_isBackground) _fuse_argv[_fuse_argc++]="-f";
  FOR(i,colon+1,argc) _fuse_argv[_fuse_argc++]=argv[i];
  log_print_roots(0);
  const int fuse_stat=fuse_main(_fuse_argc,(char**)_fuse_argv,&xmp_oper,NULL);
  _fuse_started=true;
  log_msg(RED_WARNING" fuse_main returned %d\n",fuse_stat);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS();
}
// napkon      astra zenika Agathe  find
////////////////////////////////////////////////////////////////////////////
// SSMetaData zipinfo //s-mcpb-ms03/slow2/incoming/Z1/Data/30-0089/20230719_Z1_ZW_027_30-0089_Serum_EAD_14eV_3A_OxoIDA_rep_01.wiff2.Zip
//
// DIE   FIXME DIE_DEBUG_NOW   DEBUG_NOW   log_debug_now  log_entered_function log_exited_function directory_print
// ASSERT ASSERT_PRINT
// _GNU_SOURCE PATH_MAX
// SIGILL SIGINT   ST_NOATIME
// HAVE_CONFIG_H  HAS_EXECVPE HAS_UNDERSCORE_ENVIRON HAS_ST_MTIM HAS_POSIX_FADVISE
// https://wiki.smartos.org/
// malloc calloc strdup  free  mmap munmap   readdir opendir
//
// malloc_untracked calloc_untracked  strdup_untracked
// MMAP_INC(...)  MUNMAP_INC(...)  MALLOC_INC(...)  FREE_INC(...)
// cg_mmap ht_destroy cg_strdup cg_free cg_free_null
//
// opendir  locked config_autogen_size_of_not_existing_file crc32
// limits strtoOAk SIZE_POINTER   COUNTm_MSTORE_MALLOC
// strncpy stpncpy  mempcpy strcat _thisPrg
// strrchr autogen_rinfiles
// analysis.tdf-shm analysis.tdf-wal
// lstat stat
//     if (!strcmp("/s-mcpb-ms03.charite.de/incoming/PRO3/Maintenance/202403/20240326_PRO3_LRS_004_MA_HEK500ng-5minHF_001_V11-20-HSoff_INF-G.d.Zip",rp)){  log_zpath("DDDDDDDDDD",zpath);}
// SIGTERM
//  statvfs    f_bsize  f_frsize  f_blocks f_bfree f_bavail
// config_autogen_estimate_filesize   autogen_rinfiles
// stat foreach_fhandle EIO local  fseek seek open
// fdescriptor fdscrptr fHandle fhandle_read_zip  #include "generated_profiler_names.c" LOCK
// mutex_fhandle
// typedef enum false_na_true { FALSE=-1,ZERO,TRUE} false_na_true_t;
//
// usleep stat PATH_MAX  WITH_STAT_CACHE  dircache_directory_from_cache
// ACT_BLOCK_THREAD thread_pretend_blocked LF() when
//  MALLOC_dir_field  MALLOC_fhandle
//  COUNT_AUTOGEN_MALLOC_TXTBUF  COUNT_MALLOC_PRELOADFILERAM_TXTBUF
// WITH_ZIPINLINE_CACHE WITH_ZIPINLINE WITH_DIRCACHE
// COUNT_FHANDLE_CONSTRUCT
// strcat strcpy strncpy stpncpy strcmp README_AUTOGENERATED.TXT realloc memcpy cgille stat_set_dir
// COUNT_TXTBUF_SEGMENT_MMAP  textbuffer_new
// COUNTm_MSTORE_MMAP
//                  COUNTER_TXTBUFSGMT_MALLOC             34             34                 15,275              -15,275 x   COUNT_TXTBUF_SEGMENT_MALLOC
//               COUNT_TXTBUF_SEGMENT_MALLOC             34             34                      0                    0
// advise_cache_in_ram
