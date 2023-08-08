<!---
(defun Make-man()
(interactive)
(save-some-buffers t)
(shell-command "pandoc ZIPsFS.1.md -s -t man | /usr/bin/man -l -")
)
-->



% ZIPsFS(1)

NAME
====

**ZIPsFS** — FUSE-based  overlay union file system which expands ZIP files

SYNOPSIS
========


ZIPsFS \[*ZIPsFS-options*\] *path-of-root1* *path-of-root*  *path-of-root3*   :  \[*fuse-options*\] *mount-point*

## Example 1

ZIPsFS -l 2GB  ~/tmp/ZIPsFS/writable ~/local/file/tree  //computer1/pub  //computer2/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt


DESCRIPTION
===========

## Summary

ZIPsFS combines multiple file structures into one, resulting in single directory structure that
contains underlying files and sub-directories from the given sources.  Created or modified files are stored in
the first file structure. The other file sources  are only read and never modified.  ZIPsFS expands ZIP files as folders. The
folder name is formed from the ZIP file name by appending ".Contents/".  This can be changed by the
user with rules based on file name patterns. It is also possible to get the content of the zip file
online directly into the containing folder without a parent folder.

## Configuration

The default behavior can be modified with rules based on file names in *configuration.c*.
For changes to take effect, re-compilation is necessary.

## Union / overlay file system

All files in the file trees (in the example four) can be accessed via the mount point (in the example *~/tmp/ZIPsFS/mnt*).
When files are created or modified, they will be stored in the first file tree (in the example *~/tmp/ZIPsFS/writable*).
If files exist in two locations, the left most root takes precedence.

## Unreliable roots

Source  file structures may come from remote sites and it may happen that a file structure is temporarily not available.
In such case, ZIPsFS should not block. It should continue to work with the remaining file systems. This is implemented as follows:
Paths starting with double slash (in the example  *//computer1/pub*) are regarded as remote paths that may be temporarily unavailable.
ZIPsFS will periodically check  file systems starting with a double slash.
If the last responds was too long ago then the respective file system is skipped.

## Modified and created files

New files are created in the first file tree, while the following file trees are not modified.
If the first root is an  empty string is passed as first file tree, no files can be created and the virtual file system is read-only.

## Cache

ZIPsFS can read certain ZIP entries entirely into RAM and provide the data from the RAM copy  at higher speed.
This may improve performance in particular  for compressed ZIP entries that are read from varying positions in the file, so-called file seek.
Which  ZIP entries are copied into RAM is controlled by rules based on file names and compression in *configuration.c*.

In addition, compressed ZIP entries are cached in RAM if the reading position ever jumps backward.

With the  option **-l** an upper limit of memory consumption for the ZIP  RAM cache is specified.

## Logs

Running ZIPsFS in the foreground with the option *-f*, allows to observe logs continuously at the console.
Running it in a terminal multiplexer like  *tmux* has the advantage that the session is persistent and continues even when
the windowing system or the terminal emulator terminate.
A log file is written in **~/.ZIPsFS/**  and is also accessible from the root of  the virtual file system.
An HTML file with status information is found in the root of the file system.

## ZIP files

Let *file.zip* be a ZIP file in any of the roots. It will  appear in the virtual file system together with a folder *file.zip.Content*.
Special rules based on file name patterns can be defined whether the contained files are shown in a sub-folder or directly in the file listing.
Normally, the folder name is formed by appending "*.Content*" to the zip file name. Conversely, complex rules can be implemented in
**configuration.c**.





ZIPsFS Options
--------------

-h

:   Prints brief usage information.


-d *SQLite-database-file*

:   To improve performance, ZIPsFS caches file directories using the name and the last-modified time stamp as key. With this option the SQLite3 database file can be specified. In this case, the database will not be cleared when ZIPsFS is started which will be done otherwise.



-l  *Maximum memory for caching ZIP-entries in the RAM*

:   Specifies a limit for the cache.  For example *-l  8G* would limit the size of the cache to 8 Gigabyte.

- c \[NEVER,SEEK,RULE,ALWAYS\]

:   Policy when ZIP entries are cached in RAM.


-o *comma separated Options*

:   *-o allow_other*     Other users can read the files


|           |                                                                                  |
|:---------:|----------------------------------------------------------------------------------|
|    NEVER  | ZIP are never cached, even not in case of backward seek.                         |
|           |                                                                                  |
|    SEEK   | ZIP entries are cached if the file position jumps backward. This is the default  |
|           |                                                                                  |
|   RULE    | ZIP entries are cached according to rules in **configuration.c**.                |
|           |                                                                                  |
|  ALWAYS   | All ZIP entries are cached.                                                      |
|           |                                                                                  |


FUSE Options
------------

-f

:   Run in foreground and display some logs at stdout. This mode is useful inside tmux.


-s

:   Disable multi-threaded operation


FILES
=====

- configuration.c:  Customizable rules. Modification requires recompilation.
- ~/.ZIPsFS:
  Contains the log file and the SQLite3 DB file

# Implementation

Written in GNU-C.

## Dependencies

 - fuse3
 - libzip

## Operation system

  - Linux 64
  - MacOS: Would require minor adaptations.


BUGS
====

Current status: Testing and Bug fixing


AUTHOR
======

Christoph Gille

SEE ALSO
========


- https://github.com/openscopeproject/ZipROFS
- https://github.com/google/fuse-archive
- https://bitbucket.org/agalanin/fuse-zip/src
- https://github.com/google/mount-zip
- https://github.com/cybernoid/archivemount
