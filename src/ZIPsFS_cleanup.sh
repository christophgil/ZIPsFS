#!/usr/bin/env bash
set -u

export ANSI_RED=$'\e[41m' ANSI_MAGENTA=$'\e[45m' ANSI_GREEN=$'\e[42m' ANSI_BLUE=$'\e[44m'  ANSI_YELLOW=$'\e[43m' ANSI_WHITE=$'\e[47m' ANSI_BLACK=$'\e[40m'
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[30m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'


readonly WANT_FREE_GIGABYTE=3000
readonly DAYS_MAX=600
readonly DAYS_MIN=10

:<<EOF
ZIPsFS periodically checks existence of the file ZIPsFS/ZIP_cleanup.sh within the first branch.
This directory branch is also used for prefetched files and automatically generated files.
If free HD capacity gets low, this script could remove files.
This script should be placed into <root-path of first (i.e. writable) root>/ZIPsFS/

EOF

call_df(){
    CAPACITY=0
    AVAILABLE=0
    local filesystem=0 used=0 remaining=0
    read -r filesystem CAPACITY used AVAILABLE remaining< <(df -BG $PWD |tee /dev/stderr |tail -n 1| tr G ' ')
    : $filesystem $remaining $used
}
# find  /path -perm -1000
del_old_files(){
    local d="$1" cond="$2"

    echo "${ANSI_INVERSE}${SRC##*/} $d  days:$ATIME$ANSI_RESET" >&2
    [[ ${d%/*} != */ZIPsFS ]] && echo "${ANSI_FG_RED}This script is in the wrong directory. Abort"$ANSI_RESET >&2 && return
    local action=-delete
    ((DRYRUN)) && action=''
    set -x
    find "$d" -atime +$ATIME $cond -print $action
    find "$d" -atime +1 -name '*[0-9].bak' $cond -print $action
    find "$d" -atime +1 -name '*[0-9].tmp' $cond -print $action
    set +x
}

readonly SRC=${BASH_SOURCE[0]}
CD_TO_SCRIPT_PARENT=1
DRYRUN=0
while getopts 'tn' o; do
    case $o in
        n) DRYRUN=1;;
        t) CD_TO_SCRIPT_PARENT=0;;
        *) echo "wrong_option $o" >&2; exit 1 ;;
    esac
done
shift $((OPTIND-1))


main(){
    echo "$ANSI_MAGENTA$SRC$ANSI_RESET" >&2
    ((CD_TO_SCRIPT_PARENT)) &&cd ${SRC%/*} || return # We are in the parent of this script
    echo "$ANSI_FG_MAGENTA$PWD$ANSI_RESET" >&2
    call_df
    ((WANT_FREE_GIGABYTE>CAPACITY)) && echo "$ANSI_FG_RED WANT_FREE_GIGABYTE ($WANT_FREE_GIGABYTE) > CAPACITY ($CAPACITY)$ANSI_RESET" >&2 && ((DRYRUN==0 && CD_TO_SCRIPT_PARENT)) && return
    for ((ATIME=DAYS_MAX; ATIME>=DAYS_MIN; ATIME=2*ATIME/3)); do
        ((WANT_FREE_GIGABYTE<AVAILABLE)) && echo "${ANSI_FG_GREEN}WANT_FREE_GIGABYTE<CAPACITY  $WANT_FREE_GIGABYTE<$CAPACITY$ANSI_RESET" >&2
        del_old_files $PWD "-perm -1000"
        # Those files are reconginzed by sticky flag S_ISVTX
        del_old_files $PWD/ZIPsFS/c  # fileconversion
    done
}



main
