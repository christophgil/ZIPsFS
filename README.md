# ZIPsFS - An fuse-based  overlay file system which expands  ZIP files

# Motivation


We use closed-source proprietory Windows software and shared libraries for data conversion of high throughput experimental data. However,
the sheer amount of file data brought the high-performance Windows machine to a stand still.
We ended up with Wine on Ubuntu which easily copes with the high amount of data.
Unfortunately, implementation and  user interface prevents usage of  UNIX techniques  to get data out of a storage without creating intermediate files.
Furthermore the software demands write access even if nothing is written.

Previously we used successfully zip-fuse to mount those  ZIP files that are required for a computation in compination with symbolic links.
However, as the size of our experiments and the number of  ZIP files is rising, this is not possible any more.

Furthermore we are concerned about the health of our spinning hard disks when 20 threads are reading a 4TB file from differen file positions
or  when  files are  read with many  backward and forward  seeks.

# Features

## Acts like a union or overlay file system
- The contents of several root folders given as absolute paths are  joined.
- Only the first root is writable.


## fail-safe
- Root folders starting with two slashes (like an UNC in Windows) are treated as remote file systems.
  ZIPsFS queries all remote root folders periodically and if a remote file system is not responding, it will be skipped.

- We use a  remote storage which   can be mounted from different nodes.  When one fails, ZIPsFS  tries the next one.
  The path must be like  ../EquiValent/123  and  ZIPsFS will try /EquiValent/124 and /EquiValent/125.

## Adaptations to the file tree


- Normally, ZIP files appear as folders. Our software requires folders ending on  .d. The zip files end with .d.Zip. Therefore ZIPsFS may remove the .Zip suffix.

- For other data files the name of the Zip file must not appear in the file tree. The containing files within the ZIP file appear directly in the listing.

Everything is   customized in  src/configuration.h



## Jumping within the file

For remote and compressed file systems, data is best read sequentially.
Reading at different positions and performing many seek operations is unfavorable.

However, this is exactly what the Windows  software is doing.

ZIPsFS can store the entire ZIP entry in RAM. All threads can read the data from the in-RAM copy.
When all file connections are closed, the RAM is released.

## Performance

ZIPsFS uses SQLite3 to keep directory listings.
This avoids time consuming reading the table of contents of ZIP files.

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
