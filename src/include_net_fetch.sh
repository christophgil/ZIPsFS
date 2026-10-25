#!/usr/bin/env bash
set -u
SRC=${BASH_SOURCE[0]}
DIR=${SRC%/*}
while read a; do    eval "${a#$}"; done  <${SRC%/*}/include_net_fetch_common.ps1 #FILTER_OUT
DIR=/var/Users/cgille/tmp/ZIPsFS/mnt/ZIPsFS/n                                    #FILTER_OUT
cc=("$c0" "$c1" "$c2" "$c3" "$c4")
main(){
    while true; do
        echo
        local url='' choice
        while [[ -z $url ]]; do
            echo
            local i=0
            for c in "${cc[@]}"; do
                echo " ($i) $c"
                ((i++))
            done
            read -r -p "$XCHOICE" choice
            [[ -z $choice ]] && exit 0
            choice=${choice%% }
            choice=${choice## }
            [[ $choice == '0' ]] && { xdg-open $URL_UP;continue;}
            [[ -n ${cc[$choice]:-} ]] && choice="${cc[$choice]:-}"
            local e
            for e in .fasta .fasta.gz; do
                [[ $choice == *$e\ * ]] && url="$FTP_EXPASY${choice%$e *}$e"
            done
        done
        echo -e "\nurl=$url"
        [[ $url == *.gz &&  $url != *.tar.gz ]] && echo && read -r -p "$XGZ" && [[ ${REPLY,,} != *y* ]] && url=${url%.gz}
        local fn=${url//:/,}
        fn=${fn//\//,}
        local vp="$DIR/$fn"
        echo "Going to display beginning of $vp. This will trigger the download process" 2>&1
        gunzip='cat'
        [[ $fn == *.gz ]] && gunzip='gunzip -c '
        set -x
        $gunzip $vp | head
        set +x
        read -r -p "Done. $XRST"
    done
}
main
