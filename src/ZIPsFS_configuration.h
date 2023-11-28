/////////////////
/// Debugging ///
/////////////////
#define DO_ASSERT_LOCK 0




/////////////////////////////////////////////////////////////////////////////////////
/// The following optional caches can be deactivated. Active: 1 Incactive: 0.     ///
/// This may help to identify program errors.
/// These caches improve the performance of ZIPsFS.                               ///
/////////////////////////////////////////////////////////////////////////////////////

#define IS_DIRCACHE 1
// The table of content of ZIP files is stored. The key is the filename and the mtime attribute.
// The table of content (toc) of ZIP files is stored in memory mapped files. The data structure is struct mstore.
// We created our own  storage  struct mstore. It is basically an array of memory mapped files.
// A cached toc is valid until the last-modified attribute changes.
// We reduce memory requirement of tocs as follows:
// Zip-entries where the ZIP-file name is part of the name (after removing all trailing dot-suffix) are abbreviated using a placeholder for the name..
// After this simplification,  many of our ZIP files have identical tocs. Consequently only reference to a previously stored toc is required.
// The dircache has a maximum capacity given in ZIPsFS_configuration.c. If exceeded, the entire cache is cleared and filling starts again.


#define IS_DIRCACHE_OPTIMIZE_NAMES 1
// When the ZIP file base name is part of the ZIP entry names, it is replaced by a specific
// symbol denoted here as * asterisk. Consider for example a Zipfile say my_record.Zip containing my_record.wiff and
// my_record.rawIdx and my_record.raw.  Substitution of the ZIP file name "my_record" by "*" results in  *.wiff and *.rawIdx and *.raw.
// This file list can be stored much more efficiently. Since many  ZIP files will have the same list
// of entries after  substitution,  only one instance of the entry list needs to be stored and can be referenced multiple times.



#define IS_MEMCACHE 1
// The content of ZIP entries is hold in RAM. This is important those that are not read sequentially.
// See the CLI parameter -c.
// This cache stores the file content of ZIP files in RAM. Depending on the size, memory is reserved either with malloc() or with mmap().
// The memory address is kept int struct fhdata. When several threads are accessing the same file, then
// only one instance of struct fhdata has a cache. It is used by all other instances with the same file path.
// Upon close, such struct fhdata instance is not deleted when there are still instances with the same path. Instead, it is marked to be deleted later.


#define IS_STATCACHE_IN_FHDATA 1
// This accelerates Bruker MS software. tdf and tdf_bin files are stored as .Zip files.
// While a tdf and tdf_bin file is open, thousands of file system requests for related files occur.
// The data associated to the open file pointer has a slot for those caches.
// This cache data is freed automatically when the tdf or tdf_bin file is closed.

#define IS_ZIPINLINE_CACHE 1
// Normally, ZIP files are shown as folders with ZIP entries being the contained files.
// However for mass spectrometry data of the company Sciex and ThermoFisher, this would the resemble the native arrangement of files.
// Instead, the entries of all ZIP files in the folder need to be displayed in the virtual file  folder.
// See config_zipentry_to_zipfile() and config_zipentries_instead_of_zipfile().
// Since the files in the virtual path originate from multiple ZIP entries, the TOC of all these ZIP files are read and
// stored in the cache. When the file listing is requested a second time, it is displayed much faster.


#define IS_STAT_CACHE 1
// This is a cache for file attributes for file path.
// When loading Brukertimstof mass spectrometry files with the vendor DLL,
// thousands and millions of redundant requests to the file system occur.


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
