"#!/usr/bin/env bash\n\
readonly local CLR_CACHE='Clear-cache' CANCEL='Cancel'\n\
dir=${BASH_SOURCE[0]}\n\
dir=${dir%/*}\n\
select s in $CLR_CACHE $CANCEL;do\n\
    if [[ $s == $CLR_CACHE ]];then\n\
        read -p 'Are you sure to clear the cache [y/n] ?' -n 1 -r\n\
        echo\n\
        if [[ ${REPLY,,} == y ]];then\n\
            stat $dir/.."FILE_CTRL_CLEARCACHE"\n\
            echo 'Clear cache done.'\n\
            break\n\
        else\n\
            echo Canceled\n\
        fi\n\
    fi\n\
done\n\
"