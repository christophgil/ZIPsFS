# Install ZIPsFS on Debian or Ubuntu

    apt-get update
    apt-get install fuse-zip   libfuse3-dev  libzip-dev build-essential unzip lynx tmux

    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh

# TroubleShooting


## Testing FUSE system

To test  FUSE, one can  check whether it is possible to mount a zip file with fuse-zip.
This test may be performed as a normal user or as ROOT.
The zip file will be mounted on ~/mnt/zip. The content of the zip-file will be accessible from this folder which acts as a mount point.

    mkdir -p ~/mnt/zip ~/test &&  zip -j ~/test/test.zip /etc/os-release &&  fuse-zip ~/test/test.zip ~/mnt/zip

The zip file is mounted here:

    ls ~/mnt/zip
