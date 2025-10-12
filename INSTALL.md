# Installation of ZIPsFS

## Requirements

 - POSIX C-compiler gcc or clang. GNU extensions not needed.
 - bash, unzip, tmux
 - libfuse any version
 - libzip


<details><summary>Optional requirements</summary>

 - binutils       (Provides /usr/bin/addr2line, which is used for debugging.  Back-traces show location in source code)

ZIPsFS allows transparent on-the-fly file conversion which requires the following:

 - docker
 - tesseract-ocr-eng
 - imagemagick
 - poppler-utils
 - pdftotext
</details><!--- Optional -->

<details><summary>MS-Window</summary>
ZIPsFS can probably not be installed directly in MS-Windows.
It may be installed in WSL.
The mountpoint can be exported as a SAMBA share.

### Problem: Files that are not listed in the parent are not accessible

In Windows, network  files are generally not accessible when they are not listed in the parent folder.

This breaks the following ZIPsFS features:

 - Appending the suffix ***@SOURCE.TXT*** to a virtual file path forms a file telling the real physical  location.

 - Accessing Internet files as regular files




### Problems dynamically generating files with Windows executables

ZIPsFS can generate files dynamically. The virtual file content can be the output of a command.

This also works for Windows executables with the compatibility layer Wine.

However, we found the following problems:

 - Usually, ZIPsFS is started in a headless environment i. e. not from a desktop environment.
   Some Windows CLI programs require a graphical display.

   Workaround: A virtual  frame-buffer like ***xvfb*** and setting the DISPLAY environment variable accordingly


 - Some Windows command-line executables do not behave reliably when launched from another executable (here ZIPsFS).
   This issue stems from the  Windows Console API which is used in  CLI programs to implement progress reports.
   Like traditional  escape sequences, the Windows Console API allows free cursor positioning.

   Workaround:

   When the special symbol ***PLACEHOLDER_EXTERNAL_QUEUE*** is specified instead of a direct executable path, ZIPsFS:

    - Pushes the task details to a queue.
    - Waits for the result.

   The actual execution of these tasks is handled by the shell script ZIPsFS_autogen_queue.sh,
   which must be started manually by the user. This script polls the queue and performs the requested conversions or operations.
   Multiple instances of the script can run in parallel, allowing concurrent task handling.

</details><!--- Windows -->




<details><summary>Ubuntu, Debian and friends (Linux)</summary>

    apt-get update
    apt-get install fuse-zip   libfuse3-dev  libzip-dev  unzip lynx tmux
    addr2line -H || apt-get install binutils

    apt-get install gcc

or

    apt-get install clang
</details><!--- Ubuntu -->



<details><summary>Apple Macintosh OSX</summary>

Download and install MacPorts https://www.macports.org/
Download and install macFUSE https://osxfuse.github.io/

Open terminal and run:

    sudo port install libzip bindfs wget unzip tmux lynx

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
</details><!--- MacOSX -->


<details><summary>FreeBSD</summary>

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
</details><!--- FreeBSD -->



<details><summary>NetBSD</summary>

    pkg_add -u zip unzip libzip fuse-unionfs perfuse bash wget tmux lynx



## TroubleShooting

### Problem running as normal user.

Running ZIPsFS as root worked well.  Normally, ZIPsFS will not run as root unless the option -r is
given.  However, we could not run ZIPsFS as a normal user because of acceess failure for /dev/puffs.
We added the user to group wheel and did

    chmod go+rw /dev/puffs

without success.

Related:  https://minux.hu/mounting-webdav-under-netbsd-unprivileged-user

Please tell me your solutions to this problem.

### Problem finding shared libraries

Shared libs libzip and libfuse were not found.  This could be fixed with

   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/pkg/lib

</details><!--- NetBSD -->
<details>
<summary>OpenBSD</summary>

    pkg_add fuse-zip libzip
    pkg_add  rsync lynx
    pkg_info -Q    ## Search

</details><!--- OpenBSD -->

<details><summary>Omnios (BSD)</summary>

Omnios  is an Illumos distribution and a descendant of OpenSolaris

I was able to compile ZIPsFS.
However, it did not work.



    pkg install fuse libzip
    pkg install tmux
    clang || pkg install developer/clang-18 # You can specify any clang version here.


## Installing fuse
There seems to be no fuse package.

    pkg search fuse

This software provides FUSE. Please install it.
[Please install solaris-sparc-fuse](https://github.com/myaut/solaris-sparc-fuse)



## Install ZIPsFS


    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip

Please modify the path of the libfuse3.so accordingly.



    wget -N $U && unzip -o main.zip &&  ./ZIPsFS-main/src/ZIPsFS.compile.sh -F /local/illumos-sshfs-master/libfuse/proto/usr/lib/amd64/libfuse.so.2.7.1


## Prepare running ZIPsFS

    export PATH=$PATH:/local/illumos-sshfs-master/libfuse/:/local/illumos-sshfs-master/libfuse/amd64
    mkdir -p /usr/lib/fs/fuse
    cp /local/illumos-sshfs-master/libfuse/amd64/fusermount.bin /usr/lib/fs/fuse/
    echo user_allow_other >>/etc/fuse.conf

## TroubleShooting


For me ZIPsFS started only as root.
I changed the permissions of /dev/fuse without success.
</details><!--- Omnios -->

<details><summary>OpenIndiana (OpenSolaris)</summary>

OpenIndiana is an Illumos distribution and a descendant of OpenSolaris


     pkg update
     pkg install tmux fuse libzip pkg:/metapackages/build-essential
     pkg install tmux fuse libzip build-essential

<!-- OpenSolaris -->
<!-- * https://artemis.sh/2022/03/07/pkgsrc-openindiana-illumos.html -->
<!-- * https://www.openindiana.org/packages/ -->
<!-- pkg publisher -->
<!-- To add a publisher to your system (requires privileges): -->
<!-- pkg set-publisher -g http://path/to/repo_uri publisher -->
<!--/var/pkg/cache -->
<!-- pkg set-property flush-content-cache-on-success True -->
<!-- https://github.com/jurikm/illumos-fusefs/raw/master/lib/libfuse-20100615.tgz -->
</details><!--- OpenIndiana -->

<details><summary>Illumos (Solaris)</summary>
  On  Solaris getting FUSE file systems to work seems tricky.
More work is needed to understand the permission. At least, ZIPsFS compiles.
</details><!--- Illumos -->


# Compilation

Run

     ./ZIPsFS.compile.sh

This creates the executable file **ZIPsFS**.

<details><summary>Trouble-Shooting Compilation</summary>


This is how the script works on different  operation systems.

There are several OS specific configurations which
are managed by the script ``ZIPsFS.compile.sh``:

 - The C header files of FUSE may not be found with standard compiler settings. The script has a list of possible locations and can extend the search path accordingly.
   Maybe you need to add the location of ``fuse.h`` of your system.

- The linker option ``-fuse3`` refers to ``libfuse3.so`` and ``-lzip`` stands for libzip. In particular the FUSE library may not be found with the default search path.
  The library files may be at different locations. The script has a list of possible places and adds those to the search path. Maybe, yours is not in the list.

- The C-libraries are slightly different in the UNIX systems. Features are detected by probing
  compilation of test code. See ``$HOME/tmp/ZIPsFS/compilation/``.  For example the ``struct
  dirent`` may or may not have a field ``d_type`` {DT_DIR (directories), DT_REG (regular files),
  ...}. This is detected by compiling test code.  The compiler option ``-DHAS_DIRENT_D_TYPE=1`` or
  ``-DHAS_DIRENT_D_TYPE=0`` sets the macro ``HAS_DIRENT_D_TYPE`` to 0 or 1 allowing for conditional
  compilation.


Quick guide to fix the problem:

 - Does the problem occur during compilation or linking?

 - Where are the files ``fuse.h`` and ``libfuseXXX.so`` on your system?   Note the extension is ``dylib`` rather than ``so`` on MacOSX.

 - Where are the files ``zip.h`` and ``libzip.so`` on your system?

 - Are the containing paths included in ``ZIPsFS.compile.sh``?

</details>

# Testing the installed ZIPsFS

    src/ZIPsFS_testing.sh  <Path to ZIPsFS executable>

    Or run the Mini tutorial in the README


<details><summary>Trouble Shooting Running ZIPsFS</summary>

For us  ZIPsFS  worked only  as **root** for Omnios, OpenBSD, FreeBSD.
Probably, this can be fixed with altering permissions of the FUSE device.

<details><summary>Check whether other  FUSE file systems</summary>

If ZIPsFS does not work you need to exclude general problem of the  FUSE system.

This can be done by testing another FUSE file system like **sshfs** or **fuse-zip** or **unionfs-fuse**.


<details><summary>Testing fuse-zip</summary>

Install fuse-zip. Ubuntu, Debian ...
    sudo apt get install fuse-zip


In the following, an empty folder is created which serves as mount point. Then a ZIP file is made and mounted with fuse-zip.
Finally, the files at the mount point are shown.


    mkdir -p ~/test/fuse-zip/mnt
    zip --fifo  ~/test/fuse-zip/test.zip  <(date)
    fuse-zip  ~/test/fuse-zip/test.zip ~/test/fuse-zip/mnt
    ls -R  ~/test/fuse-zip/mnt
</details><!--- fuse-zip --><!--- -->


<details><summary>fuse-unionfs</summary>

Install fuse-unionfs.

The following will mount */etc* onto *~/mnt/test-unionfs*.
This test may be performed as a normal user or as **root**.

    m=~/mnt/test-unionfs/
    unionfs-fuse /etc=RO $m
    ls $m

</details><!--- fuse-unionfs -->

If those fail try as root.


</details><!--- Trouble Shooting Running ZIPsFS -->
