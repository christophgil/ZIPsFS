/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Enums and structs                                         ///
// NO_SANITIZER
/////////////////////////////////////////////////////////////////
#ifndef IF1
#define IF1(zeroorone,...) __VA_ARGS__
#define WITH_PRELOADRAM 1
#define WITH_CCODE 1
#define WITH_INTERNET_DOWNLOAD 1
#define WITH_FILECONVERSION 1
#define TEST_COMPILATION_ISOLATED 1
#endif //IF1



#define IS_ZIPSFS 1
#define WITH_SERIALIZED 0


//////////////////////////////
/// Constants and Macros   ///
//////////////////////////////
#define WITH_SPECIAL_FILE WITH_PRELOADRAM
#define ZPATH_STRCAT(s)       zpath_strncat(zpath,s,9999)
#define ZPATH_STRCAT_N(s,n)   zpath_strncat(zpath,s,n)
#define ZPATH_NEWSTR(x)       zpath->x=zpath_newstr(zpath)
#define ZPATH_COMMIT(x)       zpath->x##_l=zpath->strgs_l-zpath->current_string
#define ZPATH_COMMIT_HASH(x)  ZPATH_COMMIT(x); zpath->x##_hash=hash32(zpath->strgs+zpath->current_string,zpath->x##_l)
#define zpath_assert_strlen()  _viamacro_zpath_assert_strlen(__func__,__FILE_NAME__,__LINE__,zpath)
#define Nth(array,i,defaultVal) (array?array[i]:defaultVal)
#define Nth0(array,i) Nth(array,i,0)

#define PROFILED(x) x


#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
#define EXT_ROOT_PROPERTY ".ZIPsFS.properties"
#define PATH_DOT_ZIPSFS "~/.ZIPsFS"
#define NUM_MUTEX   (mutex_roots+ROOTS)


typedef uint64_t vfolder_flags_t;
typedef enum {OUTPUT_TEXT,OUTPUT_ANSI,OUTPUT_HTML} output_formats_t;
enum {DIRENT_ISDIR=1<<0,DIRENT_IS_COMPRESSEDZIPENTRY=1<<1,DIRENT_DIRECT_NAME=1<<2,DIRENT_DIRECT_DEBUG=1<<3};
enum {DIRENT_SAVE_MASK=DIRENT_IS_COMPRESSEDZIPENTRY|DIRENT_ISDIR};


enum{
  MALLOC_TYPE_SHIFT=8,
  MALLOC_TYPE_HT=1<<MALLOC_TYPE_SHIFT,
  MALLOC_TYPE_KEYSTORE=2<<MALLOC_TYPE_SHIFT,
  MALLOC_TYPE_VALUESTORE=3<<MALLOC_TYPE_SHIFT,
  MALLOC_ID_COUNT=5<<MALLOC_TYPE_SHIFT,
};
#define CG_CLEANUP_BEFORE_EXIT() exit_ZIPsFS()

#if !WITH_PRELOADRAM && (WITH_CCODE||WITH_INTERNET_DOWNLOAD||WITH_FILECONVERSION)
#warning "Since WITH_PRELOADRAM is 0, other in the config activated features will also be deactivated"
#undef WITH_CCODE
#undef WITH_INTERNET_DOWNLOAD
#undef WITH_FILECONVERSION
#define WITH_CCODE 0
#define WITH_INTERNET_DOWNLOAD 1
#define WITH_FILECONVERSION 0
#endif // !WITH_PRELOADRAM

#if WITH_FILECONVERSION || WITH_CCODE
#define WITH_FILECONVERSION_OR_CCODE 1
#else
#define WITH_FILECONVERSION_OR_CCODE 0
#endif


/////////////
/// debug ///
/////////////

#define WITH_DEBUG_THROTTLE_DOWNLOAD 0

/////////////
/// Files ///
/////////////
#define PATH_STARTS_WITH_DIR_ZIPsFS(p)    (p[0]=='/'&&p[1]=='z'&&p[2]=='i'&&p[3]=='p'&&p[4]=='s'&&p[5]=='f'&&p[6]=='s'&&(p[7]=='/'||!p[7]))
#define DIR_ZIPsFS                   "/zipsfs"
#define DIR_SCRIPTS                  DIR_ZIPsFS"/scripts"
#define DIR_SCRIPTS_UNIX             DIR_SCRIPTS"/UNIX"
#define DIR_SCRIPTS_WINDOWS          DIR_SCRIPTS"/windows"
#define DIR_PRELOADED                DIR_ZIPsFS"/preloaded"
#define DIR_CONVERTED                DIR_ZIPsFS"/converted"
#define DIR_INTERNET                 DIR_ZIPsFS"/n"
#define DIR_LOGGED                   DIR_ZIPsFS"/log"



#define SFX_UPDATE     "._update_.htmL"
#define SFX_UPDATE_L            (sizeof(SFX_UPDATE)-1)
#define DIR_INTERNET_L          (sizeof(DIR_INTERNET)-1)
#define NET_SFX_HEADER "._Header_.TXT"
#define NET_SFX_HEADER_L (sizeof(NET_SFX_HEADER)-1)
#define NET_SFX_FAIL   "._Failed_.TXT"
#define DIRNAME_PRELOADDISK_UPDATE "UPDATE"
#define DIRNAME_INTERNET_UPDATE "_UPDATE_"


static const char *_vfolders_in_zipsfs[]={DIR_ZIPsFS,DIR_SCRIPTS,DIR_SCRIPTS_WINDOWS,DIR_SCRIPTS_UNIX,DIR_LOGGED,DIR_PRELOADED,DIR_CONVERTED,DIR_INTERNET,NULL};


#define SF_README_FOR_DIR(dir) README_##dir
#define _SF_README(dir) X("_README_"#dir".html",dir,README_##dir)

#define XMACRO_SPECIALFILES()\
  X("/warnings.log",                     DIR_LOGGED,            SFILE_LOG_WARNINGS)\
  X("/errors.log",                       DIR_LOGGED,            SFILE_LOG_ERRORS)\
  X("/_FUNCTION_CALLS.log",              DIR_LOGGED,            SFILE_LOG_FUNCTION_CALLS)\
  X("/zipsfs_ctrl.sh",                   NULL,                  SFILE_DEBUG_CTRL)\
  X("/file_system_info.html",            DIR_ZIPsFS,            SFILE_INFO)\
  X("/ZIPsFS_cleanup.sh",                DIR_SCRIPTS_UNIX,      SFILE_CLEANUP_SH)\
  X("Clear_cache.command",				 DIR_SCRIPTS_UNIX,      SFILE_CLEAR_CACHE)\
  X("Set_file_access_time.command",		 DIR_SCRIPTS_UNIX,      SFILE_SET_ATIME_SH)\
  X("Set_file_access_time.ps1",			 DIR_SCRIPTS_WINDOWS,   SFILE_SET_ATIME_PS)\
  X("Read_beginning_of_files.bat",       DIR_SCRIPTS_WINDOWS,   SFILE_READ_BEGINNING_OF_FILES_BAT)\
  X("Read_beginning_of_files.command",   DIR_SCRIPTS_WINDOWS,   SFILE_READ_BEGINNING_OF_FILES_SH)\
  X("_logging.command",                  DIR_LOGGED,            SFILE_LOGGING_COMMAND)\
  X("_NET_FETCH_.ps1",                   DIR_INTERNET,          SFILE_NET_FETCH_PS)\
  X("_NET_FETCH_.command",               DIR_INTERNET,          SFILE_NET_FETCH_SH)\
  X(NULL,                  NULL,                  SFILE_PRELOAD_UPDATE_GO)\
  X(NULL,                  NULL,                  SFILE_INTERNET_UPDATE_GO)\
  _SF_README(DIR_ZIPsFS)\
  _SF_README(DIR_LOGGED)



/************************************************************************************/
/* Special files like READMEs and scripts.                                          */
/* Their content is immutable.                                                      */
/* Each is initialized in specialfile_init() with the macro XMACRO_SPECIALFILES() */
/* For READMEs they are also initialized with XMACRO_DIRFLAGS()                     */
/* Reference by index virtualpath_t->specialfile_id  zpath_t->specialfile_id      */
/************************************************************************************/
typedef struct { const char *name,*parent,*rp; int id,name_l;} specialfile_t;
#define FOREACH_SPECIAL_FILE(sf) for(specialfile_t *sf=_specialfiles; sf<_specialfiles+SFILE_NUM; sf++) if (sf->name)


#define XMACRO_DIRFLAGS()   X(VIEWMOD_NOTHING,NULL)\
    X(VIEWMOD_NIL,"-") X(PRELOAD_UPDATE,"") X(INTERNET_UPDATE,"")\
    X(VIEWMOD_INTERNET,"n") X(VIEWMOD_KEEP_ZIP,"z") X(VIEWMOD_DECOMPRESS,"d") X(VIEWMOD_1,"1") X(VIEWMOD_1_NOT,"-1") X(VIEWMOD_FILECONVERSION,"c")\
    X(PRELOADRAM,"m") X(PRELOADDISK,"l")  X(PRELOADDISK_ENTIRE_ZIP,"z") X(PRELOADDISK_ENTIRE_ZIP_NOT,"-z")  X(PRELOADRAM_NOT,"-m") X(PRELOADDISK_NOT,"-l")\
    X(PRELOAD_SELECT_ALL,"a") X(PRELOAD_SELECT_REMOTE,"r") X(PRELOAD_SELECT_ZIP,"z")


#define ID_RANGE_VIEWMOD  0,ID_(PRELOADRAM)
#define ID_RANGE_PRELOAD  ID_(PRELOADRAM),ID_(PRELOAD_SELECT_ALL)
#define ID_RANGE_PRELOAD_SELECT  ID_(PRELOAD_SELECT_ALL),999
static bool num_in_range(int i,int from,int to){
  return from<=i && i<to;
}

#define ID_(f) ID_##f
#define ID_FLAG(f) (1ULL<<ID_##f)

#define IS_VFOLDER_ROOT(vf) (!*(vf)->childs && ((vf)->flags&(ID_FLAG(VIEWMOD_NIL)|ID_FLAG(VIEWMOD_INTERNET)|ID_FLAG(PRELOAD_UPDATE)|ID_FLAG(VIEWMOD_1)|ID_FLAG(VIEWMOD_1_NOT)|ID_FLAG(VIEWMOD_FILECONVERSION)|ID_FLAG(VIEWMOD_KEEP_ZIP)|ID_FLAG(VIEWMOD_DECOMPRESS))))
#define IS_VFOLDER_SKIP_READONLY_ROOT(zpath) (zpath->vfolder->flags&(ID_FLAG(VIEWMOD_1)|(ID_FLAG(VIEWMOD_INTERNET)|ID_FLAG(PRELOAD_UPDATE)|ID_FLAG(INTERNET_UPDATE))))


enum{
#define X(id,...) ID_(id),
  XMACRO_DIRFLAGS()
#undef X
  VFOLDER_FLAG_NUM,
#define X(filename,parent,id) id,
  XMACRO_SPECIALFILES()
#undef X
  SFILE_NUM,
};

#define X(id,c) c,
static const char *_virtualfolder_dirflags[SFILE_NUM]={XMACRO_DIRFLAGS()};
#undef X

_Static_assert(VFOLDER_FLAG_NUM<30,"VFOLDER_FLAG_NUM Too many flags");

typedef enum{xmp_open_,xmp_access_,xmp_getattr_,xmp_read_,xmp_readdir_,functions_l} enum_functions_t;
typedef enum{
  COUNTER_STAT_FAIL,COUNTER_STAT_SUCCESS,
  COUNTER_OPENDIR_FAIL,COUNTER_OPENDIR_SUCCESS,
  COUNTER_ZIPOPEN_FAIL,COUNTER_ZIPOPEN_SUCCESS,
  COUNTER_GETATTR_FAIL,COUNTER_GETATTR_SUCCESS,
  COUNTER_READDIR_SUCCESS,COUNTER_READDIR_FAIL,
  COUNTER_ROOTDATA_INITIALIZED,enum_count_getattr_length} enum_count_getattr_t;
typedef enum{
  ZIP_OPEN_SUCCESS,ZIP_OPEN_FAIL,
  //
  ZIP_READ_NOCACHE_SUCCESS,ZIP_READ_NOCACHE_ZERO, ZIP_READ_NOCACHE_FAIL,
  ZIP_READ_NOCACHE_SEEK_SUCCESS,ZIP_READ_NOCACHE_SEEK_FAIL,
  //
  ZIP_READ_CACHE_SUCCESS,ZIP_READ_CACHE_FAIL,
  ZIP_READ_CACHE_CRC32_SUCCESS,ZIP_READ_CACHE_CRC32_FAIL,
  //
  COUNT_RETRY_PRELOADRAM,
  //
  counter_rootdata_num} enum_counter_rootdata_t;



/*****************/
/* Sepcial files */
/*****************/
#define SFILE_IS_IMMUTABLE(id)      (id>0 && id<SFILE_NUM && !_specialfiles[id].rp && _specialfiles[id].name)
/***************/
/* String      */
/***************/
//#define PATH_STARTS_WITH(vp,vp_l,dir,dir_l)   (((vp_l)>dir_l && (vp)[dir_l]=='/' || dir_l==vp_l)  &&  cg_startsWith(vp,vp_l,dir,dir_l))
#define       STR_EQUALS(vp,vp_l,s)           (((vp_l)==sizeof(s)-1&&!memcmp((vp),s,sizeof(s)-1)))
/*******************************************/
/* Enums and corresponding stringarray     */
/* String-array   enum_name_S              */
/* Number of elements: enum_name_N         */
/* (highlight-regexp "\\bX(")              */
/*******************************************/



#define XMACRO_ENUMS()\
  X(enum_preloadram_status, C(PRELOADRAM_STATUS_NIL)C(PRELOADRAM_QUEUED)C(PRELOADRAM_READING)C(PRELOADRAM_DONE));\
  X(enum_when_preloadram_zip, C(PRELOADRAM_NEVER)C(PRELOADRAM_SEEK)C(PRELOADRAM_RULE)C(PRELOADRAM_COMPRESSED)C(PRELOADRAM_ALWAYS));\
  X(enum_mstoreid, C(HT_MALLOC_UNDEFINED)C(HT_MALLOC_warnings)C(HT_MALLOC__ht_count_by_ext)C(HT_MALLOC_file_ext)C(HT_MALLOC_inodes)C(HT_MALLOC_transient_cache)C(HT_MALLOC_without_dups));\
  X(enum_warnings, C(WARN_INIT)C(WARN_MISC)C(WARN_CONFIG)C(WARN_STR)C(WARN_INODE)C(WARN_THREAD)C(WARN_MALLOC)C(WARN_ROOT)C(WARN_OPEN)C(WARN_READ)C(WARN_ZIP_FREAD)C(WARN_READDIR)\
    C(WARN_SEEK)C(WARN_ZIP)C(WARN_GETATTR)C(WARN_STAT)C(WARN_FHANDLE)C(WARN_DIRCACHE) C(WARN_PRELOADFILE) C(WARN_PRELOADRAM) C(WARN_PRELOADDISK)C(WARN_FORMAT)C(WARN_DEBUG)C(WARN_CHARS)\
    C(WARN_RETRY)C(WARN_FILECONVERSION)C(WARN_C)C(WARN_NET)C(WARN_PATH));\
  X(enum_root_thread, C(PTHREAD_NIL)C(PTHREAD_ASYNC)C(PTHREAD_PRELOAD)C(PTHREAD_DIRCACHE)C(PTHREAD_INVALIDATE_PATH)C(PTHREAD_MISC));\
  X(enum_mutex, C(mutex_nil)C(mutex_start_thread)C(mutex_fhandle)C(mutex_async)C(mutex_mutex_count)C(mutex_mstore_init)C(mutex_fileconversion_init)C(mutex_dircache_queue)\
    C(mutex_log_count)C(mutex_crc)C(mutex_memUsage)C(mutex_dircache)C(mutex_idx)C(mutex_validchars)C(mutex_specialfile)C(mutex_validcharsdir)C(mutex_textbuffer_usage)\
    C(mutex_roots)C(mutex_log));\
  X(enum_log_flags, C(LOG_DEACTIVATE_ALL)C(LOG_FUSE_METHODS_ENTER)C(LOG_FUSE_METHODS_EXIT)C(LOG_ZIP)C(LOG_ZIPFLAT)C(LOG_EVICT_FROM_CACHE)C(LOG_PRELOADRAM)C(LOG_REALPATH)C(LOG_FILECONVERSION)C(LOG_READ_BLOCK)\
    C(LOG_TRANSIENT_ZIPENTRY_CACHE)C(LOG_STAT)C(LOG_OPEN)C(LOG_OPENDIR)\
    C(LOG_INFINITY_LOOP_RESPONSE)C(LOG_INFINITY_LOOP_STAT)C(LOG_INFINITY_LOOP_PRELOADRAM)C(LOG_INFINITY_LOOP_DIRCACHE)C(LOG_INFINITY_LOOP_INVALIDATE_PATH)C(LOG_INFINITY_LOOP_MISC));\
  X(enum_async, C(ASYNC_NIL)C(ASYNC_STAT)C(ASYNC_READDIR)C(ASYNC_OPENFILE)C(ASYNC_OPENZIP));\
  X(enum_mallocid, C(COUNT_UNDEFINED) C(COUNT_MALLOC_TESTING)\
    C(COUNT_HT_MALLOC_TRANSIENT_CACHE)C(COUNT_MALLOC_KEY_TRANSIENT_CACHE)\
    C(COUNT_HT_MALLOC_MISMATCH)C(COUNT_HT_MALLOC_KEY_MISMATCH)\
    C(COUNT_HT_MALLOC_NODUPS)\
    C(COUNT_MSTORE_MMAP_NODUPS) C(COUNT_MSTORE_MALLOC_NODUPS)\
    C(COUNT_MSTORE_MMAP)    C(COUNT_MSTORE_MALLOC)\
    C(COUNTER_TXTBUFSGMT_MMAP) C(COUNTER_TXTBUFSGMT_MALLOC)\
    C(COUNT_TXTBUF_SEGMENT_MALLOC)  C(COUNT_TXTBUF_SEGMENT_MMAP)\
    C(COUNT_MSTORE_MMAP_DIR_FILENAMES)           C(COUNT_MSTORE_MALLOC_DIR_FILENAMES)\
    C(COUNT_MSTORE_MMAP_TRANSIENT_CACHE_VALUES)       C(COUNT_MSTORE_MALLOC_TRANSIENT_CACHE_VALUES)\
    C(COUNT_HT_MALLOC_KEY)\
    C(COUNT_FILECONVERSION_MALLOC_replacements)\
    C(COUNT_FILECONVERSION_MALLOC_argv)\
    C(COUNT_FILECONVERSION_MALLOC_TXTBUF)\
    C(COUNT_MALLOC_dir_field)\
    C(COUNT_MALLOC_dircachejobs)\
    C(COUNT_PRELOADRAM_MALLOC)\
    C(COUNT_MALLOC_PRELOADRAM_TXTBUF)\
    C(COUNT_ZIP_OPEN)\
    C(COUNT_ZIP_FOPEN)\
    C(COUNT_FHANDLE_CONSTRUCT)\
    C(COUNT_FHANDLE_ARRAY_MALLOC)\
    C(COUNT_FHANDLE_TRANSIENT_CACHE)\
    C(COUNT_PAIRS_END)\
    C(COUNT_PRELOADRAM_WAITFOR_TIMEOUT)\
    C(COUNT_LCOPY_WAITFOR_TIMEOUT)\
    C(COUNT_READZIP_PRELOADRAM)\
    C(COUNT_READZIP_PRELOADRAM_BECAUSE_SEEK_BWD)\
    C(COUNT_STAT_FROM_CACHE)\
    C(COUNT_PTHREAD_LOCK)\
    C(COUNT_SEQUENTIAL_INODE));\
  X(enum_ctrl_action, C(ACT_NIL)C(ACT_KILL_ZIPSFS)C(ACT_FORCE_UNBLOCK)C(ACT_CANCEL_THREAD)C(ACT_NO_LOCK)C(ACT_BAD_LOCK)C(ACT_CLEAR_CACHE));\
  X(enum_clear_cache, C(CLEAR_ALL_CACHES)C(CLEAR_DIRCACHE)C(CLEAR_ZIPFLATCACHE)C(CLEAR_STATCACHE))
#define C(a) a,
#define X(name,...) typedef enum{__VA_ARGS__ name##_N}  name##_t
XMACRO_ENUMS();
#undef C
#undef X
#define C(a) #a,
#define X(name,...) static const char *name##_S[]={__VA_ARGS__ NULL}
XMACRO_ENUMS();
#undef X
#undef C






static const int _malloc_free_mismatch_allowed[enum_mallocid_N]={[COUNT_MSTORE_MMAP]=2, [COUNT_MSTORE_MALLOC]=2, [COUNT_FHANDLE_ARRAY_MALLOC]=2};
/******************************/
/* End enum with String array */
/******************************/



 /**********************************************************************************************************/
 /* virtualfolder_t *_virtualfolders contains a catalogue of  Virtual folders starting with DIR_ZIPsFS     */
 /* Example /zipsfs/d/-/     /zipsfs/-/l/r/. It is  initialized at the beginning in virtualfolders_init()  */
 /* virtualpath_t->vfolder has  initilly a reference to  _virtualfolder[0] meaning no virtual folder.      */
 /* In   vipa_setSpecialFile() virtualfolders_t may get a reference of a _virtualfolder[n]                 */
 /* zpath_t inherits.                                                                                      */
/* See macros VFOLDER_...() */
 /**********************************************************************************************************/
enum {VIRTUALFOLDER_CHILDS_MAX=31,VIRTUALFOLDER_MAX=1023};
typedef struct{
  const char *path;
  int path_l;
  uint64_t flags;
  const char *childs[VIRTUALFOLDER_CHILDS_MAX+1];
} virtualfolder_t;


typedef struct{
  const char *ext;
  atomic_uint counts[counter_rootdata_num];
  long rank;
  clock_t wait;
} counter_rootdata_t;
struct fileconversion_files;


#define XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME()    X(fsize,off_t); X(fmtime,uint64_t);X(fflags,uint8_t);X(finode,uint64_t);X(fcrc,uint32_t);
#define XMACRO_DIRECTORY_ARRAYS()                  X(fname,char *); XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME();
#define X(field,type) type *field;
typedef struct{
  struct timespec dir_mtim;
  int files_l;
  XMACRO_DIRECTORY_ARRAYS();
} directory_core_t;
#undef X

/////////////////////////////////////////////////////////////////////////
///  Union file system:
///  root_t holds the data for a branch
///  This is the root path  of an upstream source file tree.
///  ZIPsFS is a union file system and combines all source directories
//////////////////////////////////////////////////////////////////////////

enum{PSEUDO_ENTRYIDX_FOR_DECOMPRESS=1, PSEUDO_ENTRYIDX_FOR_UPDATE, PSEUDO_ENTRYIDX_FOR_ZIP_AS_DIR, INODE_OFFSET_ENTRY};
#define ZPATH_MAKE_INODE(zpath,entryIdx) make_inode((zpath)->stat_rp.st_ino,(zpath)->root,(entryIdx), ZP_RP(zpath))
enum {FILETYPEDATA_NUM=1024, FILETYPEDATA_FREQUENT_NUM=64};
// #define foreach_fhandle_also_emty(id,d) int id=_fhandle_n;for(fHandle_t *d;--id>=0 && ((d=fhandle_at_index(id))||true);)

#define foreach_fhandle_also_emty(id,d) fHandle_t *d; for(int id=_fhandle_n;--id>=0 && ((d=fhandle_at_index(id))||true);)
#define fhandle_virtualpath_equals(d,e) (d!=e && D_VP_HASH(e)==D_VP_HASH(d) && !strcmp(D_VP(d),D_VP(e)))
#define fhandle_zip_ftell(d) d->zip_fread_position
// cppcheck-suppress-macro constVariablePointer
#define foreach_fhandle_including_pending_destruct(id,d)  foreach_fhandle_also_emty(id,d) if (d->flags)


// cppcheck-suppress-macro constVariablePointer
#define FOREACH_FHANDLE(id,d)  foreach_fhandle_also_emty(id,d) if (d->flags && !(d->flags&FHANDLE_DESTROY_LATER))




/* flags for config_file_attribute_cache_TTL() */

enum {STATCACHE_ROOT_IS_REMOTE=1<<3,STATCACHE_ROOT_IS_WRITABLE=1<<4,STATCACHE_IS_PRELOADDISK=1<<5,STATCACHE_ROOT_WITH_TIMEOUT=1<<6,STATCACHE_ROOT_IS_WORM=1<<7,STATCACHE_ROOT_IS_IMMUTABLE=1<<8,STATCACHE_IS_PFXPLAIN=1<<9};



#if WITH_DIRCACHE || WITH_STATCACHE || WITH_TIMEOUT_READDIR
#define WITH_DIRCACHE_or_STATCACHE_or_TIMEOUT_READDIR 1
#else
#define WITH_DIRCACHE_or_STATCACHE_or_TIMEOUT_READDIR 0
#endif

#define XMACRO_HT_GLOBAL() X(_ht_intern_vp)X(_ht_intern_fileext)X(_ht_valid_chars)X(_ht_inodes_vp)X(_ht_count_by_ext) IF1(WITH_FILECONVERSION_OR_CCODE,X(_ht_fsize))
#define XMACRO_HT_ROOT()   X(ht_dircache_queue)X(ht_dircache)X(ht_inodes)X(ht_filetypedata)\
       IF1(WITH_STATCACHE,X(ht_stat))\
       IF1(WITH_ZIPFLATCACHE,X(ht_zipflatcache_vpath_to_rule))\
       IF1(WITH_DIRCACHE_or_STATCACHE_or_TIMEOUT_READDIR, X(ht_int_fname)X(ht_int_fnamearray))


#define XMACRO_MSTORE_ROOT()   IF1(WITH_DIRCACHE_or_STATCACHE_or_TIMEOUT_READDIR,X(dircache_mstore))

#define foreach_root(r)    for(root_t *r=_root; r<_root+_root_n; r++)
//FINDRP_FILECONVERSION_CUT=1<<13, FINDRP_FILECONVERSION_CUT_NOT=1<<14
enum {  FINDRP_IN_OPEN=1<<12,  FINDRP_IS_PFXPLAIN=1<<16, FINDRP_STAT_TOCACHE_ALWAYS=1<<19, FINDRP_CACHE_ONLY=1<<20, FINDRP_CACHE_NOT=1<<21, FINDRP_DIR_PRELOADED_ONLY=1<<22};
enum{FLAGS_FILLDIR_FINDRP_BEGIN=FINDRP_IN_OPEN};
typedef struct{
  const char *rootpath, *rootpath_orig, *rootpath_mountpoint, *path_prefix, *pathpfx_slash_to_null;
  int rootpath_l,path_prefix_l;
  struct statvfs statvfs;
  dev_t  st_dev;
  uint32_t log_count_delayed,log_count_delayed_periods,log_count_restarted;
  long cache_TTL;
#define X(s) mstore_t s;
  XMACRO_MSTORE_ROOT()
#undef X
#define X(ht) ht_t ht;
    XMACRO_HT_ROOT()
#undef X
    pthread_t thread[enum_root_thread_N];
  int thread_count_started[enum_root_thread_N];
  int thread_when_canceled[enum_root_thread_N];
  pid_t thread_pid[enum_root_thread_N];
  counter_rootdata_t filetypedata_dummy,filetypedata_all, filetypedata[FILETYPEDATA_NUM],filetypedata_frequent[FILETYPEDATA_FREQUENT_NUM];
  bool filetypedata_initialized, blocked, writable, remote,has_timeout,thread_already_started[enum_root_thread_N],noatime, one_file_system, follow_symlinks,worm,immutable,preload,preload_entire_zip;
  int decompress_mask;  /* These flags also serve for filler_add() */
  _Static_assert(((1<<COMPRESSION_NUM)-1<FLAGS_FILLDIR_FINDRP_BEGIN),"COMPRESSION_NUM ... is part of this");
  /*  In one  opt   FILLDIR_XXXX FINDRP and COMPRESSION */
  int seq_fsid;
  unsigned long f_fsid; /* From statvfs.f_fsid */
  pthread_mutex_t async_mtx[enum_async_N];
  int async_go[enum_async_N];
  int async_task_id[enum_async_N];
  volatile atomic_ulong thread_when[enum_root_thread_N],thread_when_success[enum_root_thread_N]; _Static_assert(sizeof(atomic_ulong)>=sizeof(time_t),"");
  IF1(WITH_TIMEOUT_OPENFILE,int  async_openfile_fd,async_openfile_flags;  char async_openfile_path[MAX_PATHLEN+1]);
  IF1(WITH_TIMEOUT_STAT,    struct stat async_stat;  zpath_t *async_stat_path);
  IF1(WITH_TIMEOUT_READDIR, directory_t *async_dir);
  IF1(WITH_TIMEOUT_OPENZIP, struct async_zipfile *async_zipfile);
  const char *probe_path;
  int probe_path_timeout,probe_path_response_ttl;
  int stat_timeout_seconds,readdir_timeout_seconds,openfile_timeout_seconds;
  const char **path_deny, **path_allow, **preload_extensions;
  atomic_int test1;
} root_t;

////////////////
/// zippath  ///
////////////////

/***************************************************************************************************************************/
/* This struct is initialized with the data of virtualpath_t.                                                              */
/* In the function test_realpath(), a matching existing file for the virtual path will be looked for for each file branch. */
/* If found, then the struct will contain this real path and the struct stat data.                                         */
/* Can be copied by value             (-:                                                                                  */
/***************************************************************************************************************************/
#define XMACRO_ZIPPATH_NEED_RESET() X(virtualpath_without_entry)X(entry_path)X(realpath)

/*  Let Virtualpath be "/foo.zip.Content/bar". virtualpath_without_entry will be "/foo.zip.Content". */
/* Let Virtualpath be "/foo.zip.Content/bar". entry_path will be "bar". */
/* The physical path of the file. In case of a ZIP entry, the path of the ZIP-file */

typedef struct{
#define X(s) int s,s##_l;
  char strgs[ZPATH_STRGS];  int strgs_l; /* Contains all strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  X(vp) XMACRO_ZIPPATH_NEED_RESET()
#undef X
    ht_hash_t virtualpath_hash,virtualpath_without_entry_hash;
  zip_uint32_t zipcrc32;
  root_t *root;
  int current_string; /* The String that is currently build within strgs with zpath_newstr().  With zpath_commit() this marks the end of String. */
  struct stat stat_rp,stat_vp;
  char *zipfile_append, *vfile_sfx;
  int zipfile_l, zipfile_cutr, specialfile_id;
  uint32_t is_decompressed;
  const virtualfolder_t *vfolder;
  IF1(WITH_PRELOADRAM,off_t preloadram_need_bytes);
  enum{
    VP_IS_ZIP_AS_DIR=1<<12,
    ZP_IS_IN_FHANDLE=1<<15,
    ZP_NOT_EXPAND_SYMLINKS=1<<16,
    /* End early */
    ZP_IS_PRELOADED_ZIPFILE_BUT_NOT_ZIPENTRY=1<<17,         /* Decided not to extract ZIP entry as file as it is going to preloaded into RAM */
    ZP_DOES_NOT_EXIST=1<<21,
    ZP_IS_COMPRESSEDZIPENTRY=1<<22,
    ZP_IS_FROM_TRANSIENT_CACHE=1<<23,
    ZP_IS_ZIPENTRY=1<<24,                    /* Indicates a ZIP entry after realpath found */
    ZP_OVERFLOW=1<<25,                       /* The fixed size string buffer of zpath_t was too small. Path too long */
    ZP_RESET_IF_NEXISTS=1<<27,               /* Instructs   test_realpath() to Reset  */
    ZP_TRY_ZIP=1<<28,                        /* Instructs   test_realpath() to consider Zip and DIR_IS_TRY_ZIP() */
    ZP_TRY_DECOMPRESS=1<<29,
    ZP_MASK_KEEP_NOT_RESET=(ZP_NOT_EXPAND_SYMLINKS|ZP_IS_IN_FHANDLE|VP_IS_ZIP_AS_DIR|ZP_TRY_DECOMPRESS)
  }flags;
  enum{
    ZP_FLAG2_TRANSIENT_CACHE_ONCE=1<<3,
    ZP_FLAG2_CHECKED_EXISTENCE_COMPRESSED=1<<4,   /* Check only once*/
  }flags2;
}  zpath_t;



#define ZPF(f)     ((zpath)->flags&(f))
#define ZPF2(f)    ((zpath)->flags2&(f))
#define ZPR()      ((zpath)->root)
#define D_ZPF(f) (d->zpath.flags&(f))
#define ZPRP() rootpath((zpath)->root)
/*  ZP_OVERFLOW The paths are to long and the buffer strgs[ZPATH_STRGS] to small */
#define set_realpath_to_writable_folder(zpath, dir) zpath_set_realpath(zpath,_writable_path,dir,VP()+VFOLDER_PATH_L(zpath))
#define ZPATH_REALPATH_WRITABLE_FOLDER(dst,zpath,dir)  stpcpy(stpcpy(stpcpy((dst),_writable_path),dir),ZP_VP(zpath)+VFOLDER_PATH_L(zpath))
#define REALPATH_WRITABLE_FOLDER(dst,vipa,dir)  stpcpy(stpcpy(stpcpy((dst),_writable_path),dir),vipa->vp+VFOLDER_PATH_L(vipa))






#define ZPATH_CUT(zpath,num_chars_to_be_removed)    (zpath)->strgs[(zpath)->strgs_l-=num_chars_to_be_removed]=0
typedef struct{
  zip_file_t *zf;
  zip_t *za;
  zpath_t azf_zpath;
} async_zipfile_t;
static const async_zipfile_t async_zipfile_empty;


#define DIRECTORY_DIM_STACK IF01(WITH_TESTING_REALLOC,MAX(256,ULIMIT_S),4)
typedef struct{
  STRUCT_NOT_ASSIGNABLE();
  zpath_t dir_zpath;
#define X(field,type) type _stack_##field[DIRECTORY_DIM_STACK];
  XMACRO_DIRECTORY_ARRAYS();
#undef X
  directory_core_t core; // Only this data goes to dircache.
  mstore_t filenames;
  int files_capacity,cached_vp_to_zip;
  bool debug, dir_is_dircache, dir_is_destroyed, dir_is_success, has_file_containing_placeholder;
  /* The following concern asynchronized reading */
  bool async_never,when_readdir_call_stat_and_store_in_cache,always_to_cache;
  IF1(WITH_TIMEOUT_READDIR, ht_t *ht_intern_names);
} directory_t;
#define DIR_RP()         ((dir)->dir_zpath.strgs+(dir)->dir_zpath.realpath)
#define DIR_RP_L()       ((dir)->dir_zpath.realpath_l)
#define DIR_VP()         ((dir)->dir_zpath.strgs+(dir)->dir_zpath.vp)
#define DIR_VP_L()       ((dir)->dir_zpath.vp_l)
#define DIR_VP0()        ((dir)->dir_zpath.strgs+(dir)->dir_zpath.virtualpath_without_entry)
#define DIR_VP0_L()      ((dir)->dir_zpath.virtualpath_without_entry_l)
#define DIR_ROOT()       ((dir)->dir_zpath.root)
#define DIR_IS_TRY_ZIP() (((dir)->dir_zpath.flags&(ZP_TRY_ZIP|ZP_IS_ZIPENTRY))!=0)
#define DIR_IS_ZIP()     (((dir)->dir_zpath.flags&ZP_IS_ZIPENTRY)!=0)

#define ROOT_WHEN_SUCCESS(r,t)  atomic_load(r->thread_when_success+t)
#define ROOT_WHEN_ITERATED(r,t) atomic_load(r->thread_when+t)

#define ROOT_SUCCESS_SECONDS_AGO(r) (time(NULL)-ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC))
#define ROOT_NOT_RESPONDING(r)  (r->remote && ROOT_SUCCESS_SECONDS_AGO(r)>r->probe_path_timeout)


#define ZPATH_ROOT_WRITABLE() (ZPR() && 0!=(ZPR()->writable))
#define ZP_RP(zpath) ((zpath)->strgs+(zpath)->realpath)
#define ZP_VP(zpath) ((zpath)->strgs+(zpath)->vp)
#define EP()         (zpath->strgs+zpath->entry_path)
#define EP_L()       (zpath->entry_path_l)
#define VP_L()       (zpath->vp_l)
#define RP()         (zpath->strgs+(zpath)->realpath)
#define VP()         (zpath->strgs+(zpath)->vp)
#define VP_HASH()    (zpath->virtualpath_hash)
#define RP_L()       (zpath->realpath_l)
#define VP0()        (zpath->strgs+zpath->virtualpath_without_entry)
#define VP0_L()      (zpath->virtualpath_without_entry_l)
#define D_ROOT(d)    ((d)->zpath.root)
#define D_VP(d)      ((d)->zpath.strgs+d->zpath.vp)
#define D_VP0(d)     ((d)->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP0(d)     ((d)->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP_HASH(d) ((d)->zpath.virtualpath_hash)
#define D_EP(d)      ((d)->zpath.strgs+d->zpath.entry_path)
#define D_EP_L(d)    ((d)->zpath.entry_path_l)
#define D_VP_L(d)    ((d)->zpath.vp_l)
#define D_RP(d)      ((d)->zpath.strgs+d->zpath.realpath)
#define D_RP_L(d)    ((d)->zpath.realpath_l)
#define D_ST_SIZE(d) (d->zpath.stat_vp.st_size)
#define NEW_ZIPPATH(vipa)  zpath_t __zp={0},*zpath=&__zp;zpath_init(zpath,vipa)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath);  found=find_realpath(0,zpath);
#define FIND_REALPATH_NOT_EXPAND_SYMLINK(virtpath)    NEW_ZIPPATH(virtpath); /* zpath->flags|=ZP_NOT_EXPAND_SYMLINKS;*/  found=find_realpath(0,zpath);

enum {FHANDLE_LOG2_BLOCK_SIZE=5, FHANDLE_BLOCKS=512};
enum {FHANDLE_BLOCK_SIZE=1<<FHANDLE_LOG2_BLOCK_SIZE, FHANDLE_MAX=FHANDLE_BLOCKS*FHANDLE_BLOCK_SIZE};


/************************************************************************/
/* This struct initially holds the virtual path and derived information */
/* in the FUSE functions.                                               */
/* Later, the data will be copied into an instance of struct zpath_t    */
/************************************************************************/
typedef struct{
  const char *vp;//*preloadpfx;
  int vp_l,flags;
  char *zipfile_append, *vfile_sfx;
  int zipfile_l, zipfile_cutr, specialfile_id;
  const virtualfolder_t *vfolder;
} virtualpath_t;
const static virtualpath_t empty_virtualpath;
#define FOREACH_VIRTUALFOLDER(maybe_const,folder)  for(maybe_const virtualfolder_t *folder=_virtualfolders+1; folder->path; folder++)
#define IS_VIRTUAL_FOLDER(vipa) (VFOLDER_PATH(vipa) && (vipa)->vp_l==VFOLDER_PATH_L(vipa))
#define NEW_VIRTUALPATH(vpath) virtualpath_t vipa; char vp_buffer[MAX_PATHLEN+1]; virtualpath_init(&vipa,vpath,vp_buffer)

#define FUSE_PREAMBLE_Q(vpath) virtualpath_t vipa; char vp_buffer[MAX_PATHLEN+1]; int er=virtualpath_init(&vipa,vpath,vp_buffer); if(er) return -er
#define FUSE_PREAMBLE(vpath) FUSE_PREAMBLE_Q(vpath);  IF_LOG_FLAG(LOG_FUSE_METHODS_ENTER)log_entered_function("%s",vpath)
#define FUSE_PREAMBLE_W(create_or_del,vpath) FUSE_PREAMBLE(vpath); er=virtualpath_error(&vipa,create_or_del); if (er) return -er

#define VFOLDER_PATH(zpath) ((zpath)->vfolder->path)
#define VFOLDER_PATH_L(zpath) ((zpath)->vfolder->path_l)
#define VFOLDER_FLAGS(zpath) ((zpath)->vfolder->flags)
#define VFOLDER_HAS_FLAG(zpath,f) ((zpath)->vfolder->flags&ID_FLAG(f))
#define VFOLDER_HAS_FLAG2(zpath,f1,f2) ((zpath)->vfolder->flags&(ID_FLAG(f1)|ID_FLAG(f2)))
#define VFOLDER_HAS_FLAG3(zpath,f1,f2,f3) ((zpath)->vfolder->flags&(ID_FLAG(f1)|ID_FLAG(f2)|ID_FLAG(f3)))

#define VFOLDER_HAS_FLAG_d(f) VFOLDER_HAS_FLAG(&d->zpath,f)

#define ZPATH_IS_FILECONVERSION() (VFOLDER_HAS_FLAG(zpath,VIEWMOD_FILECONVERSION))
#define IS_SPECIALFILE(zpath,id) ((zpath)->specialfile_id==(id))
#define IS_UPDATE_GO(zpath) (IF1(WITH_INTERNET_DOWNLOAD, IS_SPECIALFILE(zpath,SFILE_INTERNET_UPDATE_GO)||)\
                             IF1(WITH_PRELOADRAM,        IS_SPECIALFILE(zpath,SFILE_PRELOAD_UPDATE_GO)||)false)

//#define VFOLDER_IS_UPDATE(zpath,preload_update,internet_update) (VFOLDER_HAS_FLAG2(zpath,preload_update,internet_update) && ENDSWITH(VP(),VP_L(),SFX_UPDATE))
/******************************************************************************************************************/
/* this struct contains data associated with a file handle.													   */
/* It is created in xmp_open unless for plain regular files not needing preloading.							   */
/* It is then used in xmp_read()																				   */
/* Instances  are stored in the linear list _fhandle. The list may have holes.									   */
/* WITH_PRELOADRAM: It may also contain the cached file content. Improve non-sequential reading				   */
/*                  Only one of all instances with a specific virtual path stores the cached zip entry			   */
/*                  See config_advise_cache_zipentry_in_ram() and  CLI parameter -c.							   */
/******************************************************************************************************************/

typedef struct{
  uint64_t fhandle_fh;
  int fd_real, count_backward_seek;
  off_t offset_expected;
  zip_t *zip_archive;
  zip_file_t *zip_file;
  zpath_t zpath;
  volatile time_t accesstime;
  volatile int errorno;
  enum {FHANDLE_ACTIVE=1<<0,FHANDLE_DESTROY_LATER=1<<1,FHANDLE_PREPARE_ONCE_IN_RW=1<<2,FHANDLE_SEEK_FW_FAIL=1<<3,FHANDLE_SEEK_BW_FAIL=1<<4,FHANDLE_WITH_TRANSIENT_ZIPENTRY_CACHES=1<<5,FHANDLE_IS_FILECONVERSION=1<<6,FHANDLE_WITH_PRELOADRAM=1<<8,FHANDLE_PRELOADRAM_MASTER=1<<9, FHANDLE_PRELOADRAM_COMPLETE=1<<10,FHANDLE_SPECIAL_FILE=1<<11,FHANDLE_IS_CCODE=1<<12,FHANDLE_PRELOADDISK_QUEUE=1<<13,FHANDLE_PRELOADDISK_RUN=1<<14, FHANDLE_PRELOAD_ALREADY_QUEUED=1<<15, FHANDLE_IS_MUTEX_INITIALIZED_0=1<<16,FHANDLE_IS_MUTEX_INITIALIZED_1=1<<17,FHANDLE_SERIALIZED=1<<18, FHANDLE_NEED_INVALIDATE_PATH=1<<19,} flags;

  uint8_t already_logged;
  volatile int64_t offset,n_read;
  volatile atomic_int is_busy; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex[2]; /* Costs 40 Bytes */
  counter_rootdata_t *filetypedata;
  off_t zip_fread_position;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,ht_t *ht_transient_cache);
  IF1(WITH_PRELOADRAM, struct preloadram * volatile preloadram; int how_often_bwdseek);
  atomic_int volatile count_calls_read;
  IF1(WITH_FILECONVERSION, enum{FILECONVERSION_UNINITILIZED,FILECONVERSION_SUCCESS,FILECONVERSION_FAIL} fileconversion_state);
  pid_t pid;
}  fHandle_t;
static const fHandle_t FHANDLE_EMPTY;

#define fhandle_mutex_flag(id) (id?FHANDLE_IS_MUTEX_INITIALIZED_0:FHANDLE_IS_MUTEX_INITIALIZED_1)
#define fhandle_mutex_initialized(id,d) (d->flags&fhandle_mutex_flag(id))




#define fhandle_lock(d) _fhandle_lock(0,d);            DETECT_RESOURCE_LEAK_B(_cppcheck_rsc_leak_fhandle_lock0)
#define fhandle_unlock(d) _fhandle_unlock(0,d);        DETECT_RESOURCE_LEAK_E(_cppcheck_rsc_leak_fhandle_lock0)

#define fhandle_zip_lock(d) _fhandle_lock(1,d);        DETECT_RESOURCE_LEAK_B(_cppcheck_rsc_leak_fhandle_lock1)
#define fhandle_zip_unlock(d) _fhandle_unlock(1,d);    DETECT_RESOURCE_LEAK_E(_cppcheck_rsc_leak_fhandle_lock1)

// lock
// cppcheck     -suppress-macro nullPointerRedundantCheck

#define REQUIRES_1(x) "Requires "#x"=1. Is "STRINGIZE(x)"."

#define RP_HELP(x,type) _root_property_example=x;_root_property_type=type
#define RP_PRINT(code)
#define RP_GET(code)
#define RP_NUMBER(unit,x,assign_to) RP_HELP(x,unit)                        RP_PRINT(if (assign_to) sprintf(buf,"%jd",IM(assign_to)))        RP_GET(assign_to=rp_parse_number(v))
#define RP_01(x,assign_to)          RP_HELP(x,"0 or 1")                    RP_PRINT(if (assign_to) sprintf(buf,"%d",assign_to))               RP_GET(assign_to=rp_parse_01(v))
#define RP_PATHS(x,assign_to)       RP_HELP(x,"File paths separated by :") RP_PRINT(if (assign_to) rp_print_paths(buf,sizeof(buf),assign_to)) RP_GET(assign_to=(const char**)cg_split_string(":",v))
#define RP_LIST(x,assign_to)        RP_HELP(x,"Like .jpg,.tgz,.tar.gz")    RP_PRINT(if (assign_to) rp_print_paths(buf,sizeof(buf),assign_to)) RP_GET(assign_to=(const char**)cg_split_string(" ",v))
#define RP_VPATH(x,assign_to)       RP_HELP(x,"Virtual file path")         RP_PRINT(if (assign_to) sprintf(buf,"%s",assign_to))               RP_GET(assign_to=rp_parse_vpath(r,v))
#define RP_RPATH(x,assign_to)       RP_HELP(x,"Real absolute file path")   RP_PRINT(if (assign_to) sprintf(buf,"%s",assign_to))               RP_GET(assign_to=rp_parse_rpath(r,v))

#define XMACRO_ROOT_PROPERTY()\
  X(path_deny,                RP_PATHS("/dir/forbidden:/also_forbidden",r->path_deny))\
    X(path_allow,               RP_PATHS("/dir/only_this:/or_this",r->path_allow))\
    X(path_prefix,              RP_VPATH("/db  Begin of Virtual paths. Improves performance",r->path_prefix))\
    X(preload,                  RP_01(REQUIRES_1(WITH_PRELOADDISK),r->preload))\
    X(preload_entire_zip,       RP_01(REQUIRES_1(WITH_PRELOADDISK),r->preload_entire_zip))\
    X(preload_extensions,       RP_LIST("Not implemented yet",r->preload_extensions))\
    X(decompression,            rp_parse_decompress(r,v))\
  X(probe_path_timeout,       RP_NUMBER("Seconds","("STRINGIZE(PROBE_PATH_TIMEOUT_SECONDS)")    Periodically check statfs()",r->probe_path_timeout))\
  X(probe_path_response_ttl,  RP_NUMBER("Seconds","("STRINGIZE(PROBE_PATH_RESPONSE_TTL_SECONDS)")    Periodically check statfs()",r->probe_path_response_ttl))\
  X(probe_path,               RP_RPATH("Other path instead of root path for statfs( ",r->probe_path))\
  X(stat_timeout,             RP_NUMBER("Seconds","("STRINGIZE(STAT_TIMEOUT_SECONDS)")   Under construction. "REQUIRES_1(WITH_TIMEOUT_STAT),r->stat_timeout_seconds))\
  X(readdir_timeout,          RP_NUMBER("Seconds","("STRINGIZE(READDIR_TIMEOUT_SECONDS)")   Under construction. "REQUIRES_1(WITH_TIMEOUT_READDIR),r->readdir_timeout_seconds))\
    X(openfile_timeout,         RP_NUMBER("Seconds","("STRINGIZE(OPENFILE_TIMEOUT_SECONDS)")   Under construction. "REQUIRES_1(WITH_TIMEOUT_OPENZIP),r->openfile_timeout_seconds))\
    X(cache_TTL,                RP_NUMBER("Seconds","Passed to  config_file_attribute_cache_TTL()   "REQUIRES_1(WITH_STATCACHE),r->cache_TTL))\
    X(worm,                     RP_01("Existing files never change. Flag passed to config_file_attribute_cache_TTL()",r->worm))\
    X(immutable,                RP_01("File tree does not change. Flag passed to config_file_attribute_cache_TTL()",r->immutable))\
    X(follow_symlinks,          RP_01("Always active if WITH_FOLLOW_SYMLINK_ALL_ROOTS set to 1. Currently  "STRINGIZE(WITH_FOLLOW_SYMLINK_ALL_ROOTS)". Anyway, the function config_allow_expand_symlink(path) must return true.",r->follow_symlinks))\
  X(one_file_system,          RP_01("No cross device via symlink. Improves security",r->one_file_system))\



#define X(name,code) ROOT_PROPERTY_##name,
enum {XMACRO_ROOT_PROPERTY()ROOT_PROPERTY_NUM};
#undef X
#define X(name,code) #name,
const char *ROOT_PROPERTY[]={XMACRO_ROOT_PROPERTY()NULL};
#undef X

#define IS_VP_IN_ROOT_PFX(r,vipa) (r->path_prefix_l && r->path_prefix_l>=(vipa)->vp_l && cg_path_equals_or_is_parent((vipa)->vp,(vipa)->vp_l, r->path_prefix,r->path_prefix_l))
///////////////
/// Logging ///
///////////////

#if 1
#define LOG_FLAG_P(f)  (f && _log_flags&(1<<f))
#else
// Initiates excessive logging
#define LOG_FLAG_P(f)  (f && (LOG_OPENDIR|LOG_READ_BLOCK|LOG_STAT|LOG_OPENDIR|LOG_PRELOADRAM)&(1<<f))
#endif

#define IF_LOG_FLAG(f) if (LOG_FLAG_P(f))
#define IF_LOG_FLAG_OR(f,or) if (LOG_FLAG_P(f)||or)
//

enum {ADVISE_DIRCACHE_IS_ZIP=1<<1, ADVISE_DIRCACHE_IS_REMOTE=1<<2, ADVISE_DIRCACHE_IS_AS_IS=1<<3};
enum {ADVISE_CACHE_IS_COMPRESSEDZIPENTRY=1<<1,ADVISE_CACHE_IS_SEEK_BW=1<<2,ADVISE_CACHE_BY_POLICY=1<<3,ADVISE_CACHE_IS_ZIPENTRY=1<<4};
//
#if WITH_PRELOADRAM
struct preloadram{
  volatile  enum_preloadram_status_t preloadram_status;
  textbuffer_t *txtbuf;
  zpath_t m_zpath; /* To try find_realpath_other_root() */
  volatile off_t preloadram_l,preloadram_already;
  int64_t preloadram_took_mseconds;
  int id;
};
#endif // WITH_PRELOADRAM


#define  PRELOADFILE_ROOT_UPDATE_TIME(d,r,success)   if (d||r) root_update_time(r?r:D_ROOT(d),success?PTHREAD_PRELOAD:-PTHREAD_PRELOAD,0)


////////////////////
/// Source files ///
////////////////////

#define XMACRO() X("",ZIPsFS_cleanup,sh)\
X("../testing/testfiles/",ZIPsFS_testfiles_start_ZIPsFS,sh)\
    X("",ZIPsFS_configuration,c)\
    X("",ZIPsFS_configuration,h)\
    X("",ZIPsFS_configuration_fileconversion,c)\
    X("",ZIPsFS_configuration_fileconversion,h)\
    X("",ZIPsFS_configuration_zipfile,c)\
    X("",ZIPsFS_configuration_c,c)
#define X(dir,n,x) n##_dot_##x,
typedef enum{XMACRO()NUM_SRC_FILES} enum_sourcefiles_t;
#undef X
#define X(dir,n,x) dir#n"."#x,
static const char *enum_sourcefiles_S[]={XMACRO()NULL};
#undef X
#undef XMACRO
////////////////////
/// Directories  ///
////////////////////






// static bool _stat_direct(const int opts_findrp, struct stat *st, const int fd_parent, const char *rp, const int rp_l,  root_t *r, const char *callerFunc){

#define stat_direct(...)    _viamacro_stat_direct(0,__VA_ARGS__,__func__)
#define fstatat_direct(...) _viamacro_stat_direct(__VA_ARGS__,__func__)
#define exit_ZIPsFS()       _viamacro_exit_ZIPsFS(__func__,__LINE__);


/********/
/* Misc */
/********/
/* Better name for following option?
   ZIPsFS is based on a list  file locations with decreasing priority.
   If a file in a location further down in the list is in cache, it may dominate/mask a file at a location with higher priority.
   The option is the maximum age of the cache entry in seconds for this to be allowed.
   See FINDRP_CACHE_ONLY and zpath_stat_from_cache()
*/

enum {CACHE_TAKES_PRECEDENCE_TTL=600};

#define warning_zip_f(path,zf,txt) _viamacro_warning_zipf(__func__,__LINE__, path,NULL,zf,txt)
#define warning_zip_a(path,za,txt) _viamacro_warning_zipf(__func__,__LINE__, path,za,NULL,txt)

#define WITH_DEBUG_ALL_ZIP_PRELOADRAM 0 // PRELOADRAM_ALWAYS
#define WITH_DEBUG_NO_ZIP_FSEEK 0

#ifdef TEST_COMPILATION_ISOLATED
int main(const int argc,const char *argv[]){}
#endif
