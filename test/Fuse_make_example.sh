#!/usr/bin/env bash
set -u
readonly B=~/tmp/fuse
mkdir -p  ${B}/{rootdir_writable,mnt}
example_zips=(/usr/share/doc/texlive-doc/support/pdfjam/tests.zip /usr/share/doc/texlive-doc/bibtex/vak/test.zip /local/java/jnifuse_master.zip)
main(){
    local j k count=0 f

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
        if ((j==3));then
            for((k=0;k<3;k++));do
                local pfx=$base/big_zip/big_file_$k
                # shellcheck disable=SC2125
                if ! ls $pfx*; then
                    mkdir -p ${pfx%/*}
                    set -x
                    openssl rand -out $pfx -base64 $((2**30*3/4))
                    set +x
                    mv $pfx $pfx-$(rhash --printf='%C'  --crc32 $pfx).tdf_bin
                fi
            done
            local zip=$base/big_zip/BIG_FILE.zip
            [[ ! -s $zip ]] && zip -1 $zip $base/big_zip/big_file_*
            local zip=$base/big_zip/BIG_FILE_UNCOMPRESSED.zip
             [[ ! -s $zip ]] && zip -0 $zip $base/big_zip/big_file_*

        fi
    done
    f=$base/verybig.img
    [[ ! -f $f ]] && fallocate -l 500M $f

    f=$base/big.img
    [[ ! -f $f ]] && seq 1000 > $f





}
main
find $B -ls
