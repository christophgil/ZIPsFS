#!/usr/bin/env bash
readonly local CLR_CACHE='Clear-cache' CANCEL='Cancel'
dir=${BASH_SOURCE[0]}
dir=${dir%/*}
select s in $CLR_CACHE $CANCEL;do
    if [[ $s == $CLR_CACHE ]];then
        read -p 'Are you sure to clear the cache [y/n] ?' -n 1 -r
        echo
        if [[ ${REPLY,,} == y ]];then
            stat $dir/.."FILE_CTRL_CLEARCACHE"
            echo 'Clear cache done.'
            break
        else
            echo Canceled
        fi
    fi
done
