#!/usr/bin/env bash
set -u

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh
declare -A CC=([gzip]=gz [bzip2]=bz2 [xz]=xz [lrz]=lrz  [compress]=Z)
VP=txt/numbers.txt
RP=$REMOTE1/$VP
WITH_REMOVE=0

go1(){
    local opt=$1 c=$2  vp=${VP}
    local sfx=${CC[$c]}
    [[ -n $sfx ]] && vp=${vp%.txt} && vp+=_$sfx.txt.$sfx

    local p=$MNT/zipsfs/lr/${vp%.$sfx}
    case $opt in
        ### Make files
        -m) local f=$REMOTE1/$vp
            [[ ! -s $f ]] && $c -c $RP >$f
            ls -l $f
            ;;
        ### Delete files
        -d) set -x
            rm -v $p 2>/dev/null
            set +x;;
        ### Test Files
        -r) echo "${ANSI_INVERSE}Test preload $p$ANSI_RESET"
            ((WITH_REMOVE)) && rm -v $p 2>/dev/null
            test_checksum 1 8DC4565D $MNT/$VP
            print_src $p testing
            test_checksum 1 8DC4565D $p
            print_src $p /${MODI##*/}/;;
    esac
}

go(){
    local opt=$1
    shift
    for c in "${!CC[@]}"; do
        go1 $opt $c
    done
}

main(){
    go -m
    ls -l -d $REMOTE1/*
    read -t 5 -r -p 'Made test files: Press Enter'

    [[ ! -s $RP ]] && mkdir -p ${RP%/*} && seq 1000 >$RP
    WITH_REMOVE=0
    go -r


    read -r -p "Remove preloaded files? [Y/n] "
    [[ ${REPLY,,} != *n*  ]] && go -d
    WITH_REMOVE=1
    go -r
}


main "$@"
