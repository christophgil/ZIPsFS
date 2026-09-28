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
// cppcheck-suppress-file knownConditionTrueFalse
// #define ewlog(x) cg_endsWith(0,x,cg_strlen(x),".log",4)
// (search-forward-regexp "VFOLDER_PATH(vipa)=\\w")
#define VFILE_SFX_SOURCE "@SOURCE.TXT"
#define VFILE_SFX_ZIPCRC32 "@ARCHIVECRC32.TXT"
#define VFILE_SFX_PROPERTIES "@PROPERTIES.TXT"
#define HOMEPAGE "https://github.com/christophgil/ZIPsFS"
#define _GNU_SOURCE
#define FUSE_USE_VERSION 31
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
#define HOOK_MSTORE_CLEAR(m)   {char mpath[PATH_MAX];mstore_file(mpath,m,-1);warning(WARN_DIRCACHE,mpath,"Clearing mstore_t %s ",m->name);}
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
#include "tmp/generated_ZIPsFS.inc"
//
#include "cg_profiler.h"
// ---
#include "cg_OS_dependent.c"
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
#define  ALL_READABLE true
static pid_t _pid;
//cppcheck-suppress-macro [identicalInnerCondition]
#define fhandle_busy_start(d) {if (d) atomic_fetch_add(&d->is_busy,1);}   DETECT_RESOURCE_LEAK_B(_fhandle_busy)
//cppcheck-suppress-macro [identicalInnerCondition]
#define fhandle_busy_end(d)   {if (d) atomic_fetch_add(&d->is_busy,-1);}   DETECT_RESOURCE_LEAK_E(_fhandle_busy)
//cppcheck-suppress-macro unreadVariable
#if WITH_FILECONVERSION
#include "ZIPsFS_configuration_fileconversion.h"
#include "ZIPsFS_fileconversion_impl.h"
#endif //WITH_FILECONVERSION
#if WITH_CCODE
#include "ZIPsFS_c.c"
#endif //WITH_CCODE
struct fuse *_fuse;
static char *_mkSymlinkAfterStart, *_mnt_apparent;
static const char *_self_exe, *_mnt, *_dot_ZIPsFS;
static int _fhandle_n=0,_mnt_l=0, _debug_is_readdir;
static uint32_t _fuse_max_write;
static rlim_t _rlimit_vmemory=0;
enum {COUNT_BACKWARD_SEEK=1024};
static int _count_backward_seek[COUNT_BACKWARD_SEEK];
IF1(WITH_PRELOADRAM,static  enum_when_preloadram_zip_t _preloadram_policy=PRELOADRAM_RULE);
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
static mstore_t _mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
#define X(ht) static ht_t ht;
XMACRO_HT_GLOBAL()
#undef X
  static int _log_flags; /* The bits specifying what is logged.*/
/* *** fHandle vars and defs *** */
static float _ucpu_usage,_scpu_usage;/* user and system */
static int64_t _preloadram_bytes_limit=3L*1000*1000*1000;
static int _unused_int,_writable_path_l;
static bool _thread_unblock_ignore_existing_pid, _fuse_started, _isBackground;
static bool _logIsSilentFailed,_logIsSilentWarn,_logIsSilentError;
static const char *_fuse_argv[99], *_writable_path;
static int _fuse_argc;
static root_t _root[ROOTS], *_root_writable;
static int _root_n;  /* Num of root_t instances in _root[] */
static virtualfolder_t _virtualfolders[VIRTUALFOLDER_MAX+1];
static specialfile_t _specialfiles[SFILE_NUM];
#if WITH_INTERNET_DOWNLOAD
#include "ZIPsFS_configuration_internet.c"
#include "ZIPsFS_internet.c"
#endif //WITH_INTERNET_DOWNLOAD
static const char* rootpath(const root_t *r){
  return r?r->rootpath:NULL;
}
static const char* report_rootpath(const root_t *r){
  return !r?"No root":r->rootpath;
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
    if (s>1){
      r->has_timeout=true;
      r->stat_timeout_seconds=STAT_TIMEOUT_SECONDS;
      r->readdir_timeout_seconds=READDIR_TIMEOUT_SECONDS;
      r->openfile_timeout_seconds=OPENFILE_TIMEOUT_SECONDS;
    }
  }
  realpath(path,tmp);
  r->rootpath=r->rootpath_orig=strdup(tmp);
  if (cg_last_char(path)!='/'){
    char *slash=strrchr(tmp,'/');
    assert(slash);
    r->path_prefix=strdup(slash);
    if (isWritable) DIE(RED_ERROR" Please add a trailing slash to the root-path '%s' of the first branch because  the writable branch should not retain the last path component.\n",path);
  }
  r->rootpath_l=cg_strlen(r->rootpath);
  if (r==_root_writable){
    _writable_path=r->rootpath;
    _writable_path_l=r->rootpath_l;
    ASSERT(r->rootpath_l);
  }
  _rootdata_path_max=MAX_int(_rootdata_path_max,(r->rootpath_l+cg_strlen(r->path_prefix)+2));
  if (!r->rootpath_l || !cg_is_dir(r->rootpath))    DIE("Not a directory: '%s'  realpath: '%s'",path,r->rootpath);
  {
    struct statvfs st;
    if (statvfs(r->rootpath_orig,&st)){
      perror(r->rootpath_orig);
      exit_ZIPsFS();
    }
    r->f_fsid=st.f_fsid; /* IS allowed to be zero */
    IF1(HAS_NO_ATIME,r->noatime=(st.f_flag&ST_NOATIME));
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
  RLOOP(i,enum_async_N) pthread_mutex_init(r->async_mtx+i,NULL);
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
  root_property_read_all(r,annotations,annotations_n);
  if (r->path_prefix){
    char *p=strdup_untracked(r->path_prefix);
    if (strstr(p,"//")) DIE(RED_ERROR" The path prefix '%s' of root '%s' contains double slash.",p,rootpath(r));
    RLOOP(i,(r->path_prefix_l=strlen(p))) if (p[i]=='/') p[i]=0;
    r->pathpfx_slash_to_null=p;
  }
  {
    struct stat st;
    if (stat(path,&st)){ log_errno("stat '%s'",path); DIE("");}
    r->st_dev=st.st_dev;
  }
  if (WITH_FOLLOW_SYMLINK_ALL_ROOTS) r->follow_symlinks=1;
  root_verify(r);
}

static void root_verify(root_t *r){
  if (r->remote || r->probe_path || r->probe_path_response_ttl || r->probe_path_timeout){
    if (!r->probe_path_timeout)      r->probe_path_timeout=PROBE_PATH_TIMEOUT_SECONDS;
    if (!r->probe_path_response_ttl) r->probe_path_response_ttl=PROBE_PATH_RESPONSE_TTL_SECONDS;

#define C (r->probe_path_response_ttl*3<r->probe_path_timeout)
    if (!C){
      log_verbose("%s: %s should be much smaller than %s   Not satisfied: %d*3 < %d",rootpath(r), ROOT_PROPERTY[ROOT_PROPERTY_probe_path_response_ttl],ROOT_PROPERTY[ROOT_PROPERTY_probe_path_timeout], r->probe_path_response_ttl,r->probe_path_timeout);
      assert(C);
    }
#undef C
    if (r==_root_writable){
      DIE("The first file tree should be local, i.e. starting with single and not double slash. It should not have any of the following properties: %s  %s  %s",
          ROOT_PROPERTY[ROOT_PROPERTY_probe_path],ROOT_PROPERTY[ROOT_PROPERTY_probe_path_timeout], ROOT_PROPERTY[ROOT_PROPERTY_probe_path_response_ttl]);
    }
  }
  if (r->decompress_mask){
    if (!_writable_path_l) warning(WARN_CONFIG,r->rootpath,"Setting  --preload to root, but no writable root.");
    if (_root_writable==r) DIE("Do not set --preload for  first root.");
    _rootmask_preload|=(1<<rootindex(r));
  }
}
/*************************/
/* Parse root properties */
/*************************/
static char *_root_property_error,  *_root_property_print, *_root_property_type, *_root_property_example;
static void rp_print_paths(char *buf,const int size, const char **ss){
  if (!ss) return;
  int n=cg_str_join(buf+1, size-4, size-4,ss,"  ");
  *buf='{';
  buf[n+1]='}';
  buf[n+2]=0;
}
static bool rp_parse_01(const char *v){
  if (!v||!*v) return true;
  if ((*v=='0'||*v=='1')&&!v[1]) return *v=='1';
  _root_property_error="Only 0 or 1 allowed";
  return false;
}
static int rp_parse_number(const char *v){
  if (!v||!*v){_root_property_error="Expect number";return 0;}
  if (cg_find_invalidchar(VALIDCHARS_DIGITS,v,strlen(v))>=0)	_root_property_error="Only digits allowed";
  return atoi(v);
}
static char *rp_parse_vpath(root_t *r,char *v){
  const int v_l=cg_strlen(v);
  char tmp[PATH_MAX];
  if (v_l){
    if (cg_find_invalidchar(VALIDCHARS_PATH,v,v_l)>=0) _root_property_error="Invalid character";
    v[cg_pathlen_ignore_trailing_slash(v)]=0;
    *tmp='/'; stpcpy(stpcpy(tmp+(*v!='/'),v),r->path_prefix);
    return strdup(tmp);
  }
  return NULL;
}
static const char *rp_parse_rpath(root_t *r,char *v){
  const char *rp=realpath(v,NULL);
  if (!rp){perror(v);DIE("Cannot resolve path");}
  return rp;
}
static void rp_parse_decompress(root_t *r,char *v){
  if (!r){ RP_HELP("Preloaded and decompressed. "REQUIRES_1(WITH_PRELOADDISK),"List of  gz,bz2,xz,lrz,Z");return;}
  if (_root_property_print){
    int n=0;
    FOR(iCompress,1,COMPRESSION_NUM){
      if (!(r->decompress_mask&(1<<iCompress))) continue;
      const char *x=cg_compression_file_ext(iCompress,NULL);
      if (x && *x) n+=sprintf(_root_property_print+n,"%s%s",n?" ":"{",x+1);
    }
    if (n) _root_property_print[n++]='}';
    _root_property_print[n]=0;
  }
  char tmp[64];
  if (v){
    for(const char *t;(t=strtok(v,","));v=NULL){
      *tmp='.'; cg_strncpy0(tmp+(*t!='.'),t,63);
      const int c=cg_compression_for_filename(tmp,0);
      if (!c) _root_property_error="Property %s: Unsupported compression";
      r->decompress_mask|=(1<<c);
    }
  }
}
static void root_property_help(const int id_or_minus_1, FILE *f){
#define F "%24s | %-25s | %-94s\n"
#define X(name,code) case ROOT_PROPERTY_##name:{code;}break;
#define r NULL
#define v NULL
  if (id_or_minus_1<0) fputs(ANSI_BOLD ANSI_UNDERLINE"Properties of rootpaths"ANSI_RESET"\n\nThese are given at the cli after the rootpath in the form @name=value or in files formed by appending  "EXT_ROOT_PROPERTY"\n\n",f);
  fputs(ANSI_UNDERLINE""ANSI_BOLD,f); fprintf(f,F,"Property name","Data type", "Description"); fputs(ANSI_RESET,f);
  FOR(id,0,ROOT_PROPERTY_NUM){
    if (id_or_minus_1>=0 && id!=id_or_minus_1) continue;
    RP_HELP("",NULL);
    switch(id){XMACRO_ROOT_PROPERTY()}
    fprintf(f,F,ROOT_PROPERTY[id],_root_property_type,_root_property_example);
  }
#undef r
#undef v
#undef RP_HELP
#define RP_HELP(...)
#undef F
}
static int root_property_print(const output_formats_t outformat,const root_t *only_this_root,  char *tmp, const int tmp_max){
  tmp[0]=0;
  int l=0;
#define S(...) {if (l<tmp_max) l+=snprintf(tmp+l,tmp_max-l,__VA_ARGS__);}

#define v NULL
#undef RP_PRINT
#define RP_PRINT(code) code
  const char *reset=outformat==OUTPUT_HTML?"":outformat==OUTPUT_ANSI?ANSI_RESET:"";
  if (!only_this_root) S("\n%s *** Properties of roots *** %s",outformat==OUTPUT_ANSI?ANSI_INVERSE:"",reset);
  char buf[4096];
  _root_property_print=buf;
  foreach_root(r){
    if (only_this_root && r!=only_this_root) continue;
    int header=0;
    FOR(id,0,ROOT_PROPERTY_NUM){
      *buf=0; //memset(buf,0,sizeof(buf));
      switch(id){XMACRO_ROOT_PROPERTY();}
      if (*buf){
        if (!header++) S("\n%sProperties of %s%s\n",outformat==OUTPUT_ANSI?ANSI_UNDERLINE ANSI_BOLD:"",r->rootpath,reset);
        S("   - %s%s: %s %s\n",outformat==OUTPUT_ANSI?ANSI_FG_MAGENTA:"",ROOT_PROPERTY[id],reset,buf);
      }
    }
    if (only_this_root){
      S("   - Filesystem ID: %lx\n", r->f_fsid);
      if (r->rootpath_mountpoint) S("   - Filesystem mountpoint: %s\n", r->rootpath_mountpoint);
      S("   - Free: %'ld GB\n",((r->statvfs.f_frsize*r->statvfs.f_bfree)>>30));
      S("   - noatime: %s\n",yes_no(r->noatime));
    }



  }
  if (!only_this_root) S("For a list of supported properties run ZIPsFS -h\n");
#undef v
#undef RP_PRINT
#undef S
#define RP_PRINT(code)
  return l;
}
static void root_property_read(const char *propertypath,const int iLine,root_t *r,const char *assignment){
#undef RP_GET
#define RP_GET(code) code;
  int ff_decompress[2]={0};
  char vbuf[4096],*v=NULL;
  *vbuf=0;
  RLOOP(id,ROOT_PROPERTY_NUM){
    const char *p=ROOT_PROPERTY[id];
    const int p_l=cg_strlen(p);
    if (strncmp(assignment,p,p_l)) continue;
    v="";
    const char eq=assignment[p_l];
    if (eq){
      if (eq!='=') continue;
      v=cg_strncpy0(vbuf,assignment+p_l+1,sizeof(vbuf)-1);
    }
    _root_property_print=_root_property_error=NULL;
    switch(id){XMACRO_ROOT_PROPERTY()}
    if (_root_property_error){
      fprintf(stderr,RED_ERROR"%s:%d: %s\n",propertypath,iLine+1,_root_property_error);
      root_property_help(id,stderr);
      fputs("Press enter or Ctrl-C\n",stderr);
      cg_getc_tty();
    }
    return;
  }
  fprintf(stderr,RED_ERROR" Unknown property name '"ANSI_FG_BLUE"%s"ANSI_RESET"' ",assignment);
  if (propertypath) fprintf(stderr,"\n""in "ANSI_FG_BLUE"%s"ANSI_RESET"\n",propertypath);
  else fputs(" in command line\n",stderr);
  fprintf(stderr,"For list of supperted properties run\n"ANSI_BLACK ANSI_FG_GREEN"%s -h\n\n"ANSI_RESET,_thisPrg);
  root_property_help(-1,stderr);
  cg_getc_tty();
#undef X
#undef RP_GET
}
static void root_property_read_all(root_t *r,const char **annotations, const int annotations_n){
  static char *line=NULL;
  if (!r){ free(line); line=NULL; return;}
  char propertypath[MAX_PATHLEN+1]; stpcpy(stpcpy(propertypath,r->rootpath_orig),EXT_ROOT_PROPERTY);
  size_t capacity=0;
  FILE *f=fopen(propertypath,"r");
  if (f){
    for(int i=0; (getline(&line,&capacity,f)!=-1); i++){
      char *k;
      if ((k=strchr(line,'\n'))) *k=0;
      if ((k=strchr(line,'#')))  *k=0;
      k=line; while(isspace(*k))  k++;
      k[cg_last_nospace_char(k)+1]=0;
      if (*k) root_property_read(propertypath,i,r,k);
    }
    fclose(f);
  }
  FOR(i,0,annotations_n) root_property_read(NULL,i,r,annotations[i]+1);
}
/*****************************/
/* END Parse root properties */
/*****************************/
#define my_zip_fread(...) _viamacro_my_zip_fread(__VA_ARGS__,__func__,__LINE__)
#define fhandle_zip_fread(...) _viamacro_fhandle_zip_fread(__VA_ARGS__,__func__,__LINE__)



// ---
#include "ZIPsFS_configuration.c"
#include "ZIPsFS_debug.c"
#if WITH_STATCACHE
#include "ZIPsFS_cache_stat.c"
#endif //WITH_STATCACHE
#include "ZIPsFS_cache.c"
#if WITH_TRANSIENT_ZIPENTRY_CACHES
#include "ZIPsFS_transient_zipentry_cache.c"
#endif
// ---
#if WITH_PRELOADDISK
#include "ZIPsFS_preloaddisk.c"
#endif //WITH_PRELOADDISK
#include "ZIPsFS_async.c"
// ---
#include "ZIPsFS_filesystem_info.c"
#if WITH_PRELOADRAM
#include "ZIPsFS_preloadram.c"
#include "ZIPsFS_ctrl.c"
#include "ZIPsFS_special_file.c"
#endif // WITH_PRELOADRAM
#include "ZIPsFS_log.c"
// ---
#if WITH_ZIPFLAT
#include "ZIPsFS_zip_inline.c"
#endif //WITH_ZIPFLAT
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
static void directory_init_zpath(directory_t *dir,const zpath_t *zpath){
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
    //dir->files_capacity=DIRECTORY_DIM_STACK;
    MSTORE_INIT(&dir->filenames,4096|MSTORE_OPT_MALLOC);
    dir->filenames.mstore_counter_mmap=COUNT_MSTORE_MMAP_DIR_FILENAMES;
  }
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
static void directory_ensure_capacity(directory_t *dir, const int min, const int newCapacity){
  assert(DIR_RP());
  ASSERT(!dir->dir_is_dircache);
  ASSERT_NOT_ASSIGNED(dir);
  directory_core_t *dc=&dir->core;
  //log_entered_function("files_capacity:%d  files_l:%d  fname: %p",d->files_capacity,dc->files_l,&dc->fname);
  if (min>dir->files_capacity || dc->fname==NULL){
    assert(min>DIRECTORY_DIM_STACK);
    // _cg_realloc_array(const int id,const int size1AndOpt,const void *pOld, const size_t nOld, const size_t nNew)
    dir->files_capacity=newCapacity;
    assert(newCapacity>=min);
#define O(f) (dc->f==dir->_stack_##f?REALLOC_ARRAY_NO_FREE:0)
#define X(f,type)  if (dc->f) dc->f=cg_realloc_array(COUNT_MALLOC_dir_field,sizeof(type)|O(f),dc->f,dc->files_l,newCapacity)
    XMACRO_DIRECTORY_ARRAYS();
#undef O
#undef X
  }
  //log_exited_function("capacity: %d fname: %p",dir->files_capacity,&dc->fname);
}
static void directory_add(uint8_t flags,directory_t *dir, int64_t inode, const char *n0,uint64_t size, time_t mtime,zip_uint32_t crc){
  if (cg_empty_dot_dotdot(n0) || !dir) return;
#define L dc->files_l
  cg_thread_assert_locked(mutex_dircache);
  directory_core_t *dc=&dir->core;
  directory_ensure_capacity(dir,L+1,2*L+2);
  assert(dc->fname!=NULL);
  IF0(WITH_ZIPENTRY_PLACEHOLDER,const char *s=n0);
  IF1(WITH_ZIPENTRY_PLACEHOLDER, static char buf_for_s[MAX_PATHLEN+1];const char *s=(flags&DIRENT_DIRECT_NAME)?n0:zipentry_placeholder_insert(buf_for_s,n0,dir));
  const int s_l=cg_pathlen_ignore_trailing_slash(s);
  dc->fflags[L]=(flags&DIRENT_SAVE_MASK)|(s[s_l]=='/'?DIRENT_ISDIR:0);
  assert(dir->files_capacity>L);
#define C(name) if (dc->f##name) dc->f##name[L]=name
  C(mtime);C(size);C(inode); C(crc);
#undef C
  if (crc) ASSERT(NULL!=dc->fcrc);
  ASSERT(NULL!=dc->fname);
  if (!(flags&DIRENT_DIRECT_NAME)){
#if WITH_TIMEOUT_READDIR
    if (dir->ht_intern_names){
      LOCK(mutex_dircache, s=ht_sintern(dir->ht_intern_names,s));
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
_Static_assert(S_IXOTH==(S_IROTH>>2),"");
static void stat_set_dir(struct stat *s){
  if (s){
    s->st_size=ST_BLKSIZE;
    s->st_nlink=1;
    s->st_mode=S_IFDIR|0755;
    s->st_uid=getuid();
    s->st_gid=getgid();
  }
}

/*******************************************************************/
/* Next Schnappszahl, larger than n.                               */
/* Also  larger 2 block sizes to facilitate fuse_invalidate_path() */
/* in case of underestimation of file size.                        */
/*******************************************************************/
static off_t nextRepdigitFileSize(uint64_t n){
  return nextRepdigit(MAX(n,_fuse_max_write*2));
}

/****************************************************************************************************/
/* Single point for calling lstat() / stat()														*/
/* Invokations with  fd_parent!=0 are coming from readdir to store the attributes. Using fstatat(). */
/* For all other invokations, fd_parent is 0.  Using lstat().										*/
/* For remote paths, the calls to lstat() are reduced with an attribut cache                        */
/****************************************************************************************************/

static bool zpath_stat_direct(const int opts_findrp,zpath_t *zpath,const time_t now){
  if (!RP_L() || (opts_findrp&FINDRP_CACHE_ONLY)) return false; /* Also  see FINDRP_CACHE_NOT */
  if (!stat_direct(&zpath->stat_rp,RP())) return false;
  IF1(WITH_STATCACHE,if(ZPR()) stat_to_cache(opts_findrp,&zpath->stat_rp,VP0(),VP0_L(),ZPR(),zpath->vfolder,now?now:time(NULL)));
  return true;
}
static bool _viamacro_stat_direct(const int fd_parent,struct stat *st,const char *rp,const char *callerFunc){
  //  if (opts_findrp&FINDRP_CACHE_ONLY){log_debug_now("SKIP %s",rp); if (r)assert(r!=_root_writable);}
  //static int count;log_entered_function("#%d fd=%d %s  (%s)",count++,fd_parent,rp,callerFunc);
  cg_thread_assert_not_locked(mutex_fhandle);
  const int res=
    fd_parent>0?fstatat(fd_parent,rp+cg_last_slash(rp)+1,st,AT_SYMLINK_NOFOLLOW):
    lstat(rp,st);
  inc_count_by_ext(rp,res?COUNTER_STAT_FAIL:COUNTER_STAT_SUCCESS);
  if (res){
    *st=empty_stat;
    return false;
  }
  ASSERT(st->st_ino!=0);
  return true;
}/*stat_direct*/
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
    char rp[PATH_MAX];
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
static void  zpath_set_atime(const zpath_t *zpath){
  if (zpath && ZPR() && ZPR()->writable  && ZPR()->noatime){
    struct stat st;
    if (stat(RP(),&st)){
      log_errno("stat '%s'",RP());
    }else if (st.st_atime>time(NULL)){
      // log_verbose("Not updating atime because current atime is in the future: '%s'",RP());
    }else{
      cg_file_set_atime(true,RP(),&st,0);
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
  if (ZPF(ZP_OVERFLOW) || !s) return false;
  const int l=MIN_int(cg_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      warning(WARN_PATH|WARN_FLAG_MAYBE_EXIT,VP(),"zpath_strncat %s %d exceeding %d for \n%s\n",s,len,ZPATH_STRGS,zpath->strgs);
      zpath->flags|=ZP_OVERFLOW;
      return false;
    }
    cg_strncpy0(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return true;
}
static void zpath_reset_realpath(zpath_t *zpath){
  zpath->stat_rp=empty_stat;
  if (zpath->realpath){
    //log_debug_now("RP_L():%d realpath:%d     strgs_l=%d",RP_L(),zpath->realpath,zpath->strgs_l);
    assert(zpath->current_string==zpath->realpath); /* realpath was appended  */
    zpath->strgs_l=zpath->realpath;
  }else{
    ZPATH_NEWSTR(realpath);
  }
}
static void zpath_set_realpath(zpath_t *zpath, const char *rp1,const char *rp2,const char *rp3){
  zpath_reset_realpath(zpath);
  if (rp1==_writable_path) ZPR()=_root_writable;
  if (rp1) ZPATH_STRCAT(rp1);
  if (rp2) ZPATH_STRCAT(rp2);
  if (rp3) ZPATH_STRCAT(rp3);
  ZPATH_COMMIT(realpath);
  //log_exited_function("%s realpath:%d  rp12:%s%s   strgs_l=%d",RP(),zpath->realpath,rp1,rp2,zpath->strgs_l);
}
static int realpath_writable_folder(char *dst, const zpath_t *zpath, const char *dir){
  return stpcpy(stpcpy(stpcpy(dst,_writable_path),dir),VP()+VFOLDER_PATH_L(zpath))-dst;
}
static void _viamacro_zpath_assert_strlen(const char *fn,const char *file,const int line,zpath_t *zpath){
  bool e=false;
  int l;
#define C(a)   l=zpath->a==0?0:  strlen(zpath->strgs+zpath->a); if (l!=zpath->a##_l && (e=true)) log_error(#a" != "#a"_l   %d!=%d\n",l,zpath->a##_l);
  C(vp);C(virtualpath_without_entry);C(entry_path);
#undef C
  if (e){
    log_zpath("Error ",zpath);
    log_warn("zpath_assert_strlen in %s at "ANSI_FG_BLUE"%s:%d\n"ANSI_RESET,fn,file,line);
    ASSERT(0);
  }
}
static void zpath_reset_keep_VP(zpath_t *zpath){
#define X(f) zpath->f=zpath->f##_l=0;
  XMACRO_ZIPPATH_NEED_RESET();
#undef X
  zpath->zipcrc32=zpath->virtualpath_without_entry_hash=zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  ZPR()=NULL;
  zpath->strgs_l=zpath->vp+VP_L();
  zpath->flags=ZPF(ZP_MASK_KEEP_NOT_RESET);
  zpath->flags2=0;
}
static bool zpath_init_vp(zpath_t *zpath, const char *vp0,   const int vp0_l, const char *optionalPathComp){
  const int optionalPathComp_l=cg_strlen(optionalPathComp), vp_l=vp0_l+optionalPathComp_l+(optionalPathComp_l!=0);
  zpath->vp=zpath->strgs_l=1;  /* To distinguish from virtualpath==0  meaning not defined we use 1*/
  zpath->vp_l=vp_l;
  if (!ZPATH_STRCAT_N(vp0, vp0_l) || optionalPathComp_l && (!ZPATH_STRCAT("/") || !ZPATH_STRCAT_N(optionalPathComp,optionalPathComp_l))) return false;
  zpath->strgs[zpath->vp+vp_l]=0;
  zpath_reset_keep_VP(zpath);
  zpath->virtualpath_hash=hash32(VP(),vp_l);
  zpath->vfolder=_virtualfolders;
  return true;
}
static void zpath_init(zpath_t *zpath, const virtualpath_t *vipa){
  ASSERT(zpath);
  ASSERT(vipa);
  ASSERT(vipa->vp);
  ASSERT(!ZPF(ZP_IS_IN_FHANDLE));
  ASSERT(cg_strlen(vipa->vp)>=vipa->vp_l);
  memset(zpath,0,sizeof(zpath_t));
  zpath_init_vp(zpath,vipa->vp,vipa->vp_l,NULL);
#define C(f)   zpath->f=vipa->f
  //IF1(WITH_PRELOADDISK,C(preloadpfx));
  C(vfolder);
  C(flags);
  C(zipfile_l);
  C(zipfile_cutr);
  C(specialfile_id);
  C(zipfile_append);
  C(vfile_sfx);
#undef C
}
static bool zpath_stat(const int opts_findrp, zpath_t *zpath){
  const root_t *r=ZPR();
  //if (ENDSWITH(VP(),VP_L(),"scan"))log_entered_function("%s root:%s  vdir:%s",VP(),rootpath(r),VFOLDER_PATH(zpath));
  bool ok=zpath->stat_rp.st_ino;
  if (!ok){
    if (r){
      cg_thread_assert_not_locked(mutex_fhandle);
      IF1(WITH_STATCACHE, ok=zpath_stat_from_cache(opts_findrp,zpath));
      if (!ok && r->remote) ok=async_stat(opts_findrp,zpath);
      if (!ok) ok=zpath_stat_direct(opts_findrp,zpath,0);
    }else{
      ok=!stat(RP(),&zpath->stat_rp);
    }
    IF1(WITH_PRELOADDISK, if (!ok) ok=path_with_compress_sfx_exists(zpath));
  }
  if (ok){
    zpath->stat_vp=zpath->stat_rp;
    //    int idx=VFOLDER_IS_UPDATE(zpath,PRELOAD_UPDATE,INTERNET_UPDATE)?PSEUDO_ENTRYIDX_FOR_UPDATE:0;
    int idx=IS_UPDATE_GO(zpath)?PSEUDO_ENTRYIDX_FOR_UPDATE:0;
    if (zpath->is_decompressed>COMPRESSION_NIL){
      idx=PSEUDO_ENTRYIDX_FOR_DECOMPRESS;
      zpath->stat_vp.st_size=nextRepdigitFileSize(16*zpath->stat_rp.st_size);
    }

    if (!(ZPF(ZP_IS_ZIPENTRY))){
      if (zpath->zipfile_append||zpath->zipfile_cutr) ASSERT(ZPF(VP_IS_ZIP_AS_DIR));
      if (ZPF(VP_IS_ZIP_AS_DIR)) idx=PSEUDO_ENTRYIDX_FOR_ZIP_AS_DIR;
      zpath->stat_vp.st_ino=ZPATH_MAKE_INODE(zpath,idx);
      //if (idx==PSEUDO_ENTRYIDX_FOR_ZIP_AS_DIR)log_debug_now("PSEUDO_ENTRYIDX_FOR_ZIP_AS_DIR %s rp:%s   stat_rp.st_ino: %lu    => stat_vp.st_ino: %lu",VP(),RP(), zpath->stat_rp.st_ino,zpath->stat_vp.st_ino);
    }
  }
  return ok;
}
static bool is_preload_by_selectors(const zpath_t *zpath){
  if (ZPR()==_root_writable) return false;
#define C(c,condition)  if (VFOLDER_FLAGS(zpath)&ID_FLAG(c) && condition)return true
  C(PRELOAD_SELECT_ALL, true);
  C(PRELOAD_SELECT_REMOTE, ZPR()->remote);
  C(PRELOAD_SELECT_ZIP, ZPF(ZP_IS_ZIPENTRY));
  C(VIEWMOD_DECOMPRESS, ZPF(ZP_IS_ZIPENTRY|ZP_IS_COMPRESSEDZIPENTRY)==(ZP_IS_ZIPENTRY|ZP_IS_COMPRESSEDZIPENTRY) || (ZPR()->decompress_mask&~(1<<COMPRESSION_NIL)));
#undef C
  return false;
}


static void _viamacro_warning_zipf(const char *func, const int line, const char *path,zip_t *za, zip_file_t *zf,const char *txt){
  zip_error_t *e=zf?zip_file_get_error(zf):za?zip_get_error(za):NULL;
  if (!e) return;
  const int se=!e?0:zip_error_code_system(e);
  char s[1024];*s=0;  if (se) strerror_r(se,s,1023);
  //const int ze=!e?0:zip_error_code_zip(e);
  //warning(WARN_FHANDLE|WARN_FLAG_ONCE_PER_PATH,path,"%s    sys_err: %s %s zip_err: %s %s",txt,!e?"e is NULL":!se?"":cg_error_symbol(se),s, !ze?"":error_symbol_zip(ze), !ze?"":zip_error_strerror(e));
}




///////////////////////////////////////////
/// Store file size for generated files ///
///////////////////////////////////////////
#if WITH_FILECONVERSION_OR_CCODE
static off_t fsize_from_hashtable(const char *vp,const int vp_l){
  LOCK_N(mutex_dircache, const off_t size=(off_t)ht_get(&_ht_fsize,vp,vp_l,0));
  return size;
}
static void fsize_to_hashtable(const char *vp, const int vp_l, const off_t size){
  LOCK(mutex_dircache,ht_set(&_ht_fsize,vp,vp_l,0,(void*)size));
}
#endif //WITH_FILECONVERSION_OR_CCODE
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add(directory_t,...)
// Returns true on success.
////////////////////////////////////////////////////////////

static void directory_to_cache_maybe(directory_t *dir){
  config_exclude_files(DIR_RP(),DIR_RP_L(),dir->core.files_l, dir->core.fname,dir->core.fsize);
#if WITH_DIRCACHE
  const root_t *r=DIR_ROOT();
  bool doCache=dir->always_to_cache ||
    config_advise_cache_directory_listing(((r&&r->remote)?ADVISE_DIRCACHE_IS_REMOTE:0)|
                                          (VFOLDER_HAS_FLAG(&dir->dir_zpath,VIEWMOD_KEEP_ZIP)?ADVISE_DIRCACHE_IS_AS_IS:0)|
                                          (DIR_IS_ZIP()?ADVISE_DIRCACHE_IS_ZIP:0),
                                          DIR_RP(),DIR_RP_L(), dir->dir_zpath.stat_rp.ST_MTIMESPEC);
  if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(dir));
#endif //WITH_DIRCACHE
}

static bool readdir_from_cache_zip_or_filesystem(const int opts_findrp,directory_t *dir){
  if (!DIR_RP_L()) return false;
#if WITH_DIRCACHE
  {
    bool success=false;
    LOCK_NCANCEL(mutex_dircache,success=dircache_directory_from_cache(dir));
    //log_debug_now("rp:%s  %s  root:%s  remote:%d",DIR_RP(), success_or_fail(success),rootpath(DIR_ROOT()), DIR_ROOT() && DIR_ROOT()->remote);
    if (success) return true;
  }
#endif //WITH_DIRCACHE
  if (opts_findrp&FINDRP_IN_OPEN) dir->always_to_cache=true;
  if (!readdir_async(dir)) return false;
  return true;
}
#define DIRECTORY_PREAMBLE(isZip)    if (DIR_IS_TRY_ZIP()!=isZip) return false;   char *rp; LOCK(mutex_dircache, rp=DIR_RP(); dir->core.files_l=0) // RICHTIG  DIR_IS_TRY_ZIP
#define CONTAINS_PALCEHOLDER(n,zip) IF1(WITH_ZIPENTRY_PLACEHOLDER,dir->has_file_containing_placeholder=dir->has_file_containing_placeholder || strchr(n,PLACEHOLDER_NAME))
static bool readdir_from_zip(directory_t *dir){
  DIRECTORY_PREAMBLE(true);
  zip_t *za=my_zip_open(rp);
  root_update_time(DIR_ROOT(),za?PTHREAD_ASYNC:-PTHREAD_ASYNC,0);
  if (!za) return false;
  //static int count;log_entered_function("# %d  readdir_from_zip('%s')   ",count++,ZP_VP(&dir->dir_zpath));
  const int SB=256,N=zip_get_num_entries(za,0);
  struct zip_stat s[SB]; /* reduce num of pthread lock */
  for(int k=0;k<N;){
    int i=0;
    for(;i<SB && k<N;k++) if (!zip_stat_index(za,k,0,s+i)) i++;
        lock(mutex_dircache);
        root_update_time(DIR_ROOT(),i>0?PTHREAD_ASYNC:-PTHREAD_ASYNC,0);
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
  my_zip_close(za,rp);
  directory_to_cache_maybe(dir);
  return true;
}

#ifndef HAS_DIRENT_D_TYPE
#define HAS_DIRENT_D_TYPE 1
#endif
static bool readir_from_filesystem(directory_t *dir){
  DIRECTORY_PREAMBLE(false);
  DIR *d=opendir(rp);
  zpath_t *zpath=&dir->dir_zpath;
  const int fd=!d?-1:dirfd(d);
  IF_LOG_FLAG(LOG_OPENDIR){ static int count;log_verbose("# %d  opendir('%s')  fd:%d ",count++,rp, fd);}
  if (!d){ log_errno("opendir: %s",rp); return false; }
  inc_count_by_ext(rp,d?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL);
  root_update_time(ZPR(),PTHREAD_ASYNC,0);
  time_t now=time(NULL);
  const bool need_stat=IF01(HAS_DIRENT_D_TYPE,true,ZPR()->remote || dir->when_readdir_call_stat_and_store_in_cache);
  struct stat st;
  struct dirent *de;
  char rp2[MAX_PATHLEN+1],vp2[MAX_PATHLEN+1];
  if (need_stat){
    strcpy(rp2,RP())[RP_L()]='/';
    strcpy(vp2,VP())[VP_L()]='/';
  }
  for(int i=0;(de=readdir(d));i++){
    if (!(i++&255)) root_update_time(ZPR(),PTHREAD_ASYNC,(now=time(NULL)));
    const char *n=de->d_name;
    const int n_l=strlen(n);
    CONTAINS_PALCEHOLDER(n,zip);
    if (config_do_not_list_file(rp,n,n_l)) continue;
    int isdir=IF01(HAS_DIRENT_D_TYPE,0, isdir=(de->d_type==DT_DIR)?1:-1);// cppcheck-suppress selfAssignment
    if (need_stat && RP_L()+1+n_l<MAX_PATHLEN){
      stpcpy(rp2+RP_L()+1,n);
      if (fstatat_direct(fd,&st,rp2)){
        stpcpy(vp2+VP_L()+1,n);
        isdir=S_ISDIR(st.st_mode)?1:-1;
        IF1(WITH_STATCACHE,if(ZPR()) stat_to_cache(FINDRP_STAT_TOCACHE_ALWAYS,&st,vp2,VP_L()+1+n_l,ZPR(),zpath->vfolder,now));
      }
    }
    LOCK(mutex_dircache, directory_add(isdir==1?DIRENT_ISDIR: 0,dir,de->d_ino,n,0,0,0));
  }/* While */
  closedir(d);
  directory_to_cache_maybe(dir);
  return true;
}
/****************************************************************/
/*  The following functions are used to search for a real path  */
/*  for a given virtual path,                                   */
/*  Returns true on success                                     */
/****************************************************************/
//IS_VFOLDER_ROOT DIR_PRELOADED WITH_PRELOADDISK SFX_UPDATE compre
static bool test_realpath_pfx(const bool dirFileconversion,  int opts, zpath_t *zpath, root_t *r){
  //bool debug=strstr(VP(),"db") && r && strstr(rootpath(r),"db");
  //bool debug=r && strstr(rootpath(r),"massive");
  //bool debug=r && ENDSWITH(VP(),VP_L(),"tdf");
  const bool isInternetUD=VFOLDER_HAS_FLAG(zpath,INTERNET_UPDATE), isPreloadUD=VFOLDER_HAS_FLAG(zpath,PRELOAD_UPDATE);
  const int vfolder_l=IS_VFOLDER_ROOT(zpath->vfolder)?VFOLDER_PATH_L(zpath):0;
  //if (isInternetUD)log_entered_function("%s  root:%s  %d vfolder_l:%d",VP(),rootpath(r),IS_UPDATE_GO(zpath),vfolder_l);
  const char *vp=VP()+vfolder_l, *vp0=VP0_L()?VP0()+vfolder_l:vp;
  if ((r!=_root_writable) && VFOLDER_PATH(zpath)==DIR_INTERNET) return false;
  const int vp_l=VP_L()-vfolder_l, vp0_l=(VP0_L()?VP0_L():VP_L())-vfolder_l;
  if (r->path_allow){
    FOREACH_CSTRING(t,r->path_allow) if (cg_path_equals_or_is_parent(*t,strlen(*t),vp,vp_l)) goto allow;
    return false;
  allow:;
  }
  FOREACH_CSTRING(t,r->path_deny)   if (cg_path_equals_or_is_parent(*t,strlen(*t), vp,vp_l)) return false;
  ASSERT(strlen(vp0)==vp0_l);
  ASSERT(strlen(vp)==vp_l);
  if (r->path_prefix_l && !cg_path_equals_or_is_parent(r->path_prefix, r->path_prefix_l, vp0,vp0_l)) return false;
  ZPR()=r;
  zpath_reset_realpath(zpath);
  ZPATH_STRCAT(r->rootpath);
  ZPATH_STRCAT(isPreloadUD?DIR_PRELOADED: isInternetUD?DIR_INTERNET: dirFileconversion?DIR_CONVERTED:   (opts&FINDRP_DIR_PRELOADED_ONLY)?DIR_PRELOADED:NULL);
  const bool isSfxUD=IS_UPDATE_GO(zpath);
  const int l_skip=isInternetUD? VFOLDER_PATH_L(zpath): r->path_prefix_l;
  ZPATH_STRCAT_N(vp0+l_skip,vp0_l-l_skip-(isSfxUD?SFX_UPDATE_L:0));
  if (isInternetUD && isSfxUD) ZPATH_STRCAT(NET_SFX_HEADER);
  ZPATH_COMMIT(realpath);
  //log_debug_now("vp=%s #%d vp0=%s #%d  rp:%s   ",VP(), VP_L(), VP0(), VP0_L(), RP());
  if (ZPF(ZP_OVERFLOW) || !RP_L() || !zpath_stat(opts,zpath))return false;
  if (r->one_file_system && r->st_dev!=zpath->stat_rp.st_dev) return false;
  if (zpath->is_decompressed){
    ZPATH_STRCAT(cg_compression_file_ext(zpath->is_decompressed,NULL));
    ZPATH_COMMIT(realpath);
  }
  const mode_t m=zpath->stat_rp.st_mode;
  if (r->follow_symlinks && !ZPF(ZP_NOT_EXPAND_SYMLINKS) && S_ISLNK(m) && zpath_expand_symlinks(zpath)){
    zpath->stat_rp.st_ino=0;
    return zpath_stat(opts,zpath);
  }
  if (ZPF(ZP_TRY_ZIP)){
    if (!cg_endsWithZip(RP(),0)){ IF_LOG_FLAG(LOG_REALPATH) log_verbose("!cg_endsWithZip rp: %s\n",RP()); return false;}
    if (EP_L()){
      if (filler_readdir_zip(opts,zpath,NULL,NULL,NULL)) return false; /* This sets the file stat of zip entry */
      zpath->flags|=ZP_IS_ZIPENTRY;
    }
    //log_debug_now("ZP_IS_ZIPENTRY %s %d",VP(),ZPF(ZP_IS_ZIPENTRY));
  }
  return true;
}

static bool test_realpath(const int opts,const int zpath_flags,zpath_t *zpath, root_t *r){
  assert(r!=NULL);
  bool ok=false;
  zpath->flags|=zpath_flags;
  //  IF1(WITH_PRELOADDISK,if (VFOLDER_HAS_FLAG(zpath,PRELOAD_UPDATE)) return r==_root_writable && preloaddisk_test_realpath_preloaded_strcat_rp(true,zpath));
  IF1(WITH_PRELOADDISK,if (IS_SPECIALFILE(zpath,SFILE_PRELOAD_UPDATE_GO)) return r==_root_writable && preloaddisk_test_realpath_preloaded_strcat_rp(true,zpath));
  IF1(WITH_FILECONVERSION, if (r==_root_writable && VFOLDER_HAS_FLAG(zpath,VIEWMOD_FILECONVERSION) && test_realpath_pfx(true,opts,zpath,r)) ok=true);
  IF1(WITH_PRELOADDISK, ok=ok || r==_root_writable && preloaddisk_test_realpath_preloaded_strcat_rp(false,zpath));
  if (!ok) ok=test_realpath_pfx(false,opts,zpath,r);
  if (!ok && (zpath_flags&ZP_RESET_IF_NEXISTS)) zpath_reset_keep_VP(zpath);
  //log_exited_function("VP:%s root:%s RP:%s ZPRP:%s  %s",VP(),rootpath(r),RP(),ZPRP(),success_or_fail(ok));
  return ok;
}
static bool zpath_expand_symlinks(zpath_t *zpath){
  char target[PATH_MAX],absolute_target[PATH_MAX];
  if (cg_readlink_absolute(true,RP(),target,absolute_target)) return false;
  root_t *parent_root=NULL;
  foreach_root(r) if (cg_path_equals_or_is_parent(r->rootpath,r->rootpath_l,absolute_target,strlen(absolute_target))){ parent_root=r; break;}
  const bool ok=parent_root || config_allow_expand_symlink(RP(),target,absolute_target);
  //log_verbose("%s -> %s    '%s' parent_root:%s",RP(),target,absolute_target,yes_no(parent_root!=NULL));
  if (ok){
    zpath_set_realpath(zpath,absolute_target,NULL,NULL);
    if (parent_root) ZPR()=parent_root;
  }
  //log_exited_function("RP:'%s'    absolute_target:'%s'  parent_root:%s ok:%d",RP(),absolute_target,parent_root?parent_root->rootpath:"",ok);
  return ok;
}
/* Uses different approaches and calls test_realpath */
/* Initially, only zpath->virtualpath is defined. */
static bool find_realpath_for_root(const int opts,zpath_t *zpath,root_t *r){
  //if (VFOLDER_HAS_FLAG(zpath,PRELOADED_UPDATE))
  //  log_entered_function(" VP:%s  VP_L:%d  zipfile_l=%d  root=%s",VP(), VP_L(), zpath->zipfile_l,rootpath(r));
  if (r){
    if (r==_root_writable?VFOLDER_HAS_FLAG(zpath,VIEWMOD_1_NOT): IS_VFOLDER_SKIP_READONLY_ROOT(zpath)) return false;
    if (!wait_for_root_timeout(r)) return false;
  }
  if (VP_L()){
    if (zpath->zipfile_l){
      ZPATH_NEWSTR(virtualpath_without_entry);
      ZPATH_STRCAT_N(VP(),zpath->zipfile_l);
      ZPATH_STRCAT(zpath->zipfile_append);
      ZPATH_COMMIT_HASH(virtualpath_without_entry);
      ZPATH_NEWSTR(entry_path);
      {
        const int pos=VP0_L()+zpath->zipfile_cutr+1-cg_strlen(zpath->zipfile_append);
        if (pos<VP_L()) ZPATH_STRCAT(VP()+pos);
      }
      if (ZPF(ZP_OVERFLOW)) return false;
      ZPATH_COMMIT(entry_path);
      zpath_assert_strlen();
      if (test_realpath(opts,ZP_RESET_IF_NEXISTS|(VFOLDER_HAS_FLAG(zpath,VIEWMOD_KEEP_ZIP)?0:ZP_TRY_ZIP),zpath,r)){  /* ZP_TRY_ZIP is being set */
        if (!EP_L()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
        return true;
      }
    }
    IF1(WITH_ZIPFLAT, const yes_zero_no_t i=find_realpath_try_zipflat_rules(zpath,r); if (i) return i==YES);
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  const bool ok=test_realpath(opts,ZP_RESET_IF_NEXISTS,zpath,r);
  //log_debug_now(" %s  VP_L:%d  zipfile_l=%d  root=%s  %s  rp:%s",VP(), VP_L(), zpath->zipfile_l,rootpath(r),success_or_fail(ok) ,RP());
  return ok;
} /*find_realpath_for_root */
static long search_file_which_roots(const zpath_t *zpath){
  if (ZPATH_IS_FILECONVERSION()){
#if WITH_FILECONVERSION
    struct fileconversion_files ff={0};
    struct_fileconversion_files_init(&ff,VP(),VP_L()-(ENDSWITH(VP(),VP_L(),".log")?4:0));
    const bool ok=fileconversion_realinfiles(&ff);
    struct_fileconversion_files_destroy(&ff);
    if (ok) return 1;
#endif //WITH_FILECONVERSION
  }
  return config_search_file_which_roots(VP(),VP_L());
}
static bool find_realpath_in_roots(int opts,zpath_t *zpath, const long roots){
  if (!roots) return false;
  if (VFOLDER_HAS_FLAG(zpath,VIEWMOD_KEEP_ZIP)) opts|=FINDRP_IS_PFXPLAIN;
  zpath_reset_keep_VP(zpath);
  if (!(opts&FINDRP_CACHE_NOT)){
    IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, yes_zero_no_t ok=transient_cache_find_realpath(zpath); if (ok) return ok==YES);
    IF1(WITH_ZIPFLATCACHE,              if (zipflatcache_find_realpath(zpath,roots)) return true);
  }
  foreach_root(r){
    if (roots&(1<<rootindex(r)) && find_realpath_for_root(opts,zpath,r)){
      ASSERT(zpath->realpath!=0);
      IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, LOCK(mutex_fhandle,transient_cache_store(zpath,VP(),VP_L())));
      return true;
    }
  }
  return false;
}
static bool find_realpath(const int opts,zpath_t *zpath){
  //log_entered_function("%s ",VP());
  const long roots=search_file_which_roots(zpath);
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, foreach_root(r){ yes_zero_no_t ok=transient_cache_find_realpath(zpath); if (ok)return ok==YES;});
#define F(opt,mask) if ((mask) && find_realpath_in_roots(opts|opt,zpath,roots&(mask))) return true
  if (_root_writable){
    F(FINDRP_DIR_PRELOADED_ONLY,1);
    F(0,roots&1);
  }
  foreach_root(r) if (r->remote) F(FINDRP_CACHE_ONLY, 1<<rootindex(r));
  foreach_root(r) if (r!=_root_writable) F(0,1<<rootindex(r));
#undef F
  if (IF1(WITH_FILECONVERSION,!ZPATH_IS_FILECONVERSION() &&)  !config_not_report_stat_error(VP(),VP_L()) IF1(WITH_INTERNET_DOWNLOAD, && !net_is_internetfile(VP(),VP_L()))){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,VP(),"Not found");
  }
  return false;
} /* find_realpath_any_root */


static bool _find_realpath_other_root(zpath_t *zpath){ /*TODO*/
  if (DEBUG_NOW==DEBUG_NOW) return false; // _find_realpath_other_root TODO
  // find_realpath_other_root() ->  test_realpath_pfx() -> strgs_l wird immer laenger.
  //log_entered_function("%s",VP());
  assert(ZPR());
  assert(zpath->realpath);
  const off_t size0=zpath->stat_rp.st_size;
  ASSERT(zpath->stat_rp.st_ino);
  const root_t *prev=NULL;
  foreach_root(r){
    if (prev==ZPR()){
      zpath->strgs_l=zpath->realpath;
      zpath->realpath=0;
      if (test_realpath(0,0,zpath,r) && size0==zpath->stat_rp.st_size) return true;  // Not call test_realpath !!!!
    }
    prev=r;
  }
  return false;
} /*find_realpath_any_root*/
static bool find_realpath_other_root(zpath_t *zpath){
  cg_thread_assert_not_locked(mutex_fhandle);
  LOCK_N(mutex_fhandle,zpath_t zp=*zpath);
  const bool found=_find_realpath_other_root(&zp);
  if (found){ LOCK(mutex_fhandle,*zpath=zp);}
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
/* static MAYBE_INLINE fHandle_t* XXXXfhandle_at_index(int i){ */
/*   ASSERT_LOCKED_FHANDLE(); */
/*   static fHandle_t *_fhandle[FHANDLE_BLOCKS]; */
/* #define B (_fhandle[i>>FHANDLE_LOG2_BLOCK_SIZE]) */
/*   fHandle_t *block=B; */
/*   if (!block){ */
/*     block=B=cg_calloc(COUNT_FHANDLE_ARRAY_MALLOC,FHANDLE_BLOCK_SIZE,sizeof(fHandle_t)); */
/*     assert(block!=NULL); */
/*   } */
/*   return block+(i&(FHANDLE_BLOCK_SIZE-1)); */
/* #undef B */
/* } */


static MAYBE_INLINE fHandle_t* fhandle_at_index(int i){
  ASSERT_LOCKED_FHANDLE();
  static fHandle_t *_fhandle[FHANDLE_BLOCKS];
#define B (_fhandle[i>>FHANDLE_LOG2_BLOCK_SIZE])
  if (!B) B=cg_calloc(COUNT_FHANDLE_ARRAY_MALLOC,FHANDLE_BLOCK_SIZE,sizeof(fHandle_t));
  return B+(i&(FHANDLE_BLOCK_SIZE-1));
#undef B
}


static void _fhandle_lock( int id,fHandle_t *d){
  LOCK(mutex_fhandle,if (!fhandle_mutex_initialized(id,d)){pthread_mutex_init(d->mutex+id,NULL);d->flags|=fhandle_mutex_flag(id);});
  pthread_mutex_lock(d->mutex+id);
}
static void _fhandle_unlock(const int id,fHandle_t *d){
  if (fhandle_mutex_initialized(id,d)) pthread_mutex_unlock(d->mutex+id);
}



static void fhandle_init(fHandle_t *d, const zpath_t *zpath){
  *d=FHANDLE_EMPTY;
  d->zpath=*zpath;
  d->zpath.flags|=ZP_IS_IN_FHANDLE;
  d->filetypedata=filetypedata_for_ext(VP(),D_ROOT(d));
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
  d->fhandle_fh=fh;
  d->flags=flags;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, transient_cache_activate(d));
  //if (!(flags&FHANDLE_SPECIAL_FILE)) preloadram_infer_from_other_handle(d);
  d->flags|=FHANDLE_ACTIVE;
  COUNTER1_INC(COUNT_FHANDLE_CONSTRUCT);
  //log_exited_function("VP: %s %p",D_VP(d),d);
  d->pid=get_request_pid();
  return d;
}


/*******************************/
/* uint64_t fuse_file_info->fh.
   Either  serves as ID of fHandle_t instance */
/*******************************/
enum { LOG2_FD_ZIP_MIN=20};
enum { FD_ZIP_MIN=1<<LOG2_FD_ZIP_MIN};
static uint64_t next_fh(){
  static uint64_t fh=FD_ZIP_MIN;
  if (++fh==UINT64_MAX) fh=FD_ZIP_MIN;
  return fh;
}
static fHandle_t* fhandle_create(const int flags, uint64_t *fh, const zpath_t *zpath){

  cg_thread_assert_not_locked(mutex_fhandle);
  LOCK(mutex_fhandle,*fh=next_fh());
  while(true){
    LOCK_N(mutex_fhandle, fHandle_t *d=_fhandle_create_locked(flags,*fh,zpath)); /* zpath is now stored in fHandle_t */
    if (d) return d;
    if (zpath) log_verbose("Going to sleep and retry fhandle_create %s ...",VP());
    usleep(1000*1000);
  }
}
static int fhandle_active_readers_writers(const fHandle_t *d){
  const int b=!d?0:atomic_load(&d->is_busy);
  assert(b>=0);
  return b;
}
static void fhandle_try_destroy(fHandle_t *d){
  ASSERT_LOCKED_FHANDLE();
  //log_entered_function("%s  FHANDLE_NEED_INVALIDATE_PATH: %s", D_VP(d),yes_no(d->flags&FHANDLE_NEED_INVALIDATE_PATH));
  if (fhandle_active_readers_writers(d)){  warning(WARN_FLAG_ERROR,D_VP(d),"fhandle_active_readers_writers() %p",d);return;}
  IF1(WITH_PRELOADRAM,if (d->preloadram && (d->flags&FHANDLE_PRELOADRAM_MASTER) && !preloadram_try_destroy(d)) return);
  IF1(WITH_FILECONVERSION, const int fd=d->fd_real; d->fd_real=0;if (fd) close(fd));
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,transient_cache_destroy(d));
  fhandle_zip_fclose(true,d);
  RLOOP(i,2) if (fhandle_mutex_initialized(i,d)) pthread_mutex_destroy(d->mutex);
  IF1(WITH_FILECONVERSION, fc_maybe_reset_atime_in_future(d));
  IF1(WITH_EVICT_FROM_PAGECACHE,if (!fhandle_find_identical(d)) maybe_evict_from_filecache(0,D_RP(d),D_RP_L(d),D_EP(d),D_EP_L(d)));
  *d=FHANDLE_EMPTY;
  COUNTER2_INC(COUNT_FHANDLE_CONSTRUCT);

}
static fHandle_t* fhandle_get(const char *vp_or_null,const uint64_t fh){
  ASSERT_LOCKED_FHANDLE();
  const ht_hash_t h=hash_value_strg(vp_or_null);
  foreach_fhandle_including_pending_destruct(id,d){
    if (!vp_or_null){
      if (d->flags&FHANDLE_DESTROY_LATER) fhandle_try_destroy(d);
    }else if (fh==d->fhandle_fh && D_VP_HASH(d)==h && !strcmp(vp_or_null,D_VP(d))){
      return d;
    }
  }
  return NULL;
}

static bool fhandle_find_identical(const fHandle_t *d){
  FOREACH_FHANDLE(ie,e) if (fhandle_virtualpath_equals(d,e)) return true;
  return false;
}
/* ******************************************************************************** */
/* *** Inode *** */
static ino_t next_inode(void){
  static ino_t seq=1UL<<63;
  return seq++;
}


static ino_t make_inode(const ino_t inode0,root_t *r, const int entryIdx,const char *for_err_msg){
  const static int SHIFT_FSID=42,SHIFT_ENTRY=(SHIFT_FSID+LOG2_FILESYSTEMS);
  const ino_t fsid=r?r->seq_fsid:LOG2_FILESYSTEMS; /* Better than rootindex(r) */
  if (!inode0) warning(WARN_INODE|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_MAYBE_EXIT,"","for_err_msg='%s' inode0 is zero",for_err_msg);
  if (inode0<(1ULL<<SHIFT_FSID) && entryIdx<(1LLU<<(63-SHIFT_ENTRY))){  // (- 63 46)
    const ino_t ino=inode0| (((int64_t)entryIdx)<<SHIFT_ENTRY)| (fsid<<SHIFT_FSID);
    return ino;
  }else{
    const uint64_t key2=entryIdx|(fsid<<(64-LOG2_FILESYSTEMS)),key_high_variability=(inode0+1)^key2;
    /* Note: Exclusive or with keys. Otherwise no variability for entries in the same ZIP
       inode0+1:  The implementation of the hash map requires that at least one of  both keys is not 0. */
    LOCK_NCANCEL_N(mutex_dircache,
                   ht_entry_t *e=ht_numkey_get_entry(&r->ht_inodes,key_high_variability,key2,true);
                   ino_t inod=(ino_t)e->value;
                   if (!inod){e->value=(void*)(inod=next_inode()); COUNTER1_INC(COUNT_SEQUENTIAL_INODE);}
                   );
    return inod;
  }
}
static ino_t inode_from_virtualpath(const char *vp,const int vp_l){
      lock(mutex_dircache);
      ht_entry_t *e=ht_get_entry(&_ht_inodes_vp,vp,vp_l,0,true);
      if (!e->value) e->value=(void*)next_inode();
      ino_t i=(ino_t)e->value;
  unlock(mutex_dircache);
  return i;
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

static void filler_add(fuse_fill_dir_t filler,void *buf, const char *name, int name_l, const struct stat *st, ht_t *no_dups){
  if (strchr(name,'/')) return;
  if (!name_l) name_l=strlen(name);
  if (name[name_l]){
    char tmp[name_l+1];
    cg_strncpy0(tmp,name,name_l);
    name=tmp;
  }
  if(ht_only_once(no_dups,name,name_l)){
    assert_validchars(VALIDCHARS_FILE,name,name_l);
    filler(buf,name,st,0 COMMA_FILL_DIR_PLUS);
  }
}


static int filler_readdir_zip(const int opts_findrp,zpath_t *zpath,void *buf, fuse_fill_dir_t filler,ht_t *no_dups){
  //log_entered_function("%s",VP());
  ASSERT(!VFOLDER_HAS_FLAG(zpath,VIEWMOD_KEEP_ZIP));
  char ep[MAX_PATHLEN+1]; /* This will be the entry path of the parent dir */
  const int ep_l=filler?EP_L():MAX_int(0,cg_last_slash(EP()));
  memcpy(ep,EP(),ep_l); ep[ep_l]=0;
  const char *lastComponent=EP()+ep_l+(ep_l>0);
  const int lastComponent_l=EP_L()-(ep_l+(ep_l>0));
  if(!filler && !EP_L()) return 0; /* When the virtual path is a Zip file then just report success */
  //  if (!zpath_stat(zpath,ZPR())) return ENOENT;
  if (!zpath_stat(opts_findrp,zpath)) return ENOENT;
  directory_t mydir={0}, *dir=&mydir; mydir.debug=true;
  directory_init_zpath(dir,zpath);
  if (!readdir_from_cache_zip_or_filesystem(opts_findrp,dir)) return ENOENT;
  IF1(WITH_ZIPFLATCACHE,LOCK(mutex_dircache,zipflatcache_store_allentries_of_dir(dir)));
  char u[MAX_PATHLEN+1]; /* entry path expanded placeholder */
  directory_core_t dc=dir->core;
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
      st->st_ino=ZPATH_MAKE_INODE(zpath,idx+INODE_OFFSET_ENTRY);
      //log_debug_now("ZZZZZ iiiiiiiiiiiii  %s %lu",u,st->st_ino);
      st->st_mtime=Nth0(dc.fmtime,i);
      if (!filler){  /* ---  Called from test_realpath_or_reset() to set zpath->stat_vp --- */
        zpath->stat_vp.st_ino=st->st_ino;
        zpath->stat_vp.st_uid=getuid();
        zpath->stat_vp.st_gid=getgid();
        ASSERT(dir->files_capacity>=dc.files_l);
        if (Nth0(dc.fflags,i)&DIRENT_IS_COMPRESSEDZIPENTRY) zpath->flags|=ZP_IS_COMPRESSEDZIPENTRY;
        zpath->zipcrc32=Nth0(dc.fcrc,i);
        directory_destroy(dir);
        return 0;
      }
      filler_add(filler,buf,n,n_l,st,no_dups);
    }
  }
  directory_destroy(dir);
  return filler?0:ENOENT;
}/*filler_readdir_zip*/


static void filler_from_dir_generated(fuse_fill_dir_t filler,const directory_t *dir,void *buf, ht_t *no_dups){
  //log_debug_now("filler_from_dir %d",dir->core.files_l);
  struct stat st;
  char u[PATH_MAX];
  FOR(i,0,dir->core.files_l){
    int u_l=zipentry_placeholder_expand(u,dir->core.fname[i],ZP_RP(&dir->dir_zpath) ,dir);
    stat_init(&st,dir->core.fsize[i],NULL);
    st.st_ino=dir->core.finode[i];
    filler_add(filler,buf,u,u_l,&st,no_dups);
  }
}

// WITH_PRELOADDISK
static void filler_readdir(zpath_t *zpath, void *buf, fuse_fill_dir_t filler,ht_t *no_dups,directory_t *dir_generated){
  //log_entered_function("VP=%s  RP:%s    dir_generated:%d",VP(),RP(),dir_generated->core.files_l);
  if (zpath->zipfile_l && ZPF(ZP_TRY_ZIP) && filler_readdir_zip(0,zpath,buf,filler,no_dups)) return;
  ASSERT(zpath->stat_rp.st_ino);
  if (!zpath->stat_rp.st_ino) return;
  ASSERT(ZPR());
  const bool isInternetUD=VFOLDER_HAS_FLAG(zpath,INTERNET_UPDATE), isInternet=VFOLDER_PATH(zpath)==DIR_INTERNET,is_as_is=VFOLDER_HAS_FLAG(zpath,VIEWMOD_KEEP_ZIP);
  IF1(WITH_PRELOADDISK, const int decompress_mask=zpath_decompress_mask(zpath));
  char dirname_from_zip[MAX_PATHLEN+1];
  directory_t dir={0};
  bool readdir_success=true;
  directory_init_zpath(&dir,zpath);
  readdir_success=readdir_from_cache_zip_or_filesystem(0,&dir);
  if (readdir_success){
    const directory_core_t dc=dir.core;
    FOR(i,0,dc.files_l){
      const off_t size=Nth0(dc.fsize,i);
      char u[MAX_PATHLEN+1];*u=0; /*buffer for dir entry  name*/
      int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),&dir);
      if (!u_l || cg_empty_dot_dotdot(u) || ht_get(no_dups,u,u_l,0) || config_do_not_list_file(RP(),u,u_l)) continue;
      const bool isDir=(Nth0(dc.fflags,i)&DIRENT_ISDIR);
      if (!isDir){
        IF1(WITH_PRELOADDISK,      if (VFOLDER_HAS_FLAG(zpath,PRELOAD_UPDATE)){strcpy(u+u_l,SFX_UPDATE); u_l+=SFX_UPDATE_L;});
        IF1(WITH_INTERNET_DOWNLOAD,if ((isInternetUD||isInternet) && !(u_l=net_direntry(zpath,u,u_l))) continue);
      }
      IF1(WITH_ZIPFLAT,if (!is_as_is && config_skip_zipfile_show_zipentries_instead(u,u_l) && readdir_zipflat_from_cache(zpath,u,buf,filler,no_dups,dir_generated)) continue);
      const ino_t finode=Nth0(dc.finode,i);
      {
        struct stat st={0};
        stat_init(&st,isDir?-1:size,NULL);
        if (finode) st.st_ino=make_inode(finode,ZPR(),i+INODE_OFFSET_ENTRY,__func__);
        *dirname_from_zip=0;
        const bool also_show_zip_file_itself=!is_as_is && config_zipfilename_to_virtual_dirname(dirname_from_zip,u,u_l);
        if (!*dirname_from_zip || also_show_zip_file_itself) filler_add(filler,buf,u,u_l,&st,no_dups);
        if (*dirname_from_zip){
          stat_set_dir(&st);
          st.st_ino=make_inode(finode,ZPR(),PSEUDO_ENTRYIDX_FOR_ZIP_AS_DIR,__func__);
          filler_add(filler,buf,dirname_from_zip,0,&st,no_dups);
          //log_debug_now("Virtual: %s  rp: %s    finode=%lu -> ino:%lu ",dirname_from_zip, u, finode, st.st_ino);
        }
        IF1(WITH_FILECONVERSION, if (VFOLDER_HAS_FLAG(zpath,VIEWMOD_FILECONVERSION)) fileconversion_add_to_dir(buf,u,u_l,no_dups,dir_generated));
      }
#if WITH_PRELOADDISK
      if (!isDir && decompress_mask){
        FOR(iCompress,1,COMPRESSION_NUM){
          if (!(decompress_mask&(1<<iCompress))) continue;
          int x_l; const char *x=cg_compression_file_ext(iCompress,&x_l);
          if (!cg_endsWith(0,u,u_l,x,x_l)) continue;
          u[u_l-=x_l]=0;
          if (!ht_get(no_dups,u,u_l,0)){
            directory_add(0,dir_generated, make_inode(finode,ZPR(),PSEUDO_ENTRYIDX_FOR_DECOMPRESS,u),u,size?nextRepdigitFileSize(64*size):9999999999L, Nth0(dc.fmtime,i),0);
          }
          break;
        }
      }
#endif //WITH_PRELOADDISK
    }
    directory_destroy(&dir);
  }
  //log_exited_function("VP=%s  RP:%s",VP(),RP());
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
  char parent[PATH_MAX]; cg_stpncpy0(parent,rp,slash);
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
  if (!_writable_path_l) return EACCES;/* Only first root is writable */
  if (config_not_overwrite(vip->vp,vip->vp_l)){
    bool found;FIND_REALPATH(vip);
    if (found && ZPR()>0) return EACCES;
  }
  assert(_writable_path_l+vip->vp_l<MAX_PATHLEN);
  const int slash=cg_last_slash(vip->vp);
  REALPATH_WRITABLE_FOLDER(rp,vip,"");
  // See realpath_writable_folder // DEBUG_NOW
  if (slash<=0) return 0;
  char parent[slash+1]; cg_strncpy0(parent,vip->vp,slash);
  NEW_VIRTUALPATH(parent);
  bool found;FIND_REALPATH(&vipa);
  return !found? ENOENT:mk_parentdir_if_sufficient_storage_space(rp);
}

/********************************************************************************/
// FUSE FUSE 3.0.0rc3 The high-level init() handler now receives an additional struct fuse_config pointer that can be used to adjust high-level API specific configuration options.
#define WITH_LIBFUSE_CACHE_STAT 0
#define EVAL(a) a
#define EVAL2(a) EVAL(a)
static pid_t get_request_pid(void){
  assert(fuse_get_context()->fuse==_fuse);
  return fuse_get_context()->pid;
}

static void *xmp_init(struct fuse_conn_info *conn IF1(WITH_FUSE_3,,struct fuse_config *cfg)){
  //void *x=fuse_apply_conn_info_opts;  //cfg-async_read=1;
#if WITH_FUSE_3
  cfg->use_ino=1;
  IF1(WITH_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=200;cfg->negative_timeout=20);
  IF0(WITH_LIBFUSE_CACHE_STAT,cfg->entry_timeout=cfg->attr_timeout=2;  cfg->negative_timeout=10);
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
  _fuse=fuse_get_context()->fuse;
  _fuse_max_write=conn->max_write;
  return NULL;
}

/*************************************************************/
/* Get the tree of folder names from the  XMACRO_SPECIAL_FILES */
/*************************************************************/
static void virtualfolder_new(virtualfolder_t *f, const vfolder_flags_t flags, const char *pathcomponents[]){
  char path[99];
  {
    char *path_e=path;
    FOREACH_CSTRING(b,pathcomponents) if (*b){ if (**b!='/') *path_e++='/'; path_e=stpcpy(path_e,*b); }
    *path_e=0;
  }
  FOREACH_CSTRING(dir,_vfolders_in_zipsfs) if (!strcmp(path,*dir)){f->path=*dir; goto found;}
  f->path=strdup(path);
 found:;
  f->path_l=strlen(path);
  f->flags=flags;
  IF1(WITH_EXTRA_ASSERT, if (f->path &&!strcmp(f->path,DIR_INTERNET))assert(f->path==DIR_INTERNET));
}
static char _dirOldLogs[MAX_PATHLEN+1];
static void specialfile_init(const int id,  const char *filename,const char *parent){
  assert(id<SFILE_NUM);
  if (id==SFILE_CLEANUP_SH && !_writable_path) return;
  const bool has_real_path=filename && *filename=='/';
  specialfile_t *sf=_specialfiles+id;
  if (filename)  sf->name_l=strlen((sf->name=filename+has_real_path));
  sf->parent=parent;
  sf->id=id;
  if (has_real_path){
    char path[MAX_PATHLEN+1],tmp[MAX_PATHLEN+1];
    snprintf(path,MAX_PATHLEN,"%s/%s",_dot_ZIPsFS,sf->name);
    if (id==SFILE_CLEANUP_SH && _writable_path) snprintf(path,MAX_PATHLEN,"%s%s/%s",_writable_path,sf->parent,sf->name);
    sf->rp=strdup_untracked(path);
    struct stat st;
    if (id==SFILE_LOG_ERRORS||id==SFILE_LOG_WARNINGS){
      if (!lstat(path,&st) && st.st_size){ /* Save old logs with a mtime in file name. */
        const time_t t=st.st_mtime;
        struct tm lt;
        localtime_r(&t,&lt);
        snprintf(tmp,MAX_PATHLEN,"%s/%s",_dirOldLogs,sf->name);
        strftime(strrchr(tmp,'.'),22,"_%Y_%m_%d_%H:%M:%S",&lt);
        strcat(tmp,".log");
        if (cg_rename(path,tmp)) DIE("rename");
        const char *cmd[]={"gzip","-f","--best",tmp,NULL};
        cg_fork_exec(cmd,NULL,0,0,0);
      }
#define F _fWarnErr[id==SFILE_LOG_ERRORS]
      if (!(F=fopen(path,"w"))) DIE("Failed open '%s'",path);
      fprintf(F,"%s\n",path);
#undef F
    }
  }
}

static void specialfiles_init(){
#define X(id,...) specialfile_init(ID_##id,"_README_"#id".html",NULL);
  XMACRO_DIRFLAGS(); /* READMEs in each special folder */
#undef X
  char path[MAX_PATHLEN+1],tmp[MAX_PATHLEN+1];
  assert(_mnt);
  {
    char *d=path+strlen(cg_copy_path(path,PATH_DOT_ZIPSFS));
    strcat(d,_mnt);
    while(*++d) if (*d=='/') *d='_';
  }
  snprintf(_dirOldLogs,MAX_PATHLEN,"%s%s",path,"/old_logs");
  cg_recursive_mkdir(_dirOldLogs);
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
  _dot_ZIPsFS=strdup_untracked(path);
#define X(filename,parent,id) specialfile_init(id,filename,parent);
  XMACRO_SPECIALFILES();
#undef X
}
static const char *_ffd_foldername;
static int _ffd_length, _ffd_id_from,_ffd_id_to;
static vfolder_flags_t _flags_from_dirflags(const int id, const char *symbol, const char *dirflag){
  if (!symbol || !*symbol || !dirflag || !*dirflag || *dirflag=='-' && !dirflag[1] || !num_in_range(id,_ffd_id_from,_ffd_id_to)) return 0;
  if (*dirflag=='-'){
    if (strstr(_ffd_foldername,dirflag)) goto ok;
  }else{
    for(const char *c=_ffd_foldername;*c; c++) if (*c==*dirflag && (c==_ffd_foldername||c[-1]!='-')) goto ok;
  }
  return 0;
 ok:;
  _ffd_length+=strlen(dirflag);
  //log_debug_now("'%s'  Found dirflag %s  %ld",foldername, dirflag,strlen(dirflag));
  return 1ULL<<id;
}
static vfolder_flags_t flags_from_dirflags(const char *foldername, const int id_from, const int id_to){
  if(!strcmp(foldername,"-")) return num_in_range(id_from,ID_RANGE_VIEWMOD)?ID_FLAG(VIEWMOD_NIL):0;
  _ffd_length=0;
  _ffd_foldername=foldername;
  _ffd_id_from=id_from;
  _ffd_id_to=id_to;
  vfolder_flags_t flags=0;
#define X(id,dirflag) flags|=_flags_from_dirflags(ID_##id,#id,dirflag);
  XMACRO_DIRFLAGS();
#undef X
  if (_ffd_length!=strlen(foldername)) DIE("foldername: '%s':%zu / %d",foldername,strlen(foldername),_ffd_length);
  return flags;
}
static void virtualfolders_init(){
  //log_entered_function("");
  int vf_n=1; _virtualfolders->path=""; /* First element as default */
  FOREACH_CSTRING(d,_vfolders_in_zipsfs){
    virtualfolder_t *f=_virtualfolders+vf_n++;
    f->path_l=strlen((f->path=*d));
  }
#define N(flags) { assert(VIRTUALFOLDER_MAX>vf_n); virtualfolder_new(_virtualfolders+vf_n++,flags,ee);}
  const char  *ee[5]={DIR_ZIPsFS};
  static const char *vv[]={"-","n","1","-1","c","1z","-1z","z","d","1d","-1d","cd","1dz","-1dz","dc",NULL};
  FOREACH_CSTRING(v_pointer,vv){
    ee[1]=*v_pointer;ee[2]=ee[3]=0;
    const vfolder_flags_t flags_view=flags_from_dirflags(*v_pointer,ID_RANGE_VIEWMOD);
    N(flags_view);
    if (flags_view&ID_FLAG(VIEWMOD_INTERNET)){   ee[2]=DIRNAME_INTERNET_UPDATE;   N(flags_view|(ID_FLAG(INTERNET_UPDATE)));}
    if (flags_view&ID_FLAG(VIEWMOD_DECOMPRESS)){ ee[2]=DIRNAME_PRELOADDISK_UPDATE;N(flags_view|(ID_FLAG(PRELOAD_UPDATE)));}
    if (flags_view&(ID_FLAG(VIEWMOD_NIL)|ID_FLAG(VIEWMOD_KEEP_ZIP)|ID_FLAG(VIEWMOD_DECOMPRESS)|ID_FLAG(VIEWMOD_1)|ID_FLAG(VIEWMOD_1_NOT)|ID_FLAG(VIEWMOD_FILECONVERSION))){
      static const char *preloads[]={"-","m","l","-m","-l", "l-m","-lm","-l-m","lz","l-z",NULL};
      FOREACH_CSTRING(pl,preloads){
        ee[2]=*pl;ee[3]=ee[4]=0;
        const vfolder_flags_t flags=flags_view|flags_from_dirflags(*pl,ID_RANGE_PRELOAD);
        N(flags);
        if (flags&ID_FLAG(PRELOADDISK)){ ee[3]=DIRNAME_PRELOADDISK_UPDATE;N(flags|(ID_FLAG(PRELOAD_UPDATE)));}
        if (flags&(ID_FLAG(PRELOADDISK)|ID_FLAG(PRELOADDISK_ENTIRE_ZIP)|ID_FLAG(PRELOADDISK_ENTIRE_ZIP_NOT)|(ID_FLAG(PRELOADRAM)))){
          static const char *selectors[]={"a","r","z","rz",NULL};
          FOREACH_CSTRING(selector,selectors){
            ee[3]=*selector;
            N(flags|flags_from_dirflags(*selector,ID_RANGE_PRELOAD_SELECT));
          }
        }
      }
    }
  }
#undef N
  FOREACH_VIRTUALFOLDER(,f){
    int childs_n=cg_idx_of_NULL((void*)f->childs,VIRTUALFOLDER_CHILDS_MAX);
    assert(childs_n>=0);
    FOREACH_VIRTUALFOLDER(const,g){
      if (f->path_l<g->path_l && cg_path_equals_or_is_parent(f->path,f->path_l,g->path,g->path_l) && !strchr(g->path+f->path_l+1,'/')){
        assert(childs_n<VIRTUALFOLDER_CHILDS_MAX);
        f->childs[childs_n++]=g->path+f->path_l+1;
      }
    }
  }
}

static int virtualfolders_print_dirflags(char *buf,const int buf_max, const vfolder_flags_t flags){
  int l=0;
#define X(id,...)  if (flags&ID_FLAG(id)){ if (l<buf_max) l+=snprintf(buf+l,buf_max-l,"%s  ",#id);}
  XMACRO_DIRFLAGS();
#undef X
  return l;
}


static void debug_print_virtual_folders(){
  assert(PATH_STARTS_WITH_DIR_ZIPsFS(DIR_ZIPsFS));
  fprintf(stderr,"SFILE_NUM=%d\n",SFILE_NUM);
  fprintf(stderr,ANSI_INVERSE"Checking virtual folders and their subfolders ..."ANSI_RESET"\n");
  FOREACH_VIRTUALFOLDER(const,f){
    fprintf(stderr,"%20s\t"ANSI_FG_MAGENTA,f->path);
    char buf[4096];
    virtualfolders_print_dirflags(buf,4096,f->flags);
    fputs(buf,stderr);
    fputs(ANSI_FG_BLUE,stderr);FOREACH_CSTRING(subdir,f->childs) fprintf(stderr," %s",*subdir);
    fprintf(stderr,ANSI_FG_GREEN"%s"ANSI_RESET"\n",IS_VFOLDER_ROOT(f)?"ROOT": "");
  }
  if (cg_file_exists(__FILE__)){
    fprintf(stderr,ANSI_INVERSE"Checking names of configuration files ..."ANSI_RESET"\n");
    char tmp[PATH_MAX];
    cg_stpncpy0(tmp,__FILE__,cg_last_slash(__FILE__));
    chdir(tmp);
    FOREACH_CSTRING(s, enum_sourcefiles_S) if (!cg_file_exists(*s)) DIE(RED_ERROR"Not found '%s'",*s);
  }
  fprintf(stderr,"Done %s\n",__func__);exit(0);
}
// _specialfiles
static bool specialfile_set_stat(struct stat *st, const virtualpath_t *vipa){
  const int id=vipa->specialfile_id;
  bool ok=false;
  const specialfile_t *sf=_specialfiles+id;
  //log_entered_function("%s id:%d  SFILE_IS_IMMUTABLE:%d IS_UPDATE_GO:%d",vipa->vp,id,SFILE_IS_IMMUTABLE(id),IS_UPDATE_GO(vipa));
  if (vipa->vfile_sfx){ /* e.g. @SOURCE or @PROPERTIES */
    bool found; FIND_REALPATH(vipa);
    if (found){
      stat_init(st,specialfile_print_pathinfo(zpath,NULL),NULL);
      ok=true;
    }
  }else if (SFILE_IS_IMMUTABLE(id) || IS_UPDATE_GO(vipa)){
    stat_init(st,IF01(WITH_PRELOADRAM,0,SFILE_IS_IMMUTABLE(id)?specialfile_size(id):4096),NULL);
    time(&st->st_mtime);
    st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    ok=true;
  }else if (sf->rp){
    ok=!lstat(sf->rp,st);
    if (id==SFILE_INFO){
      if (!ok) stat_init(st,1E6,NULL); else st->st_size+=1E5;
      ok=true;
    }
  }else{
    IF1(WITH_PRELOADRAM, if (trigger_files(vipa->vp,vipa->vp_l)){ stat_init(st,0,NULL);return true;});
  }
  if (ok && ENDSWITH(vipa->vp,vipa->vp_l,".command")) st->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);
  //log_exited_function("%s   %s  S_ISDIR:%d",vipa->vp,success_or_fail(ok),  S_ISDIR(st->st_mode));
  return ok;
}

static void vipa_setSpecialFile(virtualpath_t *vipa){
  FOREACH_VIRTUALFOLDER(const,f){
    if (VFOLDER_PATH_L(vipa)>f->path_l&&cg_path_equals_or_is_parent(f->path,f->path_l,VFOLDER_PATH(vipa),VFOLDER_PATH_L(vipa))) continue; /* Take longest */
    if (!cg_path_equals_or_is_parent(f->path,f->path_l,vipa->vp,vipa->vp_l)) continue;
    vipa->vfolder=f;
  }
  const int vdir_l=VFOLDER_PATH_L(vipa);
  if (vdir_l){
    FOREACH_SPECIAL_FILE(sf){
      if (sf->parent && sf->parent!=VFOLDER_PATH(vipa)) continue;
      if (vdir_l+1+sf->name_l==vipa->vp_l && !strcmp(sf->name,vipa->vp+vdir_l+1)) vipa->specialfile_id=sf->id;
    }
    if (VFOLDER_HAS_FLAG(vipa,PRELOAD_UPDATE)  && ENDSWITH(vipa->vp,vipa->vp_l,SFX_UPDATE)) vipa->specialfile_id=SFILE_PRELOAD_UPDATE_GO;
    if (VFOLDER_HAS_FLAG(vipa,INTERNET_UPDATE) && ENDSWITH(vipa->vp,vipa->vp_l,SFX_UPDATE)) vipa->specialfile_id=SFILE_INTERNET_UPDATE_GO;
  }
}
/****************************************************************************************************************************/
/* Is the virtualpath a zip entry?                                                                                          */
/* Normally, append will be ".Content" and cutr will be 0.                                                                   */
/* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name: append will be empty and cutr will be -4; */
/****************************************************************************************************************************/
static void virtual_dirpath_to_zipfile(virtualpath_t *vipa){
  ASSERT(vipa->vp_l>=0);
  ASSERT(cg_strlen(vipa->vp)>=vipa->vp_l);
  const char *b=vipa->vp;
  for(int i=4;i<=vipa->vp_l;i++){
    if (i==vipa->vp_l || vipa->vp[i]=='/'){
      const int ret=config_virtual_dirpath_to_zipfile(b,vipa->vp+i,&vipa->zipfile_append);
      if (ret!=INT_MAX){
        vipa->zipfile_cutr=-ret;
        vipa->flags|=VP_IS_ZIP_AS_DIR;
        vipa->zipfile_l=ret+i;
        return;
      }
      if (vipa->vp[i]=='/') b=vipa->vp+i+1;
    }
  }
}
static int virtualpath_init(virtualpath_t *vipa, const char *vpath, char *buf){
  *buf=0;
  ASSERT(vpath);
  *vipa=empty_virtualpath;
  vipa->vfolder=_virtualfolders;
  vipa->vp_l=strlen((vipa->vp=vpath+(*vpath=='/'&&!vpath[1])));
  if (PATH_STARTS_WITH_DIR_ZIPsFS(vipa->vp)) vipa_setSpecialFile(vipa);
  if (64+vipa->vp_l+_rootdata_path_max>MAX_PATHLEN) return ENAMETOOLONG;

#define B(l) if (!*buf) strncpy(buf,vipa->vp,l); buf[l]=0
#define E(s) if (ENDSWITH(vipa->vp,vipa->vp_l,s)){ vipa->vfile_sfx=s; B(vipa->vp_l-(sizeof(s)-1));}
  E(VFILE_SFX_ZIPCRC32);E(VFILE_SFX_SOURCE);E(VFILE_SFX_PROPERTIES);
#undef E
  if (cg_strcasestr(vpath,"$NOCSC$")){ /* Windows no-client-side-cache */
    B(vipa->vp_l);
    cg_str_replace(0,buf,0, "$NOCSC$",7,"",0);
  }
  if (*buf){
    vipa->vp_l=strlen(buf);
    vipa->vp=buf;
  }
#undef S
#undef B
  if (!(VFOLDER_HAS_FLAG(vipa,VIEWMOD_KEEP_ZIP))) virtual_dirpath_to_zipfile(vipa);
  return 0;
}
static int virtualpath_error(const virtualpath_t *vipa,const int create_or_del){
  IF0(WITH_PRELOADRAM, if (VFOLDER_HAS_FLAG2(vipa,INTERNET_UPDATE,PRELOAD_UPDATE)) return EACCES);
  if (!_writable_path_l &&  (create_or_del==1 || IS_VFOLDER_SKIP_READONLY_ROOT(vipa))) return EACCES;
  if (vipa->vp_l==0 && VFOLDER_HAS_FLAG(vipa,PRELOADDISK)) return create_or_del==1?EEXIST:EPERM;
  if (VFOLDER_PATH(vipa) && create_or_del){
    if (VFOLDER_PATH(vipa)==DIR_INTERNET && create_or_del==1) return EEXIST;
    if (create_or_del==1 && SFILE_IS_IMMUTABLE(vipa->specialfile_id)) return EACCES;
  }
  return 0;
}
/*  Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now  additional struct fuse_file_info pointer (which, may be NULL even if the file is currently open) */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10)
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 1
#else
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 0
#endif


/********************************************************************************/
/* Consider a root tree with r->path_prefix "/db/pride"                         */
/* The parrents like "/db/" need to be captured and accepted as a valid folder. */
/********************************************************************************/
static bool vp_is_part_of_path_prefix(const zpath_t *zpath){
  static const char *dd[ROOTS+1];
  static int dd_l[ROOTS+1];
  static bool initialized;
  if (!zpath){
    assert(!initialized);
    initialized=true;
    foreach_root(r){
      if (!r->path_prefix_l) continue;
      char d[r->path_prefix_l+1]; strcpy(d,r->path_prefix);
      RLOOP(i,r->path_prefix_l){
        if (i && d[i]=='/'){
          d[i]=0;
          const int j=cg_add_to_strg_array(ADD_TO_STRG_UNIQUE|ADD_TO_STRG_STRDUP,dd,ROOTS,d);
          if (j>=0) dd_l[j]=strlen(d);
        }
      }
    }
  }else{
    assert(initialized);
    const int vfolder_l=IS_VFOLDER_ROOT(zpath->vfolder)?VFOLDER_PATH_L(zpath):0;
    static int ll[ROOTS];
    for(int i=0;dd[i];i++){
      if (cg_path_equals_or_is_parent(dd[i], dd_l[i], VP()+vfolder_l,VP_L()-vfolder_l)) return true;
    }
  }
  return false;
}
// WITH_PRELOADDISK

// specialfile_set_stat specialfile_id
static int _xmp_getattr(const virtualpath_t *vipa, struct stat *st){ /* NOT_TO_HEADER */
  //if (ENDSWITH(vipa->vp,vipa->vp_l,"htmL"))
  //log_entered_function("vipa->vp: %s   vdir=%s       specialfile=%d",vipa->vp, VFOLDER_PATH(vipa),  vipa->specialfile_id);
#define D() {stat_init(st,-1,NULL);    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l); return 0;}
  if (vipa->vp_l==VFOLDER_PATH_L(vipa)) D();
  if (specialfile_set_stat(st,vipa)) return 0;
  IF1(WITH_CCODE, if (c_getattr(st,vipa)) return 0);
  if (VFOLDER_HAS_FLAG2(vipa,PRELOAD_UPDATE,INTERNET_UPDATE) && !IS_UPDATE_GO(vipa)){
    stat_init(st,vipa->specialfile_id?4096:-1,NULL);
    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    time(&st->st_mtime);
    return 0;
  }
  bool found;FIND_REALPATH(vipa);
  //log_debug_now("%s found:%s",vipa->vp, success_or_fail(found));
  int er=0;
  if (found){
    *st=zpath->stat_vp;
    return 0;
  }else{
    if (vp_is_part_of_path_prefix(zpath)) D();
    IF1(WITH_INTERNET_DOWNLOAD, char buf[vipa->vp_l+1]; if (net_getattr(st,buf,vipa->vp, vipa->vp_l)) return 0);
    IF1(WITH_FILECONVERSION,    if (fileconversion_getattr(st,zpath,vipa)) return 0);
  }
  er=ENOENT;
  inc_count_by_ext(vipa->vp,er?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS);
  IF1(WITH_DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (er) debug_track_false_getattr_errors(vipa->vp,vipa->vp_l));
  if (config_file_is_readonly(vipa->vp,vipa->vp_l)) st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  return -er;
#undef D
}/*xmp_getattr*/
static int xmp_getattr(const char *vpath, struct stat *st IF1(WITH_XMP_GETATTR_FUSE_FILE_INFO,,struct fuse_file_info *fi_or_null)){
  FUSE_PREAMBLE(vpath);
  if (!er) er=_xmp_getattr(&vipa ,st);
  if (!er) st->st_mode|=((st->st_mode&S_IFDIR)?0777:(st->st_mode&S_IFREG)?0666:0);
  log_fuse_function(__func__,&vipa,er);
  //log_exited_function("vpath: %s %s ino: %lu  ",vpath, success_or_fail(er==0),st->st_ino);
  return er;
}


#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10) /* Not sure when parameter introduced */
#define WITH_UTIMENS_FUSE_FILE_INFO 1
#else
#define WITH_UTIMENS_FUSE_FILE_INFO 0
#endif
static int xmp_utimens(const char *vpath, const struct timespec ts[2]  IF1(WITH_UTIMENS_FUSE_FILE_INFO,,struct fuse_file_info *fi_not_used)){
  FUSE_PREAMBLE_W(1,vpath);
  bool found; FIND_REALPATH(&vipa);
  er=!found?ENOENT:   ZPR()!=_root_writable?EPERM: utimensat(0,RP(),ts,AT_SYMLINK_NOFOLLOW);
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
  int res=found?0:-ENOENT;
  IF1(WITH_INTERNET_DOWNLOAD,if (!res && VFOLDER_PATH(zpath)==DIR_INTERNET && VP_L()>DIR_INTERNET_L+6 && config_internet_must_not_delete(VP()+(DIR_INTERNET_L+1),VP_L()-(DIR_INTERNET_L+1))) res=-EPERM);
  if (!res) res=!ZPATH_ROOT_WRITABLE()?-EPERM:  minus_val_or_errno(unlink(RP()));
  log_fuse_function(__func__,&vipa,res);
  return res;
}
static int xmp_rmdir(const char *vpath){
  FUSE_PREAMBLE_W(-1,vpath);
  bool found;FIND_REALPATH_NOT_EXPAND_SYMLINK(&vipa);
  const int res=!ZPATH_ROOT_WRITABLE()?-EACCES: !found?-ENOENT: minus_val_or_errno(rmdir(RP()));
  log_fuse_function(__func__,&vipa,res);
  return res;
}
/***************************************************************************************************************/
/* Time consuming processes should be performed in xmp_read() and not in xmp_open().                           */
/* We identify  cases of time consuming file content generation here in xmp_open(). */
/* In those cases we   create an fHandle_t instance.                               */
/* Upon  1st invocation of  xmp_read(), the fHandle_t object is subjected to function fhandle_prepare_in_fuse_read_or_write().  */
/* This is when the file content is generated and stored in RAM or on disk                                       */
/***************************************************************************************************************/

static int open_for_reading(const virtualpath_t *vipa, struct fuse_file_info *fi){
  const int id=vipa->specialfile_id;
  const specialfile_t *sf=_specialfiles+id;
  uint64_t fh=0;
  if (sf->rp){
    fh=open(sf->rp,O_RDONLY);
    if (fh<3){ warning(WARN_OPEN|WARN_FLAG_ERRNO,sf->rp,"open()");  return -errno;}
    fi->fh=fh;
    return 0;
  }
  NEW_ZIPPATH(vipa);
  IF1(WITH_SPECIAL_FILE,fh=specialfile_content_to_fhandle(zpath,id)); /* Only case where file content is generated in xmp_open() */
  if (!fh){ /* ID for fHandle_t  */
    int ff=IF1(WITH_CCODE, config_c_open(C_FLAGS_FROM_ZPATH(zpath),VP(),VP_L())?FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_CCODE:)0;
    //IF1(WITH_INTERNET_DOWNLOAD,  if (!ff && VFOLDER_HAS_FLAG(vipa,INTERNET_UPDATE)) ff=FHANDLE_PREPARE_ONCE_IN_RW);
    if (!ff && find_realpath(FINDRP_IN_OPEN,zpath) IF1(WITH_FILECONVERSION, &&!fileconversion_remove_if_not_uptodate(zpath))){
      if (IS_UPDATE_GO(vipa))  ff=FHANDLE_PREPARE_ONCE_IN_RW; /* after find_realpath() */
      IF1(WITH_PRELOADDISK,      if (!ff && (is_preloaddisk_zpath(zpath) || path_with_compress_sfx_exists(zpath))) ff=FHANDLE_PREPARE_ONCE_IN_RW);
      IF1(WITH_PRELOADRAM,       if (preloadram_advise(zpath,0)) ff=FHANDLE_WITH_PRELOADRAM);
      if (ZPF(ZP_IS_ZIPENTRY))  { ff|=FHANDLE_PREPARE_ONCE_IN_RW; IF0(WITH_INEFFICIENT_ZIP_READING,fi->direct_io=1);}  /*Without direct_io,  multithreaded unordered read.*/
    }else{
      IF1(WITH_INTERNET_DOWNLOAD,if (!ff && net_is_internetfile(VP(),VP_L())) ff=FHANDLE_PREPARE_ONCE_IN_RW);
      IF1(WITH_FILECONVERSION,   if (!ff && fileconversion_check_infiles_exist(vipa)){ LOCK(mutex_fhandle,set_realpath_to_writable_folder(zpath,DIR_CONVERTED)); ff=FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_FILECONVERSION;});
    }
    if (ff) fhandle_create(ff|FHANDLE_PREPARE_ONCE_IN_RW,&fh,zpath);
  }
  if (fh){ fi->fh=fh; return 0;}
  zpath_set_atime(zpath);
  const int fd=async_openfile(zpath,fi->flags); /* POSIX file descriptor */
  if (fd>0){ fi->fh=fd; return 0;} // && (!has_proc_fs() || cg_check_path_for_fd("_xmp_open",RP(),fd))
  if (!config_not_report_stat_error(vipa->vp,vipa->vp_l)) warning(WARN_OPEN|WARN_FLAG_ERRNO,RP(),"open:  fd=%d",fd);
  return errno?-errno:-1;
}/*xmp_open*/
static int xmp_open(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  atomic_fetch_add(&_open_minus_release,1);
  errno=0;
  FUSE_PREAMBLE_W((fi->flags&(O_WRONLY||O_RDWR|O_APPEND|O_CREAT))?1:0,vpath);
  if (!er)	er=_xmp_open(&vipa,fi);
  if (fi->fh>2 && fi->fh<COUNT_BACKWARD_SEEK) _count_backward_seek[fi->fh]=0;
  //  IF_LOG_FLAG_OR(LOG_OPEN,er!=0)log_exited_function("%s res: %d  "ANSI_YELLOW"%ju"ANSI_RESET,vpath,er,UIM(fi->fh));
  return -er;
}

static int _xmp_open(const virtualpath_t *vipa, struct fuse_file_info *fi){
  int res=0;
  if (IS_SPECIALFILE(vipa,SFILE_INFO)){
    if (fi->flags&O_WRONLY) return -EPERM;
    const char *rp=print_info_file();
    if (!rp) return !errno?-1:-errno;
    const int fd=open(rp,O_RDONLY);
    if (fd<3){ warning(WARN_OPEN|WARN_FLAG_ERRNO,rp,"open(O_RDONLY)"); return -errno;}
    fi->fh=fd;
  }else if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))){
    res=-create_or_open(vipa,0775,fi);
    log_fuse_function("open_write",vipa,res);
    //log_debug_now("open_write vp:%s   res=%d ",vipa->vp,res);
  }else{
    res=open_for_reading(vipa,fi);
    log_fuse_function("open_read",vipa,res);
  }
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
  //log_entered_function("%s",vipa->vp);
  FUSE_PREAMBLE(vpath);
  // static int count=0;log_entered_function("# %d %s",count++,vpath);
  const int res=_xmp_readdir(&vipa,buf,filler,offset,fi);
  log_fuse_function(__func__,&vipa,res);
  inc_count_by_ext(vpath,res?COUNTER_READDIR_FAIL:COUNTER_READDIR_SUCCESS);
  return res;
}

static int _xmp_readdir(const virtualpath_t *vipa, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
  //log_entered_function("%s",vipa->vp);
  (void)offset;(void)fi;
  ht_t no_dups={0}; HT_INIT_WITH_KEYSTORE_DIM(&no_dups,8,4096); ht_set_id(HT_MALLOC_without_dups,&no_dups);
  no_dups.keystore->mstore_counter_mmap=COUNT_MSTORE_MMAP_NODUPS;
  no_dups.ht_counter_malloc=COUNT_HT_MALLOC_NODUPS;
  NEW_ZIPPATH(vipa);
  bool ok=false;
  const int vp_l=VP_L();
  {
    directory_t dir_generated={0};
    int opt_rp=0;
    foreach_root(r){ /* FINDRP_FILECONVERSION_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
      if (r!=_root_writable && IS_VFOLDER_SKIP_READONLY_ROOT(vipa)) continue;
      if (vp_l<r->path_prefix_l && IS_VP_IN_ROOT_PFX(r,vipa)){
        ok=true;
        filler_add(filler,buf,r->pathpfx_slash_to_null+vp_l+1,0,NULL,&no_dups);
      }else if (find_realpath_in_roots(opt_rp,zpath,1<<rootindex(r))){
        directory_init_zpath(&dir_generated,zpath);
        filler_readdir(zpath,buf,filler,&no_dups,&dir_generated);
        ok=true;
        IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,if (!VFOLDER_HAS_FLAG(zpath,VIEWMOD_KEEP_ZIP) && ZPF(ZP_IS_FROM_TRANSIENT_CACHE))break); /*Performance*/
        if (config_readir_no_other_roots(RP(),RP_L())) break; /*Performance*/
      }
    }
    filler_from_dir_generated(filler,&dir_generated,buf,&no_dups);
    directory_destroy(&dir_generated);
  }
#define A(n) filler_add(filler,buf,n,0,NULL,&no_dups)
  IF1(WITH_CCODE, if (c_readdir(zpath,buf,filler,NULL)) ok=true);
  if (!vipa->vp_l){
    ok=true;
    A((DIR_ZIPsFS)+1);
  }else if (vp_l==VFOLDER_PATH_L(vipa)){
    FOREACH_CSTRING(subdir, vipa->vfolder->childs) A(*subdir);
    FOREACH_SPECIAL_FILE(sf) if (VFOLDER_PATH(vipa)==sf->parent || (sf->id<VFOLDER_FLAG_NUM && (VFOLDER_FLAGS(vipa)&(1LLU<<sf->id)))) A(sf->name);
    ok=true;
  }
#undef A
  ht_destroy(&no_dups);
  return VFOLDER_HAS_FLAG2(vipa,PRELOAD_UPDATE,INTERNET_UPDATE) || ok?0:-1;
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
  char rp[MAX_PATHLEN+1];
  {
    const int er=realpath_mk_parent(rp,vipa);
    if (er) return -er;
  }
  if (!(fi->fh=MAX_int(0,open(rp,fi->flags|O_CREAT,mode)))){
    log_errno("open(%s,%x,%u) returned -1\n",rp,fi->flags,mode); cg_print_file_mode(mode,stderr); cg_log_open_flags(fi->flags); log_char('\n');
    return errno;
  }
  return 0;
}
static int xmp_create(const char *vpath, mode_t mode,struct fuse_file_info *fi){ /* O_CREAT|O_RDWR goes here */
  FUSE_PREAMBLE(vpath);
  er=create_or_open(&vipa,mode,fi);
  log_fuse_function(__func__,&vipa,er);
  return -er;
}
static int xmp_write(const char *vpath, const char *buf, size_t size,off_t offset, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  FUSE_PREAMBLE_Q(vpath);
  long fd;
  if (!fi){
    char real_path[MAX_PATHLEN+1];
    if ((er=realpath_mk_parent(real_path,&vipa))) return -er;
    fd=MAX_int(0,open(real_path,O_WRONLY));
  }else{
    fd=fi->fh;
    LOCK_N(mutex_fhandle, fHandle_t *d=fhandle_get(vipa.vp,fd); fhandle_busy_start(d));
    if (d){
      fhandle_prepare_in_fuse_read_or_write(d,fi->flags);
      fd=d->fd_real;
      LOCK(mutex_fhandle,fhandle_busy_end(d)); // cppcheck nullPointerOutOfResources
    }
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
  if (!_writable_path_l) return -EPERM;
  FUSE_PREAMBLE_W(1,vpath);
  char rp[MAX_PATHLEN+1];
  if ((er=realpath_mk_parent(rp,&vipa))) return -er;
  log_verbose("Going to symlink( %s , %s ",target,rp);
  if (symlink(target,rp)==-1){ log_errno("symlink( %s , %s ",target,rp); return -errno;}
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
  char rp[_writable_path_l+strlen(neu_path)+1], vp_buffer_neu[MAX_PATHLEN+1];
  virtualpath_t vipa_neu;
  virtualpath_init(&vipa_neu,neu_path,vp_buffer_neu);
  er=realpath_mk_parent(rp,&vipa_neu);
  if (!er) er=cg_rename(RP(),rp);
  return minus_val_or_errno(er);
}
//////////////////////////////////
// Functions for reading bytes ///
//////////////////////////////////
////////////////////////////////////////////////////////////////////
// Wrapping zip_fread, zip_ftell and zip_fseek                     //
// Because zip_ftell and zip_fseek do not work on compressed data  //
/////////////////////////////////////////////////////////////////////
static int my_zip_fclose(zip_file_t *zf,const char *path){
  if (!zf) return 0;
  const int ret=zip_fclose(zf);
  //log_entered_function("zf=%p path=%s ret=%d  %s",zf,path,ret,success_or_fail(ret==0));
  if (ret){
    warning_zip_f(path,zf,"Failed zip_fclose");
  }else{
    COUNTER2_INC(COUNT_ZIP_FOPEN);
  }
  return ret;
}
static int my_zip_close(zip_t *za,const char *path){
  if (!za) return 0;
  //log_entered_function("za=%p path=%s ",za,path);
  int ret=zip_close(za);
  if (ret){
    warning_zip_a(path,za,"Failed zip_close");
  }else{
    COUNTER2_INC(COUNT_ZIP_OPEN);
  }
  return ret;
}
static zip_t *my_zip_open(const char *rp){
  zip_t *za=NULL;
  if (rp){
    if (!cg_endsWithZip(rp,0)){
      IF_LOG_FLAG(LOG_ZIP) log_verbose("Does not end with '.zip'  '%s'",rp);
      return NULL;
    }
    IF_LOG_FLAG(LOG_ZIP){ static int count; log_verbose("Going to zip_open(%s) #%d ... ",rp,count);}
    RLOOP(iTry,2){
      int err=0;
      za=zip_open(rp,ZIP_RDONLY,&err);
      //log_entered_function("za=%p path=%s  %s",za,rp,success_or_fail(za!=NULL));
      inc_count_by_ext(rp,za?COUNTER_ZIPOPEN_SUCCESS:COUNTER_ZIPOPEN_FAIL);
      if (za){
        COUNTER1_INC(COUNT_ZIP_OPEN);
        break;
      }
      warning_zip_a(rp,za,"zip_open failed");
      warning(WARN_OPEN|WARN_FLAG_ERRNO,rp,"err=%d",err);
      usleep(1000);
    }
  }
  //log_exited_function("%s  za=%p",rp,za);
  return za;
}
static zip_file_t *my_zip_fopen(zip_t *za, const char *entry, const zip_flags_t flags, const char *path){
  if (!za){
    warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_FAIL,path,"za is NULL  %s",entry,zip_get_error(za));
    return NULL;
  }
  zip_file_t *zf=zip_fopen(za,entry,flags);
  //log_entered_function("zf=%p path=%s  %s",zf,path,success_or_fail(zf!=NULL));
  if (zf){
    COUNTER1_INC(COUNT_ZIP_FOPEN);
  }else{
    warning(WARN_OPEN|WARN_FLAG_ONCE_PER_PATH|WARN_FLAG_FAIL,path,"zip_fopen %s",entry,zip_get_error(za));
  }
  return zf;
}


//  RLOOP(retry,RETRY_ZIP_FREAD){
static off_t _viamacro_my_zip_fread(zip_file_t *zf, void *buf, zip_uint64_t nbytes, const char *rp,const char *func,const int line){
  if (nbytes<=0 || !zf) return 0;
  ASSERT(buf);
  const off_t n=zip_fread(zf,buf,nbytes);
  if (n<0) warning_zip_f(rp,zf," %s:%d  zip_fread()");
  return n;
}
/* If successful, the number of bytes actually read is returned. When zip_fread() is called after reaching the end of the file, 0 is returned. In case of error, -1 is returned. */
static off_t _viamacro_fhandle_zip_fread(fHandle_t *d, char *buf,  const zip_uint64_t nbytes,const char *func,const int line){
  //log_entered_function("%s %s  nbytes:%ld Caller %s:%d",D_RP(d),D_VP(d),nbytes,func,line);
  IF1(WITH_EXTRA_ASSERT, LOCK(mutex_fhandle,  assert(fhandle_active_readers_writers(d))));
  off_t todo=nbytes,sum=0;
  while(todo>0){
    const off_t n=my_zip_fread(d->zip_file,buf+sum,todo,D_RP(d));
    if (n<0) return n;
    if (!n) break;
    sum+=n;
    todo-=n;
  }
  d->zip_fread_position+=sum;
  //log_exited_function("%s %s  nbytes:%ld  sum:%ld",D_RP(d),D_VP(d),nbytes,sum);
  return sum;
}
static bool fhandle_zip_fopen(fHandle_t *d,const char *msg){
  cg_thread_assert_not_locked(mutex_fhandle);assert (d);
  fhandle_zip_lock(d);
  if (!d->zip_file){
    async_zipfile_t zip={0};
    zip.azf_zpath=d->zpath;
    zip.za=d->zip_archive;
    async_openzip(&zip);
    d->zip_fread_position=0;
    d->zip_file=zip.zf;
    d->zip_archive=zip.za;
  }
  fhandle_zip_unlock(d);
  return d->zip_file!=NULL;
}
static void fhandle_zip_fclose(const bool also_zip_archive,fHandle_t *d){
  assert(d);
  zip_file_t *zf=d->zip_file;
  zip_t *za=d->zip_archive;
  if (zf || za && also_zip_archive){
    fhandle_zip_lock(d);
    d->zip_file=NULL;
    d->offset=d->n_read=d->zip_fread_position=0;
    if (zf){
      zip_file_error_clear(zf);
      my_zip_fclose(zf,D_RP(d));
    }
    if (also_zip_archive && za) my_zip_close(za,D_RP(d));
    fhandle_zip_unlock(d);
  }
}

/* Returns true on success. May fail for seek backward. */
static bool fhandle_zip_fseek(fHandle_t *d, const off_t offset, const char *errmsg){
#define P fhandle_zip_ftell(d)
  if (offset==P) return true;
  const bool backward=offset<P;
  //IF_LOG_FLAG(LOG_ZIP);
  //log_entered_function("%p %s offset: %'jd  ftell: %'jd   backward: %s  thread: %lu"ANSI_RESET,d, D_VP(d),IM(offset),IM(P), backward?ANSI_FG_RED"Yes":ANSI_FG_GREEN"No",pthread_self());

  const int fwbw=backward?FHANDLE_SEEK_BW_FAIL:FHANDLE_SEEK_FW_FAIL;
#if VERSION_AT_LEAST(LIBZIP_VERSION_MAJOR,LIBZIP_VERSION_MINOR,1,9)
  if (!(d->flags&fwbw) && zip_file_is_seekable(d->zip_file)!=1) d->flags|=(FHANDLE_SEEK_BW_FAIL|FHANDLE_SEEK_FW_FAIL);
#endif
  /*  zip_file_is_seekable() was added in libzip 1.9.0.   /usr/include/zip.h */
  if (!(d->flags&fwbw)){
    if (!zip_fseek(d->zip_file,offset,SEEK_SET)){ d->zip_fread_position=offset;  return true;}
    warning(WARN_ZIP,D_RP(d),"zip_fseek: %s",zip_strerror(d->zip_archive));
    d->flags|=fwbw;
  }
  if (backward) return false;
  enum{buf_l=1<<20};
  char buf[buf_l]; /* Read to the respective position */
  while(offset>P){
    const off_t read=fhandle_zip_fread(d,buf,MIN(buf_l,offset-P));
    if (read<0){ warning(WARN_SEEK,D_VP(d),"fhandle_zip_fread returns <0   offset-P=%ld ",offset-P);return false;}
    if (!read){warning(WARN_SEEK,D_VP(d),"fhandle_zip_fread returns  0    offset-P=%ld ",offset-P);break;}
  }
  //log_exited_function("%p %s offset: %'jd  ftell: %'jd   backward: %s"ANSI_RESET,d, D_VP(d),IM(offset),IM(P), backward?ANSI_FG_RED"Yes":ANSI_FG_GREEN"No");
  return offset==P;
}
static off_t fhandle_read_zip(char *buf, const off_t size, const off_t offset,fHandle_t *d,bool *again){
  cg_thread_assert_not_locked(mutex_fhandle);
  if (!fhandle_zip_fopen(d,__func__)){  warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,D_VP(d),"xmp_read_fhandle_zip fhandle_zip_open returned -1"); return -1;}
  if (!fhandle_zip_fseek(d,offset,"")){ /* Worst case=seek backward - need reopen zip file. Happens often without fi->direct_io */
    IF1(WITH_PRELOADRAM,if ((*again=_preloadram_policy!=PRELOADRAM_NEVER && preloadram_is_free_ram(__func__,d,1+d->how_often_bwdseek++/9.9999f) && !VFOLDER_HAS_FLAG_d(PRELOADRAM_NOT))) return -1);
    //log_debug_now("%d %d %d %d",_preloadram_policy!=PRELOADRAM_NEVER, ramUsageForFilecontent()+D_ST_SIZE(d)<_preloadram_bytes_limit, d->how_often_bwdseek , d->zpath.dir!=DIR_NEVER_PREFETCH_RAM);;
    warning(WARN_SEEK,D_VP(d),ANSI_MAGENTA"Going to reopen zip offest=%'ld"ANSI_RESET,offset);
    fhandle_zip_fclose(false,d);    if (!fhandle_zip_fopen(d,"REWIND")) return -1;
    if (!fhandle_zip_fseek(d,offset,"REWIND")) return -1;
    assert(offset==P);
  }else if (offset>P){
    //warning(WARN_SEEK,D_VP(d),ANSI_RED"should not happen offset=%'ld > P=%'ld    "ANSI_RESET,offset,P);
    return -1;
  }
  const off_t num=fhandle_zip_fread(d,buf,size);
  fhandle_counter_inc(d,num<0?ZIP_READ_NOCACHE_FAIL:!num?ZIP_READ_NOCACHE_ZERO:ZIP_READ_NOCACHE_SUCCESS);
  return num;
#undef P
}/*fhandle_read_zip*/

/********************************************************************************/
/* Called on first invocation of xmp_write() or xmp_read() for fHandle_t object */
/* Maybe generate file content                                                  */
/********************************************************************************/
#define DEBUG_D_WRITE_TEXTBUF(d)  {log_debug_now("%s   textbuf-len:%ld  complete:%d root:%s\nTEXT: ",D_VP(d),!d->preloadram?-1:textbuffer_length(d->preloadram->txtbuf),0!=(d->flags&FHANDLE_PRELOADRAM_COMPLETE),rootpath(D_ROOT(d)));\
    if (d->preloadram) textbuffer_write_fd(d->preloadram->txtbuf,STDERR_FILENO);}
#define D_HAS_TB() (IF01(WITH_PRELOADRAM,false,d->preloadram && d->preloadram->txtbuf))
#if WITH_FUSE_INVALIDATE_PATH
static void fHandle_wait_little_while_need_invalidate_path(const fHandle_t *d, const int us){
  if (!(d->flags&FHANDLE_NEED_INVALIDATE_PATH)) return;
  const int wait=1e5;
  for(int i=us/wait; --i>=0  &&  (d->flags&FHANDLE_NEED_INVALIDATE_PATH); i++){ if (!(i%20)) fputs(" Still  FHANDLE_NEED_INVALIDATE_PATH ",stderr); usleep(wait);}
  log_verbose("Waiting while FHANDLE_NEED_INVALIDATE_PATH released %s ",success_or_fail(!(d->flags&FHANDLE_NEED_INVALIDATE_PATH)));
}
#endif //WITH_FUSE_INVALIDATE_PATH
static void fhandle_prepare_in_fuse_read_or_write(fHandle_t *d,const int open_flags){
  zpath_t *zpath=&d->zpath;
  {
    LOCK_N(mutex_fhandle, const bool go=(d->flags&FHANDLE_PREPARE_ONCE_IN_RW);   d->flags&=~FHANDLE_PREPARE_ONCE_IN_RW);
    IF1(WITH_FUSE_INVALIDATE_PATH,if (!go){ fHandle_wait_little_while_need_invalidate_path(d,1e7);return;});
  }
  if (IS_VFOLDER_SKIP_READONLY_ROOT(zpath))    ZPR()=_root_writable;
  const ssize_t size=d->zpath.stat_vp.st_size;
  IF1(WITH_PRELOADDISK,       if (IS_SPECIALFILE(zpath,SFILE_PRELOAD_UPDATE_GO)) preloaddisk_uptodate_or_update(d));
  IF1(WITH_INTERNET_DOWNLOAD, if (IS_SPECIALFILE(zpath,SFILE_INTERNET_UPDATE_GO)) net_update(d));

  IF1(WITH_CCODE, c_file_content_to_fhandle(d));
  IF1(WITH_FILECONVERSION, if (open_flags==O_RDONLY && (d->flags&FHANDLE_IS_FILECONVERSION)) fileconversion_run(d));
  if (!D_HAS_TB()){
    bool do_open=false;
    do_open=!ZPF(ZP_IS_ZIPENTRY)  && !(d->flags&FHANDLE_WITH_PRELOADRAM);
    int Done=0;
    //  if (D_HAS_TB() && (d->flags&FHANDLE_PRELOADRAM_COMPLETE)){ do_open=false; Done=1;}

    IF1(WITH_INTERNET_DOWNLOAD, if (VFOLDER_PATH(zpath)==DIR_INTERNET               && !Done++){   do_open=net_maybe_download_zpath(zpath);});
    if (open_flags==O_RDONLY){
#if WITH_PRELOADDISK
      if (!Done && !ZPR()->remote && zpath->is_decompressed){ Done=1; if (!fHandle_preloadfile_now(d)) d->errorno=EPIPE;}
      if (!Done && is_preloaddisk_zpath(zpath)){
        Done=1;
        ASSERT(!d->fd_real);
        do_open=!(d->errorno=preloaddisk(d)) && !D_ZPF(ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY);
      }
#endif //WITH_PRELOADDISK
    }
    IF1(IS_CHECKING_CODE,putchar(Done));
#if WITH_FUSE_INVALIDATE_PATH
    if (!d->errorno && d->zpath.stat_vp.st_size>size && ((d->flags&FHANDLE_WITH_PRELOADRAM)||do_open)){
      root_start_thread(D_ROOT(d),PTHREAD_INVALIDATE_PATH,false);
      log_verbose("Set FHANDLE_NEED_INVALIDATE_PATH to %s",VP());
      d->flags|=FHANDLE_NEED_INVALIDATE_PATH;
      // fHandle_wait_little_while_need_invalidate_path(d,1e6);
      usleep(1e5);
    }
#endif //WITH_FUSE_INVALIDATE_PATH
    if (do_open){
      //log_debug_now("Going open %s",RP());
      if (!RP_L() || !zpath_stat(0,zpath)){
        if (!d->errorno) d->errorno=ENOENT;
        warning(WARN_READ,VP(),"open root: %s RP:%s  D_HAS_TB:%d",ZPRP(),RP(),D_HAS_TB());
      }else if (!(d->fd_real=open(RP(),open_flags))){
        d->errorno=errno;
        log_errno("open RP:%s",RP());
      }
    }
  }
  if (!d->errorno && d->fd_real<0) d->errorno=EIO;
}

#define IF(d) IF0(IS_CHECKING_CODE,if(d))


/***********************************************************************************************************************************************************************************/
/* Read should return exactly the number of bytes requested except on EOF or error, otherwise the rest of the data will be substituted with zeroes.                                */
/* An exception to this is when the 'direct_io' mount option is specified, in which case the return value of the read system call will reflect the return value of this operation. */
/***********************************************************************************************************************************************************************************/
static int xmp_read(const char *vpath, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  //log_entered_function(ANSI_FG_GRAY"%s Size:%'jd   Offset: %'jd +%'d buf:%p"ANSI_RESET,vpath,IM(size),IM(offset),(int)size,buf);
  ASSERT(fi!=NULL); ASSERT(fi->fh);
  FUSE_PREAMBLE_Q(vpath);
      lock(mutex_fhandle);
      fHandle_t *d=fhandle_get(vipa.vp,fi->fh);
      IF(d){d->accesstime=time(NULL);}
      const uint64_t fhandle_fh=!d?0:d->fhandle_fh;
      fhandle_busy_start(d);
  unlock(mutex_fhandle);
  if (d){
    fhandle_prepare_in_fuse_read_or_write(d,O_RDONLY);
    IF1(WITH_EXTRA_ASSERT, LOCK(mutex_fhandle,  assert(fhandle_active_readers_writers(d))));
  }
  bool again=false;
  int bytes=0;
  RLOOP(i,2){
    bytes=_xmp_read(&vipa,d,buf,size,offset,fi->fh,&again);
    if (!(bytes<=0 && again)) break;
    log_verbose(GREEN_SUCCESS"Detected backward seek. Going to preload into ram %s",vpath);
    d->flags|=FHANDLE_WITH_PRELOADRAM;
  }
  IF(d) LOCK(mutex_fhandle, assert(fhandle_fh==d->fhandle_fh); fhandle_busy_end(d));
  //log_exited_function(ANSI_FG_GRAY"%s "ANSI_YELLOW"%ju"ANSI_RESET"  Offset: %'jd  bytes: %'d"ANSI_RESET,vpath,UIM(fi->fh), IM(offset),(int)bytes);
  return bytes; // cppcheck-suppress resourceLeak
}
static int _xmp_read(const virtualpath_t *vipa, fHandle_t *d, char *buf, const size_t size, const off_t offset, uint64_t fd, bool *again){
  off_t nread=-1;
  if (d){
    ASSERT(d->accesstime);
    IF1(WITH_EXTRA_ASSERT, LOCK(mutex_fhandle,  assert(fhandle_active_readers_writers(d))));
    if (d->errorno) return d->errorno;
    if (d->fd_real) goto d_has_fd;
#if WITH_PRELOADRAM
    if (d->flags&FHANDLE_PRELOADRAM_COMPLETE){  /* FHANDLE_PRELOADRAM_COMPLETE: Avoid overhead of preloadram_wait() if FHANDLE_PRELOADRAM_COMPLETE */
      LOCK(mutex_fhandle, nread=preloadram_read(buf,d,offset,offset+size));
    }else if (d->flags&FHANDLE_WITH_PRELOADRAM){
      nread=preloadram_wait_and_read(buf,size,offset,d);
    }
    if (nread<=0 && _is_tdf_or_tdf_bin(vipa->vp)){
      if (nread<0 || offset<D_ST_SIZE(d)) warning(WARN_READ|WARN_FLAG_ERROR,D_VP(d),"%p  nread:%'jd  %'jd to %'jd   (%'jd)",d,IM(nread),IM(offset),IM(offset+size),IM(D_ST_SIZE(d)));
    }
    if (d->flags&FHANDLE_DESTROY_LATER)  warning(WARN_READ|WARN_FLAG_ERROR,D_VP(d),"FHANDLE_DESTROY_LATER  %p fi->fh: "ANSI_YELLOW"%ju"ANSI_RESET"  fhandle_fh: %ld",d,fd,UIM(d->fhandle_fh));
    if (nread>0) return nread;
#endif //WITH_PRELOADRAM
    if (nread<0 && (D_ZPF(ZP_IS_ZIPENTRY))){
      fhandle_lock(d); /* Why lock: Comming here same/different fHandle_t instances and various pthread_self() */
      nread=fhandle_read_zip(buf,size,offset,d,again);
      fhandle_unlock(d);
      if (*again){
        ASSERT(nread<0);
        IF1(WITH_PRELOADRAM,return -1);
      }
    }
    if (nread<0 && !config_not_report_stat_error(vipa->vp,vipa->vp_l)){
      LOCK_N(mutex_fhandle, const char *status=IF01(WITH_PRELOADRAM,"NA",enum_preloadram_status_S[preloadram_get_status(d)]));
      warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,d?D_RP(d):vipa->vp,"nread %jd  offset:%ld  size:%jd    n_read=%ju  status:%s",IM(nread),offset,IM(size),d->n_read,status);
    }else if(nread>0){
      LOCK(mutex_fhandle,d->n_read+=nread);
      if (offset<d->offset_expected) d->count_backward_seek++;
      d->offset_expected+=nread;
    }
    if (D_ZPF(ZP_IS_ZIPENTRY)) return nread>=0?nread:errno?-errno:-1;
  }/* if (d)*/
 d_has_fd: /* Normal reading from file descriptor  d->fd_real or fi->fh */
  if (nread<0){
    if (d && d->fd_real) fd=d->fd_real;
    const off_t seeking=offset-lseek(fd,0,SEEK_CUR);
    if (seeking<0 && fd>2 && fd<COUNT_BACKWARD_SEEK) _count_backward_seek[fd]++;
    if (seeking && offset!=lseek(fd,offset,SEEK_SET)){
      log_msg(ANSI_FG_RED""ANSI_YELLOW"SEEK_REG_FILE:"ANSI_RESET" offset: %'jd ",IM(offset));
      log_msg("Failed %s fd=%"PRIu64"\n",vipa->vp,fd);
      return errno?-errno:-1;
    }else{
      return cg_fd_read(fd,offset,buf,size);
    }
  }
  return nread;
}/*xmp_read*/

static int xmp_release(const char *vpath, struct fuse_file_info *fi){ // cppcheck-suppress [constParameterCallback]
  ASSERT(fi!=NULL);
  atomic_fetch_add(&_open_minus_release,1);
  //static atomic_int count;log_entered_function(ANSI_RED"vpath:"ANSI_RESET" %s %d",vpath, atomic_fetch_add(&count,1));
  FUSE_PREAMBLE(vpath);
  int count_backward_seek=0;
  const uint64_t fd=fi->fh;
  fHandle_t *d=NULL;
  if (fd>=FD_ZIP_MIN){
        lock(mutex_fhandle);
        d=fhandle_get(vipa.vp,fd);
        ASSERT(d);
        if (d){
          count_backward_seek=d->count_backward_seek;
          //warning(WARN_MISC,vpath,ANSI_FG_RED"%s"ANSI_RESET"  d: %p "ANSI_RESET" fi->fh: "ANSI_YELLOW"%ju"ANSI_RESET"  fhandle_fh: %ld  pid:%jd   ",__func__,d,fd,UIM(d->fhandle_fh),IM(d->pid));
          d->flags|=FHANDLE_DESTROY_LATER;
          fhandle_try_destroy(d);
        }
        unlock(mutex_fhandle);
  }else if (fd>2){
    maybe_evict_from_filecache(fd,vipa.vp,vipa.vp_l,NULL,0);
    if ((er=close(fd))){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,vpath,"close(fd: %ju)",fd);
      cg_print_path_for_fd(fd);
    }
    count_backward_seek=fd<COUNT_BACKWARD_SEEK?_count_backward_seek[fd]:0;
  }
  log_fuse_function(__func__,&vipa,count_backward_seek);
  return -er;
}
static int xmp_flush(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE(vpath);
  IF1(WITH_SPECIAL_FILE, if (vipa.specialfile_id) return 0);
  return fi->fh<FD_ZIP_MIN?fsync(fi->fh):0;
}

static void _viamacro_exit_ZIPsFS(const char *func, const int line_num){
  log_verbose("Going to exit %s:%d ...",func,line_num);
  /* osxfuse and netbsd needs two parameters:  void fuse_unmount(const char *mountpoint, struct fuse_chan *ch); */
  IF1(WITH_FUSE_3,if (_fuse_started && fuse_get_context() && fuse_get_context()->fuse) fuse_unmount(fuse_get_context()->fuse));
  fflush(stderr);
}





int main(const int argc,const char *argv[]){
  _pid=getpid();
  debug_pid_to_exe(_pid);
  IF1(WITH_CANCEL_BLOCKED_THREADS,assert(_pid==gettid()), assert(cg_pid_exists(_pid)));
  ASSERT(_virtualfolder_dirflags[ID_(PRELOAD_SELECT_ALL)]=="a");
  char tmp[PATH_MAX];
  if (realpath(*argv,tmp)) _self_exe=strdup_untracked(tmp); else DIE("Failed realpath %s",*argv);
  init_mutex();
  init_sighandler(argv[0],(1ULL<<SIGSEGV)|(1ULL<<SIGUSR1)|(1ULL<<SIGABRT),stderr);
  virtualfolders_init();

  _whenStarted=time(NULL);


  {
    _warning_color[WARN_THREAD]=ANSI_FG_RED;
    _warning_color[WARN_GETATTR]=ANSI_FG_MAGENTA;
    _warning_color[WARN_CHARS]=ANSI_YELLOW;
    _warning_color[WARN_DEBUG]=ANSI_MAGENTA;
    FOR(i,0,enum_warnings_N) _warning_channel_name[i]=(char*)enum_warnings_S[i];
  }
  int colon=0;
  FOR(i,1,argc) if (STR_EQ_C(argv[i],':')){ colon=i; break;}
  initial_msg(stderr);
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
  for(int c;(c=getopt_long(argc,(char**)argv,"+bqT:vnkhVs:c:l:L:",l_option,NULL))!=-1;){  /* The initial + prevents permutation of argv */
    switch(c){
    case 'V': exit(0);break;
    case 'T': cg_print_stacktrace_test(atoi(optarg)); exit_ZIPsFS();break;
    case 'b': _isBackground=true; break;
    case 'q': _logIsSilent=true; break;
    case 'k': _killOnError=true; break;
    case 's': _mnt_apparent=_mkSymlinkAfterStart=strdup_untracked(optarg); break;
    case 'h': ZIPsFS_usage(); root_property_help(-1,stderr); return 0;
    case 'l': IF1(WITH_PRELOADRAM,if (!preloadram_set_maxbytes(optarg)) return 1); break;
    case 'c': IF1(WITH_PRELOADRAM,if (!preloadram_set_policy(optarg))   return 1); break;
    case 'L': _rlimit_vmemory=cg_atol_kmgt(optarg); break;
    case 'v':   _log_flags|=(1<<LOG_FUSE_METHODS_ENTER);break;
    default: if (isalnum(c)) fprintf(stderr,"Wrong option '-%c'. Enter",c); cg_getc_tty(); break;
    }
  }

#if ! defined(HAS_RLIMIT) || HAS_RLIMIT
  static struct rlimit l={0};
  if (_rlimit_vmemory){
    l.rlim_cur=l.rlim_max=_rlimit_vmemory;
    log_msg("Setting rlimit virtual memory to %ju MB \n",UIM(l.rlim_max>>20));
    if (setrlimit(RLIMIT_AS,&l)) perror(ANSI_FG_RED"setrlimit(RLIMIT_AS,n)\n"ANSI_RESET);
  }
  if(MAX_NUM_OPEN_FILES){
    getrlimit(RLIMIT_NOFILE,&l);
    l.rlim_cur=MIN(l.rlim_max,MAX_NUM_OPEN_FILES);
    log_msg("Setting rlimit MAX_NUM_OPEN_FILES to %'d\n",(int)l.rlim_cur);
    if (setrlimit(RLIMIT_NOFILE,&l)) perror(ANSI_FG_RED"setrlimit(RLIMIT_NOFILE,n)\n"ANSI_RESET);
  }
#else
  log_warn("The function setrlimit() is not supported. Option -L  and MAX_NUM_OPEN_FILES will be ignored.");
#endif
  if (!getuid() || !geteuid()){
    log_strg("Running ZIPsFS as root opens unacceptable security holes.\n");
    if (_isBackground) DIE("It is only allowed in foreground mode  with option -f.");
    fprintf(stderr,"Do you accept the risks [Enter / Ctrl-C] ?\n");cg_getc_tty();
  }

  if (!colon || colon==argc-1){
    log_error("In the list of command parameters there should be  single colon ':' followed by the an empty folder as mountpoint. %s\n", !colon?"No colon":"There is no further  parameter after the colon.\n");
    _mnt=""; /* avoid assert */
    specialfiles_init();
    debug_print_virtual_folders();
    suggest_help();
    return 1;
  }
  _mnt_l=cg_strlen(realpath(argv[argc-1],tmp)); _mnt=strdup_untracked(tmp);
  if (!_mnt_apparent) _mnt_apparent=(char*)argv[argc-1];
  if (*_mnt_apparent!='/'){
    if (!getcwd(tmp,PATH_MAX-strlen(_mnt_apparent)-2)) log_errno("getcwd()");
    else _mnt_apparent=strdup(strcat(strcat(tmp,"/"),_mnt_apparent));
    cg_str_replace(0,_mnt_apparent,0, "/s-mcpb-ms03.charite.de/",0,"/s-mcpb-ms03/",0);
    //    for(int i=strlen(_mnt_apparent); i && _mnt_apparent[i]=='/') _mnt_apparent[i]==0;
  }
  cg_str_replace(0,_mnt_apparent,0, "//",2,"/",1);
  _mnt_apparent[cg_pathlen_ignore_trailing_slash(_mnt_apparent)]=0;


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

  //  log_fuse_function_fd();  warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);
  IF1(WITH_SPECIAL_FILE, specialfile_content_to_file(SFILE_DEBUG_CTRL,_specialfiles[SFILE_DEBUG_CTRL].rp));
  MSTORE_INIT(&_mstore_persistent,MSTORE_OPT_MMAP_WITH_FILE|0x10000);   MSTORE_SET_MUTEX(mutex_fhandle);
  HT_INIT_INTERNER_FILE(&_ht_intern_vp,16,DIRECTORY_CACHE_SIZE); HT_SET_MUTEX(mutex_dircache);
  HT_INIT_INTERNER_FILE(&_ht_intern_fileext,8,4096);            HT_SET_MUTEX(mutex_fhandle);
  HT_INIT(&_ht_valid_chars,HT_FLAG_NUMKEY|12);                          HT_SET_MUTEX(mutex_validchars);
  IF1(WITH_FILECONVERSION_OR_CCODE,HT_INIT_WITH_KEY_INTERNER(&_ht_fsize,9,&_ht_intern_vp));
  HT_INIT_WITH_KEYSTORE(&_ht_count_by_ext,11,&_mstore_persistent);                   HT_SET_MUTEX(mutex_fhandle); HT_SET_ID(HT_MALLOC__ht_count_by_ext);

  FOR(i,optind,colon){ /* Source roots are given at command line. Between optind and colon */
    const char *a=argv[i];
    if (*a=='@') continue;
    if (!*a){
      if (!_root_n) _root_n=1; /* _writable_path and _root_writable will be NULL */
      continue;
    }
    if (_root_n>=ROOTS) DIE("Exceeding max number of ROOTS %d.  Increase macro  ROOTS   in configuration.h and recompile!\n",ROOTS);
    root_t *r=_root+_root_n;
    int features=0;while(*argv[i+features+1]=='@') features++;
    root_init(!_root_n++,r,a, argv+i+1,features);
#if WITH_DIRCACHE_or_STATCACHE_or_TIMEOUT_READDIR
    MSTORE_INIT(&r->dircache_mstore,MSTORE_OPT_MMAP_WITH_FILE|DIRECTORY_CACHE_SIZE);      MSTORE_SET_MUTEX(mutex_dircache);
    HT_INIT_INTERNER_FILE(&r->ht_int_fname,16,DIRECTORY_CACHE_SIZE);                                   HT_SET_MUTEX(mutex_dircache);
    HT_INIT_INTERNER_FILE(&r->ht_int_fnamearray,HT_FLAG_BINARY_KEY|12,DIRECTORY_CACHE_SIZE);      HT_SET_MUTEX(mutex_dircache);
#endif
    HT_INIT(&r->ht_inodes,HT_FLAG_NUMKEY|16); HT_SET_ID(HT_MALLOC_inodes);
    HT_INIT_WITH_KEY_INTERNER(&r->ht_dircache,12,&_ht_intern_vp);
    HT_INIT_WITH_KEY_INTERNER(&_ht_inodes_vp,16,&_ht_intern_vp);
    HT_INIT_WITH_KEY_INTERNER(&r->ht_filetypedata,10,&_ht_intern_fileext);  HT_SET_ID(HT_MALLOC_file_ext);
    HT_INIT_WITH_KEY_INTERNER(&r->ht_dircache_queue,8,&_ht_intern_vp); HT_SET_MUTEX(mutex_dircache_queue);
    IF1(WITH_STATCACHE,HT_INIT_WITH_KEY_INTERNER(&r->ht_stat,16,&_ht_intern_vp));
    IF1(WITH_ZIPFLATCACHE, HT_INIT(&r->ht_zipflatcache_vpath_to_rule,HT_FLAG_NUMKEY|16); HT_SET_MUTEX(mutex_dircache));
  }/* Loop roots */
  {/* Order is important! */
    specialfiles_init();  log_fuse_function_fd();  warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);
    assert(_dot_ZIPsFS); snprintf(tmp,MAX_PATHLEN,"%s/cachedir",_dot_ZIPsFS);  mstore_set_base_path(tmp);
  }
  root_property_read_all(NULL,NULL,0); /* free line */
  vp_is_part_of_path_prefix(NULL);
  log_msg("\n\nMount point: "ANSI_FG_BLUE"'%s'"ANSI_RESET"\n\n",_mnt);
  if (!_root_n){ log_error("Missing root directories\n");return 1;}
  if (check_configuration(stderr,argv[argc-1]) && !cg_uid_is_developer() && !_isBackground){ fprintf(stderr,"Press enter\n"), cg_getc_tty();}
  warning(WARN_CONFIG,"path","msg %p",_fWarnErr[0]);
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
  log_print_roots(stderr);
  initial_msg(_fWarnErr[0]);
  check_configuration(_fWarnErr[0],argv[argc-1]);
  log_print_roots(_fWarnErr[0]);
  const int fuse_stat=fuse_main(_fuse_argc,(char**)_fuse_argv,&xmp_oper,NULL);
  _fuse_started=true;
  log_msg(RED_WARNING" fuse_main() returned %d\n",fuse_stat);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF1(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS();
}

////////////////////////////////////////////////////////////////////////////
// DIE  log_  FIXME  DIE_DEBUG_NOW   DEBUG_NOW   log_debug_now  log_entered_function     log_exited_function    directory_print
// _GNU_SOURCE    HAS_EXECVPE HAS_US_ENVIRON   HAS_ST_MTIM   HAS_POSIX_FADVISE
// USED_TO_BE
// malloc calloc strdup  free  mmap munmap   readdir opendir   --- malloc_untracked calloc_untracked  strdup_untracked



//   PSEUDO_ENTRYIDX_FOR_UPDATE   ID_FLAG(INTERNET_UPDATE

// cg_readlink_absolute
// PRIu64
//  Test mgf: mnt/zipsfs/c/-/QEP/50-0052  Test bz2  mnt/zipsfs/d/-/6600-tof2/Cal/202106/_associatedFiles/
//        cg_compression_file_ext(zpath->is_decompressed,&e_l);
//  mnt/zipsfs/d/l/UPDATE/_README_PRELOAD_UPDATE.html  # 6600-tof2/Cal/ # 202106/Cal20210604162723046.wiff
// grep bz2  //s-mcpb-ms04/dia/Misc/content_of_disks/CHA-CHA-RALSER-RAW.txt
// head  mnt/zipsfs/d/-/6600-tof2/Data/50-0055/_associatedFiles/20201215_TOF1_LS_100_50-0055_Kurth-Covid19-Dexa_P2_A1.wiff
// filler_readdir( cg_download_url  FINDRP_DIR_PRELOADED_ONLY
// https://music.apple.com/de/new
