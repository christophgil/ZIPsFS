# ZIPsFS - FUSE-based  overlay file system which expands  ZIP files
See [Installation](./INSTALL.md)

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



    ZIPsFS [ZIPsFS-options] path-of-branch1  branch2  branch3  branch4  :  [fuse-options] mount-point


## Example


### First create some example files

    b1=~/test/ZIPsFS/writable
    b2=~/test/ZIPsFS/branch1
    b3=~/test/ZIPsFS/branch2
    mnt=~/test/ZIPsFS/mnt

    mkdir -p $b1 $b2 $b3 $mnt

    for c in a b c d e f; do echo hello world $c >$b2/$c.txt; done
    for ((i=0;i<10;i++)); do echo hello world $i >$b3/$i.txt; done

    zip --fifo $b2/zipfile1.zip <(date)  <(echo $RANDOM)
    zip --fifo $b3/zipfile2.zip <(hostname)  <(ls /)
    zip --fifo $b3/20250131_this_is_a_mass_spectrometry_folder.d.Zip   <(seq 10)


### Now start ZIPsFS

    ZIPsFS   $b1 $b2 $b3 : -o allow_other  $mnt

### Browse the virtual file tree

Open a file browser or another terminal and  browse the files in

    ~/test/ZIPsFS/mnt/

### Create a file in the virtual tree

    echo "This file will be stored in ~/test/ZIPsFS/writable "> ~/test/ZIPsFS/mnt/my_file.txt

    cat ~/test/ZIPsFS/mnt/my_file.txt

### Real storage place of the created file

    ls -l ~/test/ZIPsFS/writable/

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

ZIPsFS typically runs as a foreground process.  To keep it active and monitor its output, it is
recommended to use a persistent terminal multiplexer such as tmux. This enables continuous
observation of all messages and facilitates long-running sessions.
Additional log files are stored in:

    ~/.ZIPsFS

For each mount point there are files specifying more  logs.

    log_flags.conf

See readme for details:

    log_flags.conf.readme


ZIPsFS dynamically generates an HTML status file within the virtual file system.
You can find it under the path: <Mount-Point>/ZIPsFS/
For example:

    ~/test/ZIPsFS/mnt/ZIPsFS/file_system_info.html

This file provides real-time information about the system’s current state.

## Configuration

Configuration files, identified
by the prefix **ZIPsFS_configuration**, are written in C. Any changes require recompilation and a
restart of ZIPsFS to take effect.

With the -s option, the updated ZIPsFS can seamlessly replace running instances without disrupting the virtual file system.

To illustrate how this works, let MNT represent the apparent mount point of the FUSE file system.
Suppose we are in the parent directory of MNT, enabling the use of relative paths.
Users access files through this apparent mount point, but in reality, MNT is a symbolic link to the actual mount point.
The real mount point is not directly accessed by users, as it changes each time a new instance of ZIPsFS is launched.

For example, assume the obsolete ZIPsFS instance is mounted at ./.mountpoints/MNT/1.
When a new instance replaces it, it may use any empty directory as   mount point. ZIPsFS must be started with the following command line  option:

    -s MNT

Once the new instance is running, the symbolic link is updated to point to the new mount
location. From the user's perspective, nothing changes - the apparent mount point remains MNT. To
ensure uninterrupted access, the obsolete instance should remain active for a short period to allow
ongoing file operations to complete.

If MNT  is within an exported  SAMBA or NFS path the real mount points should be in the exported file tree as well.
Include into */etc/samba/smb.conf*:

    follow symlinks = yes



## Union / overlay file system


ZIPsFS functions as a union (overlay) file system.
When files are created or modified, they are stored in the first file tree - e.g.,

    ~/test/ZIPsFS/writable

in the example setup.
If a file exists in multiple source locations, the version from the leftmost source (the first one listed) takes precedence.
To make the file system read-only, you can specify an empty string ("") as the first source. This disables file creation and modification and automatic virtual file generation.


## ZIP file Presentation

By default, ZIP files are displayed as folders with the suffix ***.Content***.
This behavior can be customized in the ***ZIPsFS_configuration.c file***.
The default configuration includes a few exceptions tailored to specific use cases in Mass Spectrometry Compatibility:

  - For ZIP files whose names start with a year and end with *.d.Zip*, the virtual folder will instead
    end with *.d*.

  - Flat File Display: For some mass spectrometry formats where files are not organized into
    subfolders within the ZIP archive, the contents are shown directly in the file list, rather than
    as a nested folder.


## File content cache



ZIPsFS optionally supports caching specific ZIP entries entirely in RAM, allowing data segments to
be served from memory in any order.
This feature significantly improves performance for software that performs random-access reads.
The ***-l*** option sets an upper limit on memory usage for the ZIP RAM cache.
When available memory runs low, ZIPsFS can either pause,  proceed without caching file data or just ignore the
memory restriction depending on the configuration.
These caching behaviors - such as which files to cache and how to handle memory pressure - are defined in the configuration.


## File attribute cache


Additional caching mechanisms are designed to accelerate file listing in large directories.


## Real file location


The physical file path, i.e., the actual storage location of a file, can be retrieved from a special
metadata file created by appending ***@SOURCE.TXT*** to the filename.

For example, to determine the real location of:

    ~/test/ZIPsFS/mnt/1.txt

Run the following command:

    cat ~/test/ZIPsFS/mnt/1.txt@SOURCE.TXT

Unfortunately, on Windows clients, these metadata files are inaccessible because they do not appear in directory listings.

## Automatic Virtual File Generation and Conversion Rules




ZIPsFS can generate and display virtual files automatically. This feature is enabled by setting the preprocessor macro **WITH_AUTOGEN** to **1** in *ZIPsFS_configuration.h*.
Generated files are stored in the first file branch, allowing them to be served instantly upon repeated requests.
A common use case for this feature is file conversion. The default rules, defined in *ZIPsFS_configuration_autogen.c*, include:

- **Image files (JPG, JPEG, PNG, GIF):**  Smaller versions at 25% and 50% scaling.
- **Image files (OCR):** Extracted text using Optical Character Recognition (OCR).
- **PDF files:** Extracted ASCII text.
- **ZIP files:** Consistency check reports, including checksums.
- **Mass spectrometry files:**  **mgf (Mascot)** and **msML** formats.
- **wiff files:** Extract ASCII text.
- **Apache Parquet files:**  **TSV** and **TSV.BZ2** formats.



For testing, copy an image file with the following command:

    cp file.png ~/test/ZIPsFS/mnt/

Auto-generated files can be viewed in the example configuration by listing the contents of:

    ls ~/test/ZIPsFS/mnt/ZIPsFS/a/


Note that some of the conversions may require Docker support.  ZIPsFS must be run by a user belonging to the *docker* group.


### Handling Unknown File Sizes in Virtual File Systems

The system cannot determine the size of files whose content has not yet been generated.
In kernel-managed virtual file systems such as */proc* and */sys*, virtual files typically report a size
of zero via *stat()*. Despite this, they are not empty and  contain dynamically generated content when read.

However, this behavior does not translate well to FUSE-based file systems.

For FUSE, returning a file size of zero to represent an unknown or dynamic size is not
recommended. Many programs interpret a size of 0 as an empty file and will not attempt to read from
it at all.
In ZIPsFS,  a placeholder or estimated size is returned if the file content has not been generated  at the time of stat().
The estimate should be large enough to allow reading the full content.
If the size is underestimated, data may be read incompletely, leading to truncated output or application errors.
This workaround allows programs to read the file as if it had content,
even though the size isn’t known in advance.
However, it may still break software that relies on accurate size reporting for buffering or memory allocation.



### Windows Console Compatibility: External Queue Workaround

Some Windows command-line executables do not behave reliably when launched directly from compiled programs.
This issue stems from  Windows Console API which is used in long-running mass spectrometry CLI programs to implement progress reports.
Like traditional  escape sequences, the Windows Console API allows free cursor positioning.
In headless environments, i.e. ZIPsFS not started from a desktop environment,
respective  programs block unless without a  console device. A virtual  frame-buffer like ***xvfb*** can solve this issue.

Nevertheless, programs may still not be runnable using the UNIX fork() and exec() paradigm.
To work around this, ZIPsFS supports delegating such tasks to an external shell script.
When the special symbol ***PLACEHOLDER_EXTERNAL_QUEUE*** is specified instead of a direct executable path, ZIPsFS:

 - Pushes the task details to a queue.
 - Waits for the result.

The actual execution of these tasks is handled by the shell script ZIPsFS_autogen_queue.sh,
which must be started manually by the user. This script polls the queue and performs the requested conversions or operations.
Multiple instances of the script can run in parallel, allowing concurrent task handling.



ZIPsFS Options
--------------

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


Debug Options
-------------

**-T**

Checks whether ZIPsFS can generate and print a backtrace in case of errors or crashes.  This feature
elies on external tools to translate memory addresses into source code locations: On Linux and
FreeBSD, it uses addr2line, typically located in /usr/bin/.  On macOS, it uses the atos tool
instead.  Ensure these tools are installed and accessible in your system's PATH for backtraces to
work correctly.



See ZIPsFS.compile.sh for activation of sanitizers.

FUSE Options
------------

These come after the colon in the command line.

**-s**

Disable multi-threaded operation. This could rescue ZIPsFS in case of threading related bugs.

**-o *comma separated Options***

**-o allow_other**
Other users are granted access.


## Fault Management for Remote File Access

Accessing remote files inherently carries a higher risk of failure. Requests may either:

 - Fail immediately with an error code, or

 - Block indefinitely, causing potential hangs.

In many FUSE file systems, a blocking access can render the entire virtual file system unresponsive.
ZIPsFS addresses this with built-in fault management for remote branches.

Remote roots in ZIPsFS are specified using a double-slash prefix, similar to UNC paths (//server/share/...).
Each remote branch is isolated in terms of fault handling and threading and has its own thread pool, ensuring faults in one do not affect others.
To avoid blocking the main file system thread, remote file operations are executed asynchronously in dedicated worker threads.

ZIPsFS remains responsive even if a remote file access hangs.  For redundantly stored files (i.e.,
available on multiple branches), another branch may take over transparently if one fails or becomes
unresponsive.

If a thread becomes unresponsive, ZIPsFS will try to terminate the stalled thread after a timeout.
As soon as the old thread does not exist any more, a new thread is started, attempting to restore
functionality to the affected branch.

If the stalled thread cannot be terminated, ZIPsFS will not create a new thread.
To check whether all threads are responding, activate logging. For details see

    ~/.ZIPsFS/.../log_flags.conf.readme

This is best resolved by restarting ZIPsFS without interrupting ongoing file accesses.

Another possibility is to start a new thread irrespectively of the still existing blocked thread.
There is a shell script ***ZIPsFS_CTRL.sh*** for this in ***~/.ZIPsFS/***.  When the blocked thread
which had been scheduled for termination wakes up, it will be terminated by the system.  However,
there is no guaranty that termination will be perforemd immediately.  For a short time, the two
threads may be active concurrently with  undefined behaviour and the risk of segmentation faults.


## Data Integrity for ZIP Entries

For ZIP entries loaded entirely into RAM:
ZIPsFS performs CRC checksum validation.
Any detected inconsistencies are logged, helping to detect corruption or transmission errors.





## LIMITATIONS

### Hard Links

Hard links are not supported, though symlinks are fully functional.

### Deleting Files

Files can only be deleted if their physical location resides in the first source. Files located in
other branches are accessed in a read-only mode, and deletion of these files would require a
mechanism to remove them from the system, which is currently not implemented.

If you require this functionality, please submit a feature request.

### Reading and Writing

Simultaneous reading and writing of a file using the same file descriptor will only function
correctly for files stored in the writable source.

BUGS
====

Current status: Testing and Bug fixing
If ZIPsFS crashes, please send the stack-trace together with the source code you were using.

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


