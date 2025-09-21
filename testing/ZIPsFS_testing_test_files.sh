#!/usr/bin/env bash
set -u
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[30m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'



run_ZIPsFS(){
    local root1=~/tmp/ZIPsFS/root1/
    local root2=/home/_cache/x/ZIPsFS/root/

    local modi=/home/_cache/$USER/ZIPsFS/modifications/
    local mnt=~/tmp/ZIPsFS///$MNT/
    rm ~/.ZIPsFS/_home_cgille_tmp_ZIPsFS_mnt/cachedir/*.cache
    mountpoint $mnt && umount $mnt
    mountpoint $mnt && umount -l $mnt
    mountpoint $mnt 2>&1 |grep 'connected' &&  sudo umount $mnt
    mountpoint $mnt && return
    ~/git_projects/ZIPsFS/ZIPsFS  "$@" -k -l 33GB  -c rule  $modi $root1 $root2  /s-mcpb-ms03/fulnas1/1/ : -o 'allow_other,attr_timeout=13,entry_timeout=13,negative_timeout=13,ac_attr_timeout=13'    $mnt
}



crc32_zip_entry(){
    unzip -v $1 $2 | sed -n 4p| awk '{ print $7}'
}

Enter(){
    read -r -p 'Enter'
}

test_is_in_listing(){
    local vp=$1
    find ${vp%/*} | grep -q -F /${vp##*/} || echo -e "\n${ANSI_FG_RED} MISSING   find ${vp%/*} | grep -q -F /${vp##*/}    $ANSI_RESET\n"
}

test_checksum(){
    local repeats=$1  crc32="$2" vp=$3  c
    rm "$vp" 2>/dev/null
test_is_in_listing $vp
    echo -n "$vp ... "
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


test_pattern(){
    local repeats=$1  pattern="$2"  vp=$3
    echo  "$ANSI_FG_BLUE$vp$ANSI_RESET pattern: '$pattern' ... "
    rm "$vp" 2>/dev/null
        test_is_in_listing $vp
    for ((i=0; i<repeats; i++)); do
        if strings $vp | grep "$pattern"; then
            echo "${ANSI_FG_GREEN} OK:$i $ANSI_RESET"
        else
            echo "${ANSI_FG_RED} FAILED:$i  $ANSI_RESET"
        fi
    done
}

test_zip_entry(){
    local vp=$1
    echo $ANSI_INVERSE"test_zip_entry $ANSI_RESET '$vp'"
    test_is_in_listing $vp
    local ze=$(< ${vp}@SOURCE.TXT)
    #    [[ -z $ze ]] && echo -n "$ze " && echo -n  "  Empty ${vp}@SOURCE.TXT " && Enter && return 1
    [[ -z $ze ]] && echo -n  "$ANSI_FG_RED  Empty ${vp}@SOURCE.TXT $ANSI_RESET" && Enter && return 1
    local z=${ze%$'\t'*}
    local e=${ze#*$'\t'}
    echo -n  "'$z'   '$e'  "
    [[ -z $z || -z $e ]] && echo -n "$ANSI_FG_RED  z: $z e: $e  $ANSI_RESET" && Enter && return 1
    local crc_zip c
    crc_zip="$(crc32_zip_entry $z $e)"
    [[ -z $crc_zip ]] && echo -n  "$ANSI_FG_RED crc_zip is empty $ANSI_RESET" && Enter && return 1
    c=$(rhash --printf=%C --crc32 $vp) || return 1
    [[ ${c,,} != "${crc_zip,,}" ]] && echo -n  "$ANSI_FG_RED c: '$c' crc_zip: '$crc_zip'  $ANSI_RESET" && Enter && return 1
    echo "${ANSI_FG_GREEN} OK $ANSI_RESET"
    local parent=${vp%$e}
    echo -n  " parent=$parent  "
    #    find $parent | grep ${e##*/} || Enter
}

test_zip_inline(){
    export GREP_COLORS='ms=01;32'
    if ls $MNT/Z1/Data/50-0079/ | grep --color=always wiff2 && sleep 1 && ls $MNT/Z1/Data/50-0079/ | grep --color=always wiff$; then
        echo -n  "$ANSI_FG_GREEN OK $ANSI_RESET"
    else
        echo -n  "$ANSI_FG_RED Error ZIP inline $ANSI_RESET"
        ls $MNT/Z1/Data/50-0079/
        Enter
    fi
}



main(){
local mnt=${1:-mnt}
    echo "${ANSI_INVERSE}Test by C-code $ANSI_RESET"
    local f=$MNT/example_generated_file/example_generated_file.txt
    ls mnt | grep example_generated_file || echo -n  "$ANSI_FG_RED ls mnt: Missing  example_generated_file  $ANSI_RESET"
    test_pattern 1 ' heap ' $f

    local size1=$(cat $f $f | wc -c)
    local size2=$(stat  --format '%s' $f)
    echo  -n 'Testing hash-map of file sizes ...'
    if [[ "$size1" != "$size2" ]]; then
        echo  "$ANSI_FG_GREEN OK $ANSI_RESET"
    else
        echo "$ANSI_FG_RED File size $f:    $size1 != $size2 $ANSI_RESET"
    fi


    echo "${ANSI_INVERSE}Test by curl$ANSI_RESET"
    test_pattern 2 ' warranties '  $MNT/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README
    test_pattern 2 ' License '     $MNT/ZIPsFS/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE


    read -r -p Enter
    test_zip_inline

    test_zip_entry $MNT/6600-tof2/Cal/202208/Cal20220819161022040.wiff
    test_zip_entry $MNT/6600-tof2/Cal/202208/Cal20220819161022040.wiff.scan

    read -t 5 -r -p 'Enter or wait 5s'

    test_zip_entry $MNT/ZIPsFS/a/misc_tests/zip/images_helpimg.ZIP.Content/media/helpimg/area1.png

    test_zip_entry $MNT/PRO1/Maintenance/202302/20230210_KTT_PRO1_MA_250BSA1_RA3_1_12194.d/analysis.tdf

    test_checksum 2 95b9d6de $MNT/ZIPsFS/a/autogen_test_files/data/mtcars.parquet.TSV.bz2
    test_checksum 1 f0e5d539 $MNT/ZIPsFS/a/autogen_test_files/data/mtcars.parquet.tsv
    test_checksum 1 'PNG image data, 55 x 39, 8-bit colormap, non-interlaced'  $MNT/ZIPsFS/a/autogen_test_files/image/error1.scale25%.png
    test_checksum 2 'PNG image data, 110 x 78, 8-bit colormap, non-interlaced' $MNT/ZIPsFS/a/autogen_test_files/image/error1.scale50%.png

    test_checksum 2 3d5171fe $MNT/ZIPsFS/a/autogen_test_files/image/error1.png.ocr.eng.txt

    test_checksum 2 A27A174D $MNT/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.strings



    echo "${ANSI_INVERSE}The following will take long $ANSI_RESET"

    test_zip_entry $MNT/6600-tof2/Cal/202208/Cal20220819161022040.wiff
    test_zip_entry $MNT/6600-tof2/Cal/202208/Cal20220819161022040.wiff.scan


    test_checksum 2 93f172e3 $MNT/ZIPsFS/a/autogen_test_files/data/report.parquet.tsv
    test_checksum 2 353901EB $MNT/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.mgf
    test_checksum 2 FB68D3A4 $MNT/ZIPsFS/a/autogen_test_files/MS-rawfile/Cal20240223150943030.wiff.mzML
}

DO_RUN_ZIPSFS=0
MNT='mnt'
while getopts 'zm:' o; do
    case $o in
        m) MNT=$OPTARG;;
        z) DO_RUN_ZIPSFS=1=1;;
        *) echo $RED_ERROR'Invalid option -'$o;exit 1;;
    esac
done
shift $((OPTIND-1))

if ((DO_RUN_ZIPSFS)); then
    run_ZIPsFS
else
    main "$@"
fi
