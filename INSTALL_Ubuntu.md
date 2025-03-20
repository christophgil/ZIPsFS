# Install ZIPsFS on Debian or Ubuntu

    apt-get update
    apt-get install fuse-zip   libfuse3-dev  libzip-dev  unzip lynx tmux
    addr2line -H || apt-get install binutils

    apt-get install gcc

or

    apt-get install clang





Finally run

     src/ZIPsFS.compile.sh
