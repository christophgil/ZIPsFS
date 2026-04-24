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

#define VFILE_SFX_INFO "@SOURCE.TXT"
#define FEHLER(zpath) ASSERT(!strstr(ZP_VP(zpath),"home"))
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
#define HOOK_MSTORE_CLEAR(m)   {char mpath[PATH_MAX+1];mstore_file(mpath,m,-1);warning(WARN_DIRCACHE,mpath,"Clearing mstore_t %s ",m->name);}
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
static const char *SFILE_REAL_PATHS[SFILE_NUM];
static int _fhandle_n=0,_mnt_l=0, _debug_is_readdir;
static rlim_t _rlimit_vmemory=0;
enum {COUNT_BACKWARD_SEEK=1024};
static int _count_backward_seek[COUNT_BACKWARD_SEEK];
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
static mstore_t _mstore_persistent; /* This grows for ever during. It is only cleared when program terminates. */
#define X(ht) static ht_t ht;
XMACRO_HT_GLOBAL()
#undef X
  static int _log_flags; /* The bits specifying what is logged.*/
/* *** fHandle vars and defs *** */
static float _ucpu_usage,_scpu_usage;/* user and system */
static int64_t _preloadram_bytes_limit=3L*1000*1000*1000;
static int _unused_int,_writable_path_l;
static bool _thread_unblock_ignore_existing_pid, _fuse_started, _isBackground, _exists_root_with_preload;
static bool _logIsSilentFailed,_logIsSilentWarn,_logIsSilentError;
static const char *_fuse_argv[99]={0}, *_writable_path, *_cleanup_script; /*"<path-first-root>"FILE_CLEANUP_SCRIPT*/;
static int _fuse_argc;
static root_t _root[ROOTS]={0}, *_root_writable;
static int _root_n=0;  /* Num of root_t instances in _root[] */
#if WITH_INTERNET_DOWNLOAD
#include "ZIPsFS_configuration_internet.c"
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
    r->f_fsid=st.f_fsid;
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
    RLOOP(i,(r->pathpfx_l=strlen(p))) if (p[i]=='/') p[i]=0;
    r->pathpfx_slash_to_null=p;
  }
  {
    struct stat st;
    if (stat(path,&st)){ log_errno("stat '%s'",path); DIE("");}
    r->st_dev=st.st_dev;
  }
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
  if (r->decompress_mask || r->preload)  _exists_root_with_preload=true;
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
  char tmp[PATH_MAX+1];
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
#define RP_HELP(x,type) _root_property_example=x;_root_property_type=type
#define RP_PRINT(code)
#define RP_GET(code)
#define RP_NUMBER(unit,x,assign_to)   RP_HELP(x,unit)                            RP_PRINT(if (assign_to) sprintf(buf,"%ld",(long)assign_to))		   RP_GET(assign_to=rp_parse_number(v))
#define RP_01(x,assign_to)       RP_HELP(x,"0 or 1")							 RP_PRINT(if (assign_to) sprintf(buf,"%d",assign_to))			   RP_GET(assign_to=rp_parse_01(v))
#define RP_PATHS(x,assign_to)    RP_HELP(x,"File paths separated by :")      RP_PRINT(if (assign_to) rp_print_paths(buf,sizeof(buf),assign_to)) RP_GET(assign_to=(const char**)cg_split_string(":",v))
#define RP_LIST(x,assign_to)     RP_HELP(x,"Like .jpg,.tgz,.tar.gz")RP_PRINT(if (assign_to) rp_print_paths(buf,sizeof(buf),assign_to)) RP_GET(assign_to=(const char**)cg_split_string(" ",v))
#define RP_VPATH(x,assign_to)    RP_HELP(x,"Virtual file path")                  RP_PRINT(if (assign_to) sprintf(buf,"%s",assign_to))               RP_GET(assign_to=rp_parse_vpath(r,v))
#define RP_RPATH(x,assign_to)    RP_HELP(x,"Real absolute file path")            RP_PRINT(if (assign_to) sprintf(buf,"%s",assign_to))               RP_GET(assign_to=rp_parse_rpath(r,v))

static void rp_parse_decompress(root_t *r,char *v){
  if (!r){ RP_HELP("Preloaded and decompressed. "REQUIRES_1(WITH_PRELOADDISK),"List of  gz,bz2,xz,lrz,Z");return;}
  if (_root_property_print){
    int n=0;
    FOR(iCompress,1,COMPRESSION_NUM){
      if (!(r->decompress_mask&(1<<iCompress))) continue;
      const char *x=cg_compression_file_ext(iCompress,NULL);
      if (x && *x) n+=sprintf(_root_property_print+n,"%s%s",n?" ":"{",x+1);
    }
    if (n) _root_property_print[n]='}';
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
static void root_property_print(const bool html,const int id_or_minus_1, FILE *f){
#define v NULL
#undef RP_PRINT
#define RP_PRINT(code) code
  const char *reset=html?"":ANSI_RESET;
  if (id_or_minus_1<0) fprintf(f,"\n%s *** Properties of roots *** %s",html?"":ANSI_INVERSE,reset);
  char buf[4096];
  _root_property_print=buf;
  foreach_root(r){
    int header=0;
    FOR(id,0,ROOT_PROPERTY_NUM){
      if (id_or_minus_1>=0 && id!=id_or_minus_1) continue;
      *buf=0;
      switch(id){XMACRO_ROOT_PROPERTY();}
      if (*buf){
        if (!header++) fprintf(f,"\n%sProperties of %s%s\n",html?"":ANSI_UNDERLINE ANSI_BOLD,r->rootpath,reset);
        fprintf(f,"   - %s%s: %s %s\n",html?"":ANSI_FG_MAGENTA,ROOT_PROPERTY[id],reset,buf);
      }
    }
  }
  if (id_or_minus_1<0) fputs("For a list of supported properties run ZIPsFS -h\n",f);
#undef v
#undef RP_PRINT
#define RP_PRINT(code)
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
#define my_zip_fread(...) _viamacro_my_zip_fread(__VA_ARGS__,__func__)
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
#include "ZIPsFS_preloadfiledisk.c"
#endif //WITH_PRELOADDISK
#include "ZIPsFS_async.c"
// ---
#include "ZIPsFS_filesystem_info.c"
#if WITH_PRELOADRAM
#include "ZIPsFS_preloadfileram.c"
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
  C(mtime);  C(size);  C(crc);  C(inode);
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


/****************************************************************************************************/
/* Single point for calling lstat() / stat()														*/
/* Invokations with  fd_parent!=0 are coming from readdir to store the attributes. Using fstatat(). */
/* For all other invokations, fd_parent is 0.  Using lstat().										*/
/* For remote paths, the calls to lstat() are reduced with an attribut cache                        */
/****************************************************************************************************/
static bool zpath_stat_direct(const int opt_filldir_findrp,zpath_t *zpath,const time_t now){
  if (!RP_L() || (opt_filldir_findrp&FINDRP_CACHE_ONLY)) return false; /* Also  see FINDRP_CACHE_NOT */
  if (!stat_direct(&zpath->stat_rp,RP())) return false;
  IF1(WITH_STATCACHE,if(zpath->root) stat_to_cache(opt_filldir_findrp,&zpath->stat_rp,VP0(),VP0_L(),zpath->root,now?now:time(NULL)));
  return true;
}
static bool _viamacro_stat_direct(const int fd_parent,struct stat *st,const char *rp,const char *callerFunc){
  //  if (opt_filldir_findrp&FINDRP_CACHE_ONLY){log_debug_now("SKIP %s",rp); if (r)assert(r!=_root_writable);}
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
  if (ZPF(ZP_OVERFLOW)) return false;
  const int l=MIN_int(cg_strlen(s),len);
  if (l){
    if (zpath->strgs_l+l+3>ZPATH_STRGS){
      warning(enum_warnings_N|WARN_FLAG_MAYBE_EXIT,"zpath_strncat %s %d exceeding ZPATH_STRGS\n",s,len);
      zpath->flags|=ZP_OVERFLOW;
      return false;
    }
    cg_strncpy0(zpath->strgs+zpath->strgs_l,s,l);
    zpath->strgs_l+=l;
  }
  return true;
}
/* static int zpath_commit_hash(const zpath_t *zpath, ht_hash_t *hash){ */
/*   const int l=zpath->strgs_l-zpath->current_string; */
/*   if (hash) *hash=hash32(zpath->strgs+zpath->current_string,l); */
/*   return l; */
/* } */
static void zpath_set_realpath(zpath_t *zpath, const char *rp1,const char *rp2){
  zpath->strgs_l-=RP_L();
  ZPATH_NEWSTR(realpath);
  if (rp1) ZPATH_STRCAT(rp1);
  if (rp2) ZPATH_STRCAT(rp2);
  ZPATH_COMMIT(realpath);
  FEHLER(zpath);
}

static void _viamacro_zpath_assert_strlen(const char *fn,const char *file,const int line,zpath_t *zpath){
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
#define X(f) zpath->f=zpath->f##_l=
  XMACRO_ZIPPATH_NEED_RESET()
#undef X
    zpath->zipcrc32=zpath->virtualpath_without_entry_hash=
    zpath->stat_vp.st_ino=zpath->stat_rp.st_ino=0;
  zpath->root=NULL;
  zpath->strgs_l=zpath->virtualpath+VP_L();
  zpath->flags=zpath->flags&ZP_KEEP_NOT_RESET_MASK;
}

static bool zpath_init_vp(zpath_t *zpath, const char *vp0,   const int vp0_l, const char *optionalPathComp){
  const int optionalPathComp_l=cg_strlen(optionalPathComp), vp_l=vp0_l+optionalPathComp_l+(optionalPathComp_l!=0);
  zpath->virtualpath=zpath->strgs_l=1;  /* To distinguish from virtualpath==0  meaning not defined we use 1*/
  zpath->virtualpath_l=vp_l;
  if (!ZPATH_STRCAT_N(vp0, vp0_l) || optionalPathComp_l && (!ZPATH_STRCAT("/") || !ZPATH_STRCAT_N(optionalPathComp,optionalPathComp_l))) return false;
  zpath->strgs[zpath->virtualpath+vp_l]=0;
  zpath_reset_keep_VP(zpath);
  zpath->virtualpath_hash=hash32(VP(),vp_l);
  return true;
}

static void zpath_init(zpath_t *zpath, const virtualpath_t *vipa){
  ASSERT(zpath);
  ASSERT(vipa);
  ASSERT(vipa->vp);
  ASSERT(!(zpath->flags&ZP_IS_IN_FHANDLE));
  ASSERT(cg_strlen(vipa->vp)>=vipa->vp_l);
  memset(zpath,0,sizeof(zpath_t));
  zpath_init_vp(zpath,vipa->vp,vipa->vp_l,NULL);
#define C(f)   zpath->f=vipa->f
  IF1(WITH_PRELOADDISK,C(preloadpfx));
  C(dir);
  C(flags);
  C(zipfile_l);
  C(zipfile_cutr);
  C(zipfile_append);
#undef C
}



static bool zpath_stat(const int opt_filldir_findrp, zpath_t *zpath){
  const root_t *r=zpath->root;
  //log_entered_function("%s root:%s  dir:%s",VP(),rootpath(r),zpath->dir);
  bool ok=zpath->stat_rp.st_ino;
  if (!ok){
    if (r){
      cg_thread_assert_not_locked(mutex_fhandle);
      IF1(WITH_STATCACHE, ok=zpath_stat_from_cache(opt_filldir_findrp,zpath));
      if (!ok && r->remote) ok=async_stat(opt_filldir_findrp,zpath);
      if (!ok) ok=zpath_stat_direct(opt_filldir_findrp,zpath,0);
    }else{
      ok=!stat(RP(),&zpath->stat_rp);
    }
    if (ok) zpath->stat_vp=zpath->stat_rp;
    IF1(WITH_PRELOADDISK, if (!ok && path_with_compress_sfx_exists(zpath)) ok=true);
    if (ok){
      int idx=zpath->is_decompressed>COMPRESSION_NIL?1:0;
      if ((zpath->dir==DIR_INTERNET_UPDATE || zpath->dir==DIR_PRELOADED_UPDATE) && S_ISREG(zpath->stat_rp.st_mode)){ zpath->stat_vp.st_size=4096; idx+=2;}
      if (!(ZPF(ZP_TRY_ZIP))) zpath->stat_vp.st_ino=zpath_make_inode(zpath,idx);
    }
  }
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
static off_t fsize_from_hashtable(const char *vp,const int vp_l){
  LOCK_N(mutex_dircache, const off_t size=(off_t)ht_get(&_ht_fsize,vp,vp_l,0));
  return size;
}
static void fsize_to_hashtable(const char *vp, const int vp_l, const off_t size){
  LOCK(mutex_dircache,ht_set(&_ht_fsize,vp,vp_l,0,(void*)size));
}
#endif //WITH_FILECONVERSION_OR_CCODE
/****************************************************************************************************************************/
/* Is the virtualpath a zip entry?                                                                                          */
/* Normally append will be ".Content" and cutr will be 0.                                                                   */
/* Bruker MS files. The ZIP file name without the zip-suffix is the  folder name: append will be empty and cutr will be -4; */
/****************************************************************************************************************************/
static int virtual_dirpath_to_zipfile(const char *vp, const int vp_l,int *cutr, char *append[]){
  ASSERT(vp_l>=0);
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
  //log_exited_function("%s cutr=%d append=%s",vp,*cutr,*append);
  return ret==INT_MAX?0:ret;
}
/////////////////////////////////////////////////////////////
// Read directory
// Is calling directory_add(directory_t,...)
// Returns true on success.
////////////////////////////////////////////////////////////

static void directory_to_cache_maybe(directory_t *dir){
  config_exclude_files(DIR_RP(),DIR_RP_L(),dir->core.files_l, dir->core.fname,dir->core.fsize);
#if WITH_DIRCACHE
  root_t *r=DIR_ROOT();
  bool doCache=dir->always_to_cache ||  //(opt_filldir_findrp&FILLDIR_FROM_OPEN) DEBUG_NOW Warum?
    config_advise_cache_directory_listing(((r&&r->remote)?ADVISE_DIRCACHE_IS_REMOTE:0)|
                                          (dir->dir_zpath.dir==DIR_PLAIN?ADVISE_DIRCACHE_IS_DIRPLAIN:0)|
                                          (DIR_IS_TRY_ZIP()?ADVISE_DIRCACHE_IS_ZIP:0),
                                          DIR_RP(),DIR_RP_L(), dir->dir_zpath.stat_rp.ST_MTIMESPEC);
  if (doCache) LOCK_NCANCEL(mutex_dircache,dircache_directory_to_cache(dir));
#endif //WITH_DIRCACHE
}

static bool readdir_from_cache_zip_or_filesystem(const int opt_filldir_findrp,directory_t *dir){
  if (!DIR_RP_L()) return false;
  const root_t *r=DIR_ROOT();
#if WITH_DIRCACHE
  {
    bool success=false;
    LOCK_NCANCEL(mutex_dircache,success=dircache_directory_from_cache(dir));
    //log_debug_now("rp:%s  %s  root:%s  remote:%d",DIR_RP(), success_or_fail(success),rootpath(DIR_ROOT()), DIR_ROOT() && DIR_ROOT()->remote);
    if (success) return true;
  }
#endif //WITH_DIRCACHE
  if (opt_filldir_findrp&FILLDIR_FROM_OPEN) dir->always_to_cache=true;
  if (!readdir_async(dir)) return false;

  return true;
}
#define DIRECTORY_PREAMBLE(isZip)    if (DIR_IS_TRY_ZIP()!=isZip) return false;   char *rp; LOCK(mutex_dircache, rp=DIR_RP(); dir->core.files_l=0)
#define CONTAINS_PALCEHOLDER(n,zip) IF1(WITH_ZIPENTRY_PLACEHOLDER,dir->has_file_containing_placeholder=dir->has_file_containing_placeholder || strchr(n,PLACEHOLDER_NAME))
static bool readdir_from_zip(directory_t *dir){
  DIRECTORY_PREAMBLE(true);
  zip_t *zip=my_zip_open(rp);
  root_update_time(DIR_ROOT(),zip?PTHREAD_ASYNC:-PTHREAD_ASYNC,0);
  if (!zip) return false;
  //static int count; log_entered_function("# %d  readdir_from_zip('%s')   ",count++,ZP_VP(&dir->dir_zpath));
  const int SB=256,N=zip_get_num_entries(zip,0);
  struct zip_stat s[SB]; /* reduce num of pthread lock */
  for(int k=0;k<N;){
    int i=0;
    for(;i<SB && k<N;k++) if (!zip_stat_index(zip,k,0,s+i)) i++;
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
  my_zip_close(zip,rp);

  directory_to_cache_maybe(dir);
  return true;
}

#ifndef HAS_DIRENT_D_TYPE
#define HAS_DIRENT_D_TYPE 1
#endif

static bool readir_from_filesystem(directory_t *dir){
  DIRECTORY_PREAMBLE(false);
  DIR *d=opendir(rp);
  const int fd=!d?-1:dirfd(d);
  IF_LOG_FLAG(LOG_OPENDIR){ static int count;log_verbose("# %d  opendir('%s')  fd:%d ",count++,rp, fd);}
  if (!d){ log_errno("opendir: %s",rp); return false; }
  inc_count_by_ext(rp,d?COUNTER_OPENDIR_SUCCESS:COUNTER_OPENDIR_FAIL);
  root_t *r=DIR_ROOT();
  root_update_time(r,PTHREAD_ASYNC,0);
  time_t now=time(NULL);
  const bool need_stat=IF01(HAS_DIRENT_D_TYPE,true,r->remote || dir->when_readdir_call_stat_and_store_in_cache);
  struct stat st;
  struct dirent *de;
  char rp2[MAX_PATHLEN+1],vp2[MAX_PATHLEN+1];
  if (need_stat){
    strcpy(rp2,DIR_RP())[DIR_RP_L()]='/';
    strcpy(vp2,DIR_VP())[DIR_VP_L()]='/';
  }
  for(int i=0;(de=readdir(d));i++){
    if (!(i++&255)) root_update_time(r,PTHREAD_ASYNC,(now=time(NULL)));
    const char *n=de->d_name;
    const int n_l=strlen(n);
    CONTAINS_PALCEHOLDER(n,zip);
    if (config_do_not_list_file(rp,n,n_l)) continue;
    int isdir=IF01(HAS_DIRENT_D_TYPE,0, isdir=(de->d_type==DT_DIR)?1:-1);// cppcheck-suppress selfAssignment
    if (need_stat && DIR_RP_L()+1+n_l<MAX_PATHLEN){
      stpcpy(rp2+DIR_RP_L()+1,n);
      if (fstatat_direct(fd,&st,rp2)){
        stpcpy(vp2+DIR_VP_L()+1,n);
        isdir=S_ISDIR(st.st_mode)?1:-1;
        IF1(WITH_STATCACHE,if(r) stat_to_cache(FINDRP_STAT_TOCACHE_ALWAYS,&st,vp2,DIR_VP_L()+1+n_l,r,now));
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
static bool test_realpath_pfx(const bool dirFileconversion,  int opt_filldir_findrp, zpath_t *zpath, root_t *r){
  const char *vp=VP(), *vp0=VP0_L()?VP0():vp;
  const int vp_l=VP_L(), vp0_l=VP0_L()?VP0_L():VP_L();
  FOREACH_CSTRING(t,r->path_allow)  if (cg_path_equals_or_is_parent(*t,strlen(*t),vp,vp_l)) goto filter_ok;
 filter_ok:
  FOREACH_CSTRING(t,r->path_deny) if (cg_path_equals_or_is_parent(*t,strlen(*t), vp,vp_l)) return false;
  if (r->pathpfx_l && !cg_path_equals_or_is_parent(r->path_prefix, r->pathpfx_l,vp0,vp0_l)) return false;
  zpath->root=r;
  if (r->worm) opt_filldir_findrp|=FINDRP_IS_WORM;
  if (r->immutable) opt_filldir_findrp|=FINDRP_IS_IMMUTABLE;
  ZPATH_NEWSTR(realpath); /* realpath is next string on strgs_l stack */
  ZPATH_STRCAT_N(r->rootpath,r->rootpath_l);
  if (dirFileconversion) ZPATH_STRCAT(DIR_FILECONVERSION);
  ZPATH_STRCAT_N(vp0+r->pathpfx_l,vp0_l-r->pathpfx_l);
  ZPATH_COMMIT(realpath);
  zpath->stat_rp=empty_stat;
  if (ZPF(ZP_OVERFLOW) || !RP_L() || !zpath_stat(opt_filldir_findrp,zpath))return false;
  if (r->one_file_system && r->st_dev!=zpath->stat_rp.st_dev) return false;
#define M zpath->stat_rp.st_mode
  if (zpath->dir==DIR_PRELOADED_UPDATE && (!(M&(S_IFREG|S_IFDIR)) || (M&S_IFREG)&&!(M&S_ISVTX))) return false;
  if ((WITH_FOLLOW_SYMLINK || r->follow_symlinks) && !ZPF(ZP_NOT_EXPAND_SYMLINKS) && S_ISLNK(M) && zpath_expand_symlinks(zpath)){
    zpath->stat_rp.st_ino=0;
    return zpath_stat(opt_filldir_findrp,zpath);
  }
#undef M
  if (ZPF(ZP_TRY_ZIP)){
    if (!cg_endsWithZip(RP(),0)){ IF_LOG_FLAG(LOG_REALPATH) log_verbose("!cg_endsWithZip rp: %s\n",RP()); return false;}
    if (EP_L() && filler_readdir_zip(opt_filldir_findrp,zpath,NULL,NULL,NULL)) return false; /* This sets the file stat of zip entry */
    zpath->flags|=ZP_IS_ZIP;
  }
  return true;
}
static bool test_realpath(const int opt_filldir_findrp,const int zpath_flags,zpath_t *zpath, root_t *r){
  assert(r!=NULL);
  bool ok=false;
  zpath->flags|=zpath_flags;
  IF1(WITH_FILECONVERSION, if (r==_root_writable && zpath->dir==DIR_FILECONVERSION  && test_realpath_pfx(true,opt_filldir_findrp,zpath,r)) ok=true);
  ok=ok || test_realpath_pfx(false,opt_filldir_findrp,zpath,r);
  if (!ok && (zpath_flags&ZP_RESET_IF_NEXISTS)) zpath_reset_keep_VP(zpath);
  return ok;
}

static bool zpath_expand_symlinks(zpath_t *zpath){
  char target[PATH_MAX+1],absolute_target[PATH_MAX+1];
  if (cg_readlink_absolute(true,RP(),target,absolute_target)) return false;
  root_t *parent_root=NULL;
  foreach_root(r) if (cg_path_equals_or_is_parent(r->rootpath,r->rootpath_l,absolute_target,strlen(absolute_target))){ parent_root=r; break;}
  const bool ok=parent_root || config_allow_expand_symlink(RP(),target,absolute_target);
  //log_verbose("%s -> %s    '%s' parent_root:%s",RP(),target,absolute_target,yes_no(parent_root!=NULL));

  if (ok){
    zpath_set_realpath(zpath,absolute_target,NULL);
    if (parent_root) zpath->root=parent_root;
  }
  //log_exited_function("RP:'%s'    absolute_target:'%s'  parent_root:%s ok:%d",RP(),absolute_target,parent_root?parent_root->rootpath:"",ok);
  return ok;
}
/* Uses different approaches and calls test_realpath */
/* Initially, only zpath->virtualpath is defined. */


static bool find_realpath_for_root(const int opt_filldir_findrp,zpath_t *zpath,root_t *r){
  //log_entered_function(" %s  VP_L:%d  zipfile_l=%d",VP(), VP_L(), zpath->zipfile_l);
  if (r){
    if (r==_root_writable?  zpath->dir==DIR_EXCLUDE_FIRST_ROOT: DIR_REQUIRES_WRITABLE_ROOT(zpath->dir)){
      return false;
    }
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
      //EP_L()=zpath_commit(zpath);
      ZPATH_COMMIT(entry_path);
      zpath_assert_strlen();
      if (test_realpath(opt_filldir_findrp,(zpath->dir==DIR_PLAIN?0:ZP_TRY_ZIP)|ZP_RESET_IF_NEXISTS,zpath,r)){
        if (!EP_L()) stat_set_dir(&zpath->stat_vp); /* ZIP file without entry path */
        return true;
      }
    }
    IF1(WITH_ZIPFLAT, const yes_zero_no_t i=find_realpath_try_zipflat_rules(zpath,r); if (i) return i==YES);
  }
  /* Just a file */
  zpath_reset_keep_VP(zpath);
  return test_realpath(opt_filldir_findrp,ZP_RESET_IF_NEXISTS,zpath,r);
} /*find_realpath_for_root */
static long search_file_which_roots(const zpath_t *zpath){
  if (ZPATH_IS_FILECONVERSION(zpath)){
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
static bool find_realpath_in_roots(int opt_filldir_findrp,zpath_t *zpath, const long roots){
  if (!roots) return false;
  if (zpath->dir==DIR_PLAIN) opt_filldir_findrp|=FINDRP_IS_PFXPLAIN;
  zpath_reset_keep_VP(zpath);
  if (!(opt_filldir_findrp&FINDRP_CACHE_NOT)){

    IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, yes_zero_no_t ok=transient_cache_find_realpath(zpath); if (ok) return ok==YES);
    IF1(WITH_ZIPFLATCACHE,              if (zipflatcache_find_realpath(zpath,roots)) return true);
  }
  foreach_root(r){
    if (roots&(1<<rootindex(r)) && find_realpath_for_root(opt_filldir_findrp,zpath,r)){
      ASSERT(zpath->realpath!=0);
      IF1(WITH_TRANSIENT_ZIPENTRY_CACHES, LOCK(mutex_fhandle,transient_cache_store(zpath,VP(),VP_L())));
      return true;
    }
  }
  return false;
}
static bool find_realpath(const int opt_filldir_findrp,zpath_t *zpath){
  const long roots=search_file_which_roots(zpath);
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,   foreach_root(r){ yes_zero_no_t ok=transient_cache_find_realpath(zpath); if (ok)return ok==YES;});
#define F(mask) if ((mask) && find_realpath_in_roots(opt,zpath,mask)) return true
  int opt=opt_filldir_findrp;
  if (_root_writable) F(roots&1);
  //log_debug_now(ANSI_MAGENTA"Erste Runde %s"ANSI_RESET,VP());
  foreach_root(r){
    opt=opt_filldir_findrp|FINDRP_CACHE_ONLY;
    if (r->remote) F(roots&(1<<rootindex(r)));
  }
  //log_debug_now(ANSI_MAGENTA"Zweite Runde %s"ANSI_RESET,VP());
  foreach_root(r){
    //    opt=opt_filldir_findrp|(r->remote?FINDRP_CACHE_NOT:0);
    opt=opt_filldir_findrp;
    if (r!=_root_writable) F(roots&(1<<rootindex(r)));
  }
#undef F
  if (IF1(WITH_FILECONVERSION,!ZPATH_IS_FILECONVERSION(zpath) &&)  !config_not_report_stat_error(VP(),VP_L()) IF1(WITH_INTERNET_DOWNLOAD, && !net_is_internetfile(VP(),VP_L()))){
    warning(WARN_STAT|WARN_FLAG_ONCE_PER_PATH,VP(),"Not found");
  }
  return false;
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
static void fhandle_init(fHandle_t *d, const zpath_t *zpath){
  *d=FHANDLE_EMPTY;
  pthread_mutex_init(&d->mutex_read,NULL);
  d->zpath=*zpath;
  d->zpath.flags|=ZP_IS_IN_FHANDLE;
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
  d->fhandle_fh=fh;
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
enum { LOG2_FD_ZIP_MIN=20};
enum { FD_ZIP_MIN=1<<LOG2_FD_ZIP_MIN};
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
  fhandle_destroy_those_that_are_marked();
  const ht_hash_t h=hash_value_strg(vipa->vp);
  fHandle_t* e=NULL;
  foreach_fhandle(id,d){
    if (fh==d->fhandle_fh && D_VP_HASH(d)==h && !strcmp(vipa->vp,D_VP(d))){ e=d; break;}
  }
  //log_exited_function("fh: %lu '%s' found: %s",fh,vipa->vp,success_or_fail(e!=NULL));
  return e;
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
  //if (e->value)log_debug_now(GREEN_SUCCESS" vp:%s inode:%ld",vp,(long)e->value);
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


static void filler_add(const int opt_filldir_findrp,fuse_fill_dir_t filler,void *buf, const char *name, int name_l,const char *sfx, const struct stat *st, ht_t *no_dups){
  if (strchr(name,'/')) return;
  IF1(WITH_FILECONVERSION,if(opt_filldir_findrp&FILLDIR_FILECONVERSION) fileconversion_filldir(filler,buf,name,st,no_dups);else)
    {
      if (!name_l) name_l=strlen(name);
      const int sfx_l=cg_strlen(sfx);
      char tmp[name_l+1+sfx_l];
      if (name[name_l] || sfx_l){ /* Trim to length name_l */
        memcpy(tmp,name,name_l);
        if (sfx_l) memcpy(tmp+name_l,sfx,sfx_l);
        tmp[name_l+sfx_l]=0;
        name=tmp;
      }
      if (ht_only_once(no_dups,name,name_l)){
        assert_validchars(VALIDCHARS_FILE,name,name_l);
        filler(buf,name,st,0 COMMA_FILL_DIR_PLUS);
      }
#define X(x,code) if ((opt_filldir_findrp&(1<<COMPRESSION_##x)) && cg_endsWith(0,name,name_l,"."#x,sizeof(#x))) filler_add(0,filler,buf,name,name_l-sizeof(#x),sfx,st,no_dups);
      IF1(WITH_PRELOADDISK_DECOMPRESS,if (opt_filldir_findrp){XMACRO_COMPRESSION()});
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
      //st->st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,idx,RP());
      st->st_ino=zpath_make_inode(zpath,idx);

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
      filler_add(opt_filldir_findrp,filler,buf,n,n_l,NULL,st,no_dups);
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
  const char *append_ext=zpath->dir==DIR_PRELOADED_UPDATE?SFX_UPDATE: zpath->dir==DIR_INTERNET_UPDATE?NET_SFX_UPDATE:NULL;
  directory_t dir={0};  directory_init_zpath(&dir,zpath);
  if (readdir_from_cache_zip_or_filesystem(opt_filldir_findrp,&dir)){
    char u[MAX_PATHLEN+1]; /*buffers for unsimplify_fname() */
    const directory_core_t dc=dir.core;
    FOR(i,0,dc.files_l){
      if (cg_empty_dot_dotdot(dc.fname[i])) continue;
      int u_l=zipentry_placeholder_expand(u,dc.fname[i],RP(),&dir);
      IF1(WITH_INTERNET_DOWNLOAD,if (zpath->dir==DIR_INTERNET_UPDATE && !net_internet_filename_colon(u,u_l)) continue);
      //IF1(WITH_INTERNET_DOWNLOAD, if (_writable_path_l && (opt_filldir_findrp&FILLDIR_STRIP_NET_HEADER)) u_l=net_filename_from_header_file(u,u_l));
      if (!u_l || 0==(opt_filldir_findrp&FILLDIR_FILECONVERSION) && no_dups && ht_get(no_dups,u,u_l,0)) continue;
      IF1(WITH_ZIPFLAT,if (zpath->dir!=DIR_PLAIN && config_skip_zipfile_show_zipentries_instead(u,u_l) && readdir_zipflat_from_cache(opt_filldir_findrp,zpath,u,buf,filler,no_dups)) continue);
      struct stat st;
      if ((opt_filldir_findrp&FILLDIR_FILES_S_ISVTX) && (!cg_stat_parent_and_file(RP(),RP_L(),u,u_l, &st)|| S_IFREG==(st.st_mode&(S_ISVTX|S_IFREG)))) continue;
      const bool isDIR=Nth0(dc.fflags,i)&DIRENT_ISDIR;
      stat_init(&st,isDIR?-1:Nth0(dc.fsize,i),NULL);
      //st.st_ino=make_inode(zpath->stat_rp.st_ino,zpath->root,0,RP());
      st.st_ino=zpath_make_inode(zpath,0);
      if (!config_do_not_list_file(RP(),u,u_l)){
        *dirname_from_zip=0;
        const bool also_show_zip_file_itself=!append_ext && zpath->dir!=DIR_PLAIN && config_zipfilename_to_virtual_dirname(dirname_from_zip,u,u_l);
        if (*dirname_from_zip){
          stat_set_dir(&st);
          filler_add(opt_filldir_findrp,filler,buf,dirname_from_zip,0,NULL,&st,no_dups);
          if (also_show_zip_file_itself) filler_add(opt_filldir_findrp,filler,buf,u,u_l,isDIR?NULL:append_ext,&st,no_dups); // cppcheck-suppress knownConditionTrueFalse
        }else{
          filler_add(opt_filldir_findrp|(zpath->dir==DIR_PLAIN?0:zpath->root->decompress_mask),filler,buf,u,u_l,isDIR?NULL:append_ext,&st,no_dups);
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
  if (!_writable_path_l) return EACCES;/* Only first root is writable */
  if (config_not_overwrite(vip->vp,vip->vp_l)){
    bool found;FIND_REALPATH(vip);
    if (found && zpath->root>0) return EACCES;
  }
  assert(_writable_path_l+vip->vp_l<MAX_PATHLEN);
  const int slash=cg_last_slash(vip->vp);
  stpcpy(stpcpy(rp,_writable_path),vip->vp);
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
  return NULL;
}


static void init_special_files(){
  if (cg_uid_is_developer() && cg_file_exists(__FILE__)){
    const int slash=cg_last_slash(__FILE__);
    char tmp[PATH_MAX+1];strcpy(tmp,__FILE__);
    RLOOP(i,enum_configuration_src_N){
      char *e=stpcpy(tmp+slash+1,enum_configuration_src_S[i]);
      strcpy(e,".h"); if (cg_file_exists(tmp)) continue;
      strcpy(e,".c"); if (cg_file_exists(tmp)) continue;
      DIE(RED_ERROR"Not found '%s'",tmp);
    }
  }
}
static bool special_file_set_stat(struct stat *st, const virtualpath_t *vipa){
  const int id=vipa->special_file_id;
  bool ok=false;
  if (SFILE_IS_IN_RAM(id)){
    stat_init(st,id==SFILE_INFO?cg_file_size(SFILE_REAL_PATHS[SFILE_INFO])+222222: IF01(WITH_PRELOADRAM,0,special_file_size(id)),NULL);
    time(&st->st_mtime);
    st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP);
    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    ok=true;
  }else if (SFILE_REAL_PATHS[id]){
    ok=!lstat(SFILE_REAL_PATHS[id],st);
  }else if ((vipa->flags&ZP_IS_PATHINFO)){
    stat_init(st,PATH_MAX,NULL);
    ok=true;
  }
  IF1(WITH_PRELOADRAM,  else if (trigger_files(vipa->vp,vipa->vp_l)){ stat_init(st,0,NULL);return true;});
  if (ok && ENDSWITH(vipa->vp,vipa->vp_l,".command")) st->st_mode|=(S_IXOTH|S_IXUSR|S_IXGRP);
  return ok;
}

static void vipa_setSpecialFile(virtualpath_t *vipa){
  const char *found_p=NULL;
  int found_p_l=0, found_id=0;
  RLOOP(id,SFILE_NUM){
    const int p_l=SFILE_PARENTS_L[id];
    const char *p=SFILE_PARENTS[id];
    if (!p_l || !cg_path_equals_or_is_parent(p,p_l,vipa->vp,vipa->vp_l)) continue;
    if (found_p_l<=p_l){  /* If previously found p==DIR_ZIPsFS then p==DIR_INTERNET is stronger. */
      found_p_l=p_l;
      found_p=p;
      if (SFILE_NAMES_L[id] && p_l+1+SFILE_NAMES_L[id]==vipa->vp_l  && !strcmp(SFILE_NAMES[id],vipa->vp+p_l+1)) found_id=id;
    }
  }
  if (found_p_l){
    vipa->special_file_id=found_id;
    vipa->dir=found_p;
    vipa->dir_l=found_p_l;
    if (found_p==DIR_PRELOADDISK_R||found_p==DIR_PRELOADDISK_RC||found_p==DIR_PRELOADDISK_RZ){
      vipa->preloadpfx_l=found_p_l;
      vipa->preloadpfx=found_p;
    }
    if (SPECIAL_DIR_STRIP(found_p)){vipa->vp+=found_p_l;vipa->vp_l-=found_p_l;}
  }
}

static int virtualpath_init(virtualpath_t *vipa, const char *vpath, char *buf){
  *buf=0;
  ASSERT(vpath!=NULL);
  *vipa=empty_virtualpath;
  vipa->vp_l=strlen((vipa->vp=vpath+(*vpath=='/'&&!vpath[1])));
  if (PATH_STARTS_WITH_DIR_ZIPsFS(vipa->vp)) vipa_setSpecialFile(vipa);
  if (64+vipa->vp_l+_rootdata_path_max>MAX_PATHLEN) return ENAMETOOLONG;
  int cut=0;
#define S() cut=ENDSWITH(vipa->vp,vipa->vp_l,SFX_UPDATE)?sizeof(SFX_UPDATE)-1: ENDSWITH(vipa->vp,vipa->vp_l,NET_SFX_UPDATE)?sizeof(NET_SFX_UPDATE)-1:0
#define B(l) if (!*buf) strncpy(buf,vipa->vp,l); buf[l]=0
  if (vipa->dir!=DIR_PLAIN) vipa->zipfile_l=virtual_dirpath_to_zipfile(vipa->vp, vipa->vp_l, &vipa->zipfile_cutr, &vipa->zipfile_append);
  //log_debug_now("%s vipa->zipfile_l=%d",vpath,vipa->zipfile_l);
  if (vipa->dir==DIR_INTERNET_UPDATE){
    S();
    stpcpy(stpcpy(buf,DIR_INTERNET),vipa->vp+DIR_INTERNET_UPDATE_L);
  }else if (vipa->dir==DIR_PRELOADED_UPDATE){
    S();
  }
  if (ENDSWITH(vipa->vp,vipa->vp_l,VFILE_SFX_INFO)){
    B(vipa->vp_l-(sizeof(VFILE_SFX_INFO)-1));
    vipa->flags|=ZP_IS_PATHINFO;
  }
  if (cg_strcasestr(vpath,"$NOCSC$")){ /* Windows no-client-side-cache */
    B(vipa->vp_l);
    cg_str_replace(0,buf,0, "$NOCSC$",7,"",0);
  }
  if (cut) B(vipa->vp_l);
  if (cut) ASSERT(*buf);
  if (*buf){
    vipa->vp_l=strlen(buf)-cut;
    if (cut) buf[vipa->vp_l]=0;
    vipa->vp=buf;
  }
#undef S
#undef B
  return 0;
}
static int virtualpath_error(const virtualpath_t *vipa,const int create_or_del){
  IF0(WITH_PRELOADRAM, if (vipa->dir==DIR_INTERNET_UPDATE || vipa->dir==DIR_PRELOADED_UPDATE) return EACCES);
  if (!_writable_path_l &&  (create_or_del==1 || DIR_REQUIRES_WRITABLE_ROOT(vipa->dir))) return EACCES;
  if (vipa->vp_l==0 && vipa->preloadpfx_l) return create_or_del==1?EEXIST:EPERM;
  if (vipa->dir && create_or_del){
    if (vipa->dir==DIR_INTERNET && create_or_del==1) return EEXIST;
    if (create_or_del==1 && SFILE_IS_IN_RAM(vipa->special_file_id)) return EACCES;
  }
  return 0;
}




/*  Release FUSE 2.9 The chmod, chown, truncate, utimens and getattr handlers of the high-level API now  additional struct fuse_file_info pointer (which, may be NULL even if the file is currently open) */
#if VERSION_AT_LEAST(FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION, 2,10)
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 1
#else
#define WITH_XMP_GETATTR_FUSE_FILE_INFO 0
#endif



static int _xmp_getattr(const virtualpath_t *vipa, struct stat *st){ /* NOT_TO_HEADER */
  if (special_file_set_stat(st,vipa)) return 0;
  bool vp_is_root_pfx=false;
  foreach_root(r) if (IS_VP_IN_ROOT_PFX(r,vipa)) vp_is_root_pfx=true;
  if (vipa->dir && vipa->vp_l==vipa->dir_l || vp_is_root_pfx){
    stat_init(st,-1,NULL);
    st->st_ino=inode_from_virtualpath(vipa->vp,vipa->vp_l);
    time(&st->st_mtime);
    return 0;
  }
  IF1(WITH_CCODE, if (c_getattr(st,vipa)) return 0);
  bool found;FIND_REALPATH(vipa);
  int er=0;
  if (found){
    *st=zpath->stat_vp;
  }else{
    if (vipa->dir!=DIR_PRELOADED_UPDATE){
      IF1(WITH_INTERNET_DOWNLOAD, if (net_getattr(st,vipa)) return 0);
      IF1(WITH_FILECONVERSION,    if (fileconversion_getattr(st,zpath,vipa)) return 0);
    }
    er=ENOENT;
  }
  inc_count_by_ext(vipa->vp,er?COUNTER_GETATTR_FAIL:COUNTER_GETATTR_SUCCESS);
  IF1(WITH_DEBUG_TRACK_FALSE_GETATTR_ERRORS, if (er) debug_track_false_getattr_errors(vipa->vp,vipa->vp_l));
  if (config_file_is_readonly(vipa->vp,vipa->vp_l)) st->st_mode&=~(S_IWOTH|S_IWUSR|S_IWGRP); /* Does not improve performance */
  return -er;
}/*xmp_getattr*/
static int xmp_getattr(const char *vpath, struct stat *st IF1(WITH_XMP_GETATTR_FUSE_FILE_INFO,,struct fuse_file_info *fi_or_null)){
  FUSE_PREAMBLE(vpath);
  if (!er) er=_xmp_getattr(&vipa ,st);
  log_fuse_function(__func__,&vipa,er);
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
  int res=found?0:-ENOENT;
  IF1(WITH_INTERNET_DOWNLOAD,if (!res && zpath->dir==DIR_INTERNET && VP_L()>DIR_INTERNET_L+6 && config_internet_must_not_delete(VP()+(DIR_INTERNET_L+1),VP_L()-(DIR_INTERNET_L+1))) res=-EPERM);
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
  const int id=vipa->special_file_id;
  uint64_t fh=0;
  if (SFILE_REAL_PATHS[id]){
    fh=open(SFILE_REAL_PATHS[id],O_RDONLY);
    if (fh<3){ warning(WARN_OPEN|WARN_FLAG_ERRNO,SFILE_REAL_PATHS[id],"open()");  return -errno;}
    fi->fh=fh;
    return 0;
  }
  NEW_ZIPPATH(vipa);
  IF1(WITH_SPECIAL_FILE,fh=special_file_file_content_to_fhandle(zpath,id)); /* Only case where file content is generated in xmp_open() */
  if (!fh){ /* ID for fHandle_t  */
    int ff=IF1(WITH_CCODE, config_c_open(C_FLAGS_FROM_ZPATH(zpath),VP(),VP_L())?FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_CCODE:)0;
    IF1(WITH_INTERNET_DOWNLOAD,  if (!ff && vipa->dir==DIR_INTERNET_UPDATE) ff=FHANDLE_PREPARE_ONCE_IN_RW);
    if (!ff && find_realpath(FILLDIR_FROM_OPEN,zpath)  IF1(WITH_FILECONVERSION,&&!fileconversion_remove_if_not_uptodate(zpath))){
      IF1(WITH_PRELOADDISK,      if (!ff && (is_preloaddisk_zpath(zpath) || vipa->dir==DIR_PRELOADED_UPDATE || path_with_compress_sfx_exists(zpath))) ff=FHANDLE_PREPARE_ONCE_IN_RW);
      IF1(WITH_PRELOADRAM,       if (!ff && _preloadram_policy!=PRELOADRAM_NEVER && (ZPF(ZP_IS_ZIP) || preloadram_advise(zpath,0))) ff=FHANDLE_WITH_PRELOADRAM);
    }else{
      IF1(WITH_INTERNET_DOWNLOAD,if (!ff && net_is_internetfile(VP(),VP_L())) ff=FHANDLE_PREPARE_ONCE_IN_RW);
      IF1(WITH_FILECONVERSION,   if (!ff && fileconversion_check_infiles_exist(vipa)){ LOCK(mutex_fhandle, fileconversion_set_rp(zpath,vipa)); ff=FHANDLE_PREPARE_ONCE_IN_RW|FHANDLE_IS_FILECONVERSION;});
    }
    //log_debug_now(ANSI_MAGENTA"vipa->vp: %s ff:%d  is_preloaddisk_zpath:%d  zpath->preloadpfx:%s zpath->root:%s  "ANSI_RESET,vipa->vp,ff, is_preloaddisk_zpath(zpath), zpath->preloadpfx, rootpath(zpath->root));
    //log_debug_now(ANSI_MAGENTA"vipa->vp: %s ff:%d path_with_compress_sfx_exists:%d",VP(),ff,path_with_compress_sfx_exists(zpath));
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
  errno=0;
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE_W((fi->flags&(O_WRONLY||O_RDWR|O_APPEND|O_CREAT))?1:0,vpath);
  if (!er)	er=_xmp_open(&vipa,fi);
  IF_LOG_FLAG_OR(LOG_OPEN,er!=0)log_exited_function("%s res: %d",vpath,er);
  if (fi->fh>2 && fi->fh<COUNT_BACKWARD_SEEK) _count_backward_seek[fi->fh]=0;
  return -er;
}
static int _xmp_open(const virtualpath_t *vipa, struct fuse_file_info *fi){
  int res=0;

  if (vipa->special_file_id==SFILE_INFO){
    if (fi->flags&O_WRONLY) return -EPERM;
    const char *rp=print_info_file();
    if (!rp) return !errno?-1:-errno;
    const int fd=open(rp,O_RDONLY);
    if (fd<3){ warning(WARN_OPEN|WARN_FLAG_ERRNO,rp,"open(O_RDONLY)"); return -errno;}
    fi->fh=fd;
  }else if ((fi->flags&O_WRONLY) || ((fi->flags&(O_RDWR|O_CREAT))==(O_RDWR|O_CREAT))){
    res=-create_or_open(vipa,0775,fi);
    log_fuse_function("open_write",vipa,res);
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
  FUSE_PREAMBLE(vpath);
  // static int count=0;log_entered_function("# %d %s",count++,vpath);
  const int res=_xmp_readdir(&vipa,buf,filler,offset,fi);
  log_fuse_function(__func__,&vipa,res);
  inc_count_by_ext(vpath,res?COUNTER_READDIR_FAIL:COUNTER_READDIR_SUCCESS);
  return res;
}
static int _xmp_readdir(const virtualpath_t *vipa, void *buf, fuse_fill_dir_t filler,off_t offset, struct fuse_file_info *fi){
  (void)offset;(void)fi;
  ht_t no_dups={0}; HT_INIT_WITH_KEYSTORE_DIM(&no_dups,8,4096); ht_set_id(HT_MALLOC_without_dups,&no_dups);
  no_dups.keystore->mstore_counter_mmap=COUNT_MSTORE_MMAP_NODUPS;
  no_dups.ht_counter_malloc=COUNT_HT_MALLOC_NODUPS;

  int opt=((vipa->dir==DIR_PRELOADED_UPDATE)?FILLDIR_FILES_S_ISVTX:0);
  NEW_ZIPPATH(vipa);
#define A(n) filler_add(0,filler,buf,n,0,NULL,NULL,&no_dups);
  bool ok=false;
  const int vp_l=VP_L();
  {
    int opt_rp=0;
    foreach_root(r){ /* FINDRP_FILECONVERSION_CUT_NOT means only without cut.  Giving 0 means cut and not cut. */
      if (r!=_root_writable && DIR_REQUIRES_WRITABLE_ROOT(vipa->dir)) continue;
      if (vp_l<r->pathpfx_l && IS_VP_IN_ROOT_PFX(r,vipa)){
        ok=true;
        filler_add(0,filler,buf,r->pathpfx_slash_to_null+vp_l+1,0,NULL,NULL,&no_dups);
      }else if (find_realpath_in_roots(opt_rp,zpath,1<<rootindex(r))){
        filler_readdir(opt,zpath,buf,filler,&no_dups);
        IF1(WITH_FILECONVERSION, if (ZPATH_IS_FILECONVERSION(zpath)) filler_readdir(FILLDIR_FILECONVERSION,zpath,buf,filler,&no_dups));
        ok=true;
        IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,if (ZPF(ZP_FROM_TRANSIENT_CACHE))break); /*Performance*/
        if (config_readir_no_other_roots(RP(),RP_L())) break; /*Performance*/
      }
    }
  }
  const bool is_dir_zipsfs=(vipa->vp_l==DIR_ZIPsFS_L && vipa->dir==DIR_ZIPsFS);
  if (is_dir_zipsfs) ok=true;
  FOR(i,0,SFILE_NUM){
    const char *d=SFILE_PARENTS[i], *n=SFILE_NAMES[i];
    if (!d) continue;
    if (n && vipa->dir==d && (vp_l==vipa->dir_l||!vp_l)) A(n);
    if (is_dir_zipsfs && d!=DIR_ZIPsFS) A(d+(DIR_ZIPsFS_L+1));
  }
  IF1(WITH_CCODE, if (c_readdir(zpath,buf,filler,NULL)) ok=true);
  IF1(WITH_PRELOADRAM,if (vipa->dir==DIR_INTERNET  && vipa->vp_l==DIR_INTERNET_L)  A((DIR_INTERNET_UPDATE)+(DIR_INTERNET_L+1)));
  if (!vipa->vp_l && !vipa->dir) A((DIR_ZIPsFS)+1);
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
    LOCK_N(mutex_fhandle, fHandle_t *d=fhandle_get(&vipa,fd); fhandle_busy_start(d));
    if (d){
      fhandle_prepare_in_fuse_read_or_write(d,fi->flags);
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
  IF_LOG_FLAG(LOG_ZIP){ static int count; log_verbose("Going to zip_open(%s) #%d ... ",orig,count);}
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
static off_t _viamacro_my_zip_fread(zip_file_t *file, void *buf, const zip_uint64_t nbytes, const char *func){
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
    async_zipfile_t zip;
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
        log_verbose(ANSI_MAGENTA"Seek-bwd->RAM"ANSI_RESET);
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
static void fhandle_prepare_in_fuse_read_or_write(fHandle_t *d,const int open_flags){
  if (!(d->flags&FHANDLE_PREPARE_ONCE_IN_RW)) return;
  zpath_t *zpath=&d->zpath;
  LOCK(mutex_fhandle, d->flags&=~FHANDLE_PREPARE_ONCE_IN_RW);
  if (DIR_REQUIRES_WRITABLE_ROOT(zpath->dir))    zpath->root=_root_writable;
  IF1(WITH_PRELOADDISK, if (d->zpath.dir==DIR_PRELOADED_UPDATE) preloaddisk_uptodate_or_update(d));
  IF1(WITH_CCODE, c_file_content_to_fhandle(d));
  IF1(WITH_FILECONVERSION, if (open_flags==O_RDONLY && (d->flags&FHANDLE_IS_FILECONVERSION)) fileconversion_run(d));
  bool do_open=!ZPF(ZP_IS_ZIP) && !D_HAS_TB();
  {
    int Done=0;
    if (D_HAS_TB()){                                                                                     Done=1; d->flags|=FHANDLE_PRELOADRAM_COMPLETE;}
    IF1(WITH_INTERNET_DOWNLOAD, if (zpath->dir==DIR_INTERNET_UPDATE && !Done++){                         do_open=false;net_update(d);});
    IF1(WITH_INTERNET_DOWNLOAD, if (zpath->dir==DIR_INTERNET && !Done++){                                do_open=net_maybe_download(0,zpath); });
    if (open_flags==O_RDONLY){
      IF1(WITH_PRELOADDISK,
          if (!Done && !zpath->root->remote && zpath->is_decompressed){ Done=1; if (!fHandle_preloadfile_now(d)) d->errorno=EPIPE;}
          if (!Done && is_preloaddisk_zpath(&d->zpath)){ Done=1;  do_open=false; d->errorno=preloaddisk(d);});
    }
  }
  if (do_open){
    if (!RP_L() || !zpath_stat(0,zpath)){
      if (!d->errorno) d->errorno=ENOENT;
      warning(WARN_READ,VP(),"open root: %s RP:%s  D_HAS_TB:%d",rootpath(zpath->root),RP(),D_HAS_TB());
    }else if (!(d->fd_real=open(RP(),open_flags))){
      d->errorno=errno;
      log_errno("open RP:%s",RP());
    }
  }
  if (!d->errorno && d->fd_real<0) d->errorno=EIO;
}

#define IF(d) IF0(IS_CHECKING_CODE,if(d))
static int xmp_read(const char *vpath, char *buf, const size_t size, const off_t offset,struct fuse_file_info *fi){
  IF_LOG_FLAG(LOG_READ_BLOCK)log_entered_function("%s Offset: %'ld +%'d ",vpath,(long)offset,(int)size);
  ASSERT(fi!=NULL); ASSERT(fi->fh);
  FUSE_PREAMBLE_Q(vpath);
  static off_t  debug_pos;
  LOCK_N(mutex_fhandle, fHandle_t *d=fhandle_get(&vipa,fi->fh);IF(d){ d->accesstime=time(NULL); fhandle_busy_start(d);});
  const int bytes=_xmp_read(&vipa,d,buf,size,offset,fi->fh);
  IF(d) { LOCK(mutex_fhandle,fhandle_busy_end(d));}
  //IF_LOG_FLAG_OR(LOG_READ_BLOCK,bytes<=0)log_exited_function("%s  %'ld +%'d Bytes: %'d %s",path,(long)offset,(int)size,bytes,success_or_fail(bytes>0));
  debug_pos=offset+bytes;
  return bytes; // cppcheck-suppress resourceLeak
}
static int _xmp_read(const virtualpath_t *vipa, fHandle_t *d, char *buf, const size_t size, const off_t offset,int fd){
  off_t nread=-1;
  if (d){
    ASSERT(d->is_busy>=0);
    fhandle_prepare_in_fuse_read_or_write(d,O_RDONLY);
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
      LOCK_N(mutex_fhandle, const char *status=IF01(WITH_PRELOADRAM,"NA",enum_preloadram_status_S[preloadram_get_status(d)]));
      warning(WARN_READ|WARN_FLAG_ONCE_PER_PATH,d?D_RP(d):vipa->vp,"nread %lld<0: +%ld %lld    n_read=%llu  status:%s",(LLD)nread,offset,(LLD)size,d->n_read,status);
    }
    if(nread>0){
      if (offset!=d->offset_expected) d->count_backward_seek++;
      d->offset_expected+=nread;
    }

  }
 d_has_fd: /* Normal reading from file descriptor  d->fd_real or fi->fh */
  if (nread<0){
    if (d && d->fd_real) fd=d->fd_real;
    const off_t seeking=offset-lseek(fd,0,SEEK_CUR);
    if (seeking<0 && fd>2 && fd<COUNT_BACKWARD_SEEK) _count_backward_seek[fd]++;
    if (seeking && offset!=lseek(fd,offset,SEEK_SET)){
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
  int count_backward_seek=0;
  const uint64_t fh=fi->fh;
  if (fh>=FD_ZIP_MIN){
    lock(mutex_fhandle);
    fHandle_t *d=fhandle_get(&vipa,fh);
    if (d){
      count_backward_seek=d->count_backward_seek;
      fhandle_destroy(d);
    }
    unlock(mutex_fhandle);
  }else if (fh>2){
    maybe_evict_from_filecache(fh,vipa.vp,vipa.vp_l,NULL,0);
    if ((er=close(fh))){
      warning(WARN_OPEN|WARN_FLAG_ERRNO,vpath,"close(fh: %llu)",fh);
      cg_print_path_for_fd(fh);
    }
    count_backward_seek=fh<COUNT_BACKWARD_SEEK?_count_backward_seek[fh]:0;
  }
  log_fuse_function(__func__,&vipa,count_backward_seek);
  return -er;
}
static int xmp_flush(const char *vpath, struct fuse_file_info *fi){
  ASSERT(fi!=NULL);
  FUSE_PREAMBLE(vpath);
  IF1(WITH_SPECIAL_FILE, if (vipa.special_file_id) return 0);
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
    FOR(i,0,enum_warnings_N) _warning_channel_name[i]=(char*)enum_warnings_S[i];
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
  if (argc==1) return 0;
  setlocale(LC_NUMERIC,""); /* Enables decimal grouping in fprintf */

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
    FOR(id,1,SFILE_NUM){
      if (!SFILE_HAS_REALPATH(id)) continue;
      snprintf(path,MAX_PATHLEN,"%s/%s",_dot_ZIPsFS,SFILE_NAMES[id]);
      SFILE_REAL_PATHS[id]=strdup_untracked(path);
      struct stat st;
      if (id==SFILE_LOG_ERRORS||id==SFILE_LOG_WARNINGS){
        if (!lstat(path,&st) && st.st_size){ /* Save old logs with a mtime in file name. */
          const time_t t=st.st_mtime;
          struct tm lt;
          localtime_r(&t,&lt);
          snprintf(tmp,MAX_PATHLEN,"%s/%s",dirOldLogs,SFILE_NAMES[id]);
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
    log_fuse_function_fd();
    warning(0,NULL,"");ht_set_id(HT_MALLOC_warnings,&_ht_warning);
    IF1(WITH_SPECIAL_FILE,	special_file_content_to_file(SFILE_DEBUG_CTRL,SFILE_REAL_PATHS[SFILE_DEBUG_CTRL]));
    snprintf(tmp,MAX_PATHLEN,"%s/cachedir",_dot_ZIPsFS); mstore_set_base_path(tmp);
  }
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
    if (_writable_path_l){
      char rp[MAX_PATHLEN+1];
      assert(_writable_path_l+sizeof(FILE_CLEANUP_SCRIPT)<sizeof(rp));
      _cleanup_script=strdup_untracked(strcat(strcpy(rp,_writable_path),FILE_CLEANUP_SCRIPT));
    }
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

  root_property_read_all(NULL,NULL,0); /* free line */
  log_msg("\n\nMount point: "ANSI_FG_BLUE"'%s'"ANSI_RESET"\n\n",_mnt);
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
  log_print_roots(stderr);
  const int fuse_stat=fuse_main(_fuse_argc,(char**)_fuse_argv,&xmp_oper,NULL);
  _fuse_started=true;
  log_msg(RED_WARNING" fuse_main returned %d\n",fuse_stat);
  IF1(WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT,IF1(WITH_DIRCACHE,dircache_clear_if_reached_limit_all(true,0xFFFF)));
  exit_ZIPsFS();
}

////////////////////////////////////////////////////////////////////////////
// DIE  log_  FIXME  DIE_DEBUG_NOW   DEBUG_NOW   log_debug_now  log_entered_function     log_exited_function    directory_print
// _GNU_SOURCE    HAS_EXECVPE HAS_UNDERSCORE_ENVIRON   HAS_ST_MTIM   HAS_POSIX_FADVISE
// malloc calloc strdup  free  mmap munmap   readdir opendir   --- malloc_untracked calloc_untracked  strdup_untracked
//
// ZIPsFS_print_source.sh  mnt/zipsfs/lr/Z1/Data/30-0001/20220802_Z1_AF_009_30-0001_SWATH_P01_Plasma-V-Position-Rep5.wiff  SOURCE.TX VFILE_SFX_INFO ZP_IS_PATHINFO
// md5sum   mnt/zipsfs/lrz/misc_tests/zip/20220202_A1_KTT_008_22-2222_Benchmarking4_dia.raw
// error_symbol
// cg_split_string  64 wait_for_root_timeout   find_realpath_try_zipflat_rules() find_realpath_try_zipflat()
// readdir_from_zip stat_direct
// readdir_from_cache_zip_or_filesystem  dircache_directory_from_cache dircache_directory_to_cache
// zpath_stat_from_cache( key_from_rp lrz  ENOENT ERANGE
// COPY_FLAGS_BEGIN   dircache_directory_to_cache()  dircache_directory_to_cache  dircache_directory_from_cache
//  fileconversion-fsize
//  _ht_fsize
// fileconversion_rule_t fileconversion_rule
// WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
