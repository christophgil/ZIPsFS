This page contains instructions to install the required libraries and to set up
the Fuse system.

When this is completed, ZIPsFS can be installed as described in [INSTALL](./INSTALL.md)



# MS-Windows

Install ZIPsFS in a WSL environment.


# FreeBSD

## FreeBSD: FUSE as root

Become root.

    pkg install fuse-zip lynx tmux sysutils/fusefs-libs3 libzip bash wget

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

## FreeBSD: FUSE as normal user

Now check whether fuse-zip  works if run as a normal user

    sysctl vfs.usermount=1
    echo vfs.usermount=1 >>  /etc/sysctl.conf
    pw groupadd fuse
    chgrp fuse /dev/fuse
    pw groupmod group -m <user-id>

## FreeBSD: Install ZIPsFS

    wget -N  https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    unzip main.zip
    cd ZIPsFS-main
    src/ZIPsFS.compile.sh

# NetBSD

    pkg_add zip unzip libzip fuse-unionfs perfuse bash wget

    wget -N  https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    unzip main.zip
    cd ZIPsFS-main
    src/ZIPsFS.compile.sh


## Problems on NetBSD

Running ZIPsFS as root worked well.  Normally, ZIPsFS will not run as root unless the option -r is
given.  However, we could not run ZIPsFS as a normal user because of acceess failure for /dev/puffs.
We added the user to group wheel and did chmod go+rw /dev/puffs without success.
See https://minux.hu/mounting-webdav-under-netbsd-unprivileged-user

Shared libs libzip and libfuse were not found.  This could be fixed with

   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/pkg/lib

One might first try to  get fuse-unionfs to work.

# MacOSX


Download and install MacPorts https://www.macports.org/
Download and install macFUSE https://osxfuse.github.io/

Open terminal and run:

    sudo port install libzip bindfs wget

Load the kernel. Give the correct OSX version, here 15.

    sudo kextload /Library/Filesystems/macfuse.fs/Contents/Extensions/15/macfuse.kext && echo Success

An error box might pop up:

  The system extension required for mounting macFUSE volumes could not be loaded.  Please open the
  Security & Privacy System Preferences pane, go to the General preferences and allow loading system
  software from developer "Benjamin Fleischer". A restart might be required before the system
  extension can be loaded.
  Then try mounting the volume again.

Maybe you need to restart and try again.


To check whether fuse is working try bindfs it  is a simple FUSE file system.

    mkdir -p ~/mnt/test_bindfs
    bindfs -f ~ ~/mnt/test_bindfs

The -f option means that bindfs runs in foreground.
In another terminal check whether you see the content of the home directory at the mount point.

     ls ~/mnt/test_bindfs



    wget -N  https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    unzip main.zip
    cd ZIPsFS-main
    src/ZIPsFS.compile.sh
