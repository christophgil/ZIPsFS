/*
_GNU_SOURCE is the only one you should ever define yourself.
 __USE_GNU is defined internally through a mechanism in features.h (which is included by all other glibc headers) when _GNU_SOURCE is defined, and possibly under other conditions.
 Defining or undefining __USE_GNU yourself will badly break the glibc headers.


*/
#ifndef _cg_os_dependencies_dot_h
#define _cg_os_dependencies_dot_h


#define HAS_FUSE_LSEEK 1
#ifdef __USE_GNU
#define WITH_GNU 1
#else
#define WITH_GNU 0
#endif


#if  __clang__
#  define IS_CLANG 1
#else
#  define IS_CLANG 0
#endif // __clang__


#ifdef __linux__
#  define IS_LINUX 1
#else
#  define IS_LINUX 0
#endif // __linux__


#ifdef __APPLE__
#  define IS_APPLE 1
#  define IS_NOT_APPLE 0
#  define ST_MTIMESPEC st_mtimespec
#undef HAS_FUSE_LSEEK
#define HAS_FUSE_LSEEK 0
#else
#  define IS_APPLE 0
#  define IS_NOT_APPLE 1
#  define ST_MTIMESPEC st_mtim
#endif

#ifdef __FreeBSD__
#  define IS_FREEBSD 1
#else
#  define IS_FREEBSD 0
#endif

#ifdef __OpenBSD__
#  define IS_OPENBSD 1
#else
#  define IS_OPENBSD 0
#endif



#ifdef __NetBSD__
#  define IS_NETBSD 1
#define HAS_UNDERSCORE_ENVIRON 0
#undef HAS_FUSE_LSEEK
#define HAS_FUSE_LSEEK 0
#else
#  define IS_NETBSD 0
#define HAS_UNDERSCORE_ENVIRON 1
#endif // __NetBSD__


#if IS_LINUX || IS_FREEBSD
#define IS_ORIG_FUSE 1
#else
#define IS_ORIG_FUSE 0
#endif


#if IS_NETBSD || IS_APPLE
#define HAS_BACKTRACE 0
#else
#define HAS_BACKTRACE 1
#endif


#if IS_LINUX || IS_FREEBSD || IS_NETBSD
#  define _XOPEN_SOURCE 700  /* Standard  X/Open 7, incorporating POSIX 2017   For pread()/pwrite()/utimensat() */
#endif

#endif // _cg_os_dependencies_dot_h
