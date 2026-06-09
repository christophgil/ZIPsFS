#!/usr/bin/env bash
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m'
export ANSI_RESET=$'\e[0m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m'
set -u


[[ -n ${TMUX_WINDOW_NUMBER:-} ]] && tmux rename-window -t "$TMUX_WINDOW_NUMBER" "${0##*/}"


IS_VERBOSE=0
while getopts 'v' o; do
    case $o in
        v) IS_VERBOSE=1;;
        *) echo '$0 Invalid option '$o;exit 1;;
    esac
done
shift $((OPTIND-1))

main(){
    if [[ -z ${1:-} ]]; then
        cat << EOF
Testing of files which are  stored in ZIP files in the upstream file system.
The virtual files will be read in several simultaneous threads.
The observed CRC32 hashsum will be compared to that recorded in the ZIP of the upstream file system.
.
$ANSI_UNDERLINE${ANSI_BOLD}Usage:$ANSI_RESET
   $0 <num-threads>  file1 file2 file3 ...
$ANSI_UNDERLINE${ANSI_BOLD}Examples:$ANSI_RESET
   $0 5  mnt/PRO1/Data/30-0051/*.d/analysis.tdf
EOF
        return
    fi
    THREADS=$1; shift
    echo "Number of threads: $THREADS"
    declare -A map_crc
    local f
    for f in "$@"; do
        ! ls -l $f && continue
        local crc=''
        crc=$(< $f@ARCHIVECRC32.TXT)
        if [[ -z $crc ]]; then
            echo "Not a ZIP entry?   Empty  $ANSI_FG_BLUE$f@ARCHIVECRC32.TXT$ANSI_RESET"
            crc=$(rhash --printf=%C --crc32 $f)
        fi
        map_crc[$f]="${crc^^}"
    done
    echo map_crc="${map_crc[*]}"
    read -r -p 'CRC32 checksums computed. Press enter to continue'
    for ((pass=0;1;pass++)); do
        for f in "$@"; do
            echo
            local cmd="rhash --printf=%C+%s --crc32 $f"
            local m j m2 len
            ! m=$($cmd) && exit
            m=${m%%?BAD*}
            m=${m^^}
            m=${m%%+*}
            local crc_zip="${map_crc[$f]}"
            echo -e "$cmd\t$m"
            ls -d -l $f
            [[ $m != "${crc_zip}" ]] && read -r -t 9999999 -p "${ANSI_FG_RED}Warning  map_crc='${crc_zip}' m='$m'  ${ANSI_RESET}  $f Press Enter"
            for ((j=THREADS;--j>=0;)); do
                ((IS_VERBOSE)) && echo "Going to run $cmd"
                (  m2=$($cmd) || read -r -p "Error";
                   ((IS_VERBOSE)) && echo "Done"
                   m2=${m2%%?BAD*}
                   len=${m2##*+}
                   m2=${m2%%+*}

                   m2=${m2^^}
                   local color=$'\e[32m'
                   [[ $m2 != "$m" ]] && color=$'\e[33m' && echo "$cmd"
                   echo -e "${color}pass=$pass ${color}is=$m2 ${color} should=$m"$'\e[0m'"  bytes read=$len";
                   if [[ $m != "$m2" ]]; then
                       crc32 $f
#                       rhash --printf='%C' --crc32 $f
#                       cat $f | rhash --printf='%C' --crc32 -

                       read -r -p "Error "
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
