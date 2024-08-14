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


ZIPsFS \[*ZIPsFS-options*\] *path-of-root1* *path-of-root2*  *path-of-root3*   :  \[*fuse-options*\] *mount-point*

## Example 1

ZIPsFS   ~/tmp/ZIPsFS/writable ~/local/file/tree  //computer1/pub  //computer2/pub :  -f -o allow_other  ~/tmp/ZIPsFS/mnt


DESCRIPTION
===========

## Summary

ZIPsFS acts as a union or overlay file system. It combines multiple file structures into one, resulting in a single directory structure that
contains underlying files and sub-directories from the given sources.  Created or modified files are stored in
the first file location. The other file sources  are read-only and files will  never be modified.  ZIPsFS expands ZIP files as folders. Normally, the
folder name is formed from the ZIP file name by appending ".Contents/".  This can be changed by the
administrator with rules based on file name patterns. It is also possible to inline  the files in the ZIP  files into the file listing without  the containing folder.

## Configuration

The default behavior can be modified with rules based on file names in *ZIPsFS_configuration.h* and *ZIPsFS_configuration.c*.
For changes to take effect, re-compilation and restart is necessary. Using the *-s symlink* option, the configuration can
be changed in production.


## Union / overlay file system

ZIPsFS is a union or overlay file system because several file locations are combined to one.
All files in the source file trees (in the example four) can be accessed via the mount point (in the example *~/tmp/ZIPsFS/mnt*).
When files are created or modified, they will be stored in the first file tree (in the example *~/tmp/ZIPsFS/writable*).
If files exist in two locations, the left most root takes precedence.

## Modified and created files

New files are created in the first file tree, while the following file trees are not modified.
If the first root is an  empty string is passed as first file tree, no files can be created and the virtual file system is read-only.

## Cache

ZIPsFS can read certain ZIP entries entirely into RAM and provide the data from the RAM copy  at higher speed.
This may improve performance for compressed ZIP entries that are read from varying positions in the file, so-called file file-seek.
The file entries are selected by rules based on file names and compression in *ZIPsFS_configuration.c*.

In addition, compressed ZIP entries are cached in RAM if the reading position ever jumps backward.

With the  option **-l** an upper limit of memory consumption for the ZIP  RAM cache is specified.

## Logs

Running ZIPsFS in the foreground with the option *-f*, allows to observe logs continuously at the console.
It is recommended to run it in a terminal multiplexer like  *tmux*.
Log files are in **~/.ZIPsFS/** written.
An HTML file with status information is found in and are also accessible from Mount-point/ZIPsFS/.

## ZIP files

Let *file.zip* be a ZIP file in any of the root file systems. It will  appear in the virtual file system together with a folder *file.zip.Content*.
Special rules based on file name patterns can be defined whether the contained files are shown in a sub-folder or directly in the file listing.
Normally, the folder name is formed by appending "*.Content*" to the zip file name. Conversely, complex rules can be implemented in
*ZIPsFS_configuration.c*.



## Autogeneration of files

ZIPsFS offers  files which do not yet exist, and will be generated when used.
This option is available if the first root directory which should be writable is provided.

It will not be available, if an empty string is passed as the first root directory.

The typical use-case are file conversions.
Auto-generated files are found displayed in the default file tree.
Instead,  *Mount-point/ZIPsFS/a/* replicates the entire file tree and also offers the generated files.

With the current setting, Mascot mass spectrometry files and msML files are generated
because this is required in our research group. As a more popular example, downscaled image files are offered for jpeg, jpg, png and gif files.
The two scaling levels 50% and 25% demonstrate two different options:
For size reduction to 50%, a file is generated and saved in *First-root-directory/ZIPsFS/a/Virtual-path.scale50%.ext.
When loaded a second time, the data comes without delay, because the file is already there.
For downscaling to 25%, the generated image data is not saved. Instead it is kept in RAM as long as the file handle is active.
After loading the image, the image data in the RAM is discarded. The advantage is that no HD space is used.
The disadvantage is, that for a short time the entire data resides in RAM and that the data is generated each time the file data is read.

Users can use these Imagemagick based examples as a template for their own settings in ZIPsFS_configuration_autogen.c.

Also see readme in the folder /ZIPsFS/.

Some exotic wine based Windows executables did not work well within ZIPsFS.
We had to externalize the into an external program. It is organized as an infinity loop and a queue.
If you enconunter similar problems, see PLACEHOLDER_EXTERNAL_QUEUE and

Problems/Limitations: The system does not know the file size of not-yet-generated files.
It will report upper limit of the expected   file size.

ZIPsFS Options
--------------

-h

:   Prints brief usage information.


-l  *Maximum memory for caching ZIP-entries in the RAM*

:   Specifies a limit for the cache.  For example *-l  8G* would limit the size of the cache to 8 Gigabyte.

-c \[NEVER,SEEK,RULE,COMPRESSED,ALWAYS\]

:   Policy when ZIP entries are cached in RAM.


|           |                                                                                  |
|:---------:|----------------------------------------------------------------------------------|
|    NEVER  | ZIP are never cached, even not in case of backward seek.                         |
|           |                                                                                  |
|    SEEK   | ZIP entries are cached if the file position jumps backward. This is the default  |
|           |                                                                                  |
|   RULE    | ZIP entries are cached according to rules in **configuration.c**.                |
|           |                                                                                  |
|COMPRESSED | All compressed ZIP entries are cached.                                           |
|           |                                                                                  |
|  ALWAYS   | All ZIP entries are cached.                                                      |
|           |                                                                                  |

-s *path-of-symbolic-link*

: After initialization the specified  symlink is created and points to the mount point. Previously existing links are overwritten.
This allows to restart ZIPsFS without affecting running programs.
Programs refer to the symlink rather than the real mount-point.
Consider a ZIPsFS instance which needs to be replaced by a newer one.  The newer one is started with
a different mount point. For some time both instances work simultaneously.
Once no software is reading files from the  old instance, the old instance can be terminated.

After initialization a symlink to the new mount point is created and programs start to use the new instance.


## Fault management

When source file structures are stored remotely, there is a risk that they  may be
temporarily unavailable. Overlay file systems typically freeze when calls to the file API block.
Conversely, ZIPsFS should continue to
operate with the remaining file roots. This is implemented as follows: Paths starting with double
slash (in the example *//computer1/pub*) are regarded as remote paths and treated specially.  ZIPsFS
will periodically check file systems starting with a double slash.  If the last responds was too
long ago then the respective file system is skipped. Furthermore the stat() function to obtain the
attributes for a file are queued to be  performed in extra threads.

For files which are located in ZIP files and which are first loaded entirely into RAM, the system is
also robust for interruptions and blocks during loading. The system will not freeze. After some
longer time it will try to load the same file from another root or return ENOENT.

If loading of ZIP files fail, loading will be repeated after 1s.

For ZIP entries loaded entirely into the RAM, the CRC sum is validated and possible errors are logged.



FUSE Options
------------

-f

:   Run in foreground and display some logs at stdout. This mode is useful inside tmux.


-s

:   Disable multi-threaded operation to rescue ZIPsFS in case of threading related bugs.

-o *comma separated Options*

:   *-o allow_other*     Other users can read the files

FILES
=====

- ZIPsFS_configuration.h and ZIPsFS_configuration.c and ZIPsFS_configuration_autogen.c:  Customizable rules. Modification requires recompilation.
- ~/.ZIPsFS:
  Contains the log file and cache


LIMITATIONS
===========

Hardlinks are not implemented, while symlinks work.

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
- https://github.com/mxmlnkn/ratarmount
