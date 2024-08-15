# Install ZIPsFS on Debian or Ubuntu

apt-get update
apt-get install fuse-zip   libfuse3-dev  libzip-dev build-essential unzip lynx tmux

    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh

# TroubleShooting


Check whether it is possible to mount a zip file with fuse-zip.
