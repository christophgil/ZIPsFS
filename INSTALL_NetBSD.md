# NetBSD

    pkg_add -u zip unzip libzip fuse-unionfs perfuse bash wget tmux lynx

    U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip
    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh


## TroubleShooting

Running ZIPsFS as root worked well.  Normally, ZIPsFS will not run as root unless the option -r is
given.  However, we could not run ZIPsFS as a normal user because of acceess failure for /dev/puffs.
We added the user to group wheel and did chmod go+rw /dev/puffs without success.
See https://minux.hu/mounting-webdav-under-netbsd-unprivileged-user

Shared libs libzip and libfuse were not found.  This could be fixed with

   export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/pkg/lib

One might first try to  get fuse-unionfs to work.
