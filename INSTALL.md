# Installation of executable


First install the required libraries. The command for Ubuntu Linux:

    sudo apt-get install  libfuse3 libzip

A ready-to-use executable for Linux (amd64) is found in the folder
[RELEASE](./RELEASE/). It may or may not work on your system.


# Quick installation from source code

Install the required libraries. The command for Ubuntu Linux:

    sudo apt-get install  libfuse3 libzip libfuse3-dev libzip-dev

For FreeBSD, NetBSD and MacOSX please see find instructions in [RELEASE](./INSTALL_other_than_Linux.html)
It is likely that the quick installation script will work for you.

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

# Installation from source code - alternative way

Edit and run the file [src/ZIPsFS.compile.sh](./src/ZIPsFS.compile.sh)
