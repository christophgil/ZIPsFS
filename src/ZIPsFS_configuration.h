///////////////////////////////////
/// COMPILE_MAIN=ZIPsFS        ///
/// Customizations by the user  ///
///////////////////////////////////

/////////////////////////////////
/// Limitations of pathlength ///
/////////////////////////////////
#define ZPATH_STRGS 1024
/* cg_utils.c defines   MAX_PATHLEN */
/////////////////
/// Debugging ///
/////////////////
#define WITH_ASSERT_LOCK 0
#define WITH_PTHREAD_LOCK 1 /*  Should always be true. Only if suspect deadlock set to 0 */
#define ASSERT(...) (assert(__VA_ARGS__))  // Optional assertion



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/// With the following switches, optional features like caches can be (de)activated. Active: 1 Incactive: 0
/// Tey helped to identify the source of problems during software development.
/// They also help to identify non-essential optional parts in the source code.
/// To improve readablility, deactivated code can be grayed out in the IDE.
/// See hide-ifdef-mode of EMACS.
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT 0
// The caches of file attributes  grow  during run-time.
// The cache is a bundle of memory mapped files in ~/.ZIPsFS/. See cg_mstore*.c. The OS can use  disk space to save RAM.
// Eventually, the caches get cleared when the size exceeds the limit given by  DIRECTORY_CACHE_SIZE and  DIRECTORY_CACHE_SEGMENTS.
// Currently, DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT is off as cache clearing is not yet fully tested.


#define WITH_DIRCACHE 1
// This activates sorage of the ZIP file index and the file listing of rarely changing file directories in a cache.
// The cache key is the filepath of the ZIP file or directory  plus the last-modified file  attribute.

#define WITH_DIRCACHE_OPTIMIZE_NAMES 1
// When the  base name of the ZIP file  is part of  ZIP entry names, storage space can be saved.
// The base name is replaced by a specific symbol denoted here as "*" asterisk. Consider for example a Zipfile my_record_1234.Zip containing my_record_1234.wiff and
// my_record_1234.rawIdx and my_record_1234.raw.  Substitution of the ZIP file name "my_record_1234" by "*" results in  *.wiff and *.rawIdx and *.raw.
// After substitution, the file list will be shared by many ZIP files which allows efficient storing in the cache.

#define WITH_MEMCACHE 1
#define NUM_MEMCACHE_STORE_RETRY 2
// With WITH_MEMCACHE, the file content of selected ZIP entries is hold in RAM while the respective file handle exists.
// This facilitates non-sequential file reading.
// See man fseek
// The virtual file paths are selected with the function config_advise_cache_zipentry_in_ram(). Also see CLI parameter -c.
// Depending on the size, memory is reserved either with malloc() or with mmap().
// The memory address is kept int struct fhdata as long as the file is open.
// When several threads are accessing the same file, then only one instance of struct fhdata has a reference to the RAM area with the file content.
// This cache is used by all other instances of struct fhdata with the same file path.
// Upon close, such struct fhdata instance is kept alive as long as  there are still instances with the same path. Instead, it is marked for  deletion at a later time.



#define WITH_TRANSIENT_ZIPENTRY_CACHES 1
// This accelerates Bruker MS software. tdf and tdf_bin files are stored as .Zip files.
// While a tdf and tdf_bin file is open, thousands of redundant file system requests  occur.
// For each open tdf and tdf_bin file, a cache is temporarily created for these  redundant requests.
// Upon file close, these caches are disposed.

#define WITH_ZIPINLINE 1
#define WITH_ZIPINLINE_CACHE 1
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



#define WITH_STAT_CACHE 1
// Activate a cache for file attributes of files and ZIP entries.



////////////
/// Size ///
////////////
#define LOG2_ROOTS 3  // How many file systems are combined. Less is better because then constructed inodes is more efficient.
#define DIRECTORY_CACHE_SIZE (1L<<28) // Size of file of one cache segment for  directory listings file names and attributes.
#if DO_RESET_DIRCACHE_WHEN_EXCEED_LIMIT
#define DIRECTORY_CACHE_SEGMENTS 4 // Number of segments. If Exceeded, the directory cache is cleared and filled again.
#endif
#define MEMCACHE_READ_BYTES_NUM (16*1024*1024) // When storing zip entries in RAM, number of bytes read in one go
#define SIZE_CUTOFF_MMAP_vs_MALLOC 100000


/////////////////////////////////////
/// Periodic probing remote roots ///
/////////////////////////////////////

#define ROOT_WARN_STATFS_TOOK_LONG_SECONDS 3 /* Remote roots are probed periodically using statfs() */
#define ROOT_LAST_RESPONSE_MUST_BE_WITHIN_SECONDS 6 /* Roots which have rosponded within the last n seconds are used. */
#define ROOT_SKIP_UNLESS_RESPONDED_WITHIN_SECONDS 100 // Not responded within n seconds - skip this root

////////////
/// Time ///
////////////
#define STATQUEUE_TIMEOUT_SECONDS 10 // Give up waiting for stat() result
#define STATQUEUE_SLEEP_USECONDS 200 // Sleep microseconds after checking queue again
#define ROOT_OBSERVE_EVERY_MSECONDS_RESPONDING 1000 // Check availability of remote roots to prevent blocking.
#define WITH_STAT_SEPARATE_THREADS 1 /* Recommended value is 1. Then  stat() is called in another thread to avoid blocking */


/* File operations are performed within infininty loops to avoid blocking of the user threads. The infininty loops  might get blocked by a non-returning call to the file API */
#define UNBLOCK_AFTER_SECONDS_THREAD_DIRCACHE 100
#define UNBLOCK_AFTER_SECONDS_THREAD_STATQUEUE 20
#define UNBLOCK_AFTER_SECONDS_THREAD_MEMCACHE 300
#define UNBLOCK_AFTER_SECONDS_THREAD_RESPONDING 20

///////////////////////////////////
/// Dynamically generated files ///
///////////////////////////////////

#define WITH_AUTOGEN 1
#if WITH_AUTOGEN
#define AUTOGEN_DELETE_FILES_AFTER_DAYS "99"
#define AUTOGEN_MAX_DEPENDENCIES 5 /*  Dynamically generated file can depend on n input files.  Prevents runaway loop */
#endif
