See [Installation](./INSTALL.md)

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

ZIPsFS   ~/tmp/ZIPsFS/writable ~/local/file/tree  //computer1/pub  //computer2/pub :  -f -o allow_other  ~/mnt/ZIPsFS


DESCRIPTION
===========

## Summary

ZIPsFS acts as a union or overlay file system. It combines multiple file structures into one, resulting in a single directory structure that
contains underlying files and sub-directories from the given sources.  Created or modified files are stored in
the first file location. The other file sources  are read-only and files will  never be modified.  ZIPsFS expands ZIP files as folders. Normally, the
folder name is formed from the ZIP file name by appending ".Contents/".  This can be changed
with rules based on file name patterns. Extensive configuration is possible without interrupting the  file system.
Specific features and performance tweaks  meet our needs for storing large data from mass spectrometry experiments.

## Configuration

The default behavior can be modified with rules based on file names in *ZIPsFS_configuration.h* and *ZIPsFS_configuration.c*.
For changes to take effect, re-compilation and restart is necessary. Using the *-s symlink* option, the configuration can
be changed without interrupting the file system. Ongoing computations with file access to the ZIPsFS are not affected.


## Union / overlay file system

ZIPsFS is a union or overlay file system. Several file locations are combined to one.
All files in the source file trees (in above  example command three sources) can be accessed via the mount point (in the example *~/mnt/ZIPsFS*).
When files are created or modified, they will be stored in the first file tree (in the example *~/tmp/ZIPsFS/writable*).
If files exist in two locations, the left most source file system takes precedence.

New files are created in the first file tree, while the following file trees are not modified.
If an   empty string "" or '' is given for the first source, no writable source  is used.

## ZIP files

Let *file.zip* be a ZIP file in any of the source file systems. It will  appear in the virtual file system together with a folder *file.zip.Content*.
Normally, the folder name is formed by appending "*.Content*" to the zip file name. This can be changed in *ZIPsFS_configuration.c*.

For example Sciex mass spectrometry software requires that the containing files are shown directly in the file listing rather than in a sub-folder.

## Cache

Optionally, ZIPsFS can read certain ZIP entries entirely into RAM and provide the data from the RAM copy  at higher speed.
This may improve performance for compressed ZIP entries that are read from varying positions in the file, so-called file file-seek.
With the  option **-l** an upper limit of memory consumption for the ZIP  RAM cache is specified.

Further caches aim at faster file listing of large directories.


## Logs


It is recommended to run ZIPsFS in the foreground with option *-f* within a persistent terminal multiplexer like *tmux*.

Log files are found in  **~/.ZIPsFS/**.

An HTML file with status information is dynamically generated in  the generated folder **ZIPsFS** in the virtual  ZIPsFS file system.




## Plugins - Auto-generation of virtual files

ZIPsFS can display virtual files which do not  exist, but which are generated automatically when used.
This feature must be activated in ZIPsFS_configuration.h.  The first file root is used to store the generated files.

A typical use-case are file conversions.
Auto-generated files are displayed in the virtual file tree in **ZIPsFS/a/**.
If they have not be used before, an estimated file size is reported as the real file size is not yet known.

The currently included examples demonstrate this feature and can serve as a templated for own settings.

For this purpose copy image or pdf files into one of the roots and visit the respective folder in the virtual file system.
Prepend this folder with **ZIPsFS/a/** and you will see the generated files:

    mnt=<path of mountpoint>

    mkdir $mnt/test

    cp file.png $mnt/test/

    ls -l $mnt/ZIPsFS/a/test/

- For image files (jpg, jpeg, png and gif), smaller versions of 25 % and 50 %
- For image files extracted text usign Optical Character Recognition
- For PDF files extracted ASCII text.
- For ZIP files the report of the consistency check including check-sums
- Decompression of  .tsv.bz2 and .tsv.gz files
- Mass spectrometry files: They are converted to Mascot and msML.  For wiff files, the contained ASCII text is extracted.

When opening these files  for the first time there will be some delay. This is because the files need to be generated.
When accessed a second time, the data comes without delay, because the file is already there. Furthermore, the file size will be correct.
When the upstream file changes or the last-modified attribute is updated, derived files will be generated again.

### Limitations - unknown file size

The system does not know the file size of not-yet-generated files.  This seems to be a common
problem, see
https://fuse-devel.narkive.com/tkGi5trJ/trouble-with-samba-fuse-for-files-of-unknown-size.  Any help
is appreciated.

Currently, ZIPsFS reports an upper limit of the expected file size which is not really nice.

Why cannot it be done like in /proc files (e.g.   /proc/$$/environ)?
Calling stat /proc/$$/environ  reports file size zero.
If ZIPsFS returns zero, then the content of the files are not readable.

### Limitations - nested, recursive

Currently, nesting (recursion) is not yet supported. A virtual file cannot be the basis for another
virtual file.

### ZIPsFS_autogen_queue.sh

Some exotic Wine dependent  Windows executables do not work well within ZIPsFS.
As a work around, we developed the shell script **ZIPsFS_autogen_queue.sh**.
With each pass of an  infinity loop  one task is taken from a queue and processed. One file is converted at a time per script instance.
Several instances of this shell script can run in parallel. In the settings, the symbol **PLACEHOLDER_EXTERNAL_QUEUE** is given instead of an executable program.


ZIPsFS Options
--------------

-h

Prints brief usage information.


-l  *Maximum memory for caching ZIP-entries in the RAM*

Specifies a limit for the cache.  For example *-l  8G* would limit the size of the cache to 8 Gigabyte.

-c \[NEVER,SEEK,RULE,COMPRESSED,ALWAYS\]

Policy for ZIP entries  cached in RAM.


|           |                                                                                  |
|:---------:|----------------------------------------------------------------------------------|
|   NEVER   | ZIP are never cached, even not in case of backward seek.                         |
|           |                                                                                  |
|   SEEK    | ZIP entries are cached if the file position jumps backward. This is the default  |
|           |                                                                                  |
|   RULE    | ZIP entries are cached according to rules in **configuration.c**.                |
|           |                                                                                  |
|COMPRESSED | All compressed ZIP entries are cached.                                           |
|           |                                                                                  |
|  ALWAYS   | All ZIP entries are cached.                                                      |
|           |                                                                                  |

-s *path-of-symbolic-link*

After initialization the specified  symlink is created and points to the mount point. Previously existing links are overwritten.
This allows to restart ZIPsFS without affecting running programs which access file in the virtual ZIPsFS file system.
For file paths in the virtual file system,  the symlink is used rather than the real mount-point.
Consider a running  ZIPsFS instance which needs to be replaced by a newer one.  The new ZIPsFS instance is started with
a different mount point. Both  instances work simultaneously. The symlink which used to point to the mount point of the old instance is now pointing to that of the new one.
The old instance should be let running for an hour or so until no file handle is open any more.

If the symlink is within an exported  SAMBA or NFS path, it should be relative.
This is best achieved by changing into the parent path where the symlink will be created.
Then give just the name and not the entire path of the symlink. In the /etc/samba/smb.conf give:

   follow symlinks = yes


Debug Options
-------------
See ZIPsFS.compile.sh for activation of sanitizers.

-T  Checks the capability to print a backtrace.  This requires addr2line which is usually in /usr/bin/ of Linux and FreeBSD. For MacOSX, the tool atos is used.

FUSE Options
------------

-f

Run in foreground and display some logs at stdout. This mode is useful inside tmux.

-s

Disable multi-threaded operation to rescue ZIPsFS in case of threading related bugs.

-o *comma separated Options*

-o allow_other

Other users can read the files


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



FILES
=====

- ZIPsFS_configuration.h and ZIPsFS_configuration.c and ZIPsFS_configuration_autogen.c:  Customizable rules. Modification requires recompilation.
- ~/.ZIPsFS:  Contains the log file and cache and the folder a. The later holds auto-generated files.


LIMITATIONS
===========

## Hard-links

Hard-links are not implemented, while symlinks work.

## Deleting files

Files can only be deleted when their physical location is in the first source.
Conversely, in the FUSE file systems  unionfs-fuse and fuse-overlayfs, files can be always deleted irrespectively of their physical location.
They are canceled out without actually deleting them from their physical location.
If you need the same behaviour please drop a request-for-feature.

BUGS
====

Current status: Testing and Bug fixing
If ZIPsFS crashes, please send the stack-trace together with the version number.

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

    # ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

# CURRENT STATE

Usable.

However, we are still fixing minor bugs.

# MOTIVATION

We use closed-source proprietary Windows software for reading huge experimental data from different types of mass spectrometry machines.
Most data is eventually archived in a read-only WORM file system.
With large file directories, access becomes slow particularly on Windows.
To reduce the number of files and to record the CRC hash sum, all files of one mass spectrometry measurement are bundled in one ZIP.
With less individual files search in the entire directory hierarchy takes less than 1 h.
We hoped that files in ZIP files were easily accessible using pipes, named pipes or process substitution or unzipping.
Unfortunately, these techiques did not work for us and  mounting individual ZIP files was the only successful approach.

ZIPsFS has been developed to solve the following  problems:

- Recently  the size of our experiments and thus the number of ZIP files grew enormously.
  Mounting thousands of individual ZIP files produces a very long <i>/etc/mtab</i> and puts strain on the OS.

- Some proprietary  software requires write access for files and their parent folders.

- Mass spectrometry software do not read files sequentially. Instead bytes are read  from varying file positions. This is particularly  inefficient
  for compressed  ZIP entries. The worst case is jumping backwards  <I>i.e.</I>  calling seek() with a negative value.

- Experimental records are first stored in an intermediate storage and later after verification in the final archive.
  Consequently there are different places to look for a particular file.

- Some proprietary  software fires millions of redundant requests to the file system.
  This is a problem for remote files and mounted ZIP files.

<span style="color:blue">some *blue* text</span>
<SPAN>
 <span style="color:blue">some *blue* text</span>


 <DIV style="padding:1em;border:2px solid gray;float:left;">
       File tree with zip files on hard disk:
 <BR>
       <PRE style="font-family: monospace,courier,ariel,sans-serif;">
 ├── <B style="color:#1111FF;">src</B>
 │   ├── <B style="color:#1111FF;">InstallablePrograms</B>
 │   │   └── some_software.zip
 │   │   └── my_manuscript.zip
 └── <B style="color:#1111FF;">read-write</B>
    ├── my_manuscript.zip.Content
            ├── my-modified-text.txt
       </PRE>
 </div>

 <DIV style="padding:1em;border:2px solid gray;float:right;">
       Virtual file tree presented by ZIPsFS:
       <PRE style="font-family: monospace,courier,ariel,sans-serif;">
 ├── <B style="color:#1111FF;">InstallablePrograms</B>
 │   ├── some_software.zip
 │   └── <B style="color:#1111FF;">some_software.zip.Content</B>
 │       ├── help.html
 │       ├── program.dll
 │       └── program.exe
 │   ├── my_manuscript.zip
 │   └── <B style="color:#1111FF;">my_manuscript.zip.Content</B>
 │       ├── my_text.tex
 │       ├── my_lit.bib
 │       ├── fig1.png
 │       └── fig2.png
       </PRE>
 </DIV>

 <DIV style="clear:both;">
     The file tree can be adapted to specific needs by editing <I>ZIPsFS_configuration.c</I>.
     Our mass-spectrometry files are processed with special software.
     It expects a file tree in its original form i.e. as files would not have been zipped.
     Furthermore, write permission is required for files and containing folders while files are permanently stored and cannot be modified any more.
     The folder names need to be ".d" instead of ".d.Zip.Content".
     For Sciex (zenotof) machines, all files must be in one folder without intermediate folders.
 </DIV>

 <DIV style="padding:1em;border:2px solid gray;float:left;">
                     File tree with zip files on our NAS server:
       <PRE style="font-family: monospace,courier,ariel,sans-serif;">
 ├── <B style="color:#1111FF;">brukertimstof</B>
 │   └── <B style="color:#1111FF;">202302</B>
 │       ├── 20230209_hsapiens_Sample_001.d.Zip
 │       ├── 20230209_hsapiens_Sample_002.d.Zip
 │       └── 20230209_hsapiens_Sample_003.d.Zip

 ...

 │       └── 20230209_hsapiens_Sample_099.d.Zip
 └── <B style="color:#1111FF;">zenotof</B>
    └── <B style="color:#1111FF;">202304</B>
    ├── 20230402_hsapiens_Sample_001.wiff2.Zip
    ├── 20230402_hsapiens_Sample_002.wiff2.Zip
    └── 270230402_hsapiens_Sample_003.wiff2.Zip
 ...
         └── 270230402_hsapiens_Sample_099.wiff2.Zip
       </PRE>
 </DIV>


 <DIV style="padding:1em;border:2px solid gray;float:right;">
             Virtual file tree presented by ZIPsFS:
             <PRE style="font-family: monospace,courier,ariel,sans-serif;">
 ├── <B style="color:#1111FF;">brukertimstof</B>
 │   └── <B style="color:#1111FF;">202302</B>
 │       ├── <B style="color:#1111FF;">20230209_hsapiens_Sample_001.d</B>
 │       │   ├── analysis.tdf
 │       │   └── analysis.tdf_bin
 │       ├── <B style="color:#1111FF;">20230209_hsapiens_Sample_002.d</B>
 │       │   ├── analysis.tdf
 │       │   └── analysis.tdf_bin
 │       └── <B style="color:#1111FF;">20230209_hsapiens_Sample_003.d</B>
 │           ├── analysis.tdf
 │           └── analysis.tdf_bin

 ...

 │       └── <B style="color:#1111FF;">20230209_hsapiens_Sample_099.d</B>
 │           ├── analysis.tdf
 │           └── analysis.tdf_bin
 └── <B style="color:#1111FF;">zenotof</B>
     └── <B style="color:#1111FF;">202304</B>
           ├── 20230402_hsapiens_Sample_001.timeseries.data
           ├── 20230402_hsapiens_Sample_001.wiff
           ├── 20230402_hsapiens_Sample_001.wiff2
           ├── 20230402_hsapiens_Sample_001.wiff.scan
           ├── 20230402_hsapiens_Sample_002.timeseries.data
           ├── 20230402_hsapiens_Sample_002.wiff
           ├── 20230402_hsapiens_Sample_002.wiff2
           ├── 20230402_hsapiens_Sample_002.wiff.scan
           ├── 20230402_hsapiens_Sample_003.timeseries.data
           ├── 20230402_hsapiens_Sample_003.wiff
           ├── 20230402_hsapiens_Sample_003.wiff2
           └── 20230402_hsapiens_Sample_003.wiff.scan

 ...

           ├── 20230402_hsapiens_Sample_099.timeseries.data
           ├── 20230402_hsapiens_Sample_099.wiff
           ├── 20230402_hsapiens_Sample_099.wiff2
           └── 20230402_hsapiens_Sample_099.wiff.scan
 </PRE>
 </DIV>

 <DIV style="clear:both;"></DIV>

</SPAN>


