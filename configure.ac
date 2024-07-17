#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([0.1])
AC_INIT([ZIPsFS], [2023-08-01], [christophgil@gmail.com])
AM_INIT_AUTOMAKE
AC_PROG_CC


AC_CONFIG_SRCDIR([src/ZIPsFS.c])
AC_CONFIG_HEADERS([src/config.h])

AC_CONFIG_FILES([src/Makefile])
AC_SEARCH_LIBS([sqrt], [m])

# Checks for header files.
AC_CHECK_HEADERS([stdlib.h string.h fcntl.h limits.h sys/statvfs.h unistd.h fuse.h zip.h  ctype.h errno.h grp.h locale.h math.h pthread.h stdbool.h sys/mount.h sys/param.h sys/stat.h sys/syscall.h sys/time.h sys/types.h sys/wait.h time.h utime.h])


# Check for FUSE development environment
PKG_CHECK_MODULES(FUSE,fuse3)
# PKG_CHECK_MODULES(FUSE,fuse)

PKG_CHECK_MODULES(LIBZIP,libzip)


#AC_SEARCH_LIBS([fuse_main], [fuse fuse3], [], [AC_MSG_ERROR([unable to find the fuse_main() function])  ])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UID_T
AC_TYPE_MODE_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_TYPE_UINT64_T

# Checks for library functions.
AC_FUNC_CHOWN
AC_FUNC_LSTAT_FOLLOWS_SLASHED_SYMLINK
AC_FUNC_MALLOC
AC_CHECK_FUNCS([ftruncate mkdir realpath rmdir strerror utime])


AC_OUTPUT
