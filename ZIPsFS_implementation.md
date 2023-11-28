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


## File attribute and file content caches in ZIPsFS

There are several caches for file attributes and file contents to improve performance.  They can be
deactivate using conditional compilation. The respective switches and detailed description are found
in <I>ZIPsFS_configuration.h</I>. Searching for the names of the switches allows to find the places
of implementation in the source code. Code concerning caches are bundled in <I>ZIPsFS_cache.c</I>

The caches are necessary because

 - Some mass spectrometry software reads files not sequentially but chaotically.
 - The same software excerts thousands and millions of redundand file requests
 - Beside displaying ZIP files as subdirectories containing and the ZIP entries as files in those folders, there is an option for
   displaying all entries of all ZIP files  flat in one directory.
   To list this directory sufficiently fast requires a file entry cache.

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
