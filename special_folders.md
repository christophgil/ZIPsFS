# No operation

Subfolder /-/ provides default view on the file tree.

# Updating the local copy of remote or compressed files

A HTML report is generated for each pre-loaded file found on the local
HD.The paths of the pre-loaded file and the original source will
displayed. Reading the HTML files in this folder, triggers the update
process of the corresponding file. The last-modified file attributes of
the remote and local files will be compared. If the local copy is
out-of-date, it will be refreshed. On success, the copy on the local
disk will be replaced. Success or failure will be reported in HTML.

# Accessing files from the internet

This feature is activated with the macro WITH_INTERNET_DOWNLOAD.\
This feature requires the writable file tree. The directory
[/zipsfs/n](/zipsfs/n) provides access to internet files. The file names
are formed from URLs by replacing the colon and slashes by comma. The
time attributes are taken from the http or ftp headers. The headers are
stored in separate files. The utility curl must be installed on the
computer. [Examples:]{.underline}

-   [/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE](/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE)
-   [/zipsfs/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README](/zipsfs/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README)
-   [/zipsfs/n/https,,,files.rcsb.org,download,1SBT.pdb](/zipsfs/n/https,,,files.rcsb.org,download,1SBT.pdb)
-   [/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml](/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml)
-   [/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,previous_releases,release-2022_05,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606_canonical.fasta](/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,previous_releases,release-2022_05,reference_proteomes,Eukaryota,UP000005640,UP000005640_9606_canonical.fasta)

In the last case, the file is only available as a gz compressed file.
When the \'.gz\' suffix is omitted, the file will be decompressed during
data transfer. Before, the files size is unknown and estimated. To
update already downloaded files see folder_UPDATE\_\
\
The function config_internet_hardlink_filename() may return an
additional file path where the downloaded file is copied to as well. The
current file location is /home/cgille/MS/Keep

# Not expanding ZIP files

The view option VIEWMOD_KEEP_ZIP is activated with the flag z in the
first subfolder in /zipsfs/.\
\
This directory provides access to all files without expanding ZIP
files.\
This also accelerates navigation and searching.

# Decompression of compressed files

The view option VIEWMOD_DECOMPRESS is activated with the flag d in the
first subfolder in /zipsfs/.\
\
This feature is activated with the macro WITH_PRELOADDISK.\
For all upstream compressed (bz2, gz, lrz, xz) files, decompressed
virtual files will be presented. Upon first usage they will be
downloaded. Once downloaded, the file sizes will be correctly displayed.

# The first file branch

The view option VIEWMOD_1 is activated with the flag 1 in the first
subfolder in /zipsfs/.It can be deactivated with -1.\
\
The first root is writable and contains modified, generated or
downloaded files. This file tree contains only those files that reside
in the first root. This feature requires the writable file tree.

# File-conversion - Files generated automatically from other files

The view option VIEWMOD_FILECONVERSION is activated with the flag c in
the first subfolder in /zipsfs/.\
\
This feature is activated with the macro WITH_FILECONVERSION.\
This feature requires the writable file tree.\
The file tree [/zipsfs/c](/zipsfs/c) contains all files along with
dynamically generated files. The files come into existence only when
used. They will be created using commands specified in
ZIPsFS_configuration_fileconversion.c.\

## Initiate file generation

To force file generation, it is sufficient to read at least one byte of
the file. UNIX commands head can be used. For Windows, the script
[/zipsfs/scripts/windowsRead_beginning_of_files.bat](/zipsfs/scripts/windowsRead_beginning_of_files.bat)
is convenient.\

## Unknown file size

As long as the file content has not been generated, the reported file
size is incorrect which might be a problem for some software.

## Removing old files

Preloaded and generated files may be cleaned-up after heaving not been
used for a while. The following script is periodically executed if it
exists:\
\<writable root\>/\
Currently, it does not exist.\
\
A template script ZIPsFS_cleanup.sh is found in the source code.\
\
Deletion of files can be delayed by setting the
[last-access-time]{.underline} to the current time or the future with
the following scripts:

-   [/zipsfs/scripts/UNIXSet_file_access_time.command](/zipsfs/scripts/UNIXSet_file_access_time.command)

Also See ZIPsFS_configuration_c.c

# Preloading to RAM

The preload option PRELOADRAM is activated with the flag m in the second
subfolder in /zipsfs/. Flags take precedence over configuration. .\
\
This feature is activated with the macro WITH_PRELOADDISK.\
Preloading to RAM is fast and does not use the local disk. It is not
good for files with long transfer times which are used more than once.
PRELOADRAM can be activated by file name based rules in
ZIPsFS_configuration.c. The flags in the folder name take precedence.

# Preloading the entire ZIP

The preload option PRELOADDISK_ENTIRE_ZIP is activated with the flag z
in the second subfolder in /zipsfs/.It can be deactivated with -z. This
flag is equivalent to the root tree property preload_entire_zip Flags
take precedence over file tree properties. .\
\
For remote paths, entire ZIP files will be cached on local disk.
Consider the following two example files that are available when ZIPsFS
is started with ../testing/testfiles/ZIPsFS_testfiles_start_ZIPsFS.sh.\
\

-   head
    mnt/db/pride/2022/12/PXD036786/20220121_Z1_ZW_001_30-0043_K562_SWATH_3.91ng_3.wiff2
-   head
    mnt/db/pride/2026/07/PXD062492/20231031_PRO3_KTT_174_50-0121_AstraZeneca-PROTAC_P07_B09.d/analysis.tdf

Both files originate from a FUSE mounted FTP site. Extraction of large
ZIP entries with rclone, AVFS and curlftpfs is problematic. As a
workaround, ZIPsFS will fetch the entire ZIP file with /usr/bin/curl.\
\
Whether ZIPsFS internally saves the ZIP entry as a file or not depends
on whether preloading into RAM is advised in ZIPsFS_configuration.h and
ZIPsFS_configuration.c. Only in the first example, the ZIP entry is
extracted and cached as a separate file. With the second example, files
with the ending .tdf are preloaded into RAM.

# Preload files to local disk

The preload option PRELOADDISK_NOT is activated with the flag -l in the
second subfolder in /zipsfs/.This flag is equivalent to the root tree
property preload=0 Flags take precedence over file tree properties. .\
\
This feature is activated with the macro WITH_PRELOADDISK.\
Caching on local disk is preferred over RAM in case of long transfer
times. File used multiple times are transferred only once. This feature
requires the writable file tree.

## Mark entire file branch

In the command line to start ZIPsFS, root paths can be followed by the
property preload   \@preload.gz   \@preload.bz2   \@preload.xz  
\@preload.Z   \@preload.lrz\

## Update

Preloaded files can be updated by reading the content of the
corresponding HTML files in the UPDATE folder.\
\

## Removing old files

Preloaded and generated files may be cleaned-up after heaving not been
used for a while. The following script is periodically executed if it
exists:\
\<writable root\>/\
Currently, it does not exist.\
\
A template script ZIPsFS_cleanup.sh is found in the source code.\
\
Deletion of files can be delayed by setting the
[last-access-time]{.underline} to the current time or the future with
the following scripts:

-   [/zipsfs/scripts/UNIXSet_file_access_time.command](/zipsfs/scripts/UNIXSet_file_access_time.command)

# Selectors for preloading

The preload selector PRELOAD_SELECT_ALL is activated with the flag a in
the third subfolder in /zipsfs/.\
\

-   Subfolder /a/ All files
-   Subfolder /r/ Remote files
-   Subfolder /z/ Files that are ZIP items
