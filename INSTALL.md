# Installation of ZIPsFS


Please visit the page for your OS.

 - MS-Windows: Install ZIPsFS in a WSL environment.
 - Linux
   - [Ubuntu, Debian and friends](./INSTALL_Ubuntu.md)
 - BSD
   - [MacOSX](./INSTALL_MacOSX.md)
   - [FreeBSD](./INSTALL_FreeBSD.md)
   - [NetBSD](./INSTALL_NetBSD.md)
   - [OpenBSD](./INSTALL_OpenBSD.md)
 - Solaris / Illumos
   On  Solaris getting FUSE file systems to work seems tricky.
   More work is needed to understand the permission. At least, ZIPsFS compiles.
   - [Omnios](./INSTALL_Omnios.md) Compiles. No files are seen at mount point.
   - [OpenIndiana](./INSTALL_OpenIndiana.md)  Works as root, but not as normal user.

For the auto-generated files consider installing
  - tesseract-ocr-eng
  - imagemagick
  - poppler-utils pdftotext

If your OS is not listed, continue reading here.

First install the required libraries.

 - libfuse3
 - libzip

A ready-to-use executable for Linux (amd64) is found in the folder
[RELEASE](./RELEASE/). It may or may not work on your system.





# Dependencies

ZIPsFS is written in standard C according to the POSIX industry strandard.
It has been developed on Linux and works on NetBSD, FreeBSD and MacOSX

First install the required libraries and packages.

 - bash unzip tmux
 - libfuse3 or libfuse2
 - libzip
 - C compiler gcc or clang
 - binutils       (Provides /usr/bin/addr2line, which is used for debugging.  Back-traces show location in source code)
 - It does not need the GNU extensions






# Quick installation


Alternatively, consider  [installation with autotools](./INSTALL_autotools.md) on Linux

# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>


# TroubleShooting


Are the shared libraries found on the system?
The option -fuse3 refers to libfuse3.so. Is this found in the library search paths.
Are the include files in the include file search paths?


