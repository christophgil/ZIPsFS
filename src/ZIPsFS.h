/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Enums and structs                                         ///
/////////////////////////////////////////////////////////////////
#define SIZE_POINTER sizeof(char *)


#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)



#define NUM_MUTEX   (mutex_roots+ROOTS)
#define DIRENT_ISDIR (1<<0)
#define DIRENT_IS_COMPRESSED (1<<1)
//#define DEBUG_DIRECTORY_HASH32_OF_FNAME(s)    hash32((char*)s->fname,s->files_l*sizeof(char*))

//////////////////////////////
/// Constants and Macros   ///
//////////////////////////////
#define MALLOC_TYPE_SHIFT 8
#define MALLOC_TYPE_HT (1<<MALLOC_TYPE_SHIFT)
#define MALLOC_TYPE_KEYSTORE (2<<MALLOC_TYPE_SHIFT)
#define MALLOC_TYPE_VALUESTORE (3<<MALLOC_TYPE_SHIFT)
#define MALLOC_ID_COUNT (5<<MALLOC_TYPE_SHIFT)
#define CG_CLEANUP_BEFORE_EXIT() exit_ZIPsFS()


/////////////
/// Files ///
/////////////
#define DIR_ZIPsFS "/ZIPsFS"
#define DIR_ZIPsFS_L (sizeof(DIR_ZIPsFS)-1)
#define DIRNAME_AUTOGEN "a"
#define DIR_AUTOGEN DIR_ZIPsFS"/"DIRNAME_AUTOGEN
#define DIR_AUTOGEN_L (sizeof(DIR_AUTOGEN)-1)

#define FN_SET_ATIME "/ZIPsFS_set_file_access_time"
static const char *SPECIAL_FILES[]={"/warnings.log","/errors.log","/file_system_info.html","/ZIPsFS_clear_cache.command","",FN_SET_ATIME".command",FN_SET_ATIME".ps1",FN_SET_ATIME".bat","/Readme.html",NULL};  /* SPECIAL_FILES[SFILE_LOG_WARNINGS] is first!*/
#define SFILE_BEGIN_VIRTUAL SFILE_INFO
#define PLACEHOLDER_NAME 0x07
#define DEBUG_ABORT_MISSING_TDF 1
#define IS_STAT_READONLY(st) !(st.st_mode&(S_IWUSR|S_IWGRP|S_IWOTH))


enum functions{xmp_open_,xmp_access_,xmp_getattr_,xmp_read_,xmp_readdir_,functions_l};
enum enum_count_getattr{
  COUNTER_STAT_FAIL,COUNTER_STAT_SUCCESS,
  COUNTER_OPENDIR_FAIL,COUNTER_OPENDIR_SUCCESS,
  COUNTER_ZIPOPEN_FAIL,COUNTER_ZIPOPEN_SUCCESS,
  COUNTER_GETATTR_FAIL,COUNTER_GETATTR_SUCCESS,
  COUNTER_ACCESS_FAIL,COUNTER_ACCESS_SUCCESS,
  COUNTER_READDIR_SUCCESS,COUNTER_READDIR_FAIL,
  COUNTER_ROOTDATA_INITIALIZED,enum_count_getattr_length};
enum enum_counter_rootdata{
  ZIP_OPEN_SUCCESS,ZIP_OPEN_FAIL,
  //
  ZIP_READ_NOCACHE_SUCCESS,ZIP_READ_NOCACHE_ZERO, ZIP_READ_NOCACHE_FAIL,
  ZIP_READ_NOCACHE_SEEK_SUCCESS,ZIP_READ_NOCACHE_SEEK_FAIL,
  //
  ZIP_READ_CACHE_SUCCESS,ZIP_READ_CACHE_FAIL,
  ZIP_READ_CACHE_CRC32_SUCCESS,ZIP_READ_CACHE_CRC32_FAIL,
  //
  COUNT_RETRY_STAT,
  COUNT_RETRY_MEMCACHE,
  COUNT_RETRY_ZIP_FREAD,
  //
  COUNT_MEMCACHE_INTERRUPT,
  //
  counter_rootdata_num};


enum enum_special_files{SFILE_LOG_WARNINGS,SFILE_LOG_ERRORS,SFILE_INFO,SFILE_CLEAR_CACHE,SFILE_DEBUG_CTRL,
#if WITH_AUTOGEN
                        SFILE_SET_ATIME_SH,SFILE_SET_ATIME_PS,SFILE_SET_ATIME_BAT,
#endif
                        SFILE_README,SFILE_L};

enum enum_autogen_state{AUTOGEN_UNINITILIZED,AUTOGEN_SUCCESS,AUTOGEN_FAIL};
///////////////////////////////////////
/// Enums and corresponding strings ///
///////////////////////////////////////
#define A1() C(HT_MALLOC_UNDEFINED)C(HT_MALLOC_warnings)C(HT_MALLOC_ht_count_getattr)C(HT_MALLOC_file_ext)C(HT_MALLOC_inodes)C(HT_MALLOC_transient_cache)C(HT_MALLOC_without_dups)C(HT_MALLOC_LEN)
#define A2() C(MALLOC_UNDEFINED) C(MALLOC_TESTING) C(MALLOC_HT)C(MALLOC_HT_KEY)C(MALLOC_MSTORE) C(MALLOC_HT_IMBALANCE)C(MALLOC_HT_KEY_IMBALANCE)C(MALLOC_MSTORE_IMBALANCE)\
       C(MALLOC_autogen_replacements) C(MALLOC_autogen_argv) C(MALLOC_autogen_textbuffer)\
       C(MALLOC_textbuffer) C(MALLOC_fhandle)\
       C(MALLOC_directory_field)C(MALLOC_dircachejobs)\
       C(MALLOC_memcache) C(MALLOC_memcache_textbuffer) C(MALLOC_transient_cache)\
       C(MALLOC_textbuffer_segments)C(MALLOC_textbuffer_segments_mmap)C(MALLOC_LEN)
#define A3() C(WARN_INIT)C(WARN_MISC)C(WARN_STR)C(WARN_INODE)C(WARN_THREAD)C(WARN_MALLOC)C(WARN_ROOT)C(WARN_OPEN)C(WARN_READ)C(WARN_ZIP_FREAD)C(WARN_READDIR)C(WARN_SEEK)C(WARN_ZIP)C(WARN_GETATTR)C(WARN_STAT)C(WARN_FHANDLE)C(WARN_DIRCACHE)C(WARN_MEMCACHE)C(WARN_FORMAT)C(WARN_DEBUG)C(WARN_CHARS)C(WARN_RETRY)C(WARN_AUTOGEN)C(WARN_LEN)
#define A4(x) C(memcache_nil)C(memcache_queued)C(memcache_reading)C(memcache_done)
#define A5() C(MEMCACHE_NEVER)C(MEMCACHE_SEEK)C(MEMCACHE_RULE)C(MEMCACHE_COMPRESSED)C(MEMCACHE_ALWAYS)
#define A6(x) C(STATQUEUE_NIL)C(STATQUEUE_QUEUED)C(STATQUEUE_FAILED)C(STATQUEUE_OK)
#define A7(x) C(PTHREAD_NIL)C(PTHREAD_DIRCACHE)C(PTHREAD_MEMCACHE)C(PTHREAD_STATQUEUE)C(PTHREAD_RESPONDING)C(PTHREAD_MISC0)C(PTHREAD_LEN)
#define A8(x) C(mutex_nil)C(mutex_fhandle)C(mutex_mutex_count)C(mutex_mstore_init)C(mutex_autogen_init)C(mutex_dircachejobs)C(mutex_log_count)C(mutex_crc)C(mutex_inode)C(mutex_memUsage)C(mutex_dircache)C(mutex_idx)C(mutex_statqueue)C(mutex_validchars)C(mutex_special_file)C(mutex_validcharsdir)C(mutex_textbuffer_usage)C(mutex_roots)C(mutex_len) //* mutex_roots must be last */
#define C(a) a,
enum mstoreid{A1()};
enum mallocid{A2()};
enum warnings{A3()};
#if WITH_MEMCACHE
 enum memcache_status{A4()};
 enum when_memcache_zip{A5()};
#endif //WITH_MEMCACHE
enum statqueue_status{A6()};
enum root_thread{A7()};
enum mutex{A8()};
#undef C
#define C(a) #a,
static const char *HT_MALLOC_S[]={A1()NULL};
static const char *MALLOC_S[]={A2()NULL};
static const char *MY_WARNING_NAME[]={A3()NULL};
IF1(WITH_MEMCACHE,static const char *MEMCACHE_STATUS_S[]={A4()NULL});
IF1(WITH_MEMCACHE,static const char *WHEN_MEMCACHE_S[]={A5()NULL});
static const char *STATQUEUE_STATUS_S[]={A6()NULL};
static const char *PTHREAD_S[]={A7()NULL};
static const char *MUTEX_S[]={A8()NULL};
#undef C
#undef A1
#undef A2
#undef A3
#undef A4
#undef A5
#undef A6
#undef A7
#undef A8


struct counter_rootdata{
  const char *ext;
  atomic_uint counts[counter_rootdata_num];
  long rank;
  clock_t wait;
};
typedef struct counter_rootdata counter_rootdata_t;


struct autogen_files;

#define INIT_STRUCT_AUTOGEN_FILES(ff,vp,vp_l)  struct autogen_files ff={0}; cg_stpncpy0(ff.virtualpath,vp,ff.virtualpath_l=vp_l)


// ---
#if ! WITH_DIRCACHE
#undef WITH_DIRCACHE_OPTIMIZE_NAMES
#define WITH_DIRCACHE_OPTIMIZE_NAMES 0
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif
// ---
#if ! WITH_ZIPINLINE
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif
// ---
#define C_FILE_DATA_WITHOUT_NAME()    C(fsize,off_t); C(finode,uint64_t); C(fmtime,uint64_t); C(fcrc,uint32_t); C(fflags,uint8_t);
#define C_FILE_DATA()    C(fname,char *); C_FILE_DATA_WITHOUT_NAME();
#define C(field,type) type *field;
struct directory_core{
  struct timespec mtim;
  int files_l;
  C_FILE_DATA();
};
#undef C
#define DIRECTORY_FILES_CAPACITY 256 /* Capacity on stack. If exceeds then variables go to heap */
#define DIRECTORY_IS_DIRCACHE (1<<1)
#define DIRECTORY_IS_ZIPARCHIVE (1<<2)
#define DIRECTORY_TO_QUEUE (1<<3)
struct directory{
  uint32_t dir_flags;
  const char *dir_realpath;
  int dir_realpath_l;
  struct rootdata *root;
#define C(field,type) type _stack_##field[DIRECTORY_FILES_CAPACITY];
  C_FILE_DATA();
#undef C
  struct directory_core core; // Only this data goes to dircache.
  struct mstore filenames;
  int files_capacity;
};
// ---
////////////////
/// zippath  ///
////////////////
struct zippath{
#define C(s) int s,s##_l
  char strgs[ZPATH_STRGS];  int strgs_l; /* Contains all strings: virtualpath virtualpath_without_entry, entry_path and finally realpath */
  C(virtualpath); /* The path in the FUSE fs relative to the mount point */
  C(virtualpath_without_entry);  /*  Let Virtualpath be "/foo.zip.Content/bar". virtualpath_without_entry will be "/foo.zip.Content". */
  C(entry_path); /* Let Virtualpath be "/foo.zip.Content/bar". entry_path will be "bar". */
  C(realpath); /* The physical path of the file. In case of a ZIP entry, the path of the ZIP-file */
#undef C
  ht_hash_t virtualpath_hash,virtualpath_without_entry_hash,realpath_hash;
  zip_uint32_t zipcrc32;
  struct rootdata *root;
  int current_string; /* The String that is currently build within strgs with zpath_newstr().  With zpath_commit() this marks the end of String. */
  struct stat stat_rp,stat_vp;
  uint32_t flags;
};
#define ZP_STARTS_AUTOGEN (1<<2)
#define ZP_ZIP (1<<3)
#define ZP_DOES_NOT_EXIST (1<<4)
#define ZP_IS_COMPRESSED (1<<5)
#define ZP_VERBOSE (1<<6)
#define ZP_VERBOSE2 (1<<7)
#define ZPATH_IS_ZIP() ((zpath->flags&ZP_ZIP)!=0)
#define ZPATH_IS_VERBOSE() false //((zpath->flags&ZP_VERBOSE)!=0)
#define ZPATH_IS_VERBOSE2() false //((zpath->flags&ZP_VERBOSE2)!=0)
#define ZPATH_ROOT_WRITABLE() (0!=(zpath->root->features&ROOT_WRITABLE))
#define LOG_FILE_STAT() cg_log_file_stat(zpath->realpath,&zpath->stat_rp),log_file_stat(zpath->virtualpath,&zpath->stat_vp)
#define VP() (zpath->strgs+zpath->virtualpath)
#define EP() (zpath->strgs+zpath->entry_path)
#define EP_L() zpath->entry_path_l
#define VP_L() zpath->virtualpath_l
#define RP() (zpath->strgs+zpath->realpath)
#define RP_L() (zpath->realpath_l)
#define VP0() (zpath->strgs+zpath->virtualpath_without_entry)
#define VP0_L() zpath->virtualpath_without_entry_l
#define D_VP(d) (d->zpath.strgs+d->zpath.virtualpath)
#define D_VP0(d) (d->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP0(d) (d->zpath.strgs+d->zpath.virtualpath_without_entry)
#define D_VP_HASH(d) d->zpath.virtualpath_hash
#define D_EP(d) (d->zpath.strgs+d->zpath.entry_path)
#define D_EP_L(d) d->zpath.entry_path_l
#define D_VP_L(d) d->zpath.virtualpath_l
#define D_RP(d) (d->zpath.strgs+d->zpath.realpath)
#define D_RP_HASH(d) (d->zpath.realpath_hash)
#define D_RP_L(d) (d->zpath.realpath_l)
#define NEW_ZIPPATH(virtpath)  struct zippath __zp={0},*zpath=&__zp;zpath_init(zpath,virtpath)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath);  found=find_realpath_any_root(0,zpath,NULL);
////////////////////////////////////////////////////////////////////////////////////////////////////
///   The struct fhandle "file handle" holds data associated with a file descriptor.
///   They are stored in the linear list _fhandle. The list may have holes.
///   Memcache: It may also contain the cached file content of the zip entry.
///   Only one of all instances with a specific virtual path should store the cached zip entry
////////////////////////////////////////////////////////////////////////////////////////////////////
#define FINDRP_VERBOSE (1<<0)
#define FINDRP_AUTOGEN_CUT (1<<1)
#define FINDRP_AUTOGEN_CUT_NOT (1<<2)
#define FINDRP_NOT_TRANSIENT_CACHE (1<<3)
#define FILETYPEDATA_NUM 1024
#define FILETYPEDATA_FREQUENT_NUM 64
#define FHANDLE_FLAG_ACTIVE (1<<0)
#define FHANDLE_FLAG_DESTROY_LATER (1<<1)
#define FHANDLE_FLAG_INTERRUPTED (1<<2)
#define FHANDLE_FLAG_SEEK_FW_FAIL (1<<3)
#define FHANDLE_FLAG_SEEK_BW_FAIL (1<<4)
#define FHANDLE_FLAG_WITH_TRANSIENT_ZIPENTRY_CACHES (1<<5) /* Can attach ht_transient_cache */
#define FHANDLE_FLAG_IS_AUTOGEN (1<<6)
#define FHANDLE_FLAG_WITH_MEMCACHE (1<<7)
#define FHANDLE_FLAG_WITHOUT_MEMCACHE  (1<<8)
#define FHANDLE_FLAG_MEMCACHE_COMPLETE (1<<9)
#define FHANDLE_FLAG_OPEN_LATER_IN_READ_OR_WRITE (1<<10)
// #define foreach_fhandle_also_emty(id,d) int id=_fhandle_n;for(struct fhandle *d;--id>=0 && ((d=fhandle_at_index(id))||true);)

#define foreach_fhandle_also_emty(id,d) struct fhandle *d; for(int id=_fhandle_n;--id>=0 && ((d=fhandle_at_index(id))||true);)
// cppcheck-suppress-macro constVariablePointer
#define foreach_fhandle(id,d)  foreach_fhandle_also_emty(id,d) if (d->flags)
#define fhandle_path_eq(d,path,hash) d->zpath.virtualpath_hash==hash && !strcmp(D_VP(d),path)
#define find_realpath_again_fhandle(d) zpath_reset_keep_VP(&d->zpath),find_realpath_any_root(0,&d->zpath,NULL)
#define SIZEOF_FHANDLE sizeof(struct fhandle)
#define SIZEOF_ZIPPATH sizeof(struct zippath)
#define FHANDLE_LOG2_BLOCK_SIZE 5
#define FHANDLE_BLOCKS 512
#define FHANDLE_BLOCK_SIZE (1<<FHANDLE_LOG2_BLOCK_SIZE)
#define FHANDLE_MAX (FHANDLE_BLOCKS*FHANDLE_BLOCK_SIZE)
#define STATQUEUE_ENTRY_N 128
#define STATQUEUE_FLAGS_VERBOSE (1<<1)
#define STAT_ALSO_SYSCALL (1<<1)
#define STAT_USE_CACHE (1<<2)
#define STAT_USE_CACHE_FOR_ROOT(r) (r!=_root_writable?STAT_USE_CACHE:0)
struct fhandle{
  uint64_t fh, fh_real;
  struct zip *zarchive;
  zip_file_t *zip_file;
  struct zippath zpath;
  volatile time_t accesstime;
  volatile int flags;
  IF1(WITH_AUTOGEN,enum enum_autogen_state autogen_state; int autogen_error);
  uint8_t already_logged;
  volatile int64_t offset,n_read;
  volatile atomic_int is_busy; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex_read; /* Costs 40 Bytes */
  counter_rootdata_t *filetypedata;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,struct ht *ht_transient_cache);
  bool debug_not_yet_read;
  off_t zip_fread_position;
#if WITH_MEMCACHE
  struct memcache *memcache;
  int is_memcache_store;
#endif //WITH_MEMCACHE
};
#if WITH_STAT_SEPARATE_THREADS
struct statqueue_entry{
  volatile int flags,refcount;
  char rp[MAX_PATHLEN+1]; volatile int rp_l;  volatile ht_hash_t rp_hash;
  struct stat stat;
  volatile enum statqueue_status status;
  struct timespec time;
};
#endif



/////////////////////////////////////////////////////////////////////////
///  struct rootdata holds the data for a branch
///  ZIPsFS is a union file system and combines all source directories
//////////////////////////////////////////////////////////////////////////
#define is_last_root_or_null(r)  (!r||r==_root+_root_n-1)
//#define foreach_root(ir,r)    int ir=0;for(struct rootdata *r=_root; ir<_root_n; r++,ir++)
// #define foreach_root(ir,r)    struct rootdata *r=_root; for(int ir=0; ir<_root_n; r++,ir++)
#define foreach_root1(r)    for(struct rootdata *r=_root; r<_root+_root_n; r++)

#define ROOT_WRITABLE (1<<1)
#define ROOT_REMOTE (1<<2)




struct rootdata{ /* Data for a branch. ZIPsFS acts like a union FS. */
  char rootpath[MAX_PATHLEN+1];
  int rootpath_l;
  uint32_t features;
  struct statvfs statvfs;
  uint32_t log_count_delayed,log_count_delayed_periods,log_count_restarted;
  volatile uint32_t statfs_took_deciseconds; /* how long did it take */
  struct ht dircache_queue,dircache_ht,ht_inodes; // !!
#if WITH_DIRCACHE || WITH_STAT_CACHE
  struct ht dircache_ht_fname, dircache_ht_fnamearray;
  struct mstore dircache_mstore;
#endif
  struct ht ht_filetypedata;
  pthread_t thread[PTHREAD_LEN];
  int thread_count_started[PTHREAD_LEN];
  int thread_when_loop_deciSec[PTHREAD_LEN]; /* Detect blocked loop. */
    int thread_when_canceled_deciSec[PTHREAD_LEN];
  IF1(WITH_STAT_SEPARATE_THREADS,struct statqueue_entry statqueue[STATQUEUE_ENTRY_N]);
  bool thread_pretend_blocked[PTHREAD_LEN];
  atomic_int thread_starting[PTHREAD_LEN];
  bool thread_is_run[PTHREAD_LEN];
  pid_t thread_pid[PTHREAD_LEN];
  IF1(WITH_MEMCACHE,struct fhandle *memcache_d);
  //pthread_mutex_t  mutex_zip_fread;
  counter_rootdata_t filetypedata_dummy,filetypedata_all, filetypedata[FILETYPEDATA_NUM],filetypedata_frequent[FILETYPEDATA_FREQUENT_NUM];
  bool filetypedata_initialized, blocked;



};





#if WITH_MEMCACHE
#define ADVISE_CACHE_IS_CMPRESSED (1<<1)
#define ADVISE_CACHE_IS_SEEK_BW (1<<2)
struct memcache{
  volatile enum memcache_status memcache_status;
  struct textbuffer *txtbuf;
  volatile off_t memcache_l,memcache_already_current,memcache_already;
  int64_t memcache_took_mseconds, memcache_took_mseconds_in_lock;
  int id;
};
#endif // WITH_MEMCACHE
