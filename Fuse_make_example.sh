#!/usr/bin/env bash
set -u
readonly B=~/test/fuse
mkdir -p  ${B}/{rootdir_writable,mnt}
main(){
    local j count=0 f

    for((j=1;j<4;j++));do
        local base=$B/root_$j
        mkdir -p $base/subdir1/subdir2/lastsubdir
        for d in $(find $base -type d)    ;do
            f=$d/file$count.txt
            [[ ! -f $f ]] && echo hello world $j > $f
            ((count++))
        done
    done
    f=$base/verybig.img
    [[ ! -f $f ]] && fallocate -l 500M $f

    f=$base/big.img
    [[ ! -f $f ]] && seq 1000 > $f

}
main
find $B -ls
