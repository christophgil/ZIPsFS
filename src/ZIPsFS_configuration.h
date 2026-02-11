/********************************/
/* COMPILE_MAIN=ZIPsFS          */
/* Customizations by the user   */
/********************************/


#define EXT_CONTENT  ".Content"                    /* ZIP files appear as folders. This is appended to form the folder name. */

///////////////////
/// Limitations ///
///////////////////
#define ZPATH_STRGS 1024   /* Buffer for file paths */
#define MAX_NUM_OPEN_FILES 100000   /* Unless 0, used for setrlimit(RLIMIT_NOFILE,); */


/* cg_utils.c defines   MAX_PATHLEN */
/////////////////
/// Debugging ///
/////////////////
/* ---- */
#define DEBUG_THROTTLE_DOWNLOAD 0
#define WITH_ASSERT_LOCK     0 // Optional assertion
#define WITH_EXTRA_ASSERT    0 // Optional assertion
#define WITH_TESTING_REALLOC 0 // Smaller initial size, earlier realloc
#define WITH_TESTING_UNBLOCK 0

 /******************************************************************************************************************************************************************/
 /* Avoid blocking        0 deactivated    1 activated        Applies  to rootpaths that starting with three slashs                                                */
 /* Activate this feature if  upstream file systems suffer from becoming blocked/unresonsive                                                                       */
 /* File access is performed in worker thread.  The current thread gives up after timeout.                                                                         */
 /*                                                                                                                                                                */
 /* If a remote file system blocks then start the path with three slashes and activate the following:                                                              */
 /* Disadvantage: ZIPsFS is less responsive.  Not yet fully tested.                                                                                                */
 /* Also see ASYNC_SLEEP_USECONDS                                                                                                                                  */
 /******************************************************************************************************************************************************************/
// /home/_cache/cgille/build/find-master/find.c  mnt/ | grep Mai

#define CURL_OPTS "--connect-timeout","30","--expect100-timeout","1800","--retry","3","--retry-delay","9",

#define WITH_TIMEOUT_STAT     0
#define WITH_TIMEOUT_READDIR  0
#define WITH_TIMEOUT_OPENFILE 0
#define WITH_TIMEOUT_OPENZIP  0
#define WITH_CANCEL_BLOCKED_THREADS 0

////



#define ASYNC_SLEEP_USECONDS 5000    /* Sleep microseconds after checking again.  Low values increase idle CPU consumption.  Related: WITH_TIMEOUT_xxxx  */




/***********************************************************************************************************/
/* With the following switches, optional features like caches can be (de)activated. Active: 1 Incactive: 0 */
/* Tey helped to identify the source of problems during software development.                              */
/* They also help to identify non-essential optional parts in the source code.                             */
/* To improve readablility, deactivated code can be grayed out in the IDE.                                 */
/* See hide-ifdef-mode of EMACS.                                                                           */
/***********************************************************************************************************/

#define WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT 0 // Better not yet. Needs more testing!
// The caches of file attributes  grow  during run-time.
// The cache is a bundle of memory mapped files in ~/.ZIPsFS/. See cg_mstore*.c. The OS can use  disk space to save RAM.
// Eventually, the caches get cleared when the size exceeds the limit given by  DIRECTORY_CACHE_SIZE and  NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE.
// Currently, WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT is off as cache clearing is not yet fully tested.


#define WITH_DIRCACHE 1
// This activates storage of the ZIP file index and the file listing of rarely changing file directories in a cache.
// The cache key is the filepath of the ZIP file or directory  plus the last-modified file  attribute.

#define WITH_PRELOADRAM 1
#define NUM_PRELOADRAM_STORE_RETRY 2
// With WITH_PRELOADRAM, the file content of selected ZIP entries is hold in RAM while the respective file handle exists.
// This is also required for WITH_INTERNET_DOWNLOAD WITH_FILECONVERSION and WITH_PRELOADRAM
// This facilitates non-sequential file reading.
// See man fseek
// The virtual file paths are selected with the function config_advise_cache_zipentry_in_ram(). Also see CLI parameter -c.
// Depending on the size, memory is reserved either with MALLOC() or with MMAP().
// The memory address is kept int fHandle_t as long as the file is open.
// When several threads are accessing the same file, then only one instance of fHandle_t has a reference to the RAM area with the file content.
// This cache is used by all other instances of fHandle_t with the same file path.
// Upon close, such fHandle_t instance is kept alive as long as  there are still instances with the same path. Instead, it is marked for  deletion at a later time.



#define WITH_TRANSIENT_ZIPENTRY_CACHES 1
// This accelerates Bruker MS software. tdf and tdf_bin files are stored as .Zip files.
// While a tdf and tdf_bin file is open, thousands of redundant file system requests  occur.
// For each open tdf and tdf_bin file, a cache is temporarily created for these  redundant requests.
// Upon file close, these caches are disposed.



#define RETRY_ZIP_FREAD 2
// Repeat zip_fread on failure

#define WITH_ZIPINLINE 1
#define WITH_ZIPINLINE_CACHE 1 /* Caches mapping virtual path to zip file */
// Normally, ZIP files are shown as folders with ZIP entries being the contained files.
// This is consistent for Bruker and Agilent mass spectrometry files.
// However,  Sciex and ThermoFisher are organized differently.
// The file bundle of MS records are not organized in folders.
// The files of one MS record share the constant  base name and have different file extensions.
// In case of ThermoFisher, there is only one file.
// Files from different records are displayed flat in the respective project directory.
// With WITH_ZIPINLINE activated, this flat directory structure can be generated from archived ZIP files.
// The entries of the respective ZIP files are inlined directly into the file listing without a containing folder.
// See config_containing_zipfile_of_virtual_file() and config_skip_zipfile_show_zipentries_instead().



#define WITH_STAT_CACHE 1 /* Activate a cache for file attributes */


#define WITH_EVICT_FROM_PAGECACHE 1 // See Not for MacOSX


////////////
/// Size ///
////////////
#define LOG2_FILESYSTEMS 4  // How many file systems are combined. Less is better because then constructed inodes is more efficient.
#define ROOTS 32
#define DIRECTORY_CACHE_SIZE (1L<<24) // Size of file of one cache segment for  directory listings file names and attributes.
#if WITH_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
#define NUM_BLOCKS_FOR_CLEAR_DIRECTORY_CACHE 16 // Maximum number of blocks. If Exceeded, the directory cache is cleared and filled again.
#endif
#define PRELOADRAM_READ_BYTES_NUM (16*1024*1024) // When storing zip entries in RAM, number of bytes read in one go
#define SIZE_CUTOFF_MMAP_vs_MALLOC (1<<16)

/////////////////////////////////////
/// Periodic probing remote roots ///
/////////////////////////////////////






/////////////
/// Times ///
/////////////


#define WITH_TESTING_TIMEOUTS 0
#if WITH_TESTING_TIMEOUTS
#define ROOT_RESPONSE_SECONDS 2
#define ROOT_RESPONSE_TIMEOUT_SECONDS 10
#define STAT_TIMEOUT_SECONDS      1000
#define READDIR_TIMEOUT_SECONDS  100
#define OPENFILE_TIMEOUT_SECONDS 100
#define OPENZIP_TIMEOUT_SECONDS  50
#undef NUM_PRELOADRAM_STORE_RETRY
#define NUM_PRELOADRAM_STORE_RETRY 1
#define PRELOADFILE_TIMEOUT_SECONDS 2
#else
#define ROOT_RESPONSE_SECONDS 9   /* Roots which have responded within that time are used. */
#define ROOT_RESPONSE_TIMEOUT_SECONDS 30     /* Otherwise wait until ROOT_RESPONSE_SECONDS is reached and give up waiting. */
#define STAT_TIMEOUT_SECONDS     30 // Give up waiting for stat() result
#define READDIR_TIMEOUT_SECONDS  30 // Give up waiting for opendir()  and readdir()
#define OPENFILE_TIMEOUT_SECONDS 30
#define OPENZIP_TIMEOUT_SECONDS  30
#define PRELOADFILE_TIMEOUT_SECONDS 30
#endif






////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// The worker threads are observed and if no activity for the specified amount of time, they are assumed to be blocked. ///
/// Kill blocked worker thread and re-launch new instance                                                                ///
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#if WITH_CANCEL_BLOCKED_THREADS
#if WITH_TESTING_UNBLOCK
#define UNBLOCK_AFTER_SECONDS_THREAD_ASYNC      30
#define UNBLOCK_AFTER_SECONDS_THREAD_PRELOADRAM   300
#else
#define UNBLOCK_AFTER_SECONDS_THREAD_ASYNC      300
#define UNBLOCK_AFTER_SECONDS_THREAD_PRELOADRAM   300
#endif
#endif // WITH_CANCEL_BLOCKED_THREADS




///////////////////////////////////
/// Dynamically generated files ///
///////////////////////////////////
#define WITH_FILECONVERSION           1 /* Generate files based on rules of file extensions */
#define WITH_CCODE             1 /* Generate files by C-code */
#define WITH_INTERNET_DOWNLOAD 1 /* Access to internet files like <mount-point>/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README */
#define WITH_PRELOADDISK   1 /* Files are preloadoed to fst branch for root paths preceded by --preload at CLI. Or for  virtual paths starting with /ZIPsFS/lr/, /ZIPsFS/lrc/, /ZIPsFS/lrz/. */
#define WITH_PRELOADDISK_DECOMPRESS 1



#define WITH_FILECONVERSION_HIDDEN 0  /* Hide the directory a in ZIPsFS to avoid recursive searches. However, it breaks export to Windows */
#if 0
#warning "Going to deactivate all caches for testing"
#undef WITH_DIRCACHE
#define WITH_DIRCACHE 0
#undef WITH_PRELOADRAM
#define WITH_PRELOADRAM 0
#undef WITH_TRANSIENT_ZIPENTRY_CACHES
#define WITH_TRANSIENT_ZIPENTRY_CACHES 0
#undef WITH_STAT_CACHE
#define WITH_STAT_CACHE 0
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0

#endif




////////////////////////////////////////////////////////////////////////////////////
/// Frozen threads are killed and restarted                                      ///
/// Requires that threads have a different gettid() != getpid()                  ///
/// and that existence of a thread can be tested with kill() as follows:         ///
///  pid_t pid=gettid();  .... if (0==kill(pid,0)) { Thread still  running }     ///
/// This is the case at least for LINUX                                          ///
////////////////////////////////////////////////////////////////////////////////////
//#define WITH_CANCEL_BLOCKED_THREADS IS_LINUX
