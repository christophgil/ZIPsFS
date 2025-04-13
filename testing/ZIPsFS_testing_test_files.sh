#!/usr/bin/env bash
set -u
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[30m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'



crc32_zip_entry(){
   unzip -v $1 $2 | sed -n 4p| awk '{ print $7}'
    }

Enter(){
    read -r -p 'Enter'
}
test_checksum(){
    local repeats=$1  crc32="$2" vp=$3  c
    echo -n "$vp ... "
    rm "$vp" 2>/dev/null
    for ((i=0; i<repeats; i++)); do
        if [[ $vp == *.png ]]; then
            c=$(file -b $vp) || Enter
        else
            c=$(cat $vp |rhash --printf=%C --crc32 -) || Enter
        fi
        if [[ ${c^^} == "${crc32^^}" ]]; then
            echo "${ANSI_FG_GREEN} OK:$i $ANSI_RESET"
        else
            echo "${ANSI_FG_RED} FAILED:$i '$c' != '$crc32'  $ANSI_RESET"
        fi
    done
}

test_zip_entry(){
    local vp=$1
    echo $ANSI_INVERSE"test_zip_entry $ANSI_RESET '$vp'"
    local ze=$(< ${vp}@SOURCE.TXT)
    #    [[ -z $ze ]] && echo -n "$ze " && echo -n  "  Empty ${vp}@SOURCE.TXT " && Enter && return 1
        [[ -z $ze ]] && echo -n  "  Empty ${vp}@SOURCE.TXT " && Enter && return 1
    local z=${ze%$'\t'*}
    local e=${ze#*$'\t'}
    echo -n  "'$z'   '$e'  "
    [[ -z $z || -z $e ]] && echo -n "  z: $z e: $e  " && Enter && return 1
    local crc_zip c
    crc_zip="$(crc32_zip_entry $z $e)"
    [[ -z $crc_zip ]] && echo -n  ' crc_zip is empty ' && Enter && return 1
    c=$(rhash --printf=%C --crc32 $vp) || return 1
    [[ ${c,,} != "${crc_zip,,}" ]] && echo -n  " c: '$c' crc_zip: '$crc_zip'  " && Enter && return 1
    echo "${ANSI_FG_GREEN} OK $ANSI_RESET"
    local parent=${vp%$e}
    echo -n  " parent=$parent  "
#    find $parent | grep ${e##*/} || Enter
}

main(){

   test_zip_entry mnt/6600-tof2/Cal/202208/Cal20220819161022040.wiff
   test_zip_entry mnt/6600-tof2/Cal/202208/Cal20220819161022040.wiff.scan

read -t 5 -r -p 'Enter or wait 5s'

    test_zip_entry mnt/ZIPsFS/a/misc_tests/zip/images_helpimg.ZIP.Content/media/helpimg/area1.png

    test_zip_entry mnt/PRO1/Maintenance/202302/20230210_KTT_PRO1_MA_250BSA1_RA3_1_12194.d/analysis.tdf

    test_checksum 2 95b9d6de mnt/ZIPsFS/a/autogen_test_files/data/mtcars.parquet.TSV.bz2
    test_checksum 1 f0e5d539 mnt/ZIPsFS/a/autogen_test_files/data/mtcars.parquet.tsv
    test_checksum 1 'PNG image data, 55 x 39, 8-bit colormap, non-interlaced'  mnt/ZIPsFS/a/autogen_test_files/image/error1.scale25%.png
    test_checksum 2 'PNG image data, 110 x 78, 8-bit colormap, non-interlaced' mnt/ZIPsFS/a/autogen_test_files/image/error1.scale50%.png

    test_checksum 2 3d5171fe mnt/ZIPsFS/a/autogen_test_files/image/error1.png.ocr.eng.txt

    test_checksum 2 A27A174D mnt/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.strings


    echo "${ANSI_INVERSE}The following will take long $ANSI_RESET"

   test_zip_entry mnt/6600-tof2/Cal/202208/Cal20220819161022040.wiff
   test_zip_entry mnt/6600-tof2/Cal/202208/Cal20220819161022040.wiff.scan


    test_checksum 2 93f172e3 mnt/ZIPsFS/a/autogen_test_files/data/report.parquet.tsv
    test_checksum 2 353901EB  mnt/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.mgf
    test_checksum 2 FB68D3A4 mnt/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.mzML
}

main
