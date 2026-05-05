/********************************/
/* COMPILE_MAIN=ZIPsFS          */
/* Customizations by the user   */
/********************************/


#define EXT_CONTENT  ".Content"                    /* ZIP files appear as folders. This is appended to form the folder name. */

///////////////////
/// Limitations ///
///////////////////
enum{
  ZPATH_STRGS=1024,   /* Buffer for file paths */
  MAX_NUM_OPEN_FILES=100000   /* Unless 0, used for setrlimit(RLIMIT_NOFILE,); */
};

/* cg_utils.c defines   MAX_PATHLEN */
/////////////////
/// Debugging ///
/////////////////
/* ---- */
#define WITH_DEBUG_THROTTLE_DOWNLOAD 0
#define WITH_ASSERT_LOCK     0 // Optional assertion
#define WITH_EXTRA_ASSERT    0 // Optional assertion
#define WITH_TESTING_REALLOC 0 // Smaller initial size, earlier realloc
#define WITH_TESTING_UNBLOCK 0
#define WITH_FOLLOW_SYMLINK  0 /* Or set  @follow-symlinks=0 per root.    Symlink dereference can be denied in config_allow_expand_symlink() */

/*******************************************************************************************************************************************************************/
/* Avoid blocking        0 deactivated    1 activated                                                                                                              */
/* Applies  only to rootpaths that starting with three slashs                                                                                                      */
/* Activate this feature if  upstream file systems suffer from becoming blocked/unresonsive                                                                        */
/* File access is performed in worker thread.  The current thread gives up after timeout.                                                                          */
/*                                                                                                                                                                 */
/* If a remote file system blocks then start the path with three slashes and activate the following:                                                               */
/* Disadvantage: ZIPsFS is less responsive.  Not yet fully tested.                                                                                                 */
/* Also see ASYNC_SLEEP_USECONDS                                                                                                                                   */
/*******************************************************************************************************************************************************************/
// /home/_cache/cgille/build/find-master/find.c  mnt/ | grep Mai

#define CURL_OPTS "--connect-timeout","30","--expect100-timeout","1800","--retry","3","--retry-delay","9",

#define WITH_TIMEOUT_STAT     0
#define WITH_TIMEOUT_READDIR  0
#define WITH_TIMEOUT_OPENFILE 0
#define WITH_TIMEOUT_OPENZIP  0
#define WITH_TIMEOUT_PRELOADFILE 0
#define WITH_CANCEL_BLOCKED_THREADS 0

////



enum{ASYNC_SLEEP_USECONDS=5000};    /* Sleep microseconds after checking again.  Low values increase idle CPU consumption.  Related: WITH_TIMEOUT_xxxx  */




/***********************************************************************************************************/
/* With the following switches, optional features like caches can be (de)activated. Active: 1 Incactive: 0 */
/* Tey helped to identify the source of problems during software development.                              */
/* They also help to identify non-essential optional parts in the source code.                             */
/* To improve readablility, deactivated code can be grayed out in the IDE.                                 */
/* See hide-ifdef-mode of EMACS.                                                                           */
/***********************************************************************************************************/


#define WITH_DIRCACHE 1
// This activates storage of the ZIP file index and the file listing of rarely changing file directories in a cache.
// The cache key is the filepath of the ZIP file or directory  plus the last-modified file  attribute.




#define WITH_TRANSIENT_ZIPENTRY_CACHES 1
// This accelerates Bruker MS software. tdf and tdf_bin files are stored as .Zip files.
// While a tdf and tdf_bin file is open, thousands of redundant file system requests  occur.
// For each open tdf and tdf_bin file, a cache is temporarily created for these  redundant requests.
// Upon file close, these caches are disposed.



//enum{RETRY_ZIP_FREAD=1}; /* If >1 then repeat zip_fread on failure */



/*********************************************************************************************************************************/
/* Normally, ZIP files are shown as folders with ZIP entries being the contained files.                                          */
/* This does not fit together with the organization of Sciex mass-spec experiments.                                              */
/* Measurements are recorded as a set of files with the same base name and different extensions,                               */
/* We store this set of files as a zip file with the respective base name.                                                       */
/* To show the native folder structure, the zip entries are  shown directly in the parent folder "inline".                              */
/*********************************************************************************************************************************/
#define WITH_ZIPFLAT 1
#define WITH_ZIPFLATCACHE 1 /* When listing directory contents, cache the mapping virtual path to zip file.  This facilitates subsequent identification of zip files for virtual paths. */
#define WITH_STATCACHE 1 /* DEBUG_NOW  Activate a cache for file attributes */
#define WITH_EVICT_FROM_PAGECACHE 1 // See Not for MacOSX


////////////
/// Size ///
////////////
enum{
  LOG2_FILESYSTEMS=4,  // How many file systems are combined. Less is better because then constructed inodes is more efficient.
  ROOTS=32,
  DIRECTORY_CACHE_SIZE=16*1024*1024,  // Size of file of one cache segment for  directory listings file names and attributes.
  PRELOADRAM_READ_BYTES_NUM=16*1024*1024, // When storing zip entries in RAM, number of bytes read in one go
  THRESHOLD_MALLOC_MMAP=128*1024  // Size (bytes) below -  malloc above - mmap
};
/////////////////////////////////////
/// Periodic probing remote roots ///
/////////////////////////////////////






/////////////
/// Times ///
/////////////


#define WITH_TESTING_TIMEOUTS 0
#if WITH_TESTING_TIMEOUTS
enum{
  PROBE_PATH_RESPONSE_TTL_SECONDS=2,
  PROBE_PATH_TIMEOUT_SECONDS=10,
  STAT_TIMEOUT_SECONDS=1000,
  READDIR_TIMEOUT_SECONDS=100,
  OPENFILE_TIMEOUT_SECONDS=100,
  OPENZIP_TIMEOUT_SECONDS=50,
  NUM_PRELOADRAM_STORE_RETRY=1,
 PRELOADFILE_TIMEOUT_SECONDS=4
};



#else
enum{
  NUM_PRELOADRAM_STORE_RETRY=2,
  PROBE_PATH_RESPONSE_TTL_SECONDS=9,   /* Root paths which have responded within that time are considered active. */
  PROBE_PATH_TIMEOUT_SECONDS=30,   /* Give up waiting for this root path after this time. */
  STAT_TIMEOUT_SECONDS=30, // Give up waiting for stat() result
  READDIR_TIMEOUT_SECONDS=30, // Give up waiting for opendir()  and readdir()
  OPENFILE_TIMEOUT_SECONDS=30,
  OPENZIP_TIMEOUT_SECONDS=30,
  PRELOADFILE_TIMEOUT_SECONDS=30
};


#endif






////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// The worker threads are observed and if no activity for the specified amount of time, they are assumed to be blocked. ///
/// Kill blocked worker thread and re-launch new instance                                                                ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_CANCEL_BLOCKED_THREADS
#if WITH_TESTING_UNBLOCK
enum{UNBLOCK_AFTER_SECONDS_THREAD_ASYNC=30, UNBLOCK_AFTER_SECONDS_THREAD_PRELOADRAM=300};
#else
enum{UNBLOCK_AFTER_SECONDS_THREAD_ASYNC=300, UNBLOCK_AFTER_SECONDS_THREAD_PRELOADRAM=300};
#endif
#endif // WITH_CANCEL_BLOCKED_THREADS




#define WITH_PRELOADRAM 1  /* The file content is hold in RAM while the respective file handle exists to facilitates non-sequential file reading. See fHandle_t */


///////////////////////////////////
/// Dynamically generated files ///
///////////////////////////////////
#define WITH_FILECONVERSION    1 /* Generate files based on rules of file extensions */
#define WITH_CCODE             1 /* Generate files by C-code */
#define WITH_INTERNET_DOWNLOAD 1 /* Access to internet files like <mount-point>/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README */
#define WITH_PRELOADDISK       1 /* Files are preloadoed to fst branch for root paths preceded by --preload at CLI. Or for  virtual paths starting with /ZIPsFS/lr/, /ZIPsFS/lrc/, /ZIPsFS/lrz/. */
#define WITH_PRELOADDISK_DECOMPRESS 1



#define WITH_FILECONVERSION_HIDDEN 0  /* Hide the directory c in ZIPsFS to avoid recursive searches. However, it breaks export to Windows */
#if 0
#warning "Going to deactivate all caches for testing"
#undef WITH_DIRCACHE
#define WITH_DIRCACHE 0
#undef WITH_PRELOADRAM
#define WITH_PRELOADRAM 0
#undef WITH_TRANSIENT_ZIPENTRY_CACHES
#define WITH_TRANSIENT_ZIPENTRY_CACHES 0
#undef WITH_STATCACHE
#define WITH_STATCACHE 0
#undef WITH_ZIPFLATCACHE
#define WITH_ZIPFLATCACHE 0

#endif




////////////////////////////////////////////////////////////////////////////////////
/// Frozen threads are killed and restarted                                      ///
/// Requires that threads have a different gettid() != getpid()                  ///
/// and that existence of a thread can be tested with kill() as follows:         ///
///  pid_t pid=gettid();  .... if (0==kill(pid,0)) { Thread still  running }     ///
/// This is the case at least for LINUX                                          ///
////////////////////////////////////////////////////////////////////////////////////
//#define WITH_CANCEL_BLOCKED_THREADS IS_LINUX





/******************************/
/* Clearing the cache         */
/* Not yet fully implemented. */
/* Leave deactivated          */
/******************************/
#define WITH_CLEAR_CACHE 0
#define WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT 0 // Better not yet. Needs more testing!
// The caches of file attributes  grow  during run-time.
// The cache is a bundle of memory mapped files in ~/.ZIPsFS/. See cg_mstore*.c. The OS can use  disk space to save RAM.
// Eventually, the caches get cleared when the size exceeds the limit given by  DIRECTORY_CACHE_SIZE and  NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE.
// Currently, WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT is off as cache clearing is not yet fully tested.



enum {NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE=16}; // Maximum number of blocks. If Exceeded, the directory cache is cleared and filled again.
