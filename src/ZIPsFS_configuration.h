/////////////////
/// Debugging ///
/////////////////
#define DO_ASSERT_LOCK 0
/////////////////////////////////////////////////////////////////////////////////////
/// The following optional optimazations can be deactivated with 0 for debugging. ///
/// ZIPsFS will be slower ins certain instances.                                  ///
/////////////////////////////////////////////////////////////////////////////////////
#define IS_DIRCACHE 1
#define IS_DIRCACHE_OPTIMIZE_NAMES 1
#define IS_MEMCACHE 1
#define IS_STATCACHE_IN_FHDATA 1
#define IS_ZIPINLINE_CACHE 1
#define IS_STAT_CACHE 1
#define DO_REMEMBER_NOT_EXISTS 1
/* --- */

////////////
/// Size ///
////////////
#define LOG2_ROOTS 5  // How many file systems are combined
#define DIRECTORY_CACHE_SIZE (1L<<28) // Size of file to cache directory listings
#define DIRECTORY_CACHE_SEGMENTS 4 // Number of files to cache directory listings. If Exceeded, the directory cache is cleared and filled again.
//#define DIRECTORY_CACHE_SIZE (1L<<16)
//#define DIRECTORY_CACHE_SEGMENTS 1

#define MEMCACHE_READ_BYTES_NUM 16*1024*1024 // When storing zip entries in RAM, number of bytes read in one go
////////////
/// Time ///
////////////
#define STATQUEUE_TIMEOUT_SECONDS 3
#define ROOT_OBSERVE_EVERY_SECONDS 0.3 // Check availability of remote roots to prevent blocking.
#define ROOT_OBSERVE_TIMEOUT_SECONDS 30 // Skip non-responding
#define UNBLOCK_AFTER_SEC_THREAD_STATQUEUE 12
#define UNBLOCK_AFTER_SEC_THREAD_MEMCACHE 600
#define UNBLOCK_AFTER_SEC_THREAD_DIRCACHE 600
