# ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

 - [Installation](./INSTALL.md)
 - [Configuration](./ZIPsFS_configuration.md)
 - [Generated (synthetic) files: Automatic file conversions, Accessing web resources as regular files](./ZIPsFS_generated_files.md)
 - [Improve performance  caching file content and meta data](./ZIPsFS_cache.md)
 - [Logging](ZIPsFS_logs.md)
 - [Fault management of remote upstream file systems](ZIPsFS_fault_management.md)
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

With a  trailing slash, the folder name is not part of the virtual path.


## Summary


ZIPsFS functions as a union or overlay file system, merging multiple file structures into a unified directory.
This directory presents the underlying files and subdirectories from the specified sources (branches) as a single, cohesive structure.
Any newly created or modified files are stored in the first file location, while all other sources remain read-only, ensuring that their files are never altered.
ZIPsFS treats ZIP files as expandable folders, typically naming them by appending ".Contents/" to the original ZIP file name.
However, this behavior can be customized using filename-based rules. Extensive configuration options allow adjustments. Changes can be applied without disrupting the file system.
Additionally, ZIPsFS includes specialized features and performance optimizations tailored for efficiently storing large-scale mass spectrometry data.


## Example


### First create some example files

    b1=~/test/ZIPsFS/writable/
    b2=~/test/ZIPsFS/branch1/
    b3=~/test/ZIPsFS/branch2/
    b4=~/test/ZIPsFS/branch3

Without trailing slash, the folder name will be retained in the virtual path. This is the case for ~branch3~.  Virtual file paths in that branch will start with ~<mount point>/branch3/~.

    mnt=~/test/ZIPsFS/mnt

    mkdir -p $b1 $b2 $b3 $b4 $mnt

    for c in a b c d e f; do echo hello world $c >$b2/$c.txt; done
    for ((i=0;i<10;i++)); do echo hello world $i >$b3/$i.txt; done

    zip --fifo $b2/zipfile1.zip <(date)  <(echo $RANDOM)
    zip --fifo $b3/zipfile2.zip <(hostname)  <(ls /)
    zip --fifo $b3/20250131_this_is_a_mass_spectrometry_folder.d.Zip   <(seq 10)


### Now start ZIPsFS

    ZIPsFS   $b1 $b2 $b3 $b4 : -o allow_other  $mnt

### Browse the virtual file tree

Open a file browser or another terminal and  browse the files in

    ~/test/ZIPsFS/mnt/

### Create a file in the virtual tree
The first file tree stores files. All others are read-only.

    echo "This file will be stored in ~/test/ZIPsFS/writable "> ~/test/ZIPsFS/mnt/my_file.txt

    cat ~/test/ZIPsFS/mnt/my_file.txt

### Real storage place of the created file

Just append @SOURCE.TXT

    cat ~/test/ZIPsFS/mnt/my_file.txt@SOURCE.TXT

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
metadata file created by appending ***@SOURCE.TXT*** to the filename.

For example, to determine the real location of:

    ~/test/ZIPsFS/mnt/1.txt

Run the following command:

    cat ~/test/ZIPsFS/mnt/1.txt@SOURCE.TXT



## ZIPsFS expands ZIP file entries

By default, ZIP files are displayed as folders with the suffix ***.Content***.
This behavior can be customized in the ***ZIPsFS_configuration.c file***.
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
|   RULE    | ZIP entries are cached according to rules in **configuration.c**.                  |
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

Current status: Testing and Bug fixing
If ZIPsFS crashes, please send the stack-trace together with the source code you were using.

# Use case - Archive of mass spectrometry files


We use closed-source proprietary Windows software to read large experimental data from various types
of mass spectrometry machines. The data is immediatly copied into an intermediate storage on the processing PC and
eventually archived in a read-only WORM file
system.

To reduce the number of individual files and disk usage and to allow for data integrity checks, all files from a single mass spectrometry
measurement are bundled into one ZIP archive. With fewer individual files, searching through the
entire directory hierarchy takes less than 1 hour.

We initially hoped that files inside ZIP archives would be  accessed using

 - Pipes
 - Named pipes
 - Process substitution
 - FUSE file systems with which transparently expand multiple ZIP files
 - Unzipping and storing  extracted files on disk

Unfortunately, these techniques did not work for our use case. Mounting individual ZIP files was initially the only solution. But when sample size
of large experiments got large, even this was not feasable.

ZIPsFS was developed to solve the following problems:

- **Growing Number of ZIP Files**: Recently, the size of our experiments - and therefore the number of ZIP files - has increased enormously. Mounting thousands of individual ZIP files results in a very long <i>/etc/mtab</i> file and puts a significant strain on the operating system.

- **Write Access Requirements**: Some proprietary software requires write access to both files and their parent directories.

- **Inefficiency in Random File Access**: Some mass spectrometry files are read from varying positions. Random access  is particularly inefficient for compressed ZIP entries, in particular with backward seeks. Buffering of file content is required.

- **Multiple Storage Locations**: Experimental records are initially stored in an intermediate storage location and, after verification, are moved to the final archive. Consequently, we need a union file system.

- **Resilience of storage systems:** Sometimes access to the archive gets blocked. Otherwise there are several alternative entry points which will continue to work. This adds requirement for
fault management.

- **Redundant File System Requests**: Some proprietary software generates millions of redundant requests to the file system, which is problematic for both remote files and mounted ZIP files.
File attributes need to be cached.


<SPAN>



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


