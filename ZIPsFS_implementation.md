# Dependencies

 - libfuse
 - libzip
 - Gnu-C
 - UNIX like OS.

For OS other than LINUX, some minor adaptions will be required.

# Language

ZIPsFS is written in Gnu-C because libfuse is.
Using other languages  may affect performance at the native interface.
This is particularly the case when data blocks need to be copied.
Dynamic linking of native libraries may be worse than static linking. In Java JNA is likely to be much slower than JNI.

Playing with Java and Python, it seemed that the parameter
<I>struct fuse_file_info *fi</I> cannot be used as a key to find cached data.

Python is lacking a method </I>read(buffer,offset,size)</I>. Instead, a new buffer object is created whenever a new 128k block of data is read. These many blocks need to be garbage collected.

For Therefore reasons  we decided to use GNU-C despite its many draw-backs.

# Roots

When a client program requests a file from the virtual FUSE file system, the virtual file path needs to be translated into an existing real file or ZIP entry.

All given roots are iterated  until the file or ZIP entry  is found.
The first root is the only that allows file modification and file creation.
All others are read only.

# Caches

There are several caches to improve performance. For clarifications and distinction,
functions and variables are prefixed <I>memcache_</I>, <I>dircache_<I> and <I>statcache_</I>.
Caches can be deactivated for debugging by setting the <I>IS_....</I> variables to zero.

 - <I>IS_MEMCACHE_PUT /_GET</I>: The content of ZIP entries is hold in RAM. This is important those that are not read sequentially. See  config_store_zipentry_in_memcache() and the CLI parameter <I>-c</I>.
 - <I>IS_DIRCACHE_PUT /_GET<I>: The table of content of ZIP files is stored. The key is the filename and the mtime attribute.
n all file tree roots need to be probed. This can be cached config_zipentry_to_zipfile().
 - <I>IS_STAT_CACHE</I>: See config_file_attribute_valid_seconds(). This is a cache for file attributes for file path.
 - <I>IS_STATCACHE_IN_FHDATA</I>: This accelerates Bruker MS software. While a tdf and tdf_bin file is open, lots of file system requests for related files occur. The data of the open file pointer has a slot for those caches. The cached data disappears after the tdf or tdf_bin file is closed.
 - <I>IS_ZIPINLINE_CACHE</I>: ZIPsFS can inline the content of ZIP files in file listings. See config_zipentry_to_zipfile() and config_zipentries_instead_of_zipfile(). Given a virtual path, different ZIP paths i

## Memcache

This cache stores the file content of ZIP files in RAM. Depending on the size, memory is reserved either with <I>malloc()</I> or with <I>mmap()</I>.
The memory address is kept int <I>struct fhdata</I>. When several threads are accessing the same file, then
only one instance of <I>struct fhdata</I> has a cache. It is used by all other instances with the same file path.
Upon close, such <I>struct fhdata</I> instance is not deleted when there are still instances with the same path. Instead, it is marked to be deleted later.

## Dircache

The table of content (toc) of ZIP files is stored in memory mapped files. The data structure is <I>struct mstore</I>.
We created our own  storage  <I>struct mstore</I>. It is basically an array of memory mapped files.

A cached toc is valid until the last-modified attribute changes.

We reduce memory requirement of tocs as follows:
Zip-entries where the ZIP-file name is part of the name (after removing all trailing dot-suffix) are abbreviated using a placeholder for the name..
After this simplification,  many of our ZIP files have identical tocs. Consequently only reference to a previously stored toc is required.

The dircache has a maximum capacity given in ZIPsFS_configuration.c. If exceeded, the entire cache is cleared and filling starts again.

## statcache

When loading Brukertimstof mass spectrometry files with the vendor DLL,
thousands and millions of redundant requests to the file system are issued.
Instances of  <I>struct fhdata</I> can hold a record of <I>struct stat</I> for all sub-paths during its life span.

## The file cache of the OS
After closing a file or disposing the memcache of a ZIP entry, a call to  <I>posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);</I> may remove the file from the
file cache of the OS when it is unlikely that the same file will be used in near future.  This can be  customized in <I>ZIPsFS_configuration.c</I>.

## The cache in libfuse

See  field <I>struct fuse_file_info-&gt;keep_cache</I> in function <I>xmp_open()</I>.

# Infinite loops running in own threads

- infloop_statqueue(): Calling stat asynchronously. Calling statfs() periodically to see whether the respective root is responding.
- infloop_memcache(): Loading entire ZIP entries into RAM asynchronously. One thread per root.
- infloop_dircache(): Reading directory listings and ZIP file entry listings asynchronously. One thread per root.
- infloop_unblock():  Unblock blocked threads by calling <i>pthread_cancel()</i>. They re-start automatically with a <i>pthread_cleanup_push()</i> hook. One global thread.

# When a source file system becomes unavailable

Typically, overlay or union file systems are affected when one of the source file systems is not responding.
Remote sources are at risk of failure when for example network problems occur.
The worst case is that a call to the  file API does not return. Consequently, the specific file access or even the entire FUSE file system is blocked.

Our solution is to perform such calls in a separate thread. This requires a queue to add the specific request and an infinite loop that picks these requests from the queue and completes them.
These infinite loops are run in separate threads for each root. They are listed above.

Only <i>read(int fd, void *buf, size_t count)</i> and the equivalent for ZIP entries are called directly suche that a slight risk of blocking remains.

When paths of roots  are given with leading double slash <b>//</b> (according to a convention from  MS Windows),
they are treated specially. They are used only when they have successfully run <i>fsstat()<i> recently in <i>infloop_statqueue()</i>.
