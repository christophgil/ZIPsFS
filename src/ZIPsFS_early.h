//////////////////////////////////////////////
/// COMPILE_MAIN=ZIPsFS                    ///
/// Override from ZIPsFS_settings.h        ///
/// Some settings depend on others         ///
//////////////////////////////////////////////




/* WITH_TESTING_TIMEOUTS */
#if WITH_TESTING_TIMEOUTS
#undef WITH_DIRCACHE
#define WITH_DIRCACHE 0
//
#undef WITH_STAT_CACHE
#define WITH_STAT_CACHE 0
//
#undef  WITH_ASSERT_LOCK
#define WITH_ASSERT_LOCK  0
//
#define M(time) _Static_assert(time>1,"");
#else
#define M(time) _Static_assert(time>3,"");
#endif //WITH_TESTING_TIMEOUTS

M(ROOT_RESPONSE_SECONDS);
M(STAT_TIMEOUT_SECONDS);
M(READDIR_TIMEOUT_SECONDS);
M(OPENFILE_TIMEOUT_SECONDS);
M(OPENZIP_TIMEOUT_SECONDS);
M(STAT_TIMEOUT_SECONDS);
M(READDIR_TIMEOUT_SECONDS);
M(OPENFILE_TIMEOUT_SECONDS);
M(OPENZIP_TIMEOUT_SECONDS);
#if WITH_CANCEL_BLOCKED_THREADS
M(UNBLOCK_AFTER_SECONDS_THREAD_ASYNC);
M(UNBLOCK_AFTER_SECONDS_THREAD_PRELOADRAM);
_Static_assert(ASYNC_SLEEP_USECONDS*4<1000*1000*UNBLOCK_AFTER_SECONDS_THREAD_ASYNC,"");
#endif //WITH_CANCEL_BLOCKED_THREADS
#if PRELOADFILE_TIMEOUT_SECONDS
M(PRELOADFILE_TIMEOUT_SECONDS);
#endif //PRELOADFILE_TIMEOUT_SECONDS
#undef M
/* End WITH_TESTING_TIMEOUTS */

/* assertions */
_Static_assert(ROOT_RESPONSE_TIMEOUT_SECONDS>ROOT_RESPONSE_SECONDS,"");
_Static_assert(ASYNC_SLEEP_USECONDS*4<1000*1000*ROOT_RESPONSE_SECONDS,"");
///
#undef ASSERT
#if WITH_EXTRA_ASSERT
// cppcheck-suppress-macro nullPointerRedundantCheck
#define ASSERT(...) (assert(__VA_ARGS__))
#define ASSERT_PRINT(...) ASSERT(__VA_ARGS__);log_verbose(ANSI_FG_GREEN"%s"ANSI_RESET,#__VA_ARGS__)
#else
#define ASSERT(...)
#define ASSERT_PRINT(...)
#endif
///
/* Some settings depend on others */
#if !WITH_DIRCACHE
#undef WITH_ZIPINLINE_CACHE
#define WITH_ZIPINLINE_CACHE 0
#endif //!WITH_DIRCACHE
///

#if !WITH_PRELOADDISK
      #undef WITH_PRELOADDISK_DECOMPRESS
      #define WITH_PRELOADDISK_DECOMPRESS 0
#endif //WITH_PRELOADDISK_DECOMPRESS
///

#if WITH_PRELOADRAM
#define PRINTINFO(...)   n+=snprintf(_info_sprint_address(n),_info_sprint_max(n),__VA_ARGS__)
#else
/* WITH_FILECONVERSION depends on WITH_PRELOADRAM */
#undef WITH_FILECONVERSION
#define WITH_FILECONVERSION 0
#endif // WITH_PRELOADRAM
///


#define DEBUG_DIRCACHE_COMPARE_CACHED 0 /*TO_HEADER*/
#define DEBUG_TRACK_FALSE_GETATTR_ERRORS 0




/*********************************************************************************************************************************************************************/
/* When the  base name of the ZIP file  is part of  ZIP entry names, storage space can be saved.                                                                     */
/* The base name is replaced by a specific symbol denoted here as "*" asterisk. Consider for example a Zipfile my_record_1234.Zip containing my_record_1234.wiff and */
/* my_record_1234.rawIdx and my_record_1234.raw.  Substitution of the ZIP file name "my_record_1234" by "*" results in  *.wiff and *.rawIdx and *.raw.               */
/* After substitution, the file list will be shared by many ZIP files which allows efficient storing in the cache.                                                   */
/*********************************************************************************************************************************************************************/
#define PLACEHOLDER_NAME '*'
#define WITH_ZIPENTRY_PLACEHOLDER 1
#define WITH_CLEAR_CACHE 1
