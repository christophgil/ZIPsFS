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
// _mnt
#define VFILE_SFX_INFO "@SOURCE.TXT"

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
#include "cg_exec_pipe.c"
#include "cg_download_file.c"
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
#if WITH_FILECONVERSION
#include "ZIPsFS_configuration_fileconversion.h"
#include "ZIPsFS_fileconversion_impl.h"
#endif //WITH_FILECONVERSION
#if WITH_CCODE
#include "ZIPsFS_c.c"
#endif //WITH_CCODE
static char *_mkSymlinkAfterStart, *_mnt_apparent;
static const char *_self_exe, *_mnt, *_dot_ZIPsFS;
static const char *SFILE_REAL_PATHS[SFILE_WITH_REALPATH_NUM];
static const char *SFILE_NAMES[SFILE_NUM], *SFILE_PARENTS[SFILE_NUM];
static int SFILE_PARENTS_L[SFILE_NUM];
static int _fhandle_n=0,_mnt_l=0, _debug_is_readdir;
static rlim_t _rlimit_vmemory=0;
IF1(WITH_PRELOADRAM,static enum enum_when_preloadram_zip _preloadram_policy=PRELOADRAM_RULE);
static ht_t *ht_set_id(const int id,ht_t *ht){
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
static struct mstore mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
static ht_t ht_intern_fileext,_ht_valid_chars, ht_inodes_vp,ht_count_by_ext
  IF1(WITH_FILECONVERSION_OR_CCODE,,ht_fsize)
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

static float _ucpu_usage,_scpu_usage;/* user and system */
static int64_t _preloadram_bytes_limit=3L*1000*1000*1000;
static int _unused_int;
static bool _thread_unblock_ignore_existing_pid, _fuse_started, _isBackground, _exists_root_with_preload;
static bool _logIsSilentFailed,_logIsSilentWarn,_logIsSilentError;
static const char *_fuse_argv[99]={0},
  *_root_writable_path="<path-first-root>",
  *_cleanup_script="<path-first-root>"FILE_CLEANUP_SCRIPT;
static int _fuse_argc;
static root_t _root[ROOTS]={0}, *_root_writable=NULL;
#if WITH_INTERNET_DOWNLOAD
#include "ZIPsFS_internet.c"
#endif //WITH_INTERNET_DOWNLOAD
static const char* rootpath(const root_t *r){
  return r?r->rootpath:NULL;
}
static const char* report_rootpath(const root_t *r){
  return !r?"No root" : r->rootpath;
}
static int rootindex(const root_t *r){
  return !r?-1: (int)(r-_root);
}
static int _rootdata_path_max;
static long _rootmask_preload;
static void root_init(const bool isWritable,root_t *r, const char *path, const char **annotations, const int annotations_n){
  char tmp[MAX_PATHLEN+1];
  if (isWritable){
    if (*path) (_root_writable=r)->writable=true;
  }else if (!*path){
    DIE("Command line argument for rootpath is empty.");
  }
  {
    const int s=cg_leading_slashes(path)-1;
    if ((r->remote=(s>0))) path+=s;
    r->with_timeout=(s>1);
  }
  realpath(path,tmp);
  r->rootpath=r->rootpath_orig=strdup(tmp);
  if (cg_last_char(path)!='/'){
    char *slash=strrchr(tmp,'/');
    assert(slash);
    r->pathpfx=strdup(slash);
    if (isWritable) DIE(RED_ERROR" Please add a trailing slash to the root-path '%s' of the first branch because  the writable branch should not retain the last path component.\n",path);
  }
  r->rootpath_l=cg_strlen(r->rootpath);
  _rootdata_path_max=MAX_int(_rootdata_path_max,(r->rootpath_l+cg_strlen(r->pathpfx)+2));
  if (!r->rootpath_l && r==_root_writable) return;
  if (!r->rootpath_l || !cg_is_dir(r->rootpath))    DIE("Not a directory: '%s'  realpath: '%s'",path,r->rootpath);
  {
    struct statvfs st;
    if (statvfs(r->rootpath_orig,&st)){
      perror(r->rootpath_orig);
      exit_ZIPsFS();
    }
    r->f_fsid=st.f_fsid;
    r->noatime=(st.f_flag&ST_NOATIME);

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
    if (statvfs(r->rootpath_orig,&r->statvfs)){ perror(""); DIE("statvfs");}
    FOR(iTry,0,2){
      *tmp=0;
      if (exec_on_file(EXECF_SILENT,iTry?EXECF_MOUNTPOINT_USING_DF:EXECF_MOUNTPOINT_USING_FINDMNT,tmp,MAX_PATHLEN,r->rootpath_orig)>0){
        r->rootpath_mountpoint=strdup(tmp);
        break;
      }
    }
    fputs(GREEN_DONE"\n",stderr);
  }
  if (r->preload_flags){
    if (!_root_writable) warning(WARN_CONFIG,r->rootpath,"Setting  --preload to root, but no writable root.");
    if (_root_writable==r) DIE("Do not set --preload for  first root.");
    _rootmask_preload|=(1<<rootindex(r));
  }
  root_read_properties(r,annotations,annotations_n);
  if (r->pathpfx){
    char *p=strdup(r->pathpfx);
    if (strstr(p,"//")) DIE(RED_ERROR" The path prefix '%s' of root '%s' contains double slash.",p,rootpath(r));
    RLOOP(i,(r->pathpfx_l=strlen(p))) if (p[i]=='/') p[i]=0;
    r->pathpfx_slash_to_null=p;
  }
  {
    struct stat st;
    if (stat(path,&st)){ log_errno("stat '%s'",path); DIE("");}
    r->st_dev=st.st_dev;
  }
}
#define CAPACITY (l[1])
#define L (l[0])
static char **split_paths_by_colon(char *v, int l[2], char **ss){
  for(const char *t;(t=strtok(v,":"));v=NULL){
    if (CAPACITY<=L+1) ss=realloc(ss,SIZE_POINTER*(CAPACITY=16+2*CAPACITY));
    if (*t) ss[L++]=strdup(t);
    ss[L]=NULL;
  }
  return ss;
}
#undef CAPACITY
#undef L


static yes_zero_no_t parse_key_numvalue(const char *line, const char *key, int *value){
  const int key_l=strlen(key);
  if (key_l && strncmp(line,key,key_l)) return ZERO;
  if (!line[key_l]){
    *value=1;
    return YES;
  }
  return line[key_l]!='='?ZERO:1==sscanf(line+key_l+1,"%d",value)?YES:NO;
}

static void root_read_properties(root_t *r,const char **annotations, const int annotations_n){
  static char *line=NULL;
  if (!r){ free(line); line=NULL; return;}
  r->check_response_path=r->rootpath_orig;
  r->check_response_seconds=ROOT_RESPONSE_SECONDS;
  r->check_response_seconds_max=ROOT_RESPONSE_TIMEOUT_SECONDS;
  r->stat_timeout_seconds=STAT_TIMEOUT_SECONDS;
  r->readdir_timeout_seconds=READDIR_TIMEOUT_SECONDS;
  r->openfile_timeout_seconds=OPENFILE_TIMEOUT_SECONDS;
  char tmp[MAX_PATHLEN+1], propertypath[MAX_PATHLEN+1];
  stpcpy(stpcpy(propertypath,r->rootpath_orig),EXT_ROOT_PROPERTY);
  size_t capacity=0;
  int exclude_l[2], starts_l[2],ff_decompress[2]={0};
  FOR(fromArgv,0,2){
    FILE *f=NULL;
    if (!fromArgv  && !(f=fopen(propertypath,"r"))) continue;
    for(int iLine=0; fromArgv?(iLine<annotations_n):(getline(&line,&capacity,f)!=-1); iLine++){
      if (fromArgv){
        if (!iLine) log_verbose("Root '%s' annotations at cli:%d",r->rootpath_orig,annotations_n);
        const int l=strlen(annotations[iLine]);
        if (!l) continue;
        ASSERT(*annotations[iLine]=='@');
        if (l>capacity && !(line=realloc(line,(capacity=l)))) DIE("realloc");
        strcpy(line,annotations[iLine]+1);
      }else{
        if (!iLine) log_verbose("Going to read '%s'\n",propertypath);
      }
      char *k;
      if ((k=strchr(line,'\n'))) *k=0;
      if ((k=strchr(line,'#')))  *k=0;
      k=line; while(isspace(*k))  k++;
      k[cg_last_nospace_char(k)+1]=0;
      if (!*k) continue;
      bool match=false, has_error=false;
      {
        char *v=NULL;
#define C(K)   v=NULL; if (!strncmp(k,K,sizeof(K)-1)){ v=sizeof(K)-1+k; match=true;}
        C("statfs=");
        if (v){
          const int fields=sscanf(v,"%d %d %"STRINGIZE(MAX_PATHLEN)"s",&r->check_response_seconds,&r->check_response_seconds_max,tmp);
          log_verbose("%s:%d  seconds=%d  seconds-max=%d  url='%s'\n    '%s'\n\n",propertypath,iLine+1,  r->check_response_seconds,r->check_response_seconds_max,tmp,line);
          if (!r->check_response_seconds || !r->check_response_seconds_max)  DIE("%s:%d  Expected  statfs=/path/to/root  seconds   seconds-max\n    '%s'\n\n",propertypath,iLine+1,line);
          if (fields==3){
            if (cg_is_dir(cg_path_expand_tilde(NULL,MAX_PATHLEN,tmp))) r->check_response_path=strdup_untracked(tmp);
            else DIE("%s:%d  Not a directory '%s'",propertypath,iLine+1,r->check_response_path);
          }
          if (!r->with_timeout) warning(WARN_CONFIG,propertypath,"The line %d  '%k...' will be ignored, since the root-path does not start with triple-slash.",iLine+1,k);
        }
        C(ROOT_PROPERTY_PATH_DENY_PATTERNS"=");  if (v) r->exclude_vp=split_paths_by_colon(v,exclude_l,r->exclude_vp);
        C(ROOT_PROPERTY_PATH_ALLOW_PREFIXES"="); if (v) r->starts_vp= split_paths_by_colon(v,starts_l, r->starts_vp);
        C(ROOT_PROPERTY_PATH_PREFIX"=");
#undef C
        if (v){
          v[cg_pathlen_ignore_trailing_slash(v)]=0;
          *tmp='/'; stpcpy(stpcpy(tmp+(*v!='/'),v),r->pathpfx);
          free_untracked((void*)r->pathpfx);
          r->pathpfx=strdup(tmp);
        }
      }
      {
        int value=0; /* Properties with numeric values */
#define N(expr,key,code)  switch(parse_key_numvalue(expr,key,&value)){ case ZERO:break; case YES:code;match=true;break;case NO: has_error=true;break;}
        N(k,ROOT_PROPERTY_STAT_TIMEOUT     ,r->stat_timeout_seconds=value);
        N(k,ROOT_PROPERTY_READDIR_TIMEOUT  ,r->readdir_timeout_seconds=value);
        N(k,ROOT_PROPERTY_OPENFILE_TIMEOUT ,r->openfile_timeout_seconds=value);
        N(k,ROOT_PROPERTY_ONE_FILE_SYSTEM  ,r->no_cross_device=(value==1));
        N(k,ROOT_PROPERTY_FOLLOW_SYMLINKS  ,r->follow_symlinks=(value==1));
        N(k,ROOT_PROPERTY_WORM             ,r->worm=(value==1));
        N(k,ROOT_PROPERTY_IMMUTABLE        ,r->immutable=(value==1));
        if (!strncmp(k,ROOT_PROPERTY_PRELOAD,7)){
          N(k+7,"", ff_decompress[value!=0]|=PRELOAD_YES);
          if (k[7]=='-'){
            FOR(iCompress,COMPRESSION_NIL+1,COMPRESSION_NUM){
              const char *ext=cg_compression_file_ext(iCompress,NULL);
              N(k+8,ext+(*ext=='.'),ff_decompress[value!=0]|=(1<<iCompress));
            }
          }
        }
#undef N
        if (has_error){  if(fromArgv) fprintf(stderr,RED_ERROR"%s:%d\n",propertypath,iLine+1);  DIE(RED_ERROR"'%s'",k);}
      }
      if (has_error || !match){
        if (fromArgv) log_error("CLI property '@%s'",line); else log_error("%s:%d  '%s'",propertypath,iLine+1,line);
        if (!match){
          fprintf(stderr,"Unknown property name. Supported properties:\n");
          FOREACH_CSTRING(p,ARRAY_ROOT_PROPERTY)    fprintf(stderr,"   %s= ...\n",*p);
          FOREACH_CSTRING(p,ARRAY_ROOT_PROPERTY_01) fprintf(stderr,"   %s=1\n",*p);
        }
        cg_getc_tty();
      }
    }
    if (f) fclose(f);
  }
  r->decompress_flags|=ff_decompress[1];
  r->decompress_flags&=~ff_decompress[0];
  if (r->decompress_flags){
    r->preload_flags|=r->decompress_flags|PRELOAD_YES;
    _exists_root_with_preload=true;
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
#if WITH_PRELOADDISK
#include "ZIPsFS_preloadfiledisk.c"
#endif //WITH_PRELOADDISK
#include "ZIPsFS_async.c"
// ---
#if WITH_PRELOADRAM
#include "ZIPsFS_filesystem_info.c"
#include "ZIPsFS_preloadfileram.c"
#include "ZIPsFS_ctrl.c"
#include "ZIPsFS_special_file.c"
#endif // WITH_PRELOADRAM
#include "ZIPsFS_log.c"
// ---
#if WITH_ZIPINLINE
#include "ZIPsFS_zip_inline.c"
#endif //WITH_ZIPINLINE
// ---
#if WITH_ZIPENTRY_PLACEHOLDER
#include "ZIPsFS_zipentry_placeholder.c"
#endif
// ---
#if WITH_FILECONVERSION
#include "ZIPsFS_fileconversion.c"
#define fileconversion_filecontent_append_nodestroy(ff,s,s_l)  _fileconversion_filecontent_append(TXTBUFSGMT_NO_FREE,ff,s,s_l)
#define fileconversion_filecontent_append(ff,s,s_l)          _fileconversion_filecontent_append(0,ff,s,s_l)
#define fileconversion_filecontent_append_munmap(ff,s,s_l)   _fileconversion_filecontent_append(TXTBUFSGMT_MUNMAP,ff,s,s_l)
#define C(ff,s,s_l)  _fileconversion_filecontent_append(TXTBUFSGMT_NO_FREE,ff,s,s_l)
#define H(ff,s,s_l)  _fileconversion_filecontent_append(0,ff,s,s_l)
#define M(ff,s,s_l)  _fileconversion_filecontent_append(TXTBUFSGMT_MUNMAP,ff,s,s_l)
#include "ZIPsFS_configuration_fileconversion.c"
#undef M
#undef C
#undef H
#include "ZIPsFS_fileconversion_impl.c"
#endif //WITH_FILECONVERSION
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
// directory_t //
//////////////////////
static zpath_t *directory_init_zpath(directory_t *dir,const zpath_t *zpath){
  /* Note: Must not allocate on heap */
#define X(field,type) dir->core.field=dir->_stack_##field
  if (dir->files_capacity<DIRECTORY_DIM_STACK){
    dir->files_capacity=DIRECTORY_DIM_STACK;
    XMACRO_DIRECTORY_ARRAYS();
  }
#undef X
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
static void directory_destroy(directory_t *d){
  if (d && !d->dir_is_dircache && !d->dir_is_destroyed){
    d->dir_is_destroyed=true;
#define X(field,type) if(d->core.field!=d->_stack_##field) cg_free_null(COUNT_MALLOC_dir_field,d->core.field);
    XMACRO_DIRECTORY_ARRAYS();
#undef X
    mstore_destroy(&d->filenames);
  }
}
/////////////////////////////////////////////////////////////
/// Read directory
////////////////////////////////////////////////////////////
static void directory_ensure_capacity(directory_t *d, const int min, const int newCapacity){
  assert(DIR_RP(d));
  ASSERT(!d->dir_is_dircache);
  ASSERT_NOT_ASSIGNED(d);
  struct directory_core *dc=&d->core;
  //log_entered_function("files_capacity:%d  files_l:%d  fname: %p",d->files_capacity,dc->files_l,&dc->fname);
  if (min>d->files_capacity || dc->fname==NULL){
    assert(min>DIRECTORY_DIM_STACK);
    // _cg_realloc_array(const int id,const int size1AndOpt,const void *pOld, const size_t nOld, const size_t nNew)
    d->files_capacity=newCapacity;
    assert(newCapacity>=min);
#define O(f) (dc->f==d->_stack_##f?REALLOC_ARRAY_NO_FREE:0)
#define X(f,type)  if (dc->f) dc->f=cg_realloc_array(COUNT_MALLOC_dir_field,sizeof(type)|O(f),dc->f,dc->files_l,newCapacity)
    XMACRO_DIRECTORY_ARRAYS();
#undef O
#undef X
  }
  //log_exited_function("capacity: %d fname: %p",dir->files_capacity,&dc->fname);
}
static void directory_add(uint8_t flags,directory_t *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
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
  dc->fname[L++]=(char*)s;
#undef L
}

////////////
/// stat ///
////////////
static void stat_set_dir(struct stat *s){
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

static bool _stat_direct(const int opt_filldir_findrp, struct stat *st,const char *rp, const int rp_l,  root_t *r, const char *callerFunc){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!rp_l) return false;
  const int res=lstat(rp,st);
  inc_count_by_ext(rp,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS);
  if (res){
    *st=empty_stat;
    return false;
  }
  ASSERT(st->st_ino!=0);
  ASSERT(r);
  IF1(WITH_STAT_CACHE, if(r) stat_to_cache(opt_filldir_findrp,st,rp,rp_l,r,0));
  return true;
}/*stat_direct*/
static bool stat_from_cache_or_direct_or_async(const int opt_filldir_findrp,const char *rp, const int rp_l,struct stat *st,root_t *r){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (r){
    IF1(WITH_STAT_CACHE, if (stat_from_cache(opt_filldir_findrp,st,rp,r)) return true);
    if (r->remote) return async_stat(opt_filldir_findrp,rp,rp_l,st,r);
  }
  return stat_direct(opt_filldir_findrp,st,rp,rp_l,r);
}
/////////////////////////////////////////////////////////////////////////////////////
/// Using the option -s, ZIPsFS can be restarted while it is in production        ///
/// The new instance  will use a different mount point.                           ///
/// Software is accessing ZIPsFS via Symlink and not directly the mountpoint      ///
/////////////////////////////////////////////////////////////////////////////////////
static void mkSymlinkAfterStartPrepare(){
  if (_mkSymlinkAfterStart){
    _mkSymlinkAfterStart[cg_pathlen_ignore_trailing_slash(_mkSymlinkAfterStart)]=0;
    log_verbose("Command option -s:   Going to create symlink '%s' --> '%s' ...",_mkSymlinkAfterStart,_mnt);
    if (*_mkSymlinkAfterStart=='/'){
      fprintf(stderr,RED_WARNING": "ANSI_FG_BLUE"%s"ANSI_RESET" is an absolute path. You might be unable to export the file tree with NFS and Samba.\n",_mkSymlinkAfterStart);
      if (!_isBackground){ fputs("Press Enter to continue anyway!\n",stderr); cg_getc_tty();}
    }
    if (!is_installed_curl()){ fputs("curl is not installed\n",stderr); cg_getc_tty();}
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
/// The zpath_t is used to identify the real path from the virtual path               ///
/// All strings like virtualpath are stacked in strgs                                        ///
/// strgs is initially on stack and may later go to heap.                                    ///
/// A new one is created with zpath_newstr() and one or several calls to zpath_strncat()     ///
/// The length of the created string is finally obtained with zpath_commit()                 ///
////////////////////////////////////////////////////////////////////////////////////////////////

static const char *key_from_rp(const bool internalize,const char *rp, const int rp_l, int *key_l, ht_hash_t *hash, root_t *r){
  ASSERT(rp);
  ASSERT(*rp);
  ASSERT(r!=NULL);
  const int skip=r->rootpath_l;
  ASSERT(rp_l>=skip);
  //log_debug_now("rp:%s rp_l:%d  r:%s skip:%d",rp,rp_l,r->rootpath,skip);
  ASSERT(!strncmp(rp,r->rootpath,r->rootpath_l));
  const char *key=rp+skip;
  *key_l=rp_l-skip;
  if (!*hash) *hash=hash32(key,*key_l);
  return internalize ?internalize_rp(key, *key_l, *hash,r):key;
}


static const char *internalize_rp(const char *rp,const int rp_l, ht_hash_t hash, root_t *r){
  //log_entered_function("rp:'%s' %s %s",rp, success_or_fail(!strncmp(rp,r->rootpath,r->rootpath_l)), success_or_fail(!r->pathpfx_l||!strncmp(rp+r->rootpath_l,r->pathpfx,r->pathpfx_l)));
  //log_entered_function("rp:'%s' ",rp);
  cg_thread_assert_locked(mutex_dircache);
  return ht_intern(&_root->ht_int_rp,rp,rp_l,hash?hash:hash32(rp,rp_l),HT_MEMALIGN_FOR_STRG);
}

static int zpath_strlen(const zpath_t *zpath,int s){
  return s==0?0:strlen(zpath->strgs+s);
}
static MAYBE_INLINE fHandle_t* fhandle_at_index(int i){
  ASSERT_LOCKED_FHANDLE();
  static fHandle_t *_fhandle[FHANDLE_BLOCKS];
#define B _fhandle[i>>FHANDLE_LOG2_BLOCK_SIZE]
  fHandle_t *block=B;
  if (!block){
    block=B=cg_calloc(COUNTm_FHANDLE_ARRAY_MALLOC,FHANDLE_BLOCK_SIZE,sizeof(fHandle_t));
    assert(block!=NULL);
  }
  return block+(i&(FHANDLE_BLOCK_SIZE-1));
#undef B
}
static void  zpath_set_atime(const zpath_t *zpath){

  if (zpath && zpath->root && zpath->root->writable  && zpath->root->noatime){
    //log_entered_function("'%s'",RP());
    struct stat st;
    if (stat(RP(),&st)){
      log_errno("stat '%s'",RP());
    }else if (st.st_atime>time(NULL)){
      // log_verbose("Not updating atime because current atime is in the future: '%s'",RP());
    }else{
      cg_file_set_atime(RP(),&st,0);
    }
  }
}

static int zpath_newstr(zpath_t *zpath){
  assert(zpath!=NULL);
  const int n=(zpath->current_string=++zpath->strgs_l);
  zpath->strgs[n]=0;
  return n;
}
static bool zpath_strncat(zpath_t *zpath,const char *s,int len){
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
static int zpath_commit_hash(const zpath_t *zpath, ht_hash_t *hash){
  const int l=zpath->strgs_l-zpath->current_string;
  if (hash) *hash=hash32(zpath->strgs+zpath->current_string,l);
  return l;
}
static void zpath_set_realpath(zpath_t *zpath, const char *rp){
  zpath->strgs_l=zpath->realpath;
  zpath_strcat(zpath,rp);
  ZPATH_COMMIT_HASH(zpath,realpath);
  zpath->stat_vp=zpath->stat_rp=empty_stat;
}
static void _zpath_assert_strlen(const char *fn,const char *file,const int line,zpath_t *zpath){
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
static void zpath_reset_keep_VP(zpath_t *zpath){
#define x_l_hash(x) x=x##_l=x##_hash
  zpath->entry_path=zpath->entry_path_l=x_l_hash(zpath->virtualpath_without_entry)=x_l_hash(zpath->realpath)=0;
  zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  VP0_L()=EP_L()=zpath->zipcrc32=0; /* Reset. Later probe a different realpath */
  zpath->root=NULL;
  zpath->strgs_l=zpath->virtualpath+VP_L();
  zpath->flags=zpath->flags&ZP_KEEP_NOT_RESET_MASK;
}


static void zpath_init(zpath_t *zpath, const virtualpath_t *vipa){
#define C(f)   zpath->f=vipa->f
  ASSERT(cg_strlen(vipa->vp)>=vipa->vp_l);
  assert(zpath!=NULL);
  assert(vipa!=NULL);
  assert(vipa->vp!=NULL);
  memset(zpath,0,sizeof(zpath_t));
  zpath->virtualpath=zpath->strgs_l=1;  /* To distinguish from virtualpath==0  meaning not defined we use 1*/
  zpath->virtualpath_hash=hash32(vipa->vp,vipa->vp_l);
  zpath->virtualpath_l=vipa->vp_l;
  zpath_strcat(zpath,vipa->vp);
  zpath->strgs[zpath->virtualpath+vipa->vp_l]=0;
  zpath_reset_keep_VP(zpath);
  IF1(WITH_PRELOADDISK,C(preloadpfx));
  ASSERT(vipa->vp_l==VP_L());
  ASSERT(cg_strlen(VP())>=VP_L());
  C(dir);
  C(flags);
  C(zipfile_l);
  C(zipfile_cutr);
  C(zipfile_append);
#undef C
}
//static bool zpath_stat(zpath_t *zpath,root_t *r){
static bool zpath_stat(const int opt_filldir_findrp, zpath_t *zpath){
  if (!zpath) return false;
  root_t *r=zpath->root;
  ASSERT(r);
  //log_entered_function("%s root:%s  ",VP(),rootpath(r));
#define S zpath->stat_rp
  bool ok=S.st_ino;
  if (!ok){
    ok=r?stat_from_cache_or_direct_or_async(opt_filldir_findrp,RP(),RP_L(),&S,r): !stat(RP(),&S);
    if (ok) zpath->stat_vp=S;
    IF1(WITH_PRELOADDISK, if (!ok && path_with_gz_exists(zpath,r)) ok=true);
    if (ok){
      if (zpath->dir==DIR_PRELOADED_UPDATE) zpath->stat_vp.st_size=2048;
      //if (!(ZPF(ZP_TRY_ZIP))) zpath->stat_vp.st_ino=make_inode(S.st_ino,r,zpath->is_decompressed&&zpath->is_decompressed!=(1<<COMPRESSION_NIL)?1:0,RP());
      if (!(ZPF(ZP_TRY_ZIP))) zpath->stat_vp.st_ino=make_inode(S.st_ino,r,zpath->is_decompressed>COMPRESSION_NIL?1:0,RP());
    }
  }
#undef S
  return ok;
}


#define warning_zip(path,e,txt){\
    const int ze=!e?0:zip_error_code_zip(e), se=!e?0:zip_error_code_system(e);\
    char s[1024];*s=0;  if (se) strerror_r(se,s,1023);\
    warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH,path,"%s    sys_err: %s %s zip_err: %s %s",txt,!e?"e is NULL":!se?"":cg_error_symbol(se),s, !ze?"":error_symbol_zip(ze), !ze?"":zip_error_strerror(e));}

///////////////////////////////////////////
/// Store file size for generated files ///
///////////////////////////////////////////
#if WITH_FILECONVERSION_OR_CCODE
static off_t fsize_from_hashtable(ino_t inode){
  lock(mutex_fhandle);
  const ht_entry_t *e=htentry_fsize_for_inode(inode,false);
  const off_t size=e?(off_t)e->value:-1;
  unlock(mutex_fhandle);
  return size;
}
static void fsize_to_hashtable(const char *vp, const int vp_l, const off_t size){
  lock(mutex_fhandle);
  htentry_fsize(vp,vp_l,true)->value=(void*)size;
  unlock(mutex_fhandle);
}
#endif //WITH_FILECONVERSION_OR_CCODE


/****************************************************************************************************************************/
/* Is the virtualpath a zip entry?                                                                                          */
/* Normally append will be ".Content" and cutr will be 0.                                                                   */
/* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name: append will be empty and cutr will be -4; */
/****************************************************************************************************************************/
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *cutr, char *append[]){
  ASSERT(cg_strlen(vp)>=vp_l);
  const char *b=vp;
  int ret=0;
  for(int i=4;i<=vp_l;i++){
    if (i==vp_l || vp[i]=='/'){
      if ((ret=config_virtual_dirpath_to_zipfile(b,vp+i,append))!=INT_MAX){
        *cutr=-ret;
        ret+=i;
        break;
      }
      if (vp[i]=='/') b=vp+i+1;
    }
  }
  return ret==INT_MAX?0:ret;
}
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add(directory_t,...)
// Returns true on success.
////////////////////////////////////////////////////////////
//static bool readdir_from_cache_zip_or_filesystem(directory_t *dir){log_entered_function("%s",DIR_RP(dir)); bool ok=_readdir_from_cache_zip_or_filesystem(dir);log_exited_function("%s ok: %d",DIR_RP(dir),ok);  return ok;}
static bool readdir_from_cache_zip_or_filesystem(const int opt_filldir_findrp,directory_t *dir){
  const char *rp=DIR_RP(dir);
  if (!rp || !*rp) return false;
#if WITH_DIRCACHE
  {
    bool success=false;
    LOCK_NCANCEL(mutex_dircache,success=dircache_directory_from_cache(dir));
    if (success) return true;
  }
#endif //WITH_DIRCACHE
  if (!readdir_async(dir)) return false;
  const bool doCache=(opt_filldir_findrp&FILLDIR_FROM_OPEN) ||
    config_advise_cache_directory_listing(((DIR_ROOT(dir) && DIR_ROOT(dir)->remote)?ADVISE_DIRCACHE_IS_REMOTE:0)|
                                          (dir->dir_zpath.dir==DIR_PLAIN?ADVISE_DIRCACHE_IS_DIRPLAIN:0)|
                                          (DIR_IS_ZIP(dir)?ADVISE_DIRCACHE_IS_ZIP:0),
                                          rp,DIR_RP_L(dir), dir->dir_zpath.stat_rp.ST_MTIMESPEC);
  config_exclude_files(rp,DIR_RP_L(dir),dir->core.files_l, dir->core.fname,dir->core.fsize);
  IF1(WITH_DIRCACHE,if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(dir)));
  return true;
}
#define DIRECTORY_PREAMBLE(isZip)    if (DIR_IS_ZIP(dir)!=isZip) return false;   char *rp; LOCK(mutex_dircache, rp=DIR_RP(dir); dir->core.files_l=0)
#define CONTAINS_PALCEHOLDER(n,zip) IF1(WITH_ZIPENTRY_PLACEHOLDER,dir->has_file_containing_placeholder=dir->has_file_containing_placeholder || strchr(n,PLACEHOLDER_NAME))
static bool readdir_from_zip(directory_t *dir){
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
        directory_add(S.comp_method?DIRENT_IS_COMPRESSEDZIPENTRY:0,dir,0,S.name,S.size,S.mtime,S.crc);
      }
    }
#undef S
    unlock(mutex_dircache);
  }
  my_zip_close(zip,rp);
  return true;
}


static bool readir_from_filesystem(directory_t *dir){
  DIRECTORY_PREAMBLE(false);
  DIR *d=opendir(rp);      IF_LOG_FLAG(LOG_OPENDIR) log_verbose(" opendir('%s') ",rp);
  inc_count_by_ext(rp,d?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL);
  if (!d){ log_errno("opendir: %s",rp); return false; }
  root_t *r=DIR_ROOT(dir);
  root_update_time(r,PTHREAD_ASYNC);
  struct dirent *de;  while((de=readdir(d))){
    root_update_time(r,PTHREAD_ASYNC);
    const char *n=de->d_name;
    CONTAINS_PALCEHOLDER(n,zip);
    if (config_do_not_list_file(rp,n,strlen(n))) continue;
    int isdir=0;
    {
#if defined(HAS_DIRENT_D_TYPE) && !HAS_DIRENT_D_TYPE /* OpenSolaris */
      stpcpy(stpcpy(stpcpy(s.s,DIR_RP(dir)),"/"),n);
      IF1(WITH_STAT_CACHE, if (dir->when_readdir_call_stat_and_store_in_cache) is_dir=dir_entry_to_stat_cache(DIR_RP(dir),n,r,time(NULL)));
      isdir=cg_is_dir(s.s)?1:-1;
#else /* BSD, Linux, MacOSX */
      isdir=(de->d_type==DT_DIR)?1:-1;
#endif
    }
    LOCK(mutex_dircache, directory_add(isdir==1?DIRENT_ISDIR: 0,dir,de->d_ino,n,0,0,0));
  }/* While */
  closedir(d);
  IF1(WITH_STAT_CACHE,   if (dir->when_readdir_call_stat_and_store_in_cache) dir_entries_to_stat_cache(dir,r));
  return true;
}
static bool append_realpath_warnings_errors(zpath_t *zpath){
  const int id=zpath->flags&ZP_MASK_SFILE;
  if (!id) return false;
  zpath_strcat(zpath,SFILE_REAL_PATHS[id]);
  if (id==SFILE_LOG_WARNINGS||id==SFILE_LOG_ERRORS) fflush(_fWarnErr[id==SFILE_LOG_ERRORS]);
  return true;
}
/****************************************************************/
/*  The following functions are used to search for a real path  */
/*  for a given virtual path,                                   */
/*  Returns true on success                                      */
/****************************************************************/

static bool test_realpath_pfx(const bool dirFileconversion,  int opt_filldir_findrp, zpath_t *zpath, root_t *r){
  const char *vp=VP(), *vp0=VP0_L()?VP0():vp;
  const int vp_l=VP_L(), vp0_l=VP0_L()?VP0_L():VP_L();
  FOREACH_CSTRING(t,r->starts_vp)  if (cg_path_equals_or_is_parent(*t,strlen(*t),vp,vp_l)) goto filter_ok;
 filter_ok:
  FOREACH_CSTRING(t,r->exclude_vp) if (cg_path_equals_or_is_parent(*t,strlen(*t), vp,vp_l)) return false;
  if (r->pathpfx_l && !cg_path_equals_or_is_parent(r->pathpfx, r->pathpfx_l,vp0,vp0_l)) return false;
  zpath->root=r;

  if (r->worm) opt_filldir_findrp|=FINDRP_IS_WORM;
  if (r->immutable) opt_filldir_findrp|=FINDRP_IS_IMMUTABLE;
  zpath->realpath=zpath_newstr(zpath); /* realpath is next string on strgs_l stack */
  if (!append_realpath_warnings_errors(zpath)){
    zpath_strcat(zpath,rootpath(r));
    zpath_strcat(zpath,vp0+r->pathpfx_l);
    if (dirFileconversion) zpath_strcat(zpath,DIR_FILECONVERSION);
  }
  ZPATH_COMMIT_HASH(zpath,realpath);
  zpath->stat_rp=empty_stat;
  if (ZPF(ZP_OVERFLOW)  || !RP_L() || !zpath_stat(opt_filldir_findrp,zpath)) return false;
  if (r->no_cross_device && r->st_dev!=zpath->stat_rp.st_dev) return false;
#define M zpath->stat_rp.st_mode
  if (zpath->dir==DIR_PRELOADED_UPDATE && (!(M&(S_IFREG|S_IFDIR)) || (M&S_IFREG)&&!(M&S_ISVTX)))  return false;
  if (r->follow_symlinks && !ZPF(ZP_NOT_EXPAND_SYMLINKS) && S_ISLNK(M) && zpath_expand_symlinks(zpath)){
    zpath->stat_rp.st_ino=0;
    zpath_stat(opt_filldir_findrp,zpath);
  }
#undef M
  if (ZPF(ZP_TRY_ZIP)){
    if (!cg_endsWithZip(RP(),0)){ IF_LOG_FLAG(LOG_REALPATH) log_verbose("!cg_endsWithZip rp: %s\n",RP()); return false; }
    if (EP_L() && filler_readdir_zip(opt_filldir_findrp,zpath,NULL,NULL,NULL)) return false; /* This sets the file stat of zip entry */
    zpath->flags|=ZP_IS_ZIP;
  }
  return true;
}
static bool test_realpath(const int opt_filldir_findrp,const int zpath_flags,zpath_t *zpath, root_t *r){
  assert(r!=NULL);
  bool ok=false;
  //log_entered_function("%s r:%s",VP(),rootpath(r));
  zpath->flags|=zpath_flags;
  IF1(WITH_FILECONVERSION, if (r==_root_writable && zpath->dir==DIR_FILECONVERSION  && test_realpath_pfx(true,opt_filldir_findrp,zpath,r)) ok=true);
  ok=ok || test_realpath_pfx(false,opt_filldir_findrp,zpath,r);
  if (!ok && (zpath_flags&ZP_RESET_IF_NEXISTS)) zpath_reset_keep_VP(zpath);
  return ok;
}

static bool zpath_expand_symlinks(zpath_t *zpath){
  char target[PATH_MAX+1];
  //log_entered_function("RP:%s",RP());
  if (cg_readlink_absolute(true,RP(),target)) return false;
  root_t *parent_root=NULL;
  foreach_root(r) if (cg_path_equals_or_is_parent(r->rootpath,r->rootpath_l,target,strlen(target))){ parent_root=r; break;}
  const bool ok=parent_root || config_allow_expand_symlink(RP(),target);
  if (ok){
    zpath_set_realpath(zpath,target);
    if (parent_root) zpath->root=parent_root;
  }
  //log_exited_function("RP:'%s'    target:'%s'  parent_root:%s ok:%d",RP(),target,parent_root?parent_root->rootpath:"",ok);
  return ok;
}
/* Uses different approaches and calls test_realpath */
/* Initially, only zpath->virtualpath is defined. */
static bool find_realpath_for_root(const int opt_filldir_findrp,zpath_t *zpath,root_t *r){
  if (r){
    if (r!=_root_writable && (zpath->dir==DIR_FIRST_ROOT || zpath->dir==DIR_PRELOADED_UPDATE)) return false;
    if (!wait_for_root_timeout(r)) return false;
  }
  if (VP_L()){
    if (zpath->zipfile_l){
      zpath->virtualpath_without_entry=zpath_newstr(zpath);
      zpath_strncat(zpath,VP(),zpath->zipfile_l);
      zpath_strcat(zpath,zpath->zipfile_append);
      ZPATH_COMMIT_HASH(zpath,virtualpath_without_entry);
      zpath->entry_path=zpath_newstr(zpath);
      {
        const int pos=VP0_L()+zpath->zipfile_cutr+1-cg_strlen(zpath->zipfile_append);
        if (pos<VP_L()) zpath_strcat(zpath,VP()+pos);
      }
      if (ZPF(ZP_OVERFLOW)) return false;
      EP_L()=zpath_commit(zpath);
      zpath_assert_strlen(zpath);
      if (test_realpath(opt_filldir_findrp,(zpath->dir==DIR_PLAIN?0:ZP_TRY_ZIP)|ZP_RESET_IF_NEXISTS,zpath,r)){
        if (!EP_L()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
        return true;
      }
    }
    IF1(WITH_ZIPINLINE, const yes_zero_no_t i=find_realpath_try_inline_rules(zpath,zpath->zipfile_append,r); if (i) return i==YES);
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  return test_realpath(opt_filldir_findrp,ZP_RESET_IF_NEXISTS,zpath,r);
} /*find_realpath_nocache*/
static long search_file_which_roots(const char *vp,const int vp_l,const bool path_starts_with_fileconversion){
  if (path_starts_with_fileconversion){
#if WITH_FILECONVERSION
    struct fileconversion_files ff={0};
    struct_fileconversion_files_init(&ff,vp,vp_l-(ENDSWITH(vp,vp_l,".log")?4:0));
    const bool ok=fileconversion_realinfiles(&ff);
    struct_fileconversion_files_destroy(&ff);
    if (ok) return 1;
#endif //WITH_FILECONVERSION
  }
  return config_search_file_which_roots(vp,vp_l);
}
static bool find_realpath_roots_by_mask(const int opt_filldir_findrp,zpath_t *zpath,const long roots){
  zpath_reset_keep_VP(zpath);
  IF1(WITH_ZIPINLINE_CACHE, yes_zero_no_t ok=zipinline_find_realpath_any_root(zpath, roots); if (ok) return ok==YES);
  foreach_root(r) if (roots&(1<<rootindex(r))) if (find_realpath_for_root(opt_filldir_findrp,zpath,r)) return true;
  return false;
}/*find_realpath_roots_by_mask*/

static bool find_realpath_any_root( int opt_filldir_findrp,zpath_t *zpath,const root_t *onlyThisRoot){
  //if (opt_filldir_findrp&FILLDIR_FROM_OPEN) DIE_DEBUG_NOW("FILLDIR_FROM_OPEN");
  if (zpath->dir==DIR_PLAIN) opt_filldir_findrp|=FINDRP_IS_PFXPLAIN;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,const int trans=(opt_filldir_findrp&FINDRP_NOT_TRANSIENT_CACHE)?0:transient_cache_find_realpath(zpath); if (trans) return trans==1);
  const long roots=(onlyThisRoot?(1<<rootindex(onlyThisRoot)):-1) & search_file_which_roots(VP(),VP_L(), ZPATH_IS_FILECONVERSION());
  const bool found=find_realpath_roots_by_mask(opt_filldir_findrp,zpath, roots);
  if (found) assert(zpath->realpath!=0);
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, if (!onlyThisRoot||found){ LOCK(mutex_fhandle,transient_cache_store(found?zpath:NULL,VP(),VP_L());}));
  if (!found IF1(WITH_FILECONVERSION,&&!ZPATH_IS_FILECONVERSION()) && !onlyThisRoot && !config_not_report_stat_error(VP(),VP_L()) IF1(WITH_INTERNET_DOWNLOAD, && !net_is_internetfile(VP(),VP_L()))){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,VP(),"Not found");
  }
  //log_exited_function("VP %s  %s   RP:%s",VP(),success_or_fail(found),found?RP():"");
  // cppcheck-suppress resourceLeak
  return found;  // Why resourceLeak ???
} /* find_realpath_any_root */
static bool _find_realpath_other_root(zpath_t *zpath){
  assert(zpath->root);
  assert(zpath->realpath);
  const off_t size0=zpath->stat_rp.st_size;
  ASSERT(zpath->stat_rp.st_ino);
  const root_t *prev=NULL;
  foreach_root(r){
    if (prev==zpath->root){
      zpath->strgs_l=zpath->realpath;
      zpath->realpath=0;
      if (test_realpath(0,0,zpath,r) && size0==zpath->stat_rp.st_size)  return true;
    }
    prev=r;
  }
  return false;
} /*find_realpath_any_root*/
static bool find_realpath_other_root(zpath_t *zpath){
  cg_thread_assert_not_locked(mutex_fhandle);
  //log_entered_function("VP: '%s'",VP());
  LOCK_N(mutex_fhandle,zpath_t zp=*zpath);
  const bool found=_find_realpath_other_root(&zp);
  if (found){ LOCK(mutex_fhandle,*zpath=zp);  }
  //log_exited_function("VP: '%s' RP: '%s' EP: '%s'   found: %d",VP(),RP(),EP(), found);
  return found;
}
/////////////////////////////////////////////////////////////////////////////////////
/// Data associated with file handle.
// Motivation: When the same file is accessed from two different programs,
/// We see different fi->fh
/// We use this as a key to obtain an instance of  fHandle_t
///
/// Conversely, fuse_get_context()->private_data cannot be used.
/// It returns always the same pointer address even for different file handles.
///////////////////////////////////////////////////////////////////////////////////
static void fhandle_init(fHandle_t *d, const zpath_t *zpath){
  *d=FHANDLE_EMPTY;
  pthread_mutex_init(&d->mutex_read,NULL);
  d->zpath=*zpath;
  d->filetypedata=filetypedata_for_ext(VP(),d->zpath.root);
  d->flags|=FHANDLE_ACTIVE; /* Important; This must be the last assignment */
}
static fHandle_t* _fhandle_create_locked(const int flags,const uint64_t fh, const zpath_t *zpath){
  fHandle_t *d=NULL;
  { foreach_fhandle_also_emty(ie,e) if (!e->flags){ d=e; break;}} /* Use empty slot */
  if (!d){ /* Append to list */
    if (_fhandle_n>=FHANDLE_MAX){ warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_ERROR,VP(),"Excceeding FHANDLE_MAX");return NULL;}
    d=fhandle_at_index(_fhandle_n++);
  }
  fhandle_init(d,zpath);
  d->fh=fh;
  d->flags=flags;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, transient_cache_activate(d));
  IF1(WITH_SPECIAL_FILE,if (!(flags&FHANDLE_SPECIAL_FILE)) preloadram_infer_from_other_handle(d));
  d->flags|=FHANDLE_ACTIVE;
  COUNTER1_INC(COUNT_FHANDLE_CONSTRUCT);
  return d;
}


/*******************************/
/* uint64_t fuse_file_info->fh.
   Either  serves as ID of fHandle_t instance */
/*******************************/
#define LOG2_FD_ZIP_MIN 20
#define FD_ZIP_MIN (1<<LOG2_FD_ZIP_MIN)
static uint64_t next_fh(){
  static uint64_t fh=FD_ZIP_MIN;
  if (++fh==UINT64_MAX) fh=FD_ZIP_MIN;
  return fh;
}
static fHandle_t* fhandle_create(const int flags, uint64_t *fh, const zpath_t *zpath){
  cg_thread_assert_not_locked(mutex_fhandle);
  *fh=next_fh();
  while(true){
    LOCK_N(mutex_fhandle, fHandle_t *d=_fhandle_create_locked(flags,*fh,zpath)); /* zpath is now stored in fHandle_t */
    if (d) return d;
    if (zpath) log_verbose("Going to sleep and retry fhandle_create %s ...",VP());
    usleep(1000*1000);
  }
}
static bool fhandle_currently_reading_writing(const fHandle_t *d){
  ASSERT_LOCKED_FHANDLE();
  if (!d) return false;
  assert(d->is_busy>=0);
  return d->is_busy>0;
}
static void fhandle_destroy(fHandle_t *d){
  ASSERT_LOCKED_FHANDLE();
  if (d){
    d->flags|=FHANDLE_DESTROY_LATER;
    if (!fhandle_currently_reading_writing(d)) fhandle_destroy_now(d);
    else warning(WARN_FLAG_ERROR|WARN_FLAG_ONCE_PER_PATH,D_VP(d),"fhandle_currently_reading_writing()");
  }
}
static void fhandle_destroy_now(fHandle_t *d){
  ASSERT_LOCKED_FHANDLE();
#if WITH_PRELOADRAM
  if (atomic_load(&d->is_preloading)>0) return; /* Currently not being copied to RAM */
  if (!is_preloadram_shared_with_other(d) &&          /* Another fHandle_t could take care of struct preloadram instance later */
      !preloadram_try_destroy(d)) return;             /* struct preloadram is NULL or has been destroyed */
#endif //WITH_PRELOADRAM
  IF1(WITH_FILECONVERSION,const int fd=d->fd_real; d->fd_real=0;if (fd) close(fd));
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,transient_cache_destroy(d));
  fhandle_zip_fclose(d);
  my_zip_close(d->zip_archive,D_RP(d));
  pthread_mutex_destroy(&d->mutex_read);
  IF1(WITH_FILECONVERSION, fc_maybe_reset_atime_in_future(d));
  IF1(WITH_EVICT_FROM_PAGECACHE,if (!fhandle_find_identical(d)) maybe_evict_from_filecache(0,D_RP(d),D_RP_L(d),D_EP(d),D_EP_L(d)));
  *d=FHANDLE_EMPTY;
  COUNTER2_INC(COUNT_FHANDLE_CONSTRUCT);
}
static void fhandle_destroy_those_that_are_marked(void){
  ASSERT_LOCKED_FHANDLE();
  foreach_fhandle(id,d){
    if ((d->flags&FHANDLE_DESTROY_LATER)  && !fhandle_currently_reading_writing(d)) fhandle_destroy_now(d);
  }
}
static fHandle_t* fhandle_get(const virtualpath_t *vipa,const uint64_t fh){
  ASSERT_LOCKED_FHANDLE();
  //log_msg(ANSI_FG_GRAY" fHandle %d  %lu\n"ANSI_RESET,op,fh);
  fhandle_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(vipa->vp);
  foreach_fhandle(id,d){
    if (fh==d->fh && D_VP_HASH(d)==h && !strcmp(vipa->vp,D_VP(d))) return d;
  }
  return NULL;
}
static bool fhandle_find_identical(const fHandle_t *d){
  foreach_fhandle(ie,e) if (fhandle_virtualpath_equals(d,e) && !(e->flags&FHANDLE_DESTROY_LATER)) return true;
  return false;
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t next_inode(void){
  static ino_t seq=1UL<<63;
  return seq++;
}
static ino_t make_inode(const ino_t inode0,root_t *r, const int entryIdx,const char *rp){
  const static int SHIFT_FSID=42,SHIFT_ENTRY=(SHIFT_FSID+LOG2_FILESYSTEMS);
  const ino_t fsid=r?r->seq_fsid:LOG2_FILESYSTEMS; /* Better than rootindex(r) */
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,rp,"inode0 is zero");
  if (inode0<(1L<<SHIFT_FSID) && entryIdx<(1<<(63-SHIFT_ENTRY))){  // (- 63 46)
    const ino_t ino= inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| (fsid<<SHIFT_FSID);
    return ino;
  }else{
    ht_t *ht=&r->ht_inodes;
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
  static ht_entry_t *e;
  LOCK_NCANCEL(mutex_inode, e=ht_get_entry(&ht_inodes_vp,vp,vp_l,0,true));
  if (!e->value) e->value=(void*)next_inode();
  return (ino_t)e->value;
}
/* ******************************************************************************** */
/* *** Zip *** */
static int zipentry_placeholder_expand(char *u,const char *orig, const char *rp, const directory_t *dir){
  if (!orig) return 0;
  int len=cg_pathlen_ignore_trailing_slash(orig);
  if (len>=MAX_PATHLEN){ warning(WARN_STR|WARN_FLAG_ERROR,u,"Exceed_MAX_PATHLEN"); return 0;}
  stpcpy(u,orig)[len]=0;
  IF1(WITH_ZIPENTRY_PLACEHOLDER, if (!dir||!dir->has_file_containing_placeholder) len=zipentry_placeholder_expand2(u,rp));
  return len;
}
////////////////////////////////////////////////////////////////////////////////////////
/// List entries in ZIP file directory.                                              ///
/// Directories are not explicitely contained in dir->core                           ///
/// Consequently, path components are successively removed from the right side.      ///
/// Parameter  filler:  Not NULL If called for directory listing                     ///
///                     NULL for running stat for a specific ZIP entry               ///
////////////////////////////////////////////////////////////////////////////////////////


static void filler_add(const int opt_filldir_findrp,fuse_fill_dir_t filler,void *buf, const char *name, int name_l,const struct stat *st, ht_t *no_dups){
  IF1(WITH_FILECONVERSION,if(opt_filldir_findrp&FILLDIR_FILECONVERSION)fileconversion_filldir(filler,buf,name,st,no_dups);else)
    {
      if (!name_l) name_l=strlen(name);
      char namebuf[name_l+1];
      if (name[name_l]){ /* Trim to length name_l */
        cg_strncpy0(namebuf,name,name_l);
        name=namebuf;
      }
      if (ht_only_once(no_dups,name,name_l)){
        assert_validchars(VALIDCHARS_FILE,name,name_l);
        filler(buf,name,st,0 COMMA_FILL_DIR_PLUS);
      }
     #define X(lower,upper) if ((opt_filldir_findrp&(1<<COMPRESSION_##upper)) && cg_endsWith(0,name,name_l,"."#lower,sizeof(#lower))) filler_add(0,filler,buf,name,name_l-sizeof(#lower),st,no_dups);
      IF1(WITH_PRELOADDISK_DECOMPRESS,if (opt_filldir_findrp){ XMACRO_COMPRESSION();});
#undef X
    }
}



static int filler_readdir_zip(const int opt_filldir_findrp,zpath_t *zpath,void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  ASSERT(zpath->dir!=DIR_PLAIN);
  char ep[MAX_PATHLEN+1]; /* This will be the entry path of the parent dir */
  const int ep_l=filler?EP_L():MAX_int(0,cg_last_slash(EP()));
  memcpy(ep,EP(),ep_l); ep[ep_l]=0;
  const char *lastComponent=EP()+ep_l+(ep_l>0);
  const int lastComponent_l=EP_L()-(ep_l+(ep_l>0));
  if(!filler && !EP_L()) return 0; /* When the virtual path is a Zip file then just report success */
  //  if (!zpath_stat(zpath,zpath->root)) return ENOENT;
  if (!zpath_stat(opt_filldir_findrp,zpath)) return ENOENT;
  directory_t mydir={0}, *dir=&mydir; mydir.debug=true;
  directory_init_zpath(dir,zpath);
  if (!readdir_from_cache_zip_or_filesystem(opt_filldir_findrp,dir)) return ENOENT;
  IF1(WITH_ZIPINLINE_CACHE,LOCK(mutex_dircache,to_cache_vpath_to_zippath(dir)));
  char u[MAX_PATHLEN+1]; /* entry path expanded placeholder */
  struct directory_core dc=dir->core;
  int idx=0;
  FOR(i,0,dc.files_l){
    if (cg_empty_dot_dotdot(dc.fname[i])) continue;
    int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),dir);
    bool isdir=false;
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
        ASSERT(dir->files_capacity>=dc.files_l);
        if (Nth0(dc.fflags,i)&DIRENT_IS_COMPRESSEDZIPENTRY) zpath->flags|=ZP_IS_COMPRESSEDZIPENTRY;
        zpath->zipcrc32=Nth0(dc.fcrc,i);
        directory_destroy(dir);
        return 0;
      }
      filler_add(opt_filldir_findrp,filler,buf,n,n_l,st,no_dups);
    }
  }
  directory_destroy(dir);
  return filler?0:ENOENT;
}/*filler_readdir_zip*/
static bool filler_readdir(const int opt_filldir_findrp,zpath_t *zpath, void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  if (!RP_L()) return false;
  if (ZPF(ZP_TRY_ZIP)) return filler_readdir_zip(opt_filldir_findrp,zpath,buf,filler,no_dups);
  if (!zpath->stat_rp.st_ino) return true;
  ASSERT(zpath->root!=NULL);
  char dirname_from_zip[MAX_PATHLEN+1];
  ASSERT(zpath!=NULL);
  directory_t dir={0};  directory_init_zpath(&dir,zpath);
  if (readdir_from_cache_zip_or_filesystem(opt_filldir_findrp,&dir)){
    char u[MAX_PATHLEN+1]; /*buffers for unsimplify_fname() */
    const struct directory_core dc=dir.core;
    FOR(i,0,dc.files_l){
      if (cg_empty_dot_dotdot(dc.fname[i])) continue;
      int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),&dir);
      //IF1(WITH_INTERNET_DOWNLOAD, if (_root_writable && (opt_filldir_findrp&FILLDIR_STRIP_NET_HEADER)) u_l=net_filename_from_header_file(u,u_l));
      if (!u_l || 0==(opt_filldir_findrp&FILLDIR_FILECONVERSION) && no_dups && ht_get(no_dups,u,u_l,0)) continue;
      IF1(WITH_ZIPINLINE,if (zpath->dir!=DIR_PLAIN && config_skip_zipfile_show_zipentries_instead(u,u_l) && readdir_inline_from_cache(opt_filldir_findrp,zpath,u,buf,filler,no_dups)) continue);
      struct stat st;
      if ((opt_filldir_findrp&FILLDIR_FILES_S_ISVTX) && (!cg_stat_parent_and_file(RP(),RP_L(),u,u_l, &st)|| S_IFREG==(st.st_mode&(S_ISVTX|S_IFREG)))){
        //log_debug_now("Skip %s %s",RP(),u);
        continue;
      }
      stat_init(&st,(Nth0(dc.fflags,i)&DIRENT_ISDIR)?-1:Nth0(dc.fsize,i),NULL);
      st.st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,0,RP());
      if (!config_do_not_list_file(RP(),u,u_l)){
        *dirname_from_zip=0;
        const bool also_show_zip_file_itself=zpath->dir!=DIR_PLAIN && config_zipfilename_to_virtual_dirname(dirname_from_zip,u,u_l);
        if (*dirname_from_zip){
          stat_set_dir(&st);
          filler_add(opt_filldir_findrp,filler,buf,dirname_from_zip,0,&st,no_dups);
          if (also_show_zip_file_itself) filler_add(opt_filldir_findrp,filler,buf,u,u_l,&st,no_dups); // cppcheck-suppress knownConditionTrueFalse
        }else{
          filler_add(opt_filldir_findrp|(zpath->dir==DIR_PLAIN?0:zpath->root->preload_flags),filler,buf,u,u_l,&st,no_dups);
        }
      }
    }
    directory_destroy(&dir);
  }
  return true;
}
static int minus_val_or_errno(int res){ return res==-1?-errno:-res;}
static int xmp_releasedir(const char *path, struct fuse_file_info *fi){ return 0;} // cppcheck-suppress [constParameterCallback]
static int xmp_statfs(const char *path, struct statvfs *st){
  return minus_val_or_errno(statvfs(_root->rootpath,st));
}
/************************************************************************************************/
static int mk_parentdir_if_sufficient_storage_space(const char *rp){
  const int slash=cg_last_slash(rp);
  if (slash<=0) return EINVAL;
  if (!cg_recursive_mk_parentdir(rp)){ warning(WARN_OPEN|WARN_FLAG_ERRNO,rp,"failed cg_recursive_mk_parentdir"); return EPERM;}
  char parent[PATH_MAX+1]; cg_stpncpy0(parent,rp,slash);
  struct statvfs st;
  if (statvfs(parent,&st)){ warning(WARN_OPEN|WARN_FLAG_ERRNO,parent,"Going return EIO"); return EIO;}
  const long free=st.f_frsize*st.f_bavail, total=st.f_frsize*st.f_blocks;
  if (config_has_sufficient_storage_space(rp,free,total)) return 0;
  warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH,parent,"%s: config_has_sufficient_storage_space(Available=%'ld GB, Total=%'ld GB)",strerror(ENOSPC),free>>30,total>>30);
  return ENOSPC;
}

/******************************************************************************/
/*  Create real path in writable branche.                                     */
/*  Return EACCES if file should not be overwritten                           */
/*  If the parent path exists in any root, then create it in _root_writable.  */
/******************************************************************************/
static int realpath_mk_parent(char *rp, const virtualpath_t *vip){
  if (!_root_writable) return EACCES;/* Only first root is writable */
  if (config_not_overwrite(vip->vp,vip->vp_l)){
    bool found;FIND_REALPATH(vip);
    if (found && zpath->root>0) return EACCES;
  }
  assert(_root_writable->rootpath_l+vip->vp_l<MAX_PATHLEN);
  const int slash=cg_last_slash(vip->vp);
  stpcpy(stpcpy(rp,_root_writable->rootpath),vip->vp);
  if (slash<=0) return 0;
  char parent[slash+1]; cg_strncpy0(parent,vip->vp,slash);
  NEW_VIRTUALPATH(parent);
  bool found;FIND_REALPATH(&vipa);
  return !found? ENOENT:mk_parentdir_if_sufficient_storage_space(rp);
}

/********************************************************************************/
// FUSE FUSE 3.0.0rc3 The high-level init() handler now receives an additional struct fuse_config pointer that can be used to adjust high-level API specific configuration options.
#define DO_LIBFUSE_CACHE_STAT 0
#define EVAL(a) a
#define EVAL2(a) EVAL(a)
static void *xmp_init(struct fuse_conn_info *conn IF1(WITH_FUSE_3,,struct fuse_config *cfg)){
  //void *x=fuse_apply_conn_info_opts;  //cfg-async_read=1;
#if WITH_FUSE_3
  cfg->use_ino=1;
  IF1(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(DO_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
#endif
  log_verbose(GREEN_SUCCESS"FUSE filesystem initialized at '%s'\n",_mnt);
  if (_mkSymlinkAfterStart){
    struct stat st;
    if (lstat(_mkSymlinkAfterStart,&st)) perror(_mkSymlinkAfterStart);
    if (((S_IFREG|S_IFDIR)&st.st_mode) && !(S_IFLNK&st.st_mode)){
      warning(WARN_MISC,""," Cannot make symlink %s =>%s  because %s is a file or dir\n",_mkSymlinkAfterStart,_mnt,_mkSymlinkAfterStart);
      DIE("");
    }
  }
  return NULL;
}

static void setSpecialFile(const int opt, const enum enum_special_files id,virtualpath_t *vipa){
#define D vipa->dir
  const int dir_l=SFILE_PARENTS_L[id];
  D=SFILE_PARENTS[id];
  vipa->dir_l=dir_l;
  if (!strcmp(SFILE_NAMES[id],vipa->vp+dir_l+1)){
    vipa->special_file_id=id;
    if (id==SFILE_LOG_WARNINGS || id==SFILE_LOG_ERRORS) vipa->flags|=id;
  }
  if (opt&ZP_VP_STRIP_PFX){ vipa->vp+=dir_l; vipa->vp_l-=dir_l;}
  if (D==DIR_PRELOADDISK_R||D==DIR_PRELOADDISK_RC||D==DIR_PRELOADDISK_RZ){
    vipa->preloadpfx_l=dir_l;
    vipa->preloadpfx=D;
  }
  vipa->flags|=opt;
  #undef D
}


static void init_special_files(){
#define X(opt,name,parent,id) SFILE_NAMES[id]=name; SFILE_PARENTS[id]=parent; SFILE_PARENTS_L[id]=sizeof(parent)-1;
  XMACRO_SPECIAL_FILES();
#undef X
  if (cg_uid_is_developer() && cg_file_exists(__FILE__)){
    const int slash=cg_last_slash(__FILE__);
    char tmp[PATH_MAX+1];strcpy(tmp,__FILE__);
    RLOOP(i,ZIPsFS_configuration_NUM){
      char *e=stpcpy(tmp+slash+1,ZIPSFS_CONFIGURATION_S[i]);
      strcpy(e,".h"); if (cg_file_exists(tmp)) continue;
      strcpy(e,".c"); if (cg_file_exists(tmp)) continue;
      DIE(RED_ERROR"Not found '%s'",tmp);
    }
  }
}

static int virtualpath_init(virtualpath_t *vipa, const char *vpath, char *buf){
  ASSERT(vpath!=NULL);
  *vipa=empty_virtualpath;
  vipa->vp_l=strlen((vipa->vp=vpath+(*vpath=='/'&&!vpath[1])));
  if (PATH_STARTS_WITH_DIR_ZIPsFS(vipa->vp)){
    vipa->dir=DIR_ZIPsFS;
#define X(opt,name,parent,id) if (name) if (parent==DIR_ZIPsFS || PATH_STARTS_WITH(vipa->vp,vipa->vp_l,parent,sizeof(parent)-1)) setSpecialFile(opt,id,vipa);
    XMACRO_SPECIAL_FILES();
#undef X
  }
  //log_debug_now("vpath:'%s' dir:'%s'  special_file_id:%d ",vpath,vipa->dir, vipa->special_file_id);
  if (64+vipa->vp_l+_rootdata_path_max>MAX_PATHLEN) return ENAMETOOLONG;
  if (vipa->dir!=DIR_PLAIN) vipa->zipfile_l=virtual_dirpath_to_zipfile(vipa->vp, vipa->vp_l, &vipa->zipfile_cutr, &vipa->zipfile_append);
  *buf=0;
  if (ENDSWITH(vipa->vp,vipa->vp_l,VFILE_SFX_INFO)){ cg_strncpy0(buf,vipa->vp,vipa->vp_l-(sizeof(VFILE_SFX_INFO)-1)); vipa->flags|=ZP_IS_PATHINFO; }
  if (cg_strcasestr(vpath,"$NOCSC$")){ /* Windows no-client-side-cache */
    if (!*buf) strcpy(buf,vipa->vp);
    cg_str_replace(0,buf,0, "$NOCSC$",7,"",0);
  }
  if (*buf){
    vipa->vp_l=strlen(buf);
    vipa->vp=buf;
  }
  //  if ((vipa->dir==DIR_FIRST_ROOT||vipa->dir==DIR_FILECONVERSION) && PATH_STARTS_WITH_DIR_ZIPsFS(vipa->vp)) return ENOENT;
  //if ((vipa->flags&ZP_VP_STRIP_PFX) && PATH_STARTS_WITH_DIR_ZIPsFS(vipa->vp)) return ENOENT;
  return 0;
}
static int virtualpath_error(const virtualpath_t *vipa,const int create_or_del){
  if (create_or_del==1 && !_root_writable) return EACCES;
  if (vipa->vp_l==0 && vipa->preloadpfx_l) return create_or_del==1?EEXIST:EPERM;
  if (vipa->dir && create_or_del){
    if (vipa->dir==DIR_INTERNET && create_or_del==1) return EEXIST;
    if (create_or_del==1 && vipa->special_file_id>SFILE_BEGIN_IN_RAM) return EACCES;
  }
  return 0;
}
/*  Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now  additional struct fuse_file_info pointer (which, may be NULL even if the file is currently open) */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10)
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 1
#else
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 0
#endif
static int xmp_getattr(const char *vpath, struct stat *st IF1(WITH_XMP_GETATTR_FUSE_FILE_INFO,,struct fuse_file_info *fi_or_null)){
  //LOG_ENTERED_F();
  const int res=_xmp_getattr(vpath,st);
  LOG_FUSE_RES(vpath,res);
  //log_exited_function("%s res: %d",vpath,res);
  return res;
}



static int _xmp_getattr(const char *vpath, struct stat *st){
  FUSE_PREAMBLE(vpath);
  IF1(WITH_SPECIAL_FILE,if (special_file_set_stat(st,&vipa)) return 0);
  bool vp_is_root_pfx=false;
  foreach_root(r) if (IS_VP_IN_ROOT_PFX(r,&vipa)) vp_is_root_pfx=true;
  if (vipa.dir && vipa.vp_l==vipa.dir_l || vp_is_root_pfx){
    stat_init(st,-1,NULL);
    st->st_ino=inode_from_virtualpath(vipa.vp,vipa.vp_l);
    time(&st->st_mtime);
    return 0;
  }

  IF1(WITH_CCODE, if (c_getattr(st,&vipa)) return 0);
  bool found;FIND_REALPATH(&vipa);

  if (found){
    *st=zpath->stat_vp;
  }else{
    if (vipa.dir!=DIR_PRELOADED_UPDATE){
    IF1(WITH_INTERNET_DOWNLOAD, if (net_getattr(st,&vipa))           return 0);
    IF1(WITH_FILECONVERSION,           if (fileconversion_getattr(st,zpath,&vipa)) return 0);
    }
    er=ENOENT;
  }
  inc_count_by_ext(vipa.vp,er?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS);
  IF1(DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (er) debug_track_false_getattr_errors(vipa.vp,vipa.vp_l));
  if (config_file_is_readonly(vipa.vp,vipa.vp_l)) st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  return -er;
}/*xmp_getattr*/
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10) /* Not sure when parameter introduced */
#define WITH_UTIMENS_FUSE_FILE_INFO 1
#else
#define WITH_UTIMENS_FUSE_FILE_INFO 0
#endif
static int xmp_utimens(const char *vpath, const struct timespec ts[2]  IF1(WITH_UTIMENS_FUSE_FILE_INFO,,struct fuse_file_info *fi_not_used)){
  FUSE_PREAMBLE_W(1,vpath);
  bool found; FIND_REALPATH(&vipa);
  er=!found?ENOENT:   zpath->root!=_root_writable?EPERM: utimensat(0,RP(),ts,AT_SYMLINK_NOFOLLOW);
  return minus_val_or_errno(er);   /* don't use utime/utimes since they follow symlinks */
}
static int xmp_readlink(const char *vpath, char *buf, size_t size){
  FUSE_PREAMBLE(vpath);
  bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
  if (!found) return -ENOENT;
  const int n=readlink(RP(),buf,size-1);
  return n==-1?-errno: (buf[n]=0);
}
static int xmp_unlink(const char *vpath){
  FUSE_PREAMBLE_W(-1,vpath);
  bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
  LOG_FUSE_RES(vpath,found);
  return !found?-ENOENT: !ZPATH_ROOT_WRITABLE()?-EACCES:  minus_val_or_errno(unlink(RP()));
}
static int xmp_rmdir(const char *vpath){
  FUSE_PREAMBLE_W(-1,vpath);
  bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
  LOG_FUSE_RES(vpath,found);
  return !ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(rmdir(RP()));
}


/***************************************************************************************************************/
/* Time consuming processes should be performed in xmp_read() and not in xmp_open().                           */
/* We identify  cases of time consuming file content generation here in xmp_open(). */
/* In those cases we   create an fHandle_t instance.                               */
/* Upon  1st invocation of  xmp_read(), the fHandle_t object is subjected to function fhandle_prepare_in_RW().  */
/* This is when the file content is generated and stored in RAM or on disk                                       */
/***************************************************************************************************************/
static int open_for_reading(const virtualpath_t *vipa, struct fuse_file_info *fi){
  NEW_ZIPPATH(vipa);
  uint64_t fh=IF01(WITH_SPECIAL_FILE,0,special_file_file_content_to_fhandle(zpath,vipa->special_file_id)); /* Only case where file content is generated in xmp_open() */
  if (!fh){ /* ID for fHandle_t  */
    int ff=0;
#define N (!ff) /* cppcheck-suppress-macro knownConditionTrueFalse */
    IF1(WITH_CCODE,             if (N && config_c_open(C_FLAGS_FROM_ZPATH(),VP(),VP_L())) ff=FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_CCODE);
    if (N){
      if (find_realpath_any_root(FILLDIR_FROM_OPEN,zpath,NULL)  IF1(WITH_FILECONVERSION,&&!fileconversion_remove_if_not_up_to_date(zpath))){
        IF1(WITH_PRELOADDISK, if (N && (is_preloaddisk_zpath(zpath) || vipa->dir==DIR_PRELOADED_UPDATE)) ff=FHANDLE_PREPARE_ONCE_IN_RW);
        IF1(WITH_PRELOADRAM,  if (N && _preloadram_policy!=PRELOADRAM_NEVER && (ZPF(ZP_IS_ZIP) || preloadram_advise(zpath,0))) ff=FHANDLE_WITH_PRELOADRAM);
      }else{
        IF1(WITH_INTERNET_DOWNLOAD, if (N && net_is_internetfile(VP(),VP_L())) ff=FHANDLE_PREPARE_ONCE_IN_RW);
#if WITH_FILECONVERSION
        if (N && _realpath_fileconversion && ZPATH_IS_FILECONVERSION()){
          struct fileconversion_files FF={0};
          struct_fileconversion_files_init(&FF,vipa->vp,vipa->vp_l);
          if (fileconversion_realinfiles(&FF)>=0){
            LOCK(mutex_fhandle, fileconversion_zpath_init(zpath,vipa));
            ff=FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_FILECONVERSION;
          }
          struct_fileconversion_files_destroy(&FF);
        }
#endif //WITH_FILECONVERSION
      }
    }
#undef N
    if (ff) fhandle_create(ff|FHANDLE_PREPARE_ONCE_IN_RW,&fh,zpath);
  }
  if (fh){ fi->fh=fh; return 0;}
  zpath_set_atime(zpath);
  const int fd=async_openfile(zpath,fi->flags); /* POSIX file descriptor */
  if (fd>0){  fi->fh=fd; return 0;} // && (!has_proc_fs() || cg_check_path_for_fd("_xmp_open",RP(),fd))
  if (!config_not_report_stat_error(vipa->vp,vipa->vp_l)) warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"open:  fd=%d",fd);
  return errno?-errno:-1;
}/*xmp_open*/
static int xmp_open(const char *vpath, struct fuse_file_info *fi){
  //log_entered_function("vpath:%s",vpath);
  errno=0;
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE_W((fi->flags&(O_WRONLY||O_RDWR|O_APPEND|O_CREAT))?1:0,vpath);
  int res;
  if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))){
    res=-create_or_open(&vipa,0775,fi);
  }else{
    res=open_for_reading(&vipa,fi);
  }
  IF_LOG_FLAG_OR(LOG_OPEN,res!=0) log_exited_function("%s res: %d",vpath,res);
  return res;
}
static int xmp_truncate(const char *vpath, off_t size IF1(WITH_FUSE_3,,struct fuse_file_info *fi)){
  FUSE_PREAMBLE_W(1,vpath);
  int res;
  IF1(WITH_FUSE_3,if (fi) res=ftruncate(fi->fh,size); else)
    {
      bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
      res=!ZPATH_ROOT_WRITABLE()?EACCES: found?truncate(RP(),size):ENOENT;
    }
  return minus_val_or_errno(res);
}
/***********/
/* Readdir */
/***********/
/** unsigned int cache_readdir:1; FOPEN_CACHE_DIR Can be filled in by opendir. It signals the kernel to  enable caching of entries returned by readdir(). */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION,3,5)  /* FUSE 3.5 Added a new cache_readdir flag to fuse_file_info to enable caching of readdir results. */
#define WITH_XMP_READDIR_FLAGS 1
#else
#define WITH_XMP_READDIR_FLAGS 0
#endif
static int xmp_readdir(const char *vpath, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi IF1(WITH_XMP_READDIR_FLAGS,,enum fuse_readdir_flags flags)){
  const int res=_xmp_readdir(vpath,buf,filler,offset,fi);
  LOG_FUSE_RES(vpath,res);
  inc_count_by_ext(vpath,res?COUNTER_READDIR_FAIL:COUNTER_READDIR_SUCCESS);
  return res;
}
static int _xmp_readdir(const char *vpath, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
  (void)offset;(void)fi;
  ht_t no_dups={0}; ht_set_id(HT_MALLOC_without_dups,ht_init_with_keystore_dim(&no_dups,"xmp_readdir_no_dups",8,4096));
  no_dups.keystore->mstore_counter_mmap=COUNT_MSTORE_MMAP_NODUPS;
  no_dups.ht_counter_malloc=COUNT_HT_MALLOC_NODUPS;
  FUSE_PREAMBLE(vpath);
  int opt=((vipa.dir==DIR_PRELOADED_UPDATE)?FILLDIR_FILES_S_ISVTX:0);
  NEW_ZIPPATH(&vipa);
#define A(n) filler_add(0,filler,buf,n,0,NULL,&no_dups);
  bool ok=false;
  const int vp_l=VP_L();
  {
    int opt_rp=0;
    foreach_root(r){ /* FINDRP_FILECONVERSION_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
      if ((vipa.dir==DIR_FIRST_ROOT||vipa.dir==DIR_PRELOADED_UPDATE) && r!=_root_writable) continue;
      if (vp_l<r->pathpfx_l && IS_VP_IN_ROOT_PFX(r,&vipa)){
        ok=true;
        filler_add(0,filler,buf,r->pathpfx_slash_to_null+vp_l+1,0,NULL,&no_dups);
      }else if (find_realpath_any_root(opt_rp,zpath,r)){
        //          r->decompress_flags?FILLDIR_STRIP_GZ:0
        opt_rp|=FINDRP_NOT_TRANSIENT_CACHE; /* Transient cache only once */
        filler_readdir(opt,zpath,buf,filler,&no_dups);
        IF1(WITH_FILECONVERSION, if (ZPATH_IS_FILECONVERSION()) filler_readdir(FILLDIR_FILECONVERSION,zpath,buf,filler,&no_dups));
        ok=true;
        IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,if (ZPF(ZP_FROM_TRANSIENT_CACHE))break); /*Performance*/
        if (config_readir_no_other_roots(RP(),RP_L())) break; /*Performance*/
      }
    }
  }
  FOR(i,0,SFILE_NUM){
    if (SFILE_NAMES[i] && vipa.dir && (vp_l==vipa.dir_l || !vp_l)  && vipa.dir==SFILE_PARENTS[i]) A(SFILE_NAMES[i]);
  }
  IF1(WITH_CCODE, if (c_readdir(zpath,buf,filler,NULL)) ok=true);
  if (!(vipa.flags&ZP_VP_STRIP_PFX) && vipa.dir==DIR_ZIPsFS  && vipa.vp_l==DIR_ZIPsFS_L){ /* Childs of folder ZIPsFS */
#define N(dir) A((dir)+DIR_ZIPsFS_L+1)
    N(DIR_FILECONVERSION); N(DIR_PRELOADDISK_R); N(DIR_PRELOADDISK_RC);  N(DIR_PRELOADDISK_RZ);  N(DIR_PRELOADED_UPDATE); N(DIR_INTERNET); N(DIR_FIRST_ROOT); N(DIR_PLAIN);
#undef N
    ok=true;
  }
  //  if (!vipa.vp_l && !(vipa.flags&ZP_VP_STRIP_PFX)) A((DIR_ZIPsFS)+1);
  if (!vipa.vp_l && !vipa.dir) A((DIR_ZIPsFS)+1);
  //if (!vipa.vp_l)log_debug_now(" vipa.dir: '%s'",vipa.dir);

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
  FUSE_PREAMBLE(vpath);
  if (!(er=realpath_mk_parent(real_path,&vipa))) er=mkdir(real_path,mode);
  return minus_val_or_errno(er);
}
static int create_or_open(const virtualpath_t *vipa, mode_t mode, struct fuse_file_info *fi){
  //log_entered_function("'%s'",vipa->vp);
  char rp[MAX_PATHLEN+1];
  {
    const int er=realpath_mk_parent(rp,vipa);
    if (er) return -er;
  }
  if (!(fi->fh=MAX_int(0,open(rp,fi->flags|O_CREAT,mode)))){
    log_errno("open(%s,%x,%u) returned -1\n",rp,fi->flags,mode); cg_log_file_mode(mode); cg_log_open_flags(fi->flags); log_char('\n');
    return errno;
  }
  return 0;
}
static int xmp_create(const char *vpath, mode_t mode,struct fuse_file_info *fi){ /* O_CREAT|O_RDWR goes here */
  FUSE_PREAMBLE(vpath);
  er=create_or_open(&vipa,mode,fi);
  LOG_FUSE_RES(vpath,er);
  return -er;
}
static int xmp_write(const char *vpath, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  FUSE_PREAMBLE(vpath);
  long fd;
  if (!fi){
    char real_path[MAX_PATHLEN+1];
    if ((er=realpath_mk_parent(real_path,&vipa))) return -er;
    fd=MAX_int(0,open(real_path,O_WRONLY));
  }else{
    fd=fi->fh;
    LOCK_N(mutex_fhandle, fHandle_t *d=fhandle_get(&vipa,fd); fhandle_busy_start(d));
    if (d){
      fhandle_prepare_in_RW(d,fi->flags);
      fd=d->fd_real;
    }
    LOCK(mutex_fhandle, fhandle_busy_end(d)); // cppcheck nullPointerOutOfResources
    if (d && d->errorno) return d->errorno;
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
  if (!_root_writable) return -EPERM;
  FUSE_PREAMBLE_W(1,vpath);
  char rp[MAX_PATHLEN+1];
  if ((er=realpath_mk_parent(rp,&vipa))) return -er;
  log_verbose("Going to symlink( %s , %s ",target,rp);
  if (symlink(target,rp)==-1){
    log_errno("symlink( %s , %s ",target,rp);
    return -errno;
  }
  return 0;
}
static int xmp_rename(const char *old_path, const char *neu_path IF1(WITH_FUSE_3,, const uint32_t flags)){
  bool eexist=false;
  FUSE_PREAMBLE_W(1,neu_path);
#if WITH_GNU && WITH_FUSE_3
  if (flags&RENAME_NOREPLACE){
    bool found;FIND_REALPATH(&vipa);
    if (found) eexist=true;
  }else if (flags) return -EINVAL;
#endif // WITH_GNU


  virtualpath_init(&vipa,old_path,vp_buffer);
  bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
  if (!found) return -ENOENT;
  if (eexist) return -EEXIST;
  if (!ZPATH_ROOT_WRITABLE()) return -EACCES;
  char neu[MAX_PATHLEN+1];
  er=realpath_mk_parent(neu,&vipa);
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
static off_t fhandle_zip_fread(fHandle_t *d, void *buf,  zip_uint64_t nbytes, const char *errmsg){
  IF1(WITH_EXTRA_ASSERT, LOCK(mutex_fhandle,  assert(fhandle_currently_reading_writing(d))))
    const off_t read=my_zip_fread(d->zip_file,buf,nbytes);
  if (read<0){ warning_zip(D_VP(d),zip_file_get_error(d->zip_file)," fhandle_zip_fread()");return -1;}
  d->zip_fread_position+=read;
  return read;
}
/* Returns true on success. May fail for seek backward. */
static bool fhandle_zip_fseek(fHandle_t *d, const off_t offset, const char *errmsg){
  off_t skip=(offset-fhandle_zip_ftell(d));
  if (!skip) return true;
  const bool backward=(skip<0);
  IF_LOG_FLAG(LOG_ZIP) log_verbose("%p %s offset: %lld  ftell: %lld   diff: %lld  backward: %s"ANSI_RESET,d, D_VP(d),(LLD)offset,(LLD)fhandle_zip_ftell(d),(LLD)skip, backward?ANSI_FG_RED"Yes":ANSI_FG_GREEN"No");
  const int fwbw=backward?FHANDLE_SEEK_BW_FAIL:FHANDLE_SEEK_FW_FAIL;
#if VERSION_AT_LEAST(LIBZIP_VERSION_MAJOR,LIBZIP_VERSION_MINOR,1,9)
  if (!(d->flags&fwbw) && !zip_file_is_seekable(d->zip_file)) d->flags|=(FHANDLE_SEEK_BW_FAIL|FHANDLE_SEEK_FW_FAIL);
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
static zip_file_t *fhandle_zip_open(fHandle_t *d,const char *msg){
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
static void fhandle_zip_fclose(fHandle_t *d){
  zip_file_t *z=d->zip_file;
  d->zip_file=NULL;
  if (z){
    zip_file_error_clear(z);
    my_zip_fclose(z,D_RP(d));
  }
}
static off_t fhandle_read_zip(char *buf, const off_t size, const off_t offset,fHandle_t *d){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!fhandle_zip_open(d,__func__)){
    warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,D_VP(d),"xmp_read_fhandle_zip fhandle_zip_open returned -1");
    return -1;
  }
  IF1(WITH_PRELOADRAM,if(_preloadram_policy!=PRELOADRAM_NEVER && tdf_or_tdf_bin(D_VP(d))) warning(WARN_PRELOADRAM|WARN_FLAG_ERROR|WARN_FLAG_ONCE_PER_PATH,D_RP(d),"tdf or tdf_bin should be preloadram"));
  if (offset!=fhandle_zip_ftell(d)){   /* ***  offset>d: Need to skip data.   offset<d  means we need seek backward *** */
    if (!fhandle_zip_fseek(d,offset,"")){
#if WITH_PRELOADRAM
      if (_preloadram_policy!=PRELOADRAM_NEVER){ /* Worst case=seek backward - consider using cache */
        if (preloadram_wait(d,offset+size)){
          fhandle_zip_fclose(d);
          COUNTER1_INC(COUNT_READZIP_PRELOADRAM_BECAUSE_SEEK_BWD);
          fhandle_counter_inc(d,ZIP_READ_CACHE_SUCCESS);
          LOCK_N(mutex_fhandle,const off_t nread=preloadram_read(buf,d,offset,offset+size));
          return nread;
        }
        fhandle_counter_inc(d,ZIP_READ_CACHE_FAIL);
      }
#endif //WITH_PRELOADRAM
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

/********************************************************************************/
/* Called on first invocation of xmp_write() or xmp_read() for fHandle_t object */
/* Maybe generate file content                                                  */
/********************************************************************************/
#define D_HAS_TB() IF01(WITH_PRELOADRAM,false,(d->preloadram && d->preloadram->txtbuf))
static void fhandle_prepare_in_RW(fHandle_t *d,const int open_flags){
  if (!(d->flags&FHANDLE_PREPARE_ONCE_IN_RW)) return;
  LOCK(mutex_fhandle, d->flags&=~FHANDLE_PREPARE_ONCE_IN_RW);
  IF1(WITH_PRELOADDISK, if (D_IS_PRELOADDISK_UPDATE(d)) preloaddisk_uptodate_or_update(d));
  IF1(WITH_CCODE, c_file_content_to_fhandle(d));
  IF1(WITH_FILECONVERSION, if (open_flags==O_RDONLY && (d->flags&FHANDLE_IS_FILECONVERSION)) fileconversion_run(d));
  bool Done=false;
  if (D_HAS_TB()){ Done=true; d->flags|=FHANDLE_PRELOADRAM_COMPLETE;}
  zpath_t *zpath=&d->zpath;
  IF1(WITH_INTERNET_DOWNLOAD, net_maybe_download(zpath));
  IF1(WITH_PRELOADDISK, if (!Done && (Done=(open_flags==O_RDONLY && is_preloaddisk_zpath(&d->zpath)))) d->errorno=preloaddisk(d));
  if (!Done && !ZPF(ZP_IS_ZIP) && !(d->fd_real=open(RP(),open_flags))){
    d->errorno=errno;
    log_errno("open RP:%s",RP());
  }
  if (!d->errorno && d->fd_real<0) d->errorno=EIO;
}

#define IF(d) IF0(IS_CHECKING_CODE,if(d))
// cppcheck-suppress constParameterCallback
static int xmp_read(const char *vpath, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  IF_LOG_FLAG(LOG_READ_BLOCK)log_entered_function("%s Offset: %'ld +%'d ",vpath,(long)offset,(int)size);
  ASSERT(fi!=NULL); ASSERT(fi->fh);
  FUSE_PREAMBLE(vpath);
  LOCK_N(mutex_fhandle, fHandle_t *d=fhandle_get(&vipa,fi->fh);IF(d){ d->accesstime=time(NULL); fhandle_busy_start(d);});
  const int bytes=_xmp_read(&vipa,d,buf,size,offset,fi->fh);
  IF(d) { LOCK(mutex_fhandle,fhandle_busy_end(d));}
  //IF_LOG_FLAG_OR(LOG_READ_BLOCK,bytes<=0)log_exited_function("%s  %'ld +%'d Bytes: %'d %s",path,(long)offset,(int)size,bytes,success_or_fail(bytes>0));
  return bytes;
}
static int _xmp_read(const virtualpath_t *vipa, fHandle_t *d, char *buf, const size_t size, const off_t offset,int fd){
  off_t nread=-1;
  if (d){
    ASSERT(d->is_busy>=0);
    fhandle_prepare_in_RW(d,O_RDONLY);
    if (d->errorno) return d->errorno;
    if (d->fd_real) goto d_has_fd;
#if WITH_PRELOADRAM
    /* Improve performance: Avoid overhead of preloadram_wait() if FHANDLE_PRELOADRAM_COMPLETE */
    if (d->flags&FHANDLE_PRELOADRAM_COMPLETE){
      LOCK(mutex_fhandle, nread=preloadram_read(buf,d,offset,offset+size));
    };
    if (d->flags&FHANDLE_WITH_PRELOADRAM)  nread=preloadram_wait_and_read(buf,size,offset,d);
#endif //WITH_PRELOADRAM
    if (nread<0 && (d->zpath.flags&ZP_IS_ZIP)){
      pthread_mutex_lock(&d->mutex_read); /* Why lock: Comming here same/different fHandle_t instances and various pthread_self() */
      nread=fhandle_read_zip(buf,size,offset,d);
      pthread_mutex_unlock(&d->mutex_read);
    }
    LOCK_N(mutex_fhandle, if (nread>0) d->n_read+=nread);
    if (nread<0 && !config_not_report_stat_error(vipa->vp,vipa->vp_l)){
      LOCK_N(mutex_fhandle, const char *status=IF01(WITH_PRELOADRAM,"NA",PRELOADRAM_STATUS_S[preloadram_get_status(d)]));
      warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,d?D_RP(d):vipa->vp,"nread %lld<0: +%ld %lld    n_read=%llu  status:%s",(LLD)nread,offset,(LLD)size,d->n_read,status);
    }
  }
 d_has_fd: /* Normal reading from file descriptor  d->fd_real or fi->fh */
  if (nread<0){
    if (d && d->fd_real) fd=d->fd_real;
    //log_debug_now("fd: %d  '%s'",fd,cg_path_for_fd(NULL,fd));
    if (offset-lseek(fd,0,SEEK_CUR) && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK_REG_FILE:"ANSI_RESET" offset: %'lld ",(LLD)offset),log_msg("Failed %s fd=%llu\n",vipa->vp,(LLU)fd);
      nread=-1;
    }else{
      nread=pread(fd,buf,size,offset);
    }
  }
  return nread<0 && errno?-errno:nread;
}/*xmp_read*/
static int xmp_release(const char *vpath, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE(vpath);
  const uint64_t fh=fi->fh;
  if (fh>=FD_ZIP_MIN){
    lock(mutex_fhandle);
    fHandle_t *d=fhandle_get(&vipa,fh);
    if (d) fhandle_destroy(d); else maybe_evict_from_filecache(fh,vipa.vp,vipa.vp_l,NULL,0);
    unlock(mutex_fhandle);
  }else if (fh>2 && (er=close(fh))){
    warning(WARN_OPEN|WARN_FLAG_ERRNO,vpath,"close(fh: %llu)",fh);
    cg_print_path_for_fd(fh);
  }
  return -er;
}
static int xmp_flush(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE(vpath);
  IF1(WITH_SPECIAL_FILE, if (vipa.special_file_id) return 0);
  return fi->fh<FD_ZIP_MIN?fsync(fi->fh):0;
}

static void _exit_ZIPsFS(const char *func, const int line_num){
  log_verbose("Going to exit %s:%d ...",func,line_num);
  /* osxfuse and netbsd needs two parameters:  void fuse_unmount(const char *mountpoint, struct fuse_chan *ch); */
  IF1(WITH_FUSE_3,if (_fuse_started && fuse_get_context() && fuse_get_context()->fuse) fuse_unmount(fuse_get_context()->fuse));
  fflush(stderr);
}
int main(const int argc,const char *argv[]){



  _pid=getpid();
  IF1(WITH_CANCEL_BLOCKED_THREADS,assert(_pid==gettid()), assert(cg_pid_exists(_pid)));
  char tmp[PATH_MAX+1];
  if (realpath(*argv,tmp)) _self_exe=strdup_untracked(tmp); else DIE("Failed realpath %s",*argv);
  init_mutex();
  init_sighandler(argv[0],(1L<<SIGSEGV)|(1L<<SIGUSR1)|(1L<<SIGABRT),stderr);
  init_special_files();
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
  S(utimens);
  S(readlink);
  S(readdir);
  S(symlink); S(unlink);
  S(rmdir); S(mkdir);  S(rename);    S(truncate);
  S(open);    S(create);    S(read);  S(write);   S(release); S(releasedir); S(statfs);
  S(flush);   /* Not needed opendir,access  WITH_FUSE_3:lseek */
#undef S
  static const struct option l_option[]={{"help",0,NULL,'h'}, {"version",0,NULL,'V'}, {NULL,0,NULL,0}};
  for(int c;(c=getopt_long(argc,(char**)argv,"+bqT:nkhVs:c:l:L:",l_option,NULL))!=-1;){  /* The initial + prevents permutation of argv */
    switch(c){
    case 'V': exit(0);break;
    case 'T': cg_print_stacktrace_test(atoi(optarg)); exit_ZIPsFS();break;
    case 'b': _isBackground=true; break;
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 's': _mnt_apparent=_mkSymlinkAfterStart=strdup_untracked(optarg); break;
    case 'h': ZIPsFS_usage();  return 0;
    case 'l': IF1(WITH_PRELOADRAM,if (!preloadram_set_maxbytes(optarg)) return 1); break;
    case 'c': IF1(WITH_PRELOADRAM,if (!preloadram_set_policy(optarg))   return 1); break;
    case 'L': _rlimit_vmemory=cg_atol_kmgt(optarg); break;
    default: if (isalnum(c)) fprintf(stderr,"Wrong option '-%c'. Enter",c); cg_getc_tty(); break;
    }
  }
#if ! defined(HAS_RLIMIT) || HAS_RLIMIT
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
    _mnt_l=cg_strlen(realpath(argv[argc-1],tmp)); _mnt=strdup_untracked(tmp);
    if (!_mnt_apparent) _mnt_apparent=(char*)argv[argc-1];
    if (*_mnt_apparent!='/'){
      if (!getcwd(tmp,PATH_MAX-strlen(_mnt_apparent)-1)) log_errno("getcwd()");
      else _mnt_apparent=strdup(strcat(strcat(tmp,"/"),_mnt_apparent));
      cg_str_replace(0,_mnt_apparent,0, "/s-mcpb-ms03.charite.de/",0,"/s-mcpb-ms03/",0);

      //DIE_DEBUG_NOW(" _mkSymlinkAfterStart: '%s'  getcwd:%s  _mnt_apparent '%s'",_mkSymlinkAfterStart,getcwd(NULL,0),_mnt_apparent);
    }
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
    char path[MAX_PATHLEN+1], dirOldLogs[MAX_PATHLEN+1];
    {
      {
        char *d=path+strlen(cg_copy_path(path,PATH_DOT_ZIPSFS));
        strcat(d,_mnt); while(*++d) if (*d=='/') *d='_';
      }
      snprintf(dirOldLogs,MAX_PATHLEN,"%s%s",path,"/old_logs");
      cg_recursive_mkdir(dirOldLogs);
      strcpy(stpcpy(tmp,path),"/PID.TXT");
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
    _dot_ZIPsFS=strdup_untracked(path);
    FOR(i,0,2){
      const char *name=SFILE_NAMES[i?SFILE_LOG_ERRORS:SFILE_LOG_WARNINGS];
      snprintf(path,MAX_PATHLEN,"%s/%s",_dot_ZIPsFS,name);
      SFILE_REAL_PATHS[i?SFILE_LOG_ERRORS:SFILE_LOG_WARNINGS]=strdup_untracked(path);

      struct stat st;
      if (!PROFILED(lstat)(path,&st) && st.st_size){ /* Save old logs with a mtime in file name. */
        const time_t t=st.st_mtime;
        struct tm lt;
        localtime_r(&t,&lt);
        snprintf(tmp,MAX_PATHLEN,"%s/%s",dirOldLogs,name);
        strftime(strrchr(tmp,'.'),22,"_%Y_%m_%d_%H:%M:%S",&lt);
        strcat(tmp,".log");
        if (cg_rename(path,tmp)) DIE("rename");
        const char *cmd[]={"gzip","-f","--best",tmp,NULL};
        cg_fork_exec(cmd,NULL,0,0,0);
      }
      if (!(_fWarnErr[i]=fopen(path,"w"))) DIE("Failed open '%s'",path);
      fprintf(_fWarnErr[i],"%s\n",path);
    }
    warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);
    IF1(WITH_SPECIAL_FILE, snprintf(path,MAX_PATHLEN,"%s/%s",_dot_ZIPsFS,"ZIPsFS_CTRL.sh");
        SFILE_REAL_PATHS[SFILE_DEBUG_CTRL]=strdup_untracked(path);
        special_file_content_to_file(SFILE_DEBUG_CTRL,path));
    snprintf(tmp,MAX_PATHLEN,"%s/cachedir",_dot_ZIPsFS); mstore_set_base_path(tmp);
  }
  FOR(i,optind,colon){ /* Source roots are given at command line. Between optind and colon */
    const char *a=argv[i];
    if (!*a || *a=='@') continue;
    if (_root_n>=ROOTS) DIE("Exceeding max number of ROOTS %d.  Increase macro  ROOTS   in configuration.h and recompile!\n",ROOTS);
    root_t *r=_root+_root_n;
    int features=0;while(*argv[i+features+1]=='@') features++;
    root_init(!_root_n++,r,a, argv+i+1,features);
    if (_root_writable){
      char rp[MAX_PATHLEN+1];
      /* RLOOP(j,2){ */
      /*   stpcpy(stpcpy(rp,_root_writable->rootpath),j?DIR_FILECONVERSION: DIR_INTERNET); */
      /*   cg_recursive_mkdir(rp); */
      /*   if (!cg_is_dir(rp)) DIE("Failed creating directory %s",rp); */
      /* } */
      assert(strlen(_root_writable->rootpath)+sizeof(FILE_CLEANUP_SCRIPT)<sizeof(rp));
      _root_writable_path=strdup_untracked(_root_writable->rootpath);
      _cleanup_script=strdup_untracked(strcat(strcpy(rp,_root_writable_path),FILE_CLEANUP_SCRIPT));
    }
    ht_set_mutex(mutex_dircache,ht_init(&r->dircache_ht,"dircache",HT_FLAG_KEYS_ARE_STORED_EXTERN|12));
    ht_set_mutex(mutex_dircache_queue,ht_init(&r->dircache_queue,"dircache_queue",8));
    ht_init_interner_file(&r->ht_int_rp,"ht_int_rp",16,DIRECTORY_CACHE_SIZE); ht_set_mutex(mutex_dircache,&r->ht_int_rp);
#if WITH_DIRCACHE || WITH_STAT_CACHE || WITH_TIMEOUT_READDIR
    ht_init_interner_file(&r->ht_int_fname,"ht_int_fname",16,DIRECTORY_CACHE_SIZE);  ht_set_mutex(mutex_dircache,&r->ht_int_fname);
    ht_set_mutex(mutex_dircache,ht_init_interner_file(&r->ht_int_fnamearray,"ht_int_fnamearray",HT_FLAG_BINARY_KEY|12,DIRECTORY_CACHE_SIZE));
    mstore_set_mutex(mutex_dircache,mstore_init(&r->dircache_mstore,"dircache_mstore",MSTORE_OPT_MMAP_WITH_FILE|DIRECTORY_CACHE_SIZE));
    IF1(WITH_STAT_CACHE,ht_set_mutex(mutex_dircache,ht_init(&r->stat_ht,"stat",HT_FLAG_KEYS_ARE_STORED_EXTERN|16)));
#endif
  }/* Loop roots */
  root_read_properties(NULL,NULL,0); /* free line */
  log_msg("\n\nMount point: "ANSI_FG_BLUE"'%s'"ANSI_RESET"\n\n",_mnt);
  { /* Storing information per file type for the entire run time */
    mstore_set_mutex(mutex_fhandle,mstore_init(&mstore_persistent,"persistent",0x10000));
    ht_set_mutex(mutex_fhandle,ht_init_interner(&ht_intern_fileext,"ht_intern_fileext",8,4096));
    ht_set_mutex(mutex_validchars,ht_init(&_ht_valid_chars,"validchars",HT_FLAG_NUMKEY|12));
    ht_init_with_keystore_dim(&ht_inodes_vp,"inode_from_virtualpath",16,0x100000|MSTORE_OPT_MMAP_WITH_FILE);
    IF1(WITH_FILECONVERSION_OR_CCODE,ht_init(&ht_fsize,"fileconversion-fsize",HT_FLAG_NUMKEY|9));
  }
  IF1(WITH_ZIPINLINE_CACHE, ht_set_mutex(mutex_dircache,ht_init(&ht_zinline_cache_vpath_to_zippath,"zinline_cache_vpath_to_zippath",HT_FLAG_NUMKEY|16)));

  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  if (check_configuration(argv[argc-1])){
    warning(WARN_CONFIG,"","Non-recommended configuration.");
    if (!cg_uid_is_developer() && !_isBackground) fprintf(stderr,"Press enter\n"), cg_getc_tty();
  }
  mkSymlinkAfterStartPrepare();
  if (_isBackground)  _logIsSilent=_logIsSilentFailed=_logIsSilentWarn=_logIsSilentError=_cg_is_none_interactive=true;
  IF1(WITH_FILECONVERSION,fc_init());
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
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF1(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS();
}

////////////////////////////////////////////////////////////////////////////
// DIE  log_  FIXME  DIE_DEBUG_NOW   DEBUG_NOW   log_debug_now  log_entered_function log_exited_function directory_print
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
// opendir  locked config_fileconversion_size_of_not_existing_file crc32
// limits strtoOAk SIZE_POINTER   COUNTm_MSTORE_MALLOC
// strncpy stpncpy  mempcpy strcat _thisPrg
// strrchr fileconversion_rinfiles
// analysis.tdf-shm analysis.tdf-wal
// lstat stat
//     if (!strcmp("/s-mcpb-ms03.charite.de/incoming/PRO3/Maintenance/202403/20240326_PRO3_LRS_004_MA_HEK500ng-5minHF_001_V11-20-HSoff_INF-G.d.Zip",rp)){  log_zpath("DDDDDDDDDD",zpath);}
// SIGTERM
//  statvfs    f_bsize  f_frsize  f_blocks f_bfree f_bavail
// config_fileconversion_estimate_filesize   fileconversion_rinfiles
// stat foreach_fhandle EIO local  fseek seek open
// fdescriptor fdscrptr fHandle_t fhandle_read_zip  #include "generated_profiler_names.c" LOCK
// mutex_fhandle
// typedef enum false_na_true { FALSE=-1,ZERO,TRUE} false_na_true_t;
//
// usleep stat PATH_MAX  WITH_STAT_CACHE  dircache_directory_from_cache
// ACT_BLOCK_THREAD thread_pretend_blocked LF() when
//  MALLOC_dir_field  MALLOC_fhandle
//  COUNT_FILECONVERSION_MALLOC_TXTBUF  COUNT_MALLOC_PRELOADRAM_TXTBUF
// WITH_ZIPINLINE_CACHE WITH_ZIPINLINE WITH_DIRCACHE
// COUNT_FHANDLE_CONSTRUCT
// strcat strcpy strncpy stpncpy strcmp README_AUTOGENERATED.TXT realloc memcpy cgille stat_set_dir
//
// search_file_which_roots(   EXT_CONTENT
// README_ZIPsFS.TXT     SFILE_README_FILECONVERSION
// find_realpath_special_file FILLDIR_STRIP_NET_HEADER
// mnt/ZIPsFS/a/misc_tests/zip/images_helpimg.ZIP.Content
// SOURCE stat_to_cache stat_from_cache
// ZIPsFS_print_source.sh  mnt/ZIPsFS/lr/Z1/Data/30-0001/20220802_Z1_AF_009_30-0001_SWATH_P01_Plasma-V-Position-Rep5.wiff  SOURCE.TX VFILE_SFX_INFO ZP_IS_PATHINFO
// =virtualpath_init(&vipa,vpath,vp_buffer);  virtualpath_init
// ENOENT cg_utils_error_codes.c
//                 COUNT_TXTBUF_SEGMENT_MALLOC             63             67 x                    0                    0
//                 COUNT_TXTBUF_SEGMENT_MMAP              6             10 x              4194402              4194790 x
//fhandle_preloadram_advise
// strcspn strcspn()
// Pause while high RAM   _rootmask_preload  preload
// md5sum   mnt/ZIPsFS/lrz/misc_tests/zip/20220202_A1_KTT_008_22-2222_Benchmarking4_dia.raw
// error_symbol
// DIRNAME_PRELOADED_UPDATE  DIR_PRELOADED_UPDATE  internalize_rp()
// test_realpath(ZP_TRY_ZIP|ZP_RESET_IF_NEXISTS,zpath,r);
// usleep DEBUG_THROTTLE_DOWNLOAD
