# Installation of ZIPsFS


Please visit the page for your OS.

 - MS-Windows: Install ZIPsFS in a WSL environment.
 - [Ubuntu, Debian](./INSTALL_Ubuntu.md)
 - [MacOSX](./INSTALL_MacOSX.md)
 - [FreeBSD](./INSTALL_FreeBSD.md)
 - [NetBSD](./INSTALL_NetBSD.md)
 - [Ubuntu, Debian and friends](./INSTALL_Ubuntu.md)
 - [Solaris (Illumos, Smartos, ...)](./_INSTALL_Solaris.md)

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

    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh

Alternatively, consider  [installation with autotools](./INSTALL_autotools.md) on Linux

# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>


# TroubleShooting


Are the shared libraries found on the system?
The option -fuse3 refers to libfuse3.so. Is this found in the library search paths.
Are the include files in the include file search paths?


## Testing FUSE system

To test  FUSE, one can  check whether it is possible to mount a zip file with fuse-zip.
This test may be performed as a normal user or as ROOT.
The zip file will be mounted on ~/mnt/zip. The content of the zip-file will be accessible from this folder which acts as a mount point.

    mkdir -p ~/mnt/zip ~/test &&  zip -j ~/test/test.zip /etc/os-release &&  fuse-zip ~/test/test.zip ~/mnt/zip

The zip file is mounted here:

    ls ~/mnt/zip
