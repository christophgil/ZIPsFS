#!/usr/bin/env bash

## Specify root directory here.  Test file trees will be written here
BASE=~/test/ZIPsFS/small_test

ROOT2=readonly2
ROOTS="readonly1 $ROOT2 readonly3"

set -u

mk_test_zip(){
    local z=$BASE/$ROOT2/my_zip_file.zip
    if [[ ! -s $z ]];then
        mk_test_files $BASE/zip zipentry-
        cd $BASE/zip || return
        zip -r $z *
    fi
}

mk_test_files(){
    local root=$1 pfx=$2
    local r subdir1 subdir2 f
    for r in $ROOTS; do
        for subdir1 in a b; do
            for subdir2 in x y; do
                for f in 1 2 3 4 5 6;do
                    local f=$root/$r/$subdir1/$subdir2/${pfx}root_${root##*/}_file_$f.txt
                    [[ -f $f ]] && continue
                    mkdir -p ${f%/*}
                    echo This is file $f >$f
                    ls -l $f
                done
            done
        done
    done
    tree $BASE/reado*
}


main(){
    mk_test_files $BASE ''
    mk_test_zip
    local exe=${1:-}
    if [[ -z $exe ]];then
        exe=~/git_projects/ZIPsFS/ZIPsFS
        [[ ! -s $exe ]] && exe=~/ZIPsFS/ZIPsFS
        [[ ! -s $exe ]] && echo 'Missing argument: executable of ZIPsFS' && return 1
        echo "Using default exe $exe"
    fi

    [[ ! -s $exe ]] && echo "Argument $exe (executable of ZIPsFS) does not exist" && return 1
    local m=$BASE/mnt
    mkdir $m $BASE/writable
    echo "Maybe $m is still mounted - going to umount  ..."
    fusermount -u $m

    echo "Going to mount $m ..."
    set -x
    #    $exe $BASE/writable $ROOTS : -f -o allow_other $m
    local rr=''
    for r in $ROOTS;do rr+=" $BASE/$r"; done
    $exe $BASE/writable $rr : -f  $m
    set +x

echo 'export LD_LIBRARY_PATH=/usr/pkg/lib'

    return

}

main "$@"
