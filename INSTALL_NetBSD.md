# NetBSD

    pkg_add -u zip unzip libzip fuse-unionfs perfuse bash wget tmux lynx

#  Compile ZIPsFS

To compile ZIPsFS, run

     ./ZIPsFS.compile.sh


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

### Problems with FUSE

One might first check whether FUSE works by testing  fuse-unionfs. The following will mount */etc* onto *~/mnt/test-unionfs*.
This test may be performed as a normal user or as ROOT.

   m=~/mnt/test-unionfs/
   unionfs-fuse /etc=RO $m
   ls $m
