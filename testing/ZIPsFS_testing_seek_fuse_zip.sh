#!/usr/bin/env bash
set -u
readonly M=~/mnt/zipfile
mkdir -p $M
mountpoint -q $M && umount $M

readonly Z=/home/_cache/x/ZIPsFS/root/misc_tests/local_files/PRO3/20230612_PRO3_AF_004_MA_HEK200ng_5minHF_001_V11-20-HSoff_INF-B_1.d.Zip
readonly FUSE_ZIP='Mount with fuse-zip'
readonly ZIPROFS='Mount with ziprofs.py'



main(){

    select s in "$FUSE_ZIP" "$ZIPROFS"; do
        case $s in
            $FUSE_ZIP)
                fuse-zip $Z $M
                a=$M/analysis.tdf;;
            $ZIPROFS)
#                sudo apt-get install python3-fusepy
                /local/filesystem/ZipROFS-dev/ziprofs.py ${Z%/*} $M
                a=$M/${Z##*/}/analysis.tdf;;
        esac

        ! ls -l $a && read -r -p "Error"
        ZIPsFS_testing_seek "$@"   $a
        break
    done
}
main "$@"
