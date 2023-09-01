# Installation of executable

First install the required libraries. The command for Ubuntu Linux:

    sudo apt-get install  libfuse3 libzip

A ready-to-use executable for Linux (amd64) is found in the folder
[RELEASE](./RELEASE/).

# Installation from source code

First install the required libraries. The command for Ubuntu Linux:

    sudo apt-get install  libfuse3 libzip libfuse3-dev libzip-dev

## OPTIONAL: AUTOCONF

The following will Re-generating the file ./configure from Makefile.am
This is usually not necessary.

    aclocal
    autoconf
    automake --add-missing

## CREATE MAKEFILE

    cd ZIPsFS
    ./configure

## RUNNING MAKE

    cd src
    make

# Installation from source code - alternative way

Edit and run the file [src/ZIPsFS.compile.sh](./src/ZIPsFS.compile.sh)

# Other operation systems

- Windows: ZIPsFS is probably not portable to Windows. However, you can run it in a WSL environment.

- MacOSX, Free BSD, Solaris: For those UNIX or UNIX-like OS, only minor adaptations will be necessary.
  ZIPsFS uses the /proc file system of Linux. This might not be available.
