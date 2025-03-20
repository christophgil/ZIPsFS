# Install ZIPsFS on Omnios

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



    wget -N $U && unzip -o main.zip &&  ZIPsFS-main/src/ZIPsFS.compile.sh -F /local/illumos-sshfs-master/libfuse/proto/usr/lib/amd64/libfuse.so.2.7.1


## Prepare running ZIPsFS

    export PATH=$PATH:/local/illumos-sshfs-master/libfuse/:/local/illumos-sshfs-master/libfuse/amd64
    mkdir -p /usr/lib/fs/fuse
    cp /local/illumos-sshfs-master/libfuse/amd64/fusermount.bin /usr/lib/fs/fuse/
    echo user_allow_other >>/etc/fuse.conf

## TroubleShooting


In my case ZIPsFS started only as root.
I changed the permissions of /dev/fuse without success.




 <!-- pkg install emacs rsync -->
 <!--  Missing Lynx, sshpass -->


Finally run

     src/ZIPsFS.compile.sh
