# Installation of executable


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

For FreeBSD, NetBSD and MacOSX please see instructions in [INSTALL_other_than_Linux](./INSTALL_other_than_Linux.md)



# Quick installation from source code

Download

    wget -N  https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    unzip main.zip
    cd ZIPsFS-main

First give  the quick installation script a try:

    src/ZIPsFS.compile.sh

If this does not work, proceed:

# Installation from source code with autotools

This is not yet perfect.
For NetBSD and Apple you need to replace fuse3 by fuse in configure.ac.



The following will Re-generating the file ./configure from configure.ac and Makefile.am
This is usually not necessary.

    aclocal
    autoconf
    automake --add-missing

Create src/Makefile

     autoreconf -vif # Usually not required
    ./configure

Running Make

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
