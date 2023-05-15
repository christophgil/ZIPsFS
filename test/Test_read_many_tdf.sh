#!/usr/bin/env bash
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_GRAY=$'\e[30;1m'  ANSI_RESET=$'\e[0m'
set -u
main(){
    FN=analysis.tdf
    if [[ -z ${1:-} ]];then
        echo "Usage $0  $SHARE_STORE/PRO1/Data/30-0051     ~/tmp/ZIPsFS/mnt/PRO1/Data/30-0051/*.d"
        echo "Usage $0  $SHARE_INCOMING/PRO1/Data/30-0021  ~/tmp/ZIPsFS/mnt/PRO1/Data/30-0021/*.d"
        return
    fi
    local d zip_dir=$1
    shift
    declare -A map_crc
    map_crc[hello]=world

    for d in "$@";do
        d=${d%/}
        local crc=$(unzip -v $zip_dir/${d##*/}.Zip |grep "$FN$" | sed  's|.* \(\w\+\) *'$FN'.*|\1|g')
        map_crc[$d]=${crc^^}
    done
    echo map_crc="${map_crc[@]}"

    for((pass=0;1;pass++));do
        for d in "$@";do
            d=${d%/}
            local f=$d/$FN
            echo
             local cmd="rhash --printf=%C --crc32 $f"

             ##            local cmd="/usrxxxxx/bin/crc32 $f"
            local m j m2
            ! m=$($cmd) && exit
            m=${m%%?BAD*}
                               m=${m^^}
            echo -e "$cmd\t$m"
            ls -l $f
            [[ $m != "${map_crc[$d]}" ]] && read -r -t 9999999 -p $ANSI_FG_RED"Warning  map_crc=${map_crc[$d]} m=$m  "$ANSI_RESET"Press Enter"

            for((j=10;--j>=0;));do
                (  m2=$($cmd) || read -r -p "Error";
                   m2=${m2%%?BAD*}
                   m2=${m2^^}
                   echo "pass=$pass is=$m2   should=$m";
                   if [[ $m != "$m2" ]];then
                       crc32 $f
                       echo
                       kill $(ps -s $$ -o pid=)
                       kill -9 $$
                       fi
                ) &
            done
            sleep 0.01
            echo 'All done'
            wait
            sleep 1
        done
    done
}
main "$@"
