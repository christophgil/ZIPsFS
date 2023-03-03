#!/usr/bin/env bash
set -u
readonly B=~/test/fuse
mkdir -p  ${B}/{rootdir_writable,mnt}
ZIP1=/usr/share/doc/texlive-doc/support/pdfjam/tests.zip
ZIP2=/usr/share/doc/texlive-doc/bibtex/vak/test.zip
for z in $ZIP1 $ZIP2;do
    [[ ! -f $z ]] && read -r -p "Error file does not exist $z "
done
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


        local z=$base/test_zip/big_zip.zip
        mkdir -p ${z%/*} && cp -u $ZIP1 $z

        local z=$base/test_zip/20230202_my_data_.d.Zip
        mkdir -p ${z%/*} && cp -u $ZIP2 $z


    done
    f=$base/verybig.img
    [[ ! -f $f ]] && fallocate -l 500M $f

    f=$base/big.img
    [[ ! -f $f ]] && seq 1000 > $f





}
main
find $B -ls
