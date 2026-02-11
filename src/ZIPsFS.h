/////////////////////////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                                       ///
/// Enums and structs                                         ///
/////////////////////////////////////////////////////////////////




//////////////////////////////
/// Constants and Macros   ///
//////////////////////////////

#define WITH_SPECIAL_FILE WITH_PRELOADRAM
#define zpath_strcat(zpath,s)  zpath_strncat(zpath,s,9999)
#define ZPATH_COMMIT_HASH(zpath,x) zpath->x##_l=zpath_commit_hash(zpath,&zpath->x##_hash)
#define zpath_commit(zpath) zpath_commit_hash(zpath,NULL)
#define zpath_assert_strlen(zpath)  _zpath_assert_strlen(__func__,__FILE_NAME__,__LINE__,zpath)
#define Nth(array,i,defaultVal) (array?array[i]:defaultVal)
#define Nth0(array,i) Nth(array,i,0)

#define PROFILED(x) x

#define SIZE_POINTER sizeof(char *)
#define MAYBE_ASSERT(...) if (_killOnError) assert(__VA_ARGS__)
#define LOG_OPEN_RELEASE(path,...)
#define EXT_ROOT_PROPERTY ".ZIPsFS.properties"
#define PATH_DOT_ZIPSFS "~/.ZIPsFS"
#define NUM_MUTEX   (mutex_roots+ROOTS)
#define DIRENT_ISDIR (1<<0)
#define DIRENT_IS_COMPRESSEDZIPENTRY (1<<1)
#define DIRENT_SAVE_MASK   (DIRENT_IS_COMPRESSEDZIPENTRY|DIRENT_ISDIR)
#define DIRENT_DIRECT_NAME (1<<2)
#define DIRENT_DIRECT_DEBUG (1<<3)
//#define DEBUG_DIRECTORY_HASH32_OF_FNAME(s)    hash32((char*)s->fname,s->files_l*sizeof(char*))

#define MALLOC_TYPE_SHIFT 8
#define MALLOC_TYPE_HT (1<<MALLOC_TYPE_SHIFT)
#define MALLOC_TYPE_KEYSTORE (2<<MALLOC_TYPE_SHIFT)
#define MALLOC_TYPE_VALUESTORE (3<<MALLOC_TYPE_SHIFT)
#define MALLOC_ID_COUNT (5<<MALLOC_TYPE_SHIFT)
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
#define htentry_fsize_for_inode(inode,create) ht_numkey_get_entry(&ht_fsize,inode,0,create)
#define htentry_fsize(vp,vp_l,create) htentry_fsize_for_inode(inode_from_virtualpath(vp,vp_l),create)
#else
#define WITH_FILECONVERSION_OR_CCODE 0
#endif


/////////////
/// debug ///
/////////////

#define DEBUG_THROTTLE_DOWNLOAD 0

/////////////
/// Files ///
/////////////

#define DIR_ZIPsFS                   "/ZIPsFS"
#define DIR_ZIPsFS_L                 (sizeof(DIR_ZIPsFS)-1)
#define PATH_STARTS_WITH_DIR_ZIPsFS(p)    (p[0]=='/'&&p[1]=='Z'&&p[2]=='I'&&p[3]=='P'&&p[4]=='s'&&p[5]=='F'&&p[6]=='S'&&(p[7]=='/'||!p[7]))

#define DIR_FIRST_ROOT          DIR_ZIPsFS"/1"

#define DIR_PRELOADDISK_R        DIR_ZIPsFS"/lr"
#define DIR_PRELOADDISK_RC       DIR_ZIPsFS"/lrc"
#define DIR_PRELOADDISK_RZ       DIR_ZIPsFS"/lrz"

#define DIR_FILECONVERSION                  DIR_ZIPsFS"/c"
#define DIR_FILECONVERSION_L                (sizeof(DIR_FILECONVERSION)-1)
#define DIR_INTERNET                 DIR_ZIPsFS"/n"
#define DIR_PLAIN                    DIR_ZIPsFS"/p"
#define DIR_INTERNET_L               (sizeof(DIR_INTERNET)-1)
#define DIR_INTERNET_GZ_L            (DIR_INTERNET_L+3)


#define FILE_CLEANUP_SCRIPT          DIR_ZIPsFS"/ZIPsFS_cleanup.sh"

#define DIRNAME_PRELOADED_UPDATE     "Update_Preloaded_Files"
#define DIRNAME_PRELOADED_UPDATE_L   (sizeof(DIRNAME_PRELOADED_UPDATE)-1)
#define DIR_PRELOADED_UPDATE         DIR_ZIPsFS"/"DIRNAME_PRELOADED_UPDATE





enum enum_functions{xmp_open_,xmp_access_,xmp_getattr_,xmp_read_,xmp_readdir_,functions_l};
enum enum_count_getattr{
  COUNTER_STAT_FAIL,COUNTER_STAT_SUCCESS,
  COUNTER_OPENDIR_FAIL,COUNTER_OPENDIR_SUCCESS,
  COUNTER_ZIPOPEN_FAIL,COUNTER_ZIPOPEN_SUCCESS,
  COUNTER_GETATTR_FAIL,COUNTER_GETATTR_SUCCESS,
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
  COUNT_RETRY_PRELOADRAM,
  //
  counter_rootdata_num};



// cppcheck-suppress-macro staticStringCompare
#define XMACRO_SPECIAL_FILES()\
  X(0,                                 "warnings.log",                         DIR_ZIPsFS,           SFILE_LOG_WARNINGS)\
  X(0,                                 "errors.log",                           DIR_ZIPsFS,           SFILE_LOG_ERRORS)\
  X(0,                                 NULL,                                   "",                   SFILE_DEBUG_CTRL)\
  X(0,                                 NULL,                                   "",                   SFILE_WITH_REALPATH_NUM)\
  X(0,                                 NULL,                                   "",                   SFILE_BEGIN_IN_RAM)\
  X(0,                                 "file_system_info.html",                DIR_ZIPsFS,           SFILE_INFO)\
  X(0,                                 "ZIPsFS_set_file_access_time.command",  DIR_ZIPsFS,           SFILE_SET_ATIME_SH)\
  X(0,                                 "ZIPsFS_set_file_access_time.ps1",      DIR_ZIPsFS,           SFILE_SET_ATIME_PS)\
  X(0,                                 "ZIPsFS_set_file_access_time.bat",      DIR_ZIPsFS,           SFILE_SET_ATIME_BAT)\
  X(0,                                 "Read_beginning_of_selected_files.bat", DIR_ZIPsFS,           SFILE_READ_BEGINNING_OF_FILES_BAT)\
  X(0,                                 "ZIPsFS_clear_cache.command",           DIR_ZIPsFS,           SFILE_CLEAR_CACHE)\
  X(0,                                 "README_ZIPsFS.html",                   DIR_ZIPsFS,           SFILE_README)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_1st_root.txt",           DIR_FIRST_ROOT,       SFILE_README_FIRST_ROOT)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_plain.txt",              DIR_PLAIN,            SFILE_README_PLAIN)\
  X(0,                                 "README_ZIPsFS_net.html",               DIR_INTERNET,         SFILE_README_INTERNET)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_fileconversion.html",    DIR_FILECONVERSION,   SFILE_README_FILECONVERSION)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_preload.html",           DIR_PRELOADDISK_R,    SFILE_README_PRELOADDISK_R)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_preload.html",           DIR_PRELOADDISK_RC,   SFILE_README_PRELOADDISK_RC)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_preload.html",           DIR_PRELOADDISK_RZ,   SFILE_README_PRELOADDISK_RZ)\
  X(ZP_VP_STRIP_PFX,                   "README_ZIPsFS_preload.html",           DIR_PRELOADED_UPDATE, SFILE_README_PRELOADDISK_UPDATE)


#define X(opt,name,D,id) id,
enum enum_special_files{SFILE_NIL,XMACRO_SPECIAL_FILES() SFILE_NUM};
#undef X




#define PATH_STARTS_WITH(vp,vp_l,dir,dir_l)   (((vp_l)>dir_l && (vp)[dir_l]=='/' || dir_l==vp_l)  &&  cg_startsWith(vp,vp_l,dir,dir_l))
#define       STR_EQUALS(vp,vp_l,s)           (((vp_l)==sizeof(s)-1&&!memcmp((vp),s,sizeof(s)-1)))





//enum enum_internet_compression{INTERNET_COMPRESSION_NA, INTERNET_COMPRESSION_NONE, INTERNET_COMPRESSION_GZ, INTERNET_COMPRESSION_BZ2, INTERNET_COMPRESSION_NUM};
enum enum_fileconversion_state{FILECONVERSION_UNINITILIZED,FILECONVERSION_SUCCESS,FILECONVERSION_FAIL};
///////////////////////////////////////
/// Enums and corresponding strings ///
///////////////////////////////////////
#define A1() C(HT_MALLOC_UNDEFINED)C(HT_MALLOC_warnings)C(HT_MALLOC_ht_count_by_ext)C(HT_MALLOC_file_ext)C(HT_MALLOC_inodes)C(HT_MALLOC_transient_cache)C(HT_MALLOC_without_dups)C(HT_MALLOC_LEN)
#define A3() C(WARN_INIT)C(WARN_MISC)C(WARN_CONFIG)C(WARN_STR)C(WARN_INODE)C(WARN_THREAD)C(WARN_MALLOC)C(WARN_ROOT)C(WARN_OPEN)C(WARN_READ)C(WARN_ZIP_FREAD)C(WARN_READDIR)C(WARN_SEEK)C(WARN_ZIP)C(WARN_GETATTR)C(WARN_STAT)C(WARN_FHANDLE)C(WARN_DIRCACHE) C(WARN_PRELOADFILE) C(WARN_PRELOADRAM) C(WARN_PRELOADDISK)C(WARN_FORMAT)C(WARN_DEBUG)C(WARN_CHARS)C(WARN_RETRY)C(WARN_FILECONVERSION)C(WARN_C)C(WARN_NET)C(WARN_LEN)
#define A4(x) C(preloadram_status_nil)C(preloadram_queued)C(preloadram_reading)C(preloadram_done)C(PRELOADRAM_STATUS_NUM)
#define A5() C(PRELOADRAM_NEVER)C(PRELOADRAM_SEEK)C(PRELOADRAM_RULE)C(PRELOADRAM_COMPRESSED)C(PRELOADRAM_ALWAYS)
#define A7(x) C(PTHREAD_NIL)C(PTHREAD_ASYNC)C(PTHREAD_PRELOAD)C(PTHREAD_MISC)C(PTHREAD_LEN)
#define A8(x) C(mutex_nil)C(mutex_start_thread)C(mutex_fhandle)C(mutex_async)C(mutex_mutex_count)C(mutex_mstore_init)C(mutex_fileconversion_init)C(mutex_dircache_queue)C(mutex_log_count)C(mutex_crc)C(mutex_inode)C(mutex_memUsage)C(mutex_dircache)C(mutex_idx)C(mutex_validchars)C(mutex_special_file)C(mutex_validcharsdir)C(mutex_textbuffer_usage)C(mutex_roots)C(mutex_len) //* mutex_roots must be last */

#define A9(x) C(LOG_DEACTIVATE_ALL)C(LOG_FUSE_METHODS_ENTER)C(LOG_FUSE_METHODS_EXIT)C(LOG_ZIP)C(LOG_ZIP_INLINE)C(LOG_EVICT_FROM_CACHE)C(LOG_PRELOADRAM)C(LOG_REALPATH)C(LOG_FILECONVERSION)C(LOG_READ_BLOCK)\
       C(LOG_TRANSIENT_ZIPENTRY_CACHE)\
       C(LOG_STAT)C(LOG_OPEN)C(LOG_OPENDIR)\
       C(LOG_INFINITY_LOOP_RESPONSE)C(LOG_INFINITY_LOOP_STAT)C(LOG_INFINITY_LOOP_PRELOADRAM)C(LOG_INFINITY_LOOP_DIRCACHE)C(LOG_INFINITY_LOOP_MISC)  C(LOG_FLAG_LENGTH)

#define A10(x) C(ASYNC_NIL)C(ASYNC_STAT)C(ASYNC_READDIR)C(ASYNC_OPENFILE)C(ASYNC_OPENZIP)   C(ASYNC_LENGTH)


#define A12() C(COUNT_UNDEFINED) C(COUNT_MALLOC_TESTING)\
       C(COUNT_HT_MALLOC_TRANSIENT_CACHE)C(COUNT_MALLOC_KEY_TRANSIENT_CACHE)\
       C(COUNT_HT_MALLOC_MISMATCH)C(COUNT_HT_MALLOC_KEY_MISMATCH)\
       C(COUNT_HT_MALLOC_NODUPS)\
       C(COUNT_MSTORE_MMAP_NODUPS) C(COUNT_MSTORE_MALLOC_NODUPS)\
       C(COUNTm_MSTORE_MMAP)                C(COUNTm_MSTORE_MALLOC)\
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
       C(COUNTm_FHANDLE_ARRAY_MALLOC)\
  C(COUNT_FHANDLE_TRANSIENT_CACHE)\
       C(COUNT_PAIRS_END)\
\
       C(COUNT_PRELOADRAM_WAITFOR_TIMEOUT)\
       C(COUNT_LCOPY_WAITFOR_TIMEOUT)\
       C(COUNT_READZIP_PRELOADRAM)\
  C(COUNT_READZIP_PRELOADRAM_BECAUSE_SEEK_BWD)\
  C(COUNT_STAT_FROM_CACHE)\
  C(COUNT_PTHREAD_LOCK)\
       C(COUNT_SEQUENTIAL_INODE)\
  C(COUNT_NUM)


#define A13() C(ACT_NIL)C(ACT_KILL_ZIPSFS)C(ACT_FORCE_UNBLOCK)C(ACT_CANCEL_THREAD)C(ACT_NO_LOCK)C(ACT_BAD_LOCK)C(ACT_CLEAR_CACHE)C(ACT_LEN)

#define A11() C(ZIPsFS_configuration)C(ZIPsFS_configuration_fileconversion)C(ZIPsFS_configuration_c)C(ZIPsFS_configuration_zipfile)C(ZIPsFS_configuration_NUM)

#define C(a) a,
enum enum_mstoreid{A1()};
enum enum_mallocid{A12()};
enum enum_ctrl_action{A13()};
enum enum_warnings{A3()};
#if WITH_PRELOADRAM
enum enum_preloadram_status{A4()};
enum enum_when_preloadram_zip{A5()};
#endif //WITH_PRELOADRAM
enum enum_root_thread{A7()};
enum enum_mutex{A8()};
enum enum_log_flags{A9()};
enum enum_async{A10()};
enum enum_configuration_src{A11()};
#undef C
#define C(a) #a,
static const char *HT_MALLOC_S[]={A1()NULL};
static const char *COUNT_S[]={A12()NULL};
static const char *CTRL_ACTION_S[]={A13()NULL};
static const char *MY_WARNING_NAME[]={A3()NULL};
IF1(WITH_PRELOADRAM,static const char *PRELOADRAM_STATUS_S[]={A4()NULL});
IF1(WITH_PRELOADRAM,static const char *WHEN_PRELOADRAM_S[]={A5()NULL});
static const char *PTHREAD_S[]={A7()NULL};
static const char *MUTEX_S[]={A8()NULL};
static const char *LOG_FLAG_S[]={A9()NULL};
static const char *ASYNC_S[]={A10()NULL};
static const char *ZIPSFS_CONFIGURATION_S[]={A11()NULL};
#undef C
#undef A1
#undef A2
#undef A3
#undef A4
#undef A5
#undef A6
#undef A7
#undef A8
#undef A9
#undef A10
#undef A11
#undef A12
#undef A13




struct counter_rootdata{
  const char *ext;
  atomic_uint counts[counter_rootdata_num];
  long rank;
  clock_t wait;
};
typedef struct counter_rootdata counter_rootdata_t;


struct fileconversion_files;
// ---

#if ! WITH_DIRCACHE
#undef WITH_ZIPINLINE
#define WITH_ZIPINLINE 0
#undef WITH_ZIPENTRY_PLACEHOLDER
#define WITH_ZIPENTRY_PLACEHOLDER 0
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif
// ---
#if ! WITH_ZIPINLINE
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif
// ---
#define XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME()    X(fsize,off_t); X(fmtime,uint64_t);X(fcrc,uint32_t);X(fflags,uint8_t);X(finode,uint64_t);
#define XMACRO_DIRECTORY_ARRAYS()                  X(fname,char *); XMACRO_DIRECTORY_ARRAYS_WITHOUT_FNAME();
#define X(field,type) type *field;
struct directory_core{
  struct timespec dir_mtim;
  int files_l;
  XMACRO_DIRECTORY_ARRAYS();
};
#undef X


////////////////
/// zippath  ///
////////////////

/***************************************************************************************************************************/
/* This struct is initialized with the data of virtualpath_t.                                                              */
/* In the function test_realpath(), a matching existing file for the virtual path will be looked for for each file branch. */
/* If found, then the struct will contain this real path and the struct stat data.                                         */
/***************************************************************************************************************************/
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
  IF1(WITH_PRELOADDISK,const char *preloadpfx);
  const char *dir;
  char *zipfile_append;
  int zipfile_l, zipfile_cutr;
  uint32_t flags,is_decompressed;
  IF1(WITH_PRELOADRAM,off_t preloadram_need_bytes);


  /* The following should not be removed by path_reset_keep_VP() */
#define ZP_MASK_SFILE              ((1<<5)-1)
  _Static_assert(ZP_MASK_SFILE>SFILE_WITH_REALPATH_NUM,""); /*  Special files */
#define ZP_NOT_EXPAND_SYMLINKS     (1<<12)
#define ZP_IS_PATHINFO             (1<<14)
#define ZP_VP_STRIP_PFX            (1<<17) /* the path prefix in zpath_t.dir or virtualpath.dir is removed from VP() and virtualpath.vp */

#define ZP_KEEP_NOT_RESET_MASK     (((1<<20)-1)&~ZP_MASK_SFILE)
  /* The following should be removed by path_reset_keep_VP() */

#define ZP_DOES_NOT_EXIST          (1<<21)
#define ZP_IS_ZIP                  (1<<22)
#define ZP_IS_COMPRESSEDZIPENTRY           (1<<23)
#define ZP_FROM_TRANSIENT_CACHE    (1<<24)
#define ZP_OVERFLOW                (1<<25) /* The paths are to long and the buffer strgs[ZPATH_STRGS] to small */
#define ZP_CHECKED_EXISTENCE_COMPRESSED (1<<26)
  /*  The following instruct test_realpath */
#define ZP_RESET_IF_NEXISTS        (1<<28)
#define ZP_TRY_ZIP                 (1<<29)
#define ZP_TRY_FILECONVERSION             (1<<30)
#define ZPF(f) (zpath->flags&(f))
};
#define ZPATH_IS_FILECONVERSION() (zpath->dir==DIR_FILECONVERSION && !(zpath->flags&ZP_IS_PATHINFO))

typedef struct zippath zpath_t;

struct async_zipfile{
  zip_file_t *zf;
  struct zip *za;
  zpath_t azf_zpath;
} static const async_zipfile_empty;


#define DIRECTORY_DIM_STACK IF01(WITH_TESTING_REALLOC,MAX(256,ULIMIT_S),4)
struct directory{
  STRUCT_NOT_ASSIGNABLE();
  zpath_t dir_zpath;
#define DIR_RP(dir)      ((dir)->dir_zpath.strgs+(dir)->dir_zpath.realpath)
#define DIR_RP_L(dir)    ((dir)->dir_zpath.realpath_l)
#define DIR_VP(dir)      ((dir)->dir_zpath.strgs+(dir)->dir_zpath.virtualpath)
#define DIR_VP_L(dir)    ((dir)->dir_zpath.virtualpath_l)
#define DIR_ROOT(dir)    ((dir)->dir_zpath.root)
#define DIR_IS_ZIP(dir) (((dir)->dir_zpath.flags&ZP_TRY_ZIP)!=0)
#define X(field,type) type _stack_##field[DIRECTORY_DIM_STACK];
  XMACRO_DIRECTORY_ARRAYS();
#undef X
  struct directory_core core; // Only this data goes to dircache.
  struct mstore filenames;
  int files_capacity,cached_vp_to_zip;
  bool debug, dir_is_dircache, dir_is_destroyed, dir_is_success, has_file_containing_placeholder;
  /* The following concern asynchronized reading */
  bool async_never,when_readdir_call_stat_and_store_in_cache;
  IF1(WITH_TIMEOUT_READDIR, ht_t *ht_intern_names);
};
typedef struct directory directory_t;
#define ROOT_WHEN_SUCCESS(r,t) atomic_load(r->thread_when_success+t)
#define ROOT_WHEN_ITERATED(r,t) atomic_load(r->thread_when+t)

#define ROOT_SUCCESS_SECONDS_AGO(r) (time(NULL)-ROOT_WHEN_SUCCESS(r,PTHREAD_ASYNC))
#define ROOT_NOT_RESPONDING(r)  (r->remote && ROOT_SUCCESS_SECONDS_AGO(r)>r->check_response_seconds)





#define ZPATH_ROOT_WRITABLE() (zpath->root && 0!=(zpath->root->writable))
#define ZPATH_LOG_FILE_STAT(zpath) cg_log_file_stat(ZP_RP(zpath),&(zpath)->stat_rp),cg_log_file_stat(ZP_VP(zpath),&(zpath)->stat_vp)
//#define VP() (zpath->strgs+zpath->virtualpath)
#define EP() (zpath->strgs+zpath->entry_path)
#define EP_L() zpath->entry_path_l
#define VP_L() zpath->virtualpath_l
#define ZP_RP(zpath) ((zpath)->strgs+(zpath)->realpath)
#define ZP_VP(zpath) ((zpath)->strgs+(zpath)->virtualpath)

#define RP() ZP_RP(zpath)
#define VP() ZP_VP(zpath)

//#define RP() ZP_RP(zpath)

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
#define NEW_ZIPPATH(vipa)  zpath_t __zp={0},*zpath=&__zp;zpath_init(zpath,vipa)
#define FIND_REALPATH(virtpath)    NEW_ZIPPATH(virtpath);  found=find_realpath_any_root(0,zpath,NULL);
#define FIND_REALPATH_NOT_EXPAND_SYMLINK(virtpath)    NEW_ZIPPATH(virtpath); zpath->flags|=ZP_NOT_EXPAND_SYMLINKS;  found=find_realpath_any_root(0,zpath,NULL);
////////////////////////////////////////////////////////////////////////////////////////////////////
///   The fHandle_t "file handle" holds data associated with a file descriptor.
///   They are stored in the linear list _fhandle. The list may have holes.
///   Preloadram: It may also contain the cached file content of the zip entry.
///   Only one of all instances with a specific virtual path should store the cached zip entry
////////////////////////////////////////////////////////////////////////////////////////////////////





#define FILETYPEDATA_NUM 1024
#define FILETYPEDATA_FREQUENT_NUM 64
// #define foreach_fhandle_also_emty(id,d) int id=_fhandle_n;for(fHandle_t *d;--id>=0 && ((d=fhandle_at_index(id))||true);)

#define foreach_fhandle_also_emty(id,d) fHandle_t *d; for(int id=_fhandle_n;--id>=0 && ((d=fhandle_at_index(id))||true);)
#define fhandle_virtualpath_equals(d,e) (d!=e && D_VP_HASH(e)==D_VP_HASH(d) && !strcmp(D_VP(d),D_VP(e)))
#define fhandle_zip_ftell(d) d->zip_fread_position
// cppcheck-suppress-macro constVariablePointer
#define foreach_fhandle(id,d)  foreach_fhandle_also_emty(id,d) if (d->flags)




/* flags for config_file_attribute_valid_seconds() */

#define STAT_CACHE_ROOT_IS_REMOTE     (1<<3)
#define STAT_CACHE_ROOT_IS_WRITABLE   (1<<4)
#define STAT_CACHE_ROOT_IS_PRELOADING (1<<5)
#define STAT_CACHE_ROOT_WITH_TIMEOUT  (1<<6)
#define STAT_CACHE_ROOT_IS_WORM       (1<<7)
#define STAT_CACHE_ROOT_IS_IMMUTABLE         (1<<8)
#define STAT_CACHE_IS_PFXPLAIN        (1<<9)
#define STAT_CACHE_OPT_FOR_ROOT(opt_filldir_findrp,r)  (\
                                                        ((opt_filldir_findrp&FINDRP_IS_PFXPLAIN)?STAT_CACHE_IS_PFXPLAIN:0)|\
                                                        ((opt_filldir_findrp&FINDRP_IS_WORM)    ?STAT_CACHE_ROOT_IS_WORM:0)|\
                                                        ((opt_filldir_findrp&FINDRP_IS_IMMUTABLE)      ?STAT_CACHE_ROOT_IS_IMMUTABLE:0)|\
                                                         (r && r==_root_writable?STAT_CACHE_ROOT_IS_WRITABLE:0)|\
                                                         (r && r->remote        ?STAT_CACHE_ROOT_IS_REMOTE:0)|\
                                                         (r && r->with_timeout  ?STAT_CACHE_ROOT_WITH_TIMEOUT:0)|\
                                                         (r && r->preload_flags ?STAT_CACHE_ROOT_IS_PRELOADING:0))





#define FHANDLE_LOG2_BLOCK_SIZE 5
#define FHANDLE_BLOCKS 512
#define FHANDLE_BLOCK_SIZE (1<<FHANDLE_LOG2_BLOCK_SIZE)
#define FHANDLE_MAX (FHANDLE_BLOCKS*FHANDLE_BLOCK_SIZE)


/************************************************************************/
/* This struct initially holds the virtual path and derived information */
/* in the FUSE functions.                                               */
/* Later, the data will be copied into an instance of struct zippath    */
/************************************************************************/
struct virtualpath{
  const char *vp,*preloadpfx,*dir;
  int vp_l, preloadpfx_l,flags, dir_l;
  char *zipfile_append;
  int zipfile_l, zipfile_cutr, special_file_id;
};
typedef struct virtualpath virtualpath_t;
static virtualpath_t empty_virtualpath;

#define NEW_VIRTUALPATH(vpath) virtualpath_t vipa; char vp_buffer[MAX_PATHLEN]; virtualpath_init(&vipa,vpath,vp_buffer)
#define FUSE_PREAMBLE(vpath) virtualpath_t vipa; char vp_buffer[MAX_PATHLEN];\
  int er=virtualpath_init(&vipa,vpath,vp_buffer); if(er) return -er;\
  IF_LOG_FLAG(LOG_FUSE_METHODS_ENTER)log_entered_function("%s",vpath)

#define FUSE_PREAMBLE_W(create_or_del,vpath) FUSE_PREAMBLE(vpath); er=virtualpath_error(&vipa,create_or_del); if (er) return -er


/***************************************************************************************/
/* This struct contains data associated with a file handle.                            */
/* It is created in xmp_open unless for plain regular files not needing preloading.    */
/* It is then used in xmp_read() */
/***************************************************************************************/
struct fHandle{
  uint64_t fh;
  int fd_real;
  struct zip *zip_archive;
  zip_file_t *zip_file;
  zpath_t zpath;
  volatile time_t accesstime;
  volatile int errorno, flags;
#define FHANDLE_ACTIVE                         (1<<0)
#define FHANDLE_DESTROY_LATER                  (1<<1)
#define FHANDLE_PREPARE_ONCE_IN_RW             (1<<2)
#define FHANDLE_SEEK_FW_FAIL                   (1<<3)
#define FHANDLE_SEEK_BW_FAIL                   (1<<4)
#define FHANDLE_WITH_TRANSIENT_ZIPENTRY_CACHES (1<<5) /* Can attach ht_transient_cache */
#define FHANDLE_IS_FILECONVERSION                     (1<<6)
#define FHANDLE_WITH_PRELOADRAM                (1<<8)
#define FHANDLE_WAITED_FREE_RAM                (1<<9)
#define FHANDLE_PRELOADRAM_COMPLETE           (1<<10)
#define FHANDLE_SPECIAL_FILE                   (1<<11)
#define FHANDLE_IS_CCODE                       (1<<12)
#define FHANDLE_PRELOADFILE_QUEUE              (1<<13)
#define FHANDLE_PRELOADFILE_RUN                (1<<14)
  uint8_t already_logged;
  volatile int64_t offset,n_read;
  volatile atomic_int is_busy; /* Increases when entering xmp_read. If greater 0 then the instance must not be destroyed. */
  pthread_mutex_t mutex_read; /* Costs 40 Bytes */
  counter_rootdata_t *filetypedata;
  off_t zip_fread_position;
  IF1(WITH_TRANSIENT_ZIPENTRY_CACHES,ht_t *ht_transient_cache);
  IF1(WITH_PRELOADRAM, struct preloadram * volatile preloadram);
  atomic_int volatile is_preloading;
  IF1(WITH_FILECONVERSION,enum enum_fileconversion_state fileconversion_state);
} static const FHANDLE_EMPTY;
typedef struct fHandle fHandle_t;
/////////////////////////////////////////////////////////////////////////
///  Union file system:
///  root_t holds the data for a branch
///  This is the root path  of an upstream source file tree.
///  ZIPsFS is a union file system and combines all source directories
//////////////////////////////////////////////////////////////////////////
#define foreach_root(r)    for(root_t *r=_root; r<_root+_root_n; r++)
struct rootdata{
  const char *rootpath, *rootpath_orig, *rootpath_mountpoint, *pathpfx, *pathpfx_slash_to_null;
  int rootpath_l,pathpfx_l;
  struct statvfs statvfs;
  dev_t  st_dev;
  uint32_t log_count_delayed,log_count_delayed_periods,log_count_restarted;
  ht_t ht_int_rp,dircache_queue,dircache_ht,ht_inodes IF1(WITH_STAT_CACHE,,stat_ht);

#if WITH_DIRCACHE || WITH_STAT_CACHE || WITH_TIMEOUT_READDIR
  ht_t ht_int_fname, ht_int_fnamearray;
  struct mstore dircache_mstore;
#endif
  ht_t ht_filetypedata;
  pthread_t thread[PTHREAD_LEN];
  int thread_count_started[PTHREAD_LEN];
  int thread_when_canceled[PTHREAD_LEN];
  pid_t thread_pid[PTHREAD_LEN];
  counter_rootdata_t filetypedata_dummy,filetypedata_all, filetypedata[FILETYPEDATA_NUM],filetypedata_frequent[FILETYPEDATA_FREQUENT_NUM];
  bool filetypedata_initialized, blocked, writable, remote,with_timeout,thread_already_started[PTHREAD_LEN],noatime, no_cross_device, follow_symlinks,worm,immutable;
  int preload_flags, /* Currently decompression exclusively works by preloading to disk and preload_flags and decompress_flags are identical. */
    decompress_flags;  /* These flags also serve for filler_add() */

#define PRELOAD_YES           (1<<9)
#define FLAGS_FILLDIR_FINDRP_BEGIN    (1<<10)
#define FILLDIR_FILECONVERSION        (1<<10)
#define FILLDIR_FILES_S_ISVTX         (1<<11)
#define FILLDIR_FROM_OPEN             (1<<12)
#define FINDRP_FILECONVERSION_CUT     (1<<13)
#define FINDRP_FILECONVERSION_CUT_NOT (1<<14)
#define FINDRP_NOT_TRANSIENT_CACHE    (1<<15)
#define FINDRP_IS_PFXPLAIN            (1<<16)
#define FINDRP_IS_WORM                (1<<17)
#define FINDRP_IS_IMMUTABLE           (1<<18)
#define FINDRP_STAT_TOCACHE_ALWAYS    (1<<19)

  _Static_assert(((1<<COMPRESSION_NUM)-1<PRELOAD_YES) &&
                 ((1<<COMPRESSION_NUM)-1<FLAGS_FILLDIR_FINDRP_BEGIN),"COMPRESSION_NUM ... is part of this");
  /*  In one  opt   FILLDIR_XXXX FINDRP and COMPRESSION */


  int seq_fsid;
  unsigned long f_fsid; /* From statvfs.f_fsid */
  pthread_mutex_t async_mtx[ASYNC_LENGTH];
  int async_go[ASYNC_LENGTH];
  int async_task_id[ASYNC_LENGTH];
  volatile atomic_ulong thread_when[PTHREAD_LEN],thread_when_success[PTHREAD_LEN]; _Static_assert(sizeof(atomic_ulong)>=sizeof(time_t),"");
  IF1(WITH_TIMEOUT_OPENFILE,int  async_openfile_fd,async_openfile_flags;  char async_openfile_path[MAX_PATHLEN+1]);
  IF1(WITH_TIMEOUT_STAT,    struct stat async_stat;  const char *async_stat_path);
  IF1(WITH_TIMEOUT_READDIR, directory_t *async_dir);
  IF1(WITH_TIMEOUT_OPENZIP, struct async_zipfile *async_zipfile);

  const char *check_response_path;
  int check_response_seconds;
  int check_response_seconds_max;
  int stat_timeout_seconds,readdir_timeout_seconds,openfile_timeout_seconds;
  char **exclude_vp, **starts_vp;
};
typedef struct rootdata root_t;

#define ROOT_PROPERTY_PATH_DENY_PATTERNS      "path-deny-patterns"
#define ROOT_PROPERTY_PATH_ALLOW_PREFIXES     "path-allow-prefixes"
#define ROOT_PROPERTY_PATH_PREFIX             "path-prefix"
#define ROOT_PROPERTY_STAT_TIMEOUT            "stat-timeout"
#define ROOT_PROPERTY_READDIR_TIMEOUT         "readdir-timeout"
#define ROOT_PROPERTY_OPENFILE_TIMEOUT        "openfile-timeout"
#define ROOT_PROPERTY_ONE_FILE_SYSTEM         "one-file-system"
#define ROOT_PROPERTY_FOLLOW_SYMLINKS         "follow-symlinks"
#define ROOT_PROPERTY_WORM                    "worm"
#define ROOT_PROPERTY_IMMUTABLE               "immutable"
#define ROOT_PROPERTY_PRELOAD                 "preload"


const char *ARRAY_ROOT_PROPERTY[]={
  ROOT_PROPERTY_PATH_DENY_PATTERNS,
  ROOT_PROPERTY_PATH_ALLOW_PREFIXES,
  ROOT_PROPERTY_PATH_PREFIX,
  ROOT_PROPERTY_STAT_TIMEOUT,
  ROOT_PROPERTY_READDIR_TIMEOUT,
  ROOT_PROPERTY_OPENFILE_TIMEOUT,NULL};

const char *ARRAY_ROOT_PROPERTY_01[]={
  ROOT_PROPERTY_ONE_FILE_SYSTEM,
  ROOT_PROPERTY_FOLLOW_SYMLINKS,
  ROOT_PROPERTY_PRELOAD,NULL};



#define IS_VP_IN_ROOT_PFX(r,vipa) (r->pathpfx_l && r->pathpfx_l>=(vipa)->vp_l && cg_path_equals_or_is_parent((vipa)->vp,(vipa)->vp_l, r->pathpfx,r->pathpfx_l))

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



#define LOG_FUSE_RES(path,res)  IF_LOG_FLAG(LOG_FUSE_METHODS_ENTER)log_exited_function("%s res:%d",path,res)
//#define LOG_FUSE(path)log_entered_function("%s",path)
//#define LOG_FUSE_RES(path,res)log_exited_function("%s res:%d",path,res)

//
#define ADVISE_DIRCACHE_IS_ZIP              (1<<1)
#define ADVISE_DIRCACHE_IS_REMOTE           (1<<2)
#define ADVISE_DIRCACHE_IS_DIRPLAIN         (1<<3)
//
#define ADVISE_CACHE_IS_COMPRESSEDZIPENTRY           (1<<1) /* This  ZIP entry is compressed. */
#define ADVISE_CACHE_IS_SEEK_BW             (1<<2) /* There is currently an atempt to seek backward. This is inefficient for compressed ZIP entries. */
#define ADVISE_CACHE_BY_POLICY              (1<<3)
#define ADVISE_CACHE_IS_ZIPENTRY            (1<<4)
//
#if WITH_PRELOADRAM
struct preloadram{
  volatile enum enum_preloadram_status preloadram_status;
  textbuffer_t *txtbuf;
  zpath_t m_zpath; /* To try find_realpath_other_root() */
  volatile off_t preloadram_l,preloadram_already;
  int64_t preloadram_took_mseconds;
  int id;
};
#endif // WITH_PRELOADRAM


#define  PRELOADFILE_ROOT_UPDATE_TIME(d,r,success)   if (d || r) root_update_time(r?r:d->zpath.root,success?PTHREAD_PRELOAD:-PTHREAD_PRELOAD)

////////////////////
/// Directories  ///
////////////////////

//#define filler_add(filler,buf,name,st,no_dups) {if (ht_only_once(no_dups,name,0)){ filler(buf,name,st,0 COMMA_FILL_DIR_PLUS); cg_log_file_stat(name,st);}}
//#define filler_add(filler,buf,name,st,no_dups) {if (ht_only_once(no_dups,name,0)) filler(buf,name,st,0 COMMA_FILL_DIR_PLUS);}
#define stat_direct(...) _stat_direct(__VA_ARGS__,__func__)

#define log_entered_function_vp()  log_entered_function("%s",vp)
#define log_entered_function_vpath()  log_entered_function("%s",vpath)


#define LOG_ENTERED_VPATH() log_entered_function("%s",vpath)
#define exit_ZIPsFS() _exit_ZIPsFS(__func__,__LINE__);
