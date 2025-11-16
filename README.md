# ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

 - [Installation](./INSTALL.md)

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

pip install grip
-->


# Example usage


    ZIPsFS [ZIPsFS-options] path-of-branch1/  branch2/  branch3/  branch4/  :  [fuse-options] mount-point


# Summary


ZIPsFS functions as a **union or overlay** file system, merging multiple file structures into a unified directory.
This directory presents the underlying files and sub-directories from the specified sources (branches) as a single, cohesive structure.
Any newly created or modified files are stored in the first file location, while all other sources remain read-only, ensuring that their files are never altered.
ZIPsFS treats **ZIP files as expandable folders**, typically naming them by appending ``.Contents/`` to the original ZIP file name.
However, this behavior can be customized using filename-based rules. Extensive configuration options allow adjustments. Changes can be applied without disrupting the file system.

With a trailing slash, the folder name is not part of the virtual path, in accordance with  the trailing slash semantics of many UNIX tools.

ZIPsFS includes specialized features like **automatic file conversions** and performance
optimizations tailored for efficiently storing and accessing **large-scale mass spectrometry** data.
It manages non-sequential file reading from varying positions (so-called file seek) efficiently to
improve reading of remote and zipped files. It has a programming interface to create synthetic file
content programmatically.

ZIPsFS is best run in a **tmux** session.

# Mini tutorial

   Please [Install](./INSTALL.md)  ZIPsFS.


### Create some example files

Without trailing slash, the folder name will be retained in the virtual path. This is the case for ``branch3``.
Virtual file paths in that branch will start with *mount point*``/branch3/``.

    b1=~/test/ZIPsFS/writable/
    b2=~/test/ZIPsFS/branch1/
    b3=~/test/ZIPsFS/branch2/
    b4=~/test/ZIPsFS/branch3

    mkdir -p $b1 $b2 $b3 $b4 ~/test/ZIPsFS/mnt

    for c in a b c d e f; do echo hello world $c >$b2/$c.txt; done
    for ((i=0;i<10;i++)); do echo hello world $i >$b3/$i.txt; done

    zip --fifo $b2/zipfile1.zip <(date)  <(echo $RANDOM)
    zip --fifo $b3/zipfile2.zip <(hostname)  <(ls /)
    zip --fifo $b3/20250131_this_is_a_mass_spectrometry_folder.d.Zip   <(seq 10)


### Start ZIPsFS
In production, it is recommended to start ZIPsFS in *tmux*. For testing, just use your regular command line.

    ZIPsFS   $b1 $b2 $b3 $b4  :  ~/test/ZIPsFS/mnt

### Browse the virtual file tree

Open a file browser or another terminal and  browse the files in

    ~/test/ZIPsFS/mnt/

### Create a file in the virtual tree
The first file tree stores files. All others are read-only.

    echo "Hello" > ~/test/ZIPsFS/mnt/my_file.txt

    cat ~/test/ZIPsFS/mnt/my_file.txt

To get the real storage place of the file, append ``@SOURCE.TXT``

    cat ~/test/ZIPsFS/mnt/my_file.txt@SOURCE.TXT

### Access web resources as regular files
Make sure the UNIX tool curl is installed.

    curl --version

Note that "//:" and all slashes in the URL are replaced by commas.

    less  ~/test/ZIPsFS/mnt/ZIPsFS/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE

<details><summary>Decompress on download</summary>



With the virtual folder ``/ZIPsFS/n/gz``, a gz file is download and decompressed. In the following
case the URL is https://files.rcsb.org/download/1SBT.pdb.gz. Not that ``.gz`` is  appended to the URL.

    head  ~/test/ZIPsFS/mnt/ZIPsFS/n/gz/https,,,files.rcsb.org,download,1SBT.pdb

To download without decompression, just use the default folder ``/ZIPsFS/n/``.

    gunzip -c  ~/test/ZIPsFS/mnt/ZIPsFS/n/https,,,files.rcsb.org,download,1SBT.pdb.gz

Here,  the HTTP header does not have a ``Content-Length`` field. Bevore the file is downloaded, its file size is not known. ZIPsFS will display a
wrong file size which might be a problem for some software.

</details>


# DESCRIPTION



## ZIPsFS is a Union / overlay file system


ZIPsFS functions as a union (overlay) file system.
When files are created or modified, they are stored in the first file tree.
If a file exists in multiple source locations, the version from the leftmost source (the first one listed) takes precedence.
With an empty string as the first source,  the ZIPsFS file system read-only and file creation and modification is disabled.

The physical file path, i.e., the actual storage location of a file, can be retrieved from a
file formed by appending ``@SOURCE.TXT`` to the filename.
For example, to determine the real location of:
    ``~/test/ZIPsFS/mnt/1.txt``
Run the following command:

    cat ~/test/ZIPsFS/mnt/1.txt@SOURCE.TXT

If a remote upstream file system stops responding, the current file access is blocked (Unless the options ``WITH_ASYNC_READDIR`` ... are activated).
After some time, unresponsive upstream file trees will be skipped to prevent that file accesses get blocked.

## ZIPsFS expands ZIP file entries

By default, ZIP files are displayed as folders with the suffix ``.Content``.
This behavior can be customized.
The default configuration includes a few exceptions tailored to specific use cases in Mass Spectrometry:

  - For ZIP files whose names end with *.d.Zip*, the virtual folder will  end with *.d*.

  - Flat file list: For  Sciex instruments, each mass spectrometry record  is stored in a set of files which are not organized in
    sub-folders. For example the record ``20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01``  stored in ``20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01.wiff2.Zip`` may consist of the following files

     - 20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01.timeseries.data
     - 20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01.wiff
     - 20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01.wiff2
     - 20231122_MA_HEK_QC_hiFlow_2ug_SWATH_rep01.wiff.scan

    These are contained with those of other entries in the file listing.
    To get the full list of files,  all ZIP files need to be inspected.
    This is time consuming and performed in the background. Consequently, the file listing will be incomplete when requested for the first time.
    Only after some seconds, the file listing will be presented properly.

## ZIPsFS Options


**-h**

Prints brief usage information.



**-s *path-of-symbolic-link***
This is discussed in section Configuration.



**-c \[NEVER,SEEK,RULE,COMPRESSED,ALWAYS\]**

Policy when ZIP entries and file content is cached in RAM.


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
File content larger than this will not be cached. When memory usage is high, cached file access waits until it drops below this value.

**-b**
 Execution in background (Not recommended). We recommend running ZIPsFS in foreground in *tmux*.


## FUSE Options


Options for the FUSE system  come after the **colon** in the command line.

**-o *comma separated Options***

**-o allow_other**
Other users are granted access.




<details><summary>Project status</summary>

Author: Christoph Gille

**Current status**: Testing and Bug fixing. Already running very busy for several weeks without interruption.


If ZIPsFS crashes, please send the stack-trace together with the source code you were using.

</details>


<details><summary>Configuration</summary>
ZIPsFS can be customized:

 - optional features can be (de)-activated with  preprocessor macros "WITH_SOME_FEATURE"  which take the values 0 or 1.
 - Rules can be given
     - which files are cached in the main memory
     - which ZIP entries are inlined
 - Timeout values for accessing (remote) files
 - Automatic file conversions

Configuration files of ZIPsFS, are files written in the programming language C.
They have the prefix **ZIPsFS_configuration** and the suffix **.h** of **.c**.


ZIPsFS is customized for our needs - accessing archived high throughput data such that it can be
directly used for mass spectrometry software. These settings can be used as a sample to customize it
for other needs.


Changes require recompilation and take effect after restart of ZIPsFS.

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
</details>
<details><summary>Generated (synthetic) files: Automatic file conversions, Accessing web resources as regular files</summary>
Computations often require files from public repositories.
Files from the internet (http, ftp, https) can be accessed as files using the URL as file name. ZIPsFS takes care of downloading and updating.
They are immutable and cannot be modified  unintentionally.
In DOS, a trailing colon is a signature for device names. Therefore, the colon and all slashes in the URL need to be replaced by comma.
Comma  has been chosen as a replacement because it normally does not  occur in URLs. Furthermore, it does not require quoting in UNIX shells.

Example with *mnt/*  denoting the  mountpoint of the ZIPsFS file system:

    sudo apt-get install curl
    ls -l  mnt/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README
    more   mnt/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README
    head   mnt/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README@SOURCE.TXT

To see the real local file path append ***@SOURCE.TXT*** to the file path.

The http-header is updated according to a time-out rule in **ZIPsFS_configuration.c**.
Whether the file itself needs updating is decided upon the *Last-Modified* attribute in the http or ftp header.

Additionally, the file is accessible with a file-name containing the data in the header.
This feature can be conditionally deactivated.


This works also when the FUSE file system is accessed remotely  via SMB or NFS.
However, Windows PCs fail to access these files. This is because files do not exist for Windows, when they are not listed in the file list of the parent.

## Generation of files using programming language C

By modifying the file *ZIPsFS_configuration_c.c*, users can easily implement
files where the file content is generated dynamically using the programming language C.

Here is a predefined minimal example which explains how it works:

    <mount point>/example_generated_file/example_generated_file.txt



## Automatic Virtual File Generation and Conversion Rules

ZIPsFS can generate and display virtual files automatically. This feature is enabled by setting the preprocessor macro **WITH_AUTOGEN** to **1** in *ZIPsFS_configuration.h*.
Generated files are stored in the first file branch, allowing them to be served instantly upon repeated requests.
A common use case for this feature is file conversion. The default rules, defined in *ZIPsFS_configuration_autogen.c*, include:

- **Image files (JPG, JPEG, PNG, GIF):**  Smaller versions at 25% and 50% scaling.
- **Image files (OCR):** Extracted text using Optical Character Recognition (OCR).
- **PDF files:** Extracted ASCII text.
- **ZIP files:** Consistency check reports, including checksums.
- **Mass spectrometry files:**  **mgf (Mascot)** and **mzML** formats.
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

Example Fragpipe: Fragpipe is a software to process mass-spectrometry files. Processing
Thermo-Fisher mass-spectrometry files with the suffix raw, those are converted by Fragpipe into the
free file format mzML.  Since ZIPsFS can also convert raw files to mzML, we tried to give the
virtual mzML files as input. Initially, their reported file size is 99,999,999,999 Bytes.  This
large number was chosen to make sure that the estimated file size is larger than the real yet
unknown size. Initially Fragpipe attempts to read some bytes from the end of the file.  To determine
the reading position, it uses the overestimated file size. In this specific case it tried to read at
file position 99,999,997,952.  ZIPsFS will perform the conversion when serving the first read
request.  Since the converted mzML file is much smaller than the read position, there will be no
data and Fragpipe will fail. When however, at least one byte of the mzML files is read to initiate the
conversion process before Fragpipe is started, computation will succeeds.
</details>
<details><summary>Logging</summary>
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
</details>
<details><summary>Fault management. Timouts for remote upstream file systems. Duplicated remote trees</summary>
Accessing remote files inherently carries a higher risk of failure. Requests may either:

 - Fail immediately with an error code, or

 - Block indefinitely, causing potential hangs.

In many FUSE file systems, a blocking access can render the entire virtual file system unresponsive.
ZIPsFS addresses this with built-in fault management for remote branches.

Remote roots in ZIPsFS are specified using a double-slash prefix, similar to UNC paths (//server/share/...).
Each remote branch is isolated in terms of fault handling and threading and has its own thread pool, ensuring faults in one do not affect others.
To avoid blocking the main file system thread, remote file operations are executed asynchronously in dedicated worker threads.

## Timeouts

ZIPsFS remains responsive even if a remote file access hangs.
The fuse thread delegates the file operation to another thread and waits for its completion.
After the configurable timeout it gives up.

## Duplicated file paths

For redundantly stored files (i.e., available on multiple branches), another branch may take over
transparently if one fails or becomes unresponsive.


## Blocked worker threads
The worker thread may block permanently. In this case it can be killed automatically and restarted. However killing this thread sometimes does not work.

If the stalled thread cannot be terminated, ZIPsFS will not create a new thread.
To check whether all threads are responding, activate logging. For details see

    ~/.ZIPsFS/.../log_flags.conf.readme

This is best resolved by restarting ZIPsFS without interrupting ongoing file accesses.









## Debug Options

### The ZIPsFS option  **-T**

Checks whether ZIPsFS can generate and print a backtrace in case of errors or crashes.  This feature
elies on external tools to translate memory addresses into source code locations: On Linux and
FreeBSD, it uses addr2line, typically located in /usr/bin/.  On macOS, it uses the atos tool
instead.  Ensure these tools are installed and accessible in your system's PATH for backtraces to
work correctly.

See ZIPsFS.compile.sh for activation of sanitizers.
</details>
<details><summary>Improve performance  caching file content and meta data</summary>
&nbsp;
## File content cache


ZIPsFS optionally supports caching specific files and ZIP entries entirely in RAM, allowing data segments to
be served from memory in any order.
This feature significantly improves performance for software that performs random-access reads for remote files and for
ZIP entries.

The ``-l`` option sets an upper limit on memory usage for the ZIP RAM cache.
When available memory runs low, ZIPsFS can either pause,  proceed without caching file data or just ignore the
memory restriction depending on the configuration.
These caching behaviors - such as which files to cache and how to handle memory pressure - are defined in the configuration.


## File attribute cache

Additional caching mechanisms are designed to accelerate file listing in large directories for ZIP entries.





## Data Integrity for ZIP Entries

For ZIP entries loaded entirely into RAM:
ZIPsFS performs CRC checksum validation.
Any detected inconsistencies are logged, helping to detect corruption or transmission errors.
</details>
<details><summary>Use case - Archive of high throughput mass spectrometry files</summary>

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

</details>
<details><summary>Implementation</summary>
ZIPsFS is written in standard C for UNIX and Linux computer systems.

# Dependencies
- libfuse
- libzip

For OS other than LINUX, some minor adaptions will be required.

# Language

ZIPsFS is written in Gnu-C because libfuse is.
Using other languages  may affect performance at the native interface.
This is particularly the case when data blocks need to be copied.
Dynamic linking of native libraries may be worse than static linking. In Java JNA is likely to be much slower than JNI.

Playing with Java and Python, it seemed that the parameter
<I>struct fuse_file_info *fi</I> cannot be used as a key to find cached data.

Python is lacking a method </I>read(buffer,offset,size)</I>. Instead, a new buffer object is created whenever a new 128k block of data is read. These many blocks need to be garbage collected.

For Therefore reasons  we decided to use GNU-C despite its many draw-backs.

# Roots

When a client program requests a file from the virtual FUSE file system, the virtual file path needs to be translated into an existing real file or ZIP entry.

All given roots are iterated  until the file or ZIP entry  is found.
The first root is the only that allows file modification and file creation.
All others are read only.

# Caches


## File attribute and file content caches in ZIPsFS

There are several caches for file attributes and file contents to improve performance.  They can be
deactivate using conditional compilation. The respective switches and detailed description are found
in <I>ZIPsFS_configuration.h</I>. Searching for the names of the switches allows to find the places
of implementation in the source code. Code concerning caches are bundled in <I>ZIPsFS_cache.c</I>

The caches are necessary because

 - Some mass spectrometry software reads files not sequentially but chaotically.
 - The same software excerts thousands and millions of redundand file requests
 - Beside displaying ZIP files as subdirectories containing and the ZIP entries as files in those folders, there is an option for
   displaying all entries of all ZIP files  flat in one directory.
   To list this directory sufficiently fast requires a file entry cache.

## The file cache of the OS
After closing a file or disposing the memcache of a ZIP entry, a call to  <I>posix_fadvise(fd,0,0,POSIX_FADV_DONTNEED);</I> may remove the file from the
file cache of the OS when it is unlikely that the same file will be used in near future.  This can be  customized in <I>ZIPsFS_configuration.c</I>.

## The cache in libfuse

See  field <I>struct fuse_file_info-&gt;keep_cache</I> in function <I>xmp_open()</I>.

# Infinite loops running in own threads

- infloop_statqueue(): Calling stat asynchronously. Calling statfs() periodically to see whether the respective root is responding.
- infloop_memcache(): Loading entire ZIP entries into RAM asynchronously. One thread per root.
- infloop_dircache(): Reading directory listings and ZIP file entry listings asynchronously. One thread per root.
- infloop_unblock():  Unblock blocked threads by calling <i>pthread_cancel()</i>. They re-start automatically with a <i>pthread_cleanup_push()</i> hook. One global thread.

# When a source file system becomes unavailable

Typically, overlay or union file systems are affected when one of the source file systems is not responding.
Remote sources are at risk of failure when for example network problems occur.
The worst case is that a call to the  file API does not return. Consequently, the specific file access or even the entire FUSE file system is blocked.

Our solution is to perform such calls in a separate thread. This requires a queue to add the specific request and an infinite loop that picks these requests from the queue and completes them.
These infinite loops are run in separate threads for each root. They are listed above.

Only <i>read(int fd, void *buf, size_t count)</i> and the equivalent for ZIP entries are called directly suche that a slight risk of blocking remains.

When paths of roots  are given with leading double slash <b>//</b> (according to a convention from  MS Windows),
they are treated specially. They are used only when they have successfully run <i>fsstat()<i> recently in <i>infloop_statqueue()</i>.
</details>
<details><summary>Limitations and Bugs</summary>
There are some  Limitations:


### Software

Seems not to work for Thermo raw files in Fragpipe - under investigation

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
</details>
<details><summary>See also related sites</summary>
- https://github.com/openscopeproject/ZipROFS
- https://github.com/google/fuse-archive
- https://bitbucket.org/agalanin/fuse-zip/src
- https://github.com/google/mount-zip
- https://github.com/cybernoid/archivemount
- https://github.com/mxmlnkn/ratarmount
- https://github.com/bazil/zipfs
</details>

 - [Installation](./INSTALL.md)
