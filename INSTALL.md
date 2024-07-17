# Installation of executable


First install the required libraries.

 - libfuse3
 - libzip

A ready-to-use executable for Linux (amd64) is found in the folder
[RELEASE](./RELEASE/). It may or may not work on your system.



# Dependencies

ZIPsFS is written in standard C according to the POSIX industry strandard.  Installation works with
both compilers, gcc and clang.  Install the required libraries. The command for Ubuntu Linux:

    sudo apt-get install  libfuse3.3 libzip4 libfuse3-dev libzip-dev build-essential

For FreeBSD, NetBSD and MacOSX please see find instructions in [INSTALL_other_than_Linux](./INSTALL_other_than_Linux.md)
It is likely that the quick installation script will work for you.


# Quick installation from source code

    src/ZIPsFS_testing.sh


# Installation from source code with make

If this does not work, proceed:

## OPTIONAL: AUTOCONF

The following will Re-generating the file ./configure from configure.ac and Makefile.am
This is usually not necessary.

    aclocal
    autoconf
    automake --add-missing

## CREATE MAKEFILE

    cd ZIPsFS
    #  autoreconf -vif # Usually not required
    ./configure

## RUNNING MAKE

    cd src
    make
