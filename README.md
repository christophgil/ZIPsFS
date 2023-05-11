# ZIPsFS - An fuse-based  overlay file system which expands  ZIP files

# Motivation


We use closed-source proprietory Windows software and shared libraries for data conversion of high throughput experimental data. However,
the sheer amount of file data brought the high-performance Windows machine to a stand still.
We ended up with Wine on Ubuntu which easily copes with the high amount of data.
Unfortunately, implementation and  user interface prevents usage of  UNIX techniques  to get data out of a storage without creating intermediate files.
Furthermore the software demands write access even if nothing is written.

We used to use zip-fuse to mount   ZIP files in the storage. Before starting computation, all required ZIP files are mounted.
Symbolic links solved the problem of required write access.
However, as the size of our experiments and the number of  ZIP files grow, this is not possible any more.

Furthermore we are concerned about the health of our conventional hard disks in our RAID system when 20 threads are simultaneously reading a 4TB file at different file positions.
Furthermore, the software implementation leads to massive seek operations which is inefficient for compressed and remote data.


# Usage

We recommend to run ZIPsFS in a tmux session, however, it could also be run as a daemon.

## Usage:

    ZIPsFS [ZIPsFS-options] path-of-root1 path-of-root2 path-of-root3 :  [fuseoptions] mount-point

## Example 1

    ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable  //computer1/pub  //computer2/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt

With this command, all files in the three sources can be accessed via the path ~/tmp/ZIPsFS/mnt.
When files are created or modified, they will be stored in ~/tmp/ZIPsFS/writable.

Paths starting with double slash such as  //computer1/pub are regarded as unstable and potentially blocking.
ZIPsFS will periodically check these file systems and skip them if they are not responding. This is useful for remote paths.

With the  option -l an upper limit of memory consumption for the ZIP file RAM cache is specified. The overall size of all cached ZIP entries should not exceed 2 GB here.
With the fuse option -f, it is running in the foreground such that some debugging output can be observed at
the command prompt. In this mode the program can be stopped normally with Ctrl-C or Ctrl-Backslash. With  -o allow_other, also other users can access the the file system.

For more information on command line arguments  run ZIPsFS without parameters or with -h.
# Features

## Acts like a union or overlay file system
- The contents of several root folders given as absolute paths are  joined.
- Only the first root is writable.


## fail-safe
- Root folders can be marked as less reliable  by starting the absolute file path with two slashes instead of one.  This is in analogy to remote  paths  in Windows - so-called UNCs.
  ZIPsFS queries all remote root folders periodically and if a remote file system is not responding, it will be skipped.

## Adaptations to the file tree


- Normally, ZIP files appear as folders showing the content of the ZIP files. Our software requires folders ending on  ".d". The zip files end with ".d.Zip". Therefore  ZIPsFS may remove the ".Zip" suffix.

- For other data file types the name of the ZIP file must not appear in the file tree at all. The containing files within the ZIP file appear directly in the file listing.
  For example a directory containing 1000 ZIP files each containing 4 files will be shown as a listing of  4000  files.

This behavior is governed by rules in src/configuration.h and is based on file and path names. Users may want to addapt these rules for their needs.


## File seek

For remote and compressed file systems, data is best read sequentially.
Reading at different positions is unfavorable.

ZIPsFS can store the entire ZIP entry in RAM. All threads can read the data from the in-RAM copy.
When all file connections are closed, the file date in the RAM is released.

## Performance

ZIPsFS uses SQLite3 to keep directory listings.
When the last-modified file attribute has not changed, the stored directory listings are still valid and
das not need to be read from the ZIP files again.

# Implementation

Based on fuse3 example passthrough.
Written in Gnu-C.


# Current status

Testing and Bug fixing


## Also see

- https://github.com/openscopeproject/ZipROFS
- https://github.com/google/fuse-archive
- https://bitbucket.org/agalanin/fuse-zip/src
- https://github.com/google/mount-zip
- https://github.com/cybernoid/archivemount
