#!/usr/bin/env bash
set -u
readonly B=~/test/fuse
mkdir -p  $B{rootdir_writable,mnt}
main(){
    local j count=0 f

    for((j=1;j<4;j++));do

        mkdir -p $B/root_$j/subdir1/subdir2/lastsubdir
        for d in $(find ~/test/fuse/branch1/ -type d)    ;do
            f=$d/file$count.txt
            [[ ! -f $f ]] && echo hello world $j > $f
            ((count++))
        done
    done
    f=root_2/subdir1/verybig.img
    [[ ! -f $f ]] && fallocate -l 500M $f

    f=root_2/subdir1/big.img
    [[ ! -f $f ]] && seq 1000 > $f


}
main
