#!/usr/bin/env bash
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_GRAY=$'\e[30;1m'  ANSI_RESET=$'\e[0m'
set -u
THREADS=9
main(){
    FN=analysis.tdf
    if [[ -z ${1:-} ]];then
        echo "Usage $0  ~/tmp/ZIPsFS/mnt/PRO1/Data/30-0051 $SHARE_STORE/PRO1/Data/30-0051/*.d.Zip"
        return
    fi
    local d fuse_dir=${1%/}
    shift
    declare -A map_crc
    map_crc[hello]=world

    for d in "$@";do
        local crc=$(unzip -v $d |grep "$FN$" | sed  's|.* \(\w\+\) *'$FN'.*|\1|g')
        local key=${d##*/}
        key=${key%.Zip}
        map_crc[$key]=${crc^^}
    done
    echo map_crc="${map_crc[@]}"
    read -r -p 'CRC32 checksums computed. Press enter to contine'
    for((pass=0;1;pass++));do
        for d in "$@";do
            d=$fuse_dir/${d##*/}
            [[ -d ${d%.Zip} ]] && d=${d%.Zip}
            local f=$d/$FN
            local key=${d##*/}
            key=${key%.Zip}
            # read -r -p "Press enter d=$d f=$f key=$key"
            echo
           local cmd="rhash --printf=%C --crc32 $f"
           #             local cmd="/usr/bin/crc32 $f"
            ##            local cmd="/usrxxxxx/bin/crc32 $f"
            #echo xxxxxxxxxxxx $cmd
            local m j m2
            ! m=$($cmd) && exit
            #echo xxxxxxxxxxxx m=$m
            m=${m%%?BAD*}
            m=${m^^}
            echo -e "$cmd\t$m"
            ls -l $f
            [[ $m != "${map_crc[$key]}" ]] && read -r -t 9999999 -p $ANSI_FG_RED"Warning  map_crc=${map_crc[$d]} m=$m  "$ANSI_RESET"Press Enter"
            for((j=$THREADS;--j>=0;));do
                (  m2=$($cmd) || read -r -p "Error";
                   m2=${m2%%?BAD*}
                   m2=${m2^^}
                   if [[ $m2 != $m ]];then
                       echo -n -e $'\e[31m';
                   else
                       echo -n -e $'\e[32m';
                   fi
                   echo -e "pass=$pass is=$m2   should=$m"$'\e[0m';
                   if [[ $m != "$m2" ]];then
                       crc32 $f
                       echo
                       kill $(ps -s $$ -o pid=)
                       kill -9 $$
                   fi
                ) &
            done
            sleep 0.01
            echo 'All jobs started bg'
            wait
            echo 'All jobs done'
            sleep 1
        done
    done
}
main "$@"
