# Installation of ZIPsFS


For FreeBSD, NetBSD and MacOSX please see instructions in [INSTALL_other_than_Linux](./INSTALL_other_than_Linux.md)


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
 - autoconf




# Quick installation

    wget -N  https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    unzip main.zip
    cd ZIPsFS-main
    src/ZIPsFS.compile.sh



# Installation from source code with autotools

If above did not work, try this:
This will currently install only for systems with fuse3 i.e. Linux and FreeBSD
    ./configure
    cd src
    make


# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>


# TroubleShooting

Solving problems might be easiest  by adapting the simple installation script.

    src/ZIPsFS.compile.sh

Are the shared libraries found on the system?
The option -fuse3 refers to libfuse3.so. Is this found in the library search paths.
Are the include files in the include file search paths?



Maybe this file requires adaptations for your OS:

    cg_os_dependencies.h
