## Accessing internet files

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
