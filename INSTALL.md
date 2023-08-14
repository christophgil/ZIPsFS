# DEPENDENCIES
## Install libfuse3 and libzip

    sudo apt-get install  libfuse3 libzip libfuse3-dev libzip-dev

# OPTIONAL: AUTOCONF

The following wil Re-generating the file ./configure from Makefile.am
It is unlikely that this is necessary.

    aclocal
    autoconf
    automake --add-missing

# CREATE MAKEFILE

    cd into the ZIPsFS/ folder
    ./configure

# RUNNING MAKE

    cd src
    make

# ALTERNATIVE METHOD

Edit and run the file
    src/ZIPsFS.compile.sh

# OS OTHER THAN LINUX

The following is tested on Linux.
Other OS like MacOSX will require adaptations because the
 - /proc file system exists only on Linux

Please share your experience.
