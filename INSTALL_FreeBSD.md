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



#  Compile ZIPsFS

To compile ZIPsFS, run

     ./ZIPsFS.compile.sh

# Trouble shooting Detect problems of fuse


If ZIPsFS does not work you need to exclude that there is a general problem of the  FUSE system.
This can be done by testing another FUSE file system like sshfs or fuse-zip.
The following shows how  fuse-zip can be tested. First it needs to be installed. On Debian or Ubuntu type

    sudo apt get install fuse-zip


In the following, an empty folder is created which serves as mount point. Then a ZIP file is made and mounted with fuse-zip.
Finally, the files at the mount point are shown.


    mkdir -p ~/test/fuse-zip/mnt
    zip --fifo  ~/test/fuse-zip/test.zip  <(date)
    fuse-zip  ~/test/fuse-zip/test.zip ~/test/fuse-zip/mnt
    ls -R  ~/test/fuse-zip/mnt



If this fails, and there is a permission problem, try as root.


Finally run

     src/ZIPsFS.compile.sh
