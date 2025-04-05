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

For automatically generated files consider installing
  - docker
  - tesseract-ocr-eng
  - imagemagick
  - poppler-utils pdftotext


# If your OS is not listed, continue with the generic instructions here.


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



#  Compile ZIPsFS

To compile ZIPsFS, run

     ./ZIPsFS.compile.sh


# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>


# TroubleShooting


Are the shared libraries found on the system?
The option -fuse3 refers to libfuse3.so. Is this found in the library search paths.
Are the include files in the include file search paths?


# Trouble shooting Detect problems of fuse


If ZIPsFS does not work you need to exclude that there is a general problem of the  FUSE system.
This can be done by testing another FUSE file system like sshfs or fuse-zip.
The following shows how  fuse-zip can be tested. First it needs to be installed. On Debian or Ubuntu type

    sudo apt get install fuse-zip


In the following, an empty folder is created which serves as mount point. Then a ZIP file is made and mounted with fuse-zip.
Finally, the files at the mount point are shown.


    mkdir -p ~/test/fuse-zip/mnt
    zip --fifo  ~/test/fuse-zip/test.zip  <(date)
    fuse-zip  ~/test/fuse-zip/test.zip ~/test/fuse-zip/mnt
    ls -R  ~/test/fuse-zip/mnt



If this fails, and there is a permission problem, try as root.
