#!/usr/bin/env bash
set -u
readonly B=~/test/fuse
mkdir -p  ${B}/{rootdir_writable,mnt}
example_zips=(/usr/share/doc/texlive-doc/support/pdfjam/tests.zip /usr/share/doc/texlive-doc/bibtex/vak/test.zip /local/java/jnifuse_master.zip)
main(){
    local j count=0 f

    for((j=1;j<4;j++));do
        local base=$B/root_$j
        mkdir -p $base/subdir1/subdir2/lastsubdir
        for d in $(find $base -type d)    ;do
            f=$d/file_in_root${j}_${d##*/}_$count.txt
            [[ ! -f $f ]] && echo hello world $j > $f
            ((count++))
        done


        if ((j==2));then
            local z n=0
            for z in "${example_zips[@]}";do
                [[ ! -s $z ]] && read -r -p "No such zip file  $z" && continue
                local zip=$base/test_zip/zip_$n.zip
                mkdir -p ${zip%/*} || continue
                if [[ ! -f $zip ]];then
                    ln $z $zip || cp -u $z $zip
                fi
                local zip2=$base/advance_zip/2023_my_data_zip_$n.d.Zip
                mkdir -p ${zip2%/*} || continue
                [[ -f $zip2 ]] || ln $zip $zip2
                ((n++))
            done
        fi
    done
    f=$base/verybig.img
    [[ ! -f $f ]] && fallocate -l 500M $f

    f=$base/big.img
    [[ ! -f $f ]] && seq 1000 > $f





}
main
find $B -ls
