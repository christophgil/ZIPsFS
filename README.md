See [Installation](./INSTALL.md)

<!---
(defun Make-man()
(interactive)
(save-some-buffers t)
(shell-command "pandoc ZIPsFS.1.md -s -t man | /usr/bin/man -l -")
)
%% (query-replace-regexp " *— *" " - ")
-->



% ZIPsFS(1)

NAME
====

**ZIPsFS** - FUSE-based  overlay union file system expanding ZIP files

SYNOPSIS
========


    ZIPsFS \[*ZIPsFS-options*\] *path-of-branch1* *path-of-branch2*  *path-of-branch3*   :  \[*fuse-options*\] *mount-point*


## Example


### First create some example files

    b1=~/test/ZIPsFS/writable
    b2=~/test/ZIPsFS/branch1
    b3=~/test/ZIPsFS/branch2
    mnt=~/test/ZIPsFS/mnt

    mkdir -p $b1 $b2 $b3 $mnt

    for c in a b c d e f; do echo hello world $c >$b2/$c.txt; done
    for ((i=0;i<10;i++)); do echo hello world $i >$b3/$i.txt; done

    zip --fifo $b2/zipfile1.zip <(date)  <(date '+%Y-%M-%d %H:%m %S')
    zip --fifo $b3/zipfile2.zip <(hostname)  <(ls /)
    zip --fifo $b3/20250131_this_is_a_mass_spectrometry_folder.d.Zip <(seq 10)


### Now start ZIPsFS

    ZIPsFS   $b1 $b2 $b3 : -o allow_other  $mnt

### Browse the virtual file tree

    Open a file browser or another terminal and  browse the files in  ~/test/ZIPsFS/mnt/

### Create a file in the virtual tree

    echo "This file will be stored in ~/test/ZIPsFS/writable "> ~/test/ZIPsFS/mnt/my_file.txt

    cat ~/test/ZIPsFS/mnt/my_file.txt

### Real storage place of the created file

    ls -l ~/test/ZIPsFS/writable/my_file.txt

DESCRIPTION
===========

## Summary


ZIPsFS functions as a union or overlay file system, merging multiple file structures into a unified directory.
This directory presents the underlying files and subdirectories from the specified sources (branches) as a single, cohesive structure.
Any newly created or modified files are stored in the first file location, while all other sources remain read-only, ensuring that their files are never altered.
ZIPsFS treats ZIP files as expandable folders, typically naming them by appending ".Contents/" to the original ZIP file name.
However, this behavior can be customized using filename-based rules. Extensive configuration options allow adjustments. Changes can be applied without disrupting the file system.
Additionally, ZIPsFS includes specialized features and performance optimizations tailored for efficiently storing large-scale mass spectrometry data.


## Logs


ZIPsFS is normally running in the foreground.
It is recommended to use a  persistent terminal multiplexer like *tmux*.
This allows to observe all messages and to search back in the scroll buffer.

In addition,  log files are found in  **~/.ZIPsFS/**.

An HTML file with status information is dynamically generated in  the generated folder **<Mount-Point>/ZIPsFS/** in the virtual  ZIPsFS file system.


## Configuration

The default behavior can be customized using filename-based rules. Configuration files, identified
by the prefix ZIPsFS_configuration, are written in C. Any changes require recompilation and a
restart of ZIPsFS to take effect.

With the -s option, the updated ZIPsFS can seamlessly replace running instances without disrupting the virtual file system.

To illustrate how this works, let MNT represent the apparent mount point of the FUSE file system.
Suppose we are in the parent directory of MNT, enabling the use of relative paths.
Users access files through this apparent mount point, but in reality, MNT is a symbolic link to the actual mount point.
The real mount point is not directly accessed by users, as it changes each time a new instance of ZIPsFS is launched.

For example, assume the obsolete ZIPsFS instance is mounted at ./.mountpoints/MNT/1.
When a new instance replaces it, it may use any empty directory as   mount point  and the  option:

    -s MNT

Once the new instance is running, the symbolic link is updated to point to the new mount
location. From the user's perspective, nothing changes - the apparent mount point remains MNT. To
ensure uninterrupted access, the obsolete instance should remain active for a short period to allow
ongoing file operations to complete.



If MNT  is within an exported  SAMBA or NFS path the real mount points should be in the exported file tree as well.
Include into */etc/samba/smb.conf*:

    follow symlinks = yes



## Union / overlay file system

ZIPsFS is a union or overlay file system. Several file locations are combined to one.
When files are created or modified, they will be stored in the first file tree (in the example *~/test/ZIPsFS/writable*).
If files exist in two locations, the left most source file system takes precedence.

If an empty string is given for the first source, no writable branch is used.

## ZIP files

In the example the ZIP files are shown as folders with the suffix  "*.Content*". This can be changed in *ZIPsFS_configuration.c*.

Extra rules specified:


 - Mass spectrometry software expects folder names ending with *.d* rather than *.d.Zip.Content*. This is applied to all ZIP files with a name starting with a year and ending with .d.Zip.

 - For example Sciex mass spectrometry software requires that the containing files are shown directly in the file listing rather than in a sub-folder.



## Cache

Optionally, ZIPsFS can read certain ZIP entries entirely into RAM and provide the data from the RAM copy  at higher speed.
This may improve performance for compressed ZIP entries that are read from varying positions in a file, so-called file file-seek.
This is particularly important, when the ZIP files reside in a remote file system.
With the  option **-l** an upper limit of memory consumption for the ZIP  RAM cache is specified.
There are customizeable rules for how low memory is treated.

Further caches aim at faster file listing of large directories.





## Real file location

To see the real file path i.e. the file path where a file is physically stored,


append **@SOURCE.TXT** to the virtual file path. Example:


    cat ~/test/ZIPsFS/mnt/1.txt@SOURCE.TXT

From a Windows client, these files are not accessible.
This is because they are not listed in the parent folder.


## Auto-generation of virtual files

ZIPsFS can display virtual files  which are generated automatically.
This feature is  activated by setting  the preprocessor macro  **WITH_AUTOGEN** to  **1** in *ZIPsFS_configuration.h*.
The first file branch is used to store the generated files.


A typical use-case are file conversions. The default rules in ZIPsFS_configuration_autogen.c. comprise:

- For image files (jpg, jpeg, png and gif), smaller versions of 25 % and 50 %
- For image files extracted text usign Optical Character Recognition
- For PDF files extracted ASCII text
- For ZIP files the report of the consistency check including check-sums
- Mass spectrometry files: They are converted to mgf (Mascot)  and msML.  For wiff files, the contained 16 bit text is converted to plain ASCII.
- Apache Parquet files are converted to tsv and tsv.bz2

For testing, copy an image file:

    cp file.png ~/test/ZIPsFS/mnt/

Auto-generated files are displayed in the virtual file tree in **<Mount-Point>/ZIPsFS/a/**. Example:

    ls ~/test/ZIPsFS/mnt/ZIPsFS/a/


If they have not be used before and the real file size is still unknown, an estimated file size is reported.

Some of the conversions require support for docker.

### Limitations - unknown file size

The system does not know the file size of not-yet-generated files.  This seems to be a common
problem of UNIX and Linux. See
https://fuse-devel.narkive.com/tkGi5trJ/trouble-with-samba-fuse-for-files-of-unknown-size.  Suggestions are welcome.


Initially, ZIPsFS reports an upper estimate of the expected file size. This breaks programs that need to know
the exact file size such as */usr/bin/tail*.

How is this problem solved in the virtual file systems /proc annd /sys?
Calling stat /proc/$$/environ. Consider

    ls -l /proc/self/environ

The reported  file size is zero. Nevertheless, *cat*, *more*  and even *tail* work. Why?
If the FUSE file system  returns zero for a file, the content of the files are not readable.
Any suggested appreciated.


### ZIPsFS_autogen_queue.sh

Some scientific Windows executables do not behave well when started from a compiled programs like ZIPsFS.  The
problem is caused by the the Console API which replaces old fashion terminal escape sequences.  As a work around, the
shell script **ZIPsFS_autogen_queue.sh** can be used. ZIPsFS pushes tasks and waits for their
completion when the symbol **PLACEHOLDER_EXTERNAL_QUEUE** is given instead of an executable
program.  These tasks are performed in the shell script which need to be started explicitly.
Several instances of this shell script can run in parallel.


ZIPsFS Options
--------------

-h

Prints brief usage information.


-l  *Maximum memory for caching ZIP-entries in the RAM*

Specifies a limit for the cache.  For example *-l  8G* would limit the size of the cache to 8 Gigabyte.

-c \[NEVER,SEEK,RULE,COMPRESSED,ALWAYS\]

Policy for ZIP entries  cached in RAM.


|           |                                                                                    |
|:---------:|------------------------------------------------------------------------------------|
|   NEVER   | ZIP are never cached, even not in case of backward seek.                           |
|           |                                                                                    |
|   SEEK    | ZIP entries are cached when the file position jumps backward. This is the default  |
|           |                                                                                    |
|   RULE    | ZIP entries are cached according to rules in **configuration.c**.                  |
|           |                                                                                    |
|COMPRESSED | All compressed ZIP entries are cached.                                             |
|           |                                                                                    |
|  ALWAYS   | All ZIP entries are cached.                                                        |
|           |                                                                                    |

-s *path-of-symbolic-link*

This is explained in section Configuration.

-b Run in background. Not recommended.



Debug Options
-------------
See ZIPsFS.compile.sh for activation of sanitizers.

-T  Checks the capability to print a backtrace.  This requires addr2line which is usually in /usr/bin/ of Linux and FreeBSD. For MacOSX, the tool atos is used.

FUSE Options
------------


-s

Disable multi-threaded operation to rescue ZIPsFS in case of threading related bugs.

-o *comma separated Options*

-o allow_other

Other users can read the files


## Fault management

When source file structures are stored remotely, there is a risk that they  may be
temporarily unavailable. Overlay file systems typically freeze when calls to the file API block.
Conversely, ZIPsFS should continue to
operate with the remaining file branches. This is implemented as follows for  paths starting with double
slash (in the example *//computer1/pub*). Double slash indicates  remote paths which might get unavailable in analogy to remote UNC paths.  ZIPsFS
will periodically check file systems starting with a double slash.  If the last responds was too
long ago then the respective file system is skipped. Furthermore the stat() function to obtain the
attributes for a file are queued to be  performed in extra threads.

For files which are located in ZIP files and which are first loaded entirely into RAM, the system is
also robust for interruptions and blocks during loading. The system will not freeze. After some
longer time it will try to load the same file from another branch or return ENOENT.

If loading of ZIP files fail, loading will be repeated after 1s.

For ZIP entries loaded entirely into the RAM, the CRC sum is validated and possible errors are logged.





LIMITATIONS
===========

## Hard-links

Hard-links are not implemented, while symlinks work.

## Deleting files

Files can only be deleted when their physical location is in the first source.
Conversely, in the FUSE file systems  unionfs-fuse and fuse-overlayfs, files can be always deleted irrespectively of their physical location.
They are canceled out without actually deleting them from their physical location.
If you need the same behaviour please drop a request-for-feature.

## Reading and writing

Simultaneous Reading and writing of files with the same file descriptor will only work
for files exclusively in the writable source.

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


