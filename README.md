# ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

 - [Installation](./INSTALL.md)
 - [Configuration](./ZIPsFS_configuration.md)
 - [Generated (synthetic) files: Automatic file conversions, Accessing web resources as regular files](./ZIPsFS_generated_files.md)
 - [Logging](ZIPsFS_logs.md)
 - [Fault management. Timouts for remote upstream file systems. Duplicated remote trees](ZIPsFS_fault_management.md)
 - [Improve performance  caching file content and meta data](./ZIPsFS_cache.md)
 - [Use-case - application for high-throughput mass-spectrometry](USE_CASE.md)
 - [Limitations and Bugs](ZIPsFS_Limitations.md)
 - [See also related sites](ZIPsFS_related_sites.md)


<!---
(defun Make-man()
(interactive)
(save-some-buffers t)
(shell-command "pandoc ZIPsFS.1.md -s -t man | /usr/bin/man -l -")
)
%% (query-replace-regexp " *— *" " - ")

This seems to be a common
problem of UNIX and Linux. See
https://fuse-devel.narkive.com/tkGi5trJ/trouble-with-samba-fuse-for-files-of-unknown-size.  Suggestions are welcome.


-->


## SYNOPSIS


    ZIPsFS [ZIPsFS-options] path-of-branch1/  branch2/  branch3/  branch4/  :  [fuse-options] mount-point


## Description


ZIPsFS functions as a **union or overlay** file system, merging multiple file structures into a unified directory.
This directory presents the underlying files and subdirectories from the specified sources (branches) as a single, cohesive structure.
Any newly created or modified files are stored in the first file location, while all other sources remain read-only, ensuring that their files are never altered.
ZIPsFS treats **ZIP files as expandable folders**, typically naming them by appending ``.Contents/`` to the original ZIP file name.
However, this behavior can be customized using filename-based rules. Extensive configuration options allow adjustments. Changes can be applied without disrupting the file system.

ZIPsFS is best run in a **tmux** session.
With a trailing slash, the folder name is not part of the virtual path, in accordance with  the trailing slash semantics of many UNIX tools.

ZIPsFS includes specialized features like **automatic file conversions** and performance optimizations tailored for efficiently storing and accessing **large-scale mass spectrometry** data.
It has a programming interface to create synthetic file content programmatically.

## Mini tutorial

   Please [Install](./INSTALL.md)  ZIPsFS.


### First create some example files

Without trailing slash, the folder name will be retained in the virtual path. This is the case for ``branch3``.
Virtual file paths in that branch will start with *<mount point>*``/branch3/``.

    b1=~/test/ZIPsFS/writable/
    b2=~/test/ZIPsFS/branch1/
    b3=~/test/ZIPsFS/branch2/
    b4=~/test/ZIPsFS/branch3

    mnt=~/test/ZIPsFS/mnt

    mkdir -p $b1 $b2 $b3 $b4 $mnt

    for c in a b c d e f; do echo hello world $c >$b2/$c.txt; done
    for ((i=0;i<10;i++)); do echo hello world $i >$b3/$i.txt; done

    zip --fifo $b2/zipfile1.zip <(date)  <(echo $RANDOM)
    zip --fifo $b3/zipfile2.zip <(hostname)  <(ls /)
    zip --fifo $b3/20250131_this_is_a_mass_spectrometry_folder.d.Zip   <(seq 10)


### Start ZIPsFS
It is recommended to start ZIPsFS in tmux. For testing, just use your regular command line.

    ZIPsFS   $b1 $b2 $b3 $b4 : -o allow_other  $mnt

### Browse the virtual file tree

Open a file browser or another terminal and  browse the files in

    ~/test/ZIPsFS/mnt/

### Create a file in the virtual tree
The first file tree stores files. All others are read-only.

    echo "This file will be stored in ~/test/ZIPsFS/writable "> ~/test/ZIPsFS/mnt/my_file.txt

    cat ~/test/ZIPsFS/mnt/my_file.txt

To get the real storage place of the file, append ``@SOURCE.TXT``

    cat ~/test/ZIPsFS/mnt/my_file.txt@SOURCE.TXT

### Access web resources as regular files
Make sure the UNIX tool curl is installed. Note that "//:" in the URL is replaced by commas.

    curl --version
    less  ~/test/ZIPsFS/mnt/ZIPsFS/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE


DESCRIPTION
===========


## ZIPsFS is a Union / overlay file system


ZIPsFS functions as a union (overlay) file system.
When files are created or modified, they are stored in the first file tree - e.g.,

    ~/test/ZIPsFS/writable

in the example setup.
If a file exists in multiple source locations, the version from the leftmost source (the first one listed) takes precedence.
To make the file system read-only, you can specify an empty string ("") as the first source. This disables file creation and modification and automatic virtual file generation.

The physical file path, i.e., the actual storage location of a file, can be retrieved from a special
metadata file created by appending ``@SOURCE.TXT`` to the filename.

For example, to determine the real location of:

    ~/test/ZIPsFS/mnt/1.txt

Run the following command:

    cat ~/test/ZIPsFS/mnt/1.txt@SOURCE.TXT



## ZIPsFS expands ZIP file entries

By default, ZIP files are displayed as folders with the suffix ``.Content``.
This behavior can be customized.
The default configuration includes a few exceptions tailored to specific use cases in Mass Spectrometry Compatibility:

  - For ZIP files whose names start with a year and end with *.d.Zip*, the virtual folder will instead
    end with *.d*.

  - Flat File Display: For some mass spectrometry formats where files are not organized into
    subfolders within the ZIP archive, the contents are shown directly in the file list, rather than
    as a nested folder.


## ZIPsFS Options


**-h**

Prints brief usage information.



**-s *path-of-symbolic-link***
This is discussed in section Configuration.



**-c \[NEVER,SEEK,RULE,COMPRESSED,ALWAYS\]**

Policy for ZIP entries  cached in RAM.


|           |                                                                                    |
|:---------:|------------------------------------------------------------------------------------|
|   NEVER   | ZIP entries are never cached, even not in case of backward seek.                   |
|           |                                                                                    |
|   SEEK    | ZIP entries are cached when the file position jumps backward. This is the default  |
|           |                                                                                    |
|   RULE    | ZIP entries are cached according to customizable rules                             |
|           |                                                                                    |
|COMPRESSED | All compressed ZIP entries are cached.                                             |
|           |                                                                                    |
|  ALWAYS   | All ZIP entries are cached.                                                        |
|           |                                                                                    |

**-l  *Maximum memory for caching ZIP-entries in the RAM***

Specifies a limit for the cache.  For example *-l  8G* would limit the size of the cache to 8 Gigabyte.

**-b**
 Run in background


FUSE Options
------------

These come after the colon in the command line.

**-s**

Disable multi-threaded operation. This could rescue ZIPsFS in case of threading related bugs.

**-o *comma separated Options***

**-o allow_other**
Other users are granted access.




Project status
==============

Author: Christoph Gille

Current status: Testing and Bug fixing. Already running very busy for several weeks without interruption.

If ZIPsFS crashes, please send the stack-trace together with the source code you were using.

