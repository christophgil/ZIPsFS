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

M(ROOT_RESPONSE_WITHIN_SECONDS);
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
M(UNBLOCK_AFTER_SECONDS_THREAD_MEMCACHE);
_Static_assert(ASYNC_SLEEP_USECONDS*4<1000*1000*UNBLOCK_AFTER_SECONDS_THREAD_ASYNC,"");
#endif //WITH_CANCEL_BLOCKED_THREADS
M(MEMCACHE_TIMEOUT_SECONDS);
#undef M
/* End WITH_TESTING_TIMEOUTS */

/* assertions */
_Static_assert(ROOT_GIVEUP_AFTER_SECONDS>ROOT_RESPONSE_WITHIN_SECONDS,"");
_Static_assert(ASYNC_SLEEP_USECONDS*4<1000*1000*ROOT_RESPONSE_WITHIN_SECONDS,"");
///
#undef ASSERT
#if WITH_EXTRA_ASSERT
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
/* WITH_AUTOGEN depends on WITH_MEMCACHE */
#if ! WITH_MEMCACHE
#undef WITH_AUTOGEN
#define WITH_AUTOGEN 0
#endif // WITH_MEMCACHE
///
