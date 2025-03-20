# FreeBSD

## Setup FUSE

Become root.

    pkg install fuse-zip unzip zip lynx tmux sysutils/fusefs-libs3 libzip bash wget

Check whether fuse file systems works as root. fuse-zip is a simple FUSE file system for testing.

    mkdir -p ~/mnt/test_fuse
    fuse-zip  <path-any-zip-file> ~/mnt/test_fuse

If it does not please check fuse kernel module

    kldstat

You shoud see a line with fusefs. If not

    kldload fusefs

To load it automatically on boot, add the line to /etc/rc.conf

    kldload fusefs
    kld_list="fusefs"

## Try FUSE as root

    mkdir -p ~/mnt/zip ~/test &&  zip -j ~/test/test.zip /etc/os-release &&  fuse-zip ~/test/test.zip ~/mnt/zip

The zip file is mounted here:

    ls ~/mnt/zip


## Allow FUSE for normal user

Now check whether fuse-zip  works if run as a normal user.
If not then run

    sysctl vfs.usermount=1
    echo vfs.usermount=1 >>  /etc/sysctl.conf
    pw groupadd fuse
    chgrp fuse /dev/fuse
    pw groupmod group -m <user-id>


## Install ZIPsFS



## TroubleShooting


Finnally run

     src/ZIPsFS.compile.sh
