# ZIPsFS - Fuse-based  overlay file system which expands  ZIP files

# Motivation

We use closed-source proprietary Windows software and shared libraries for data conversion of high throughput experimental data. However,
the sheer amount of file data brought the high-performance Windows machine regularly to a stand still.
We ended up with Wine on Ubuntu which easily copes with the high amount of data.
Unfortunately, implementation and  user interface prevents usage of  UNIX techniques  to use  zipped files from our storage without creating intermediate files.
Furthermore some software demands write access to the file location.

We used to use zip-fuse to mount  ZIP files in the storage. Before starting computation, all required ZIP files were mounted.
Symbolic links solved the problem of demanded write access.
However, recently  the size of our experiments and thus the number of ZIP files grew, which rendered this method unusable.

Furthermore we are concerned about the health of our conventional hard disks since many  threads are simultaneously reading files of 2GB and more at different file positions.
There are also files which are sqlite3 databases. This leads to large numbers of seek operations which is inefficient for compressed and remote files.

ZIPsFS has been developed to solve all these problems.

# Usage


## General:

    ZIPsFS [ZIPsFS-options] path-of-root1 path-of-root2 path-of-root3 :  [fuse-options] mount-point

## Example 1

    ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable ~/local/file/tree  //computer1/pub  //computer2/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt

With this command, all files in the three sources can be accessed via the path ~/tmp/ZIPsFS/mnt.
In this respect, ZIPsFS is a so-called union or overlay file system.
When files are created or modified, they will be stored in ~/tmp/ZIPsFS/writable.

Paths starting with double slash such as  //computer1/pub are regarded as unstable and potentially blocking.
ZIPsFS will periodically check these file systems and skip them if they are not responding. This is useful for remote paths.

With the  option -l an upper limit of memory consumption for the ZIP file RAM cache is specified. The overall size of all cached ZIP entries should not exceed 2 GB here.
With the fuse option -f, it is running in the foreground such that some debugging output can be observed at
the command prompt. In this mode the program can be stopped normally with Ctrl-C or Ctrl-Backslash. With  -o allow_other, also other users can access the the file system.

For more information on command line arguments  run ZIPsFS without parameters or with -h.

## Logs

We recommend to run ZIPsFS in a tmux session, using the option -f (foreground).
In this mode, logs at stdout are enabled and are directly observable. In addition, a log file with errors and warnings
is  written into the first root folder. An HTML file with status information is found int the root of the file system.

# Features

## Acts like a union or overlay file system

- The contents of several root folders given as absolute paths are combined.
- Only the first root is writable.


## fail-safe

- Root folders can be marked as less reliable  by starting the absolute file path with two slashes rather than just one.  This is in analogy to remote  paths in Windows.
  ZIPsFS queries all remote root folders periodically. If a remote file system fails to  respond in time, it will be skipped.
  This may avoid block by  broken mounts.

## Adaptations to the file tree

- Normally, the names of ZIP files appear as folders. The ZIP entries are files in these folders. However, our software requires folders ending with  ".d" instead of  ".d.Zip".
  Therefore  ZIPsFS is capable of suppressing the  ".Zip" suffix for selected file paths.

- For other data file types the name of the ZIP file must not appear in the file tree at all. The containing files within the ZIP file appear directly in the file listing.

The rules in the user-defined configuration file  src/configuration.h are based on file paths. Changing these rules requires recompilation.

## File seek

For remote and/or compressed files, data is best read sequentially.
Reading at changing file positions is unfavorable.

ZIPsFS can store the entire ZIP entry in RAM. All threads can read the data from the in-RAM copy.
When all file connections are closed, the file date in the RAM is released.


# Implementation

Written in Gnu-C.
Based on fuse3 example passthrough.

ZIPsFS uses SQLite3 to keep directory listings.
When the last-modified file attribute has not changed, the stored directory listings are still valid and
does not need to be read from the ZIP files again.

# Current status

Testing and Bug fixing

# Operation system

Developed and tested on Linux 64 bit. May requires  minor adaptations for other UNIX like platforms like MacOS.

## Also see

- https://github.com/openscopeproject/ZipROFS
- https://github.com/google/fuse-archive
- https://bitbucket.org/agalanin/fuse-zip/src
- https://github.com/google/mount-zip
- https://github.com/cybernoid/archivemount
