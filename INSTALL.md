# Installation of ZIPsFS


Please visit the page for your OS.

 - MS-Windows: Install ZIPsFS in a WSL environment.
 - [Ubuntu, Debian](./_INSTALL_Ubuntu.md)
 - [MacOSX](./_INSTALL_MacOSX.md)
 - [FreeBSD](./_INSTALL_FreeBSD.md)
 - [NetBSD](./_INSTALL_NetBSD.md)
 - [Ubuntu, Debian](./_INSTALL_Ubuntu.md)


If your OS is not listed, continue reading here.


First install the required libraries.

 - libfuse3
 - libzip

A ready-to-use executable for Linux (amd64) is found in the folder
[RELEASE](./RELEASE/). It may or may not work on your system.





# Dependencies

ZIPsFS is written in standard C according to the POSIX industry strandard.
It has been developed on Linux.
Installation works with
both compilers, gcc and clang.

First install the required libraries and packages.

 - bash
 - libfuse3
 - libzip
 - libfuse3-dev
 - libzip-dev
 - build-essential (The C compiler and make)





# Quick installation

    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh

Alternatively, consider  [installation with autotools](./_INSTALL_autotools.md)

# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>


# TroubleShooting


Are the shared libraries found on the system?
The option -fuse3 refers to libfuse3.so. Is this found in the library search paths.
Are the include files in the include file search paths?

Maybe this file requires adaptations for your OS:

    cg_os_dependencies.h
