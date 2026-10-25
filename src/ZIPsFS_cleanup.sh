#!/usr/bin/env bash
set -u

export ANSI_RED=$'\e[41m' ANSI_MAGENTA=$'\e[45m' ANSI_GREEN=$'\e[42m' ANSI_BLUE=$'\e[44m'  ANSI_YELLOW=$'\e[43m' ANSI_WHITE=$'\e[47m' ANSI_BLACK=$'\e[40m'
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[30m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'


readonly WANT_FREE_GIGABYTE=555  # Continue until THAT much is free or until no files can be deleted
readonly DAYS_MAX=666            # Do not keep files older than
readonly DAYS_MIN=1              # Do not remove files younger than

:<<EOF

ZIPsFS periodically checks existence of the file <path-of-first-file-tree>/zipsfs/ZIP_cleanup.sh.
This script can be customized and copied to this location.

If free HD capacity gets low, this script can remove prefetched files and automatically generated
files that have not been accessed for a while.
EOF



IS_DF_GNU=0
df --version 2>&1| grep -q '\bGNU\b' && IS_DF_GNU=1

call_df(){
    #    CAPACITY=0
    AVAILABLE=0
    #    local filesystem=0 used=0 remaining=0
    #    read -r filesystem CAPACITY used AVAILABLE remaining< <(df -BG $PWD |tee /dev/stderr |tail -n 1| tr G ' ')
    #    : $filesystem $remaining $used
    if ((IS_DF_GNU)); then
        AVAILABLE=$(df -BG  --output=avail $PWD |tee /dev/stderr |tail -n 1| tr G ' ')
    else
        AVAILABLE=$(df -g $PWD |grep -v '^Filesystem' | awk '{print $4"G"}')
    fi

}
# find  /path -perm -1000
del_old_files(){
    local d="$1" cond="${2:-}"  # cond is obsolete
    local action=-delete
    ((DRYRUN)) && action=''

    if ((ATIME)); then
        echo "$SRC: del_old_files ${ANSI_INVERSE} $d  ATIME=$ATIME$ANSI_RESET" >&2
        [[ ${d%/*} != */zipsfs ]] && echo "$SRC: d='${d%/*}' ${ANSI_FG_RED}This script is in the wrong directory. Abort"$ANSI_RESET >&2 && return
        set -x
        find "$d" -atime +$ATIME $cond -print $action
        set +x
    else
        set -x
        find "$d" -atime +1 -name '*[0-9].bak' $cond -print $action
        find "$d" -atime +1 -name '*[0-9].tmp' $cond -print $action
        set +x
    fi
}

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
    if ((CD_TO_SCRIPT_PARENT)); then
        cd ${BASH_SOURCE%/*} || return
        date > $BASH_SOURCE.lastRun
    fi
    SRC=${BASH_SOURCE[0]}
    SRC=${SRC##*/}
    ATIME=0
    del_old_files $PWD/preloaded
    del_old_files $PWD/converted
    for ((ATIME=DAYS_MAX; ATIME>=DAYS_MIN; ATIME=2*ATIME/3)); do
        call_df
        echo "$SRC: ATIME=$ATIME  ${ANSI_FG_MAGENTA}PWD=$PWD$ANSI_RESET WANT_FREE_GIGABYTE=$WANT_FREE_GIGABYTE AVAILABLE=$AVAILABLE  Enough=$((AVAILABLE>WANT_FREE_GIGABYTE))" >&2
        ((AVAILABLE>WANT_FREE_GIGABYTE)) && break
###        del_old_files $PWD/preloaded "-perm -1000" # Those files are reconginzed by sticky flag S_ISVTX
        del_old_files $PWD/preloaded
        del_old_files $PWD/converted
    done
}



main

# cp /home/cgille/git_projects/ZIPsFS/src/ZIPsFS_cleanup.sh  /home/_cache/rawfile/ZIPsFS/modifications/zipsfs/
