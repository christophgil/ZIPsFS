#!/usr/bin/env bash

rm core 2>/dev/null



readonly CP=/proc/sys/kernel/core_pattern
if [[ core != $(<$CP) ]];then

    echo Going to change core_pattern ...
    sudo tee $CP <<< core

fi
false &&  ls -l $CP &&  more $CP

ulimit -c unlimited



sudo umount ~/test/fuse/mnt/
/home/cgille/git_projects/ZIPsFS/src/ZIPsFS  -f  ~/test/fuse/root_*  ~/test/fuse/mnt

ls -l core 2>/dev/null
