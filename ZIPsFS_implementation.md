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

When a client program requests a file, the virtual file path needs to be translated into an existing real file or ZIP entry.

All given roots are iterated  until the file or ZIP entry  is found.
The first root is the only that allows file modification and file creation.
All others are read only.

Typically, overlay or union file systems are affected when one of the source file systems is not responding.
In ZIPsFS there is a high probability that it continues to work.
Remote roots are indicated by leading double slash //.
Since they are more likely to fail, they are  observed by a specific  thread.
File access is skipped for roots that have not recently responded.



# Caches

There are several caches to improve performance. For clarifications and distinction,
functions and variables are prefixed <I>memcache_</I>, <I>dircache_<I> and <I>statcache_</I>.


## Memcache

This cache stores the file content of ZIP files in RAM. Depending on the size, memory is reserved either with <I>malloc()</I> or with <I>mmap()</I>.
The memory address is kept int <I>struct fhdata</I>. When several threads are accessing the same file, then
only one instance of <I>struct fhdata</I> has a cache. It is used by all other instances with the same file path.
Upon close, such <I>struct fhdata</I> instance is not deleted when there are still instances with the same path. Instead, it is marked to be deleted later.

## Dircache

The table of content (toc) of ZIP files is stored in memory mapped files. The data structure is <I>struct mstore</I>.
Originally, SQLite had been used. However, we noticed that some performance suffers because we use software that does a large number of requests.
Therefore we created our own  storage  <I>struct mstore</I>. It is basically an array of memory mapped files.

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



# When a source fails

When one of the roots become unavailable and calls to stat() fail or block, ZIPsFS should continue to function.
This is achieved by running stat() in a separate threads belonging to the respective rootdata object.
See functions starting with prefix <I>statqueue_</I>.
If the thread <I>infloop_statqueue</I> has been blocked for a longer time, it is restarted.

# Limitations:

## Cache

Because of the  granularity of time measurement in files last-modified attribute, file updates may be unnoticed. This is unlikely, but theoretically possible.
Solution?
