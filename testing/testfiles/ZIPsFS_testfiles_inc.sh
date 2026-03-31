#!/usr/bin/env bash
set -o pipefail

export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[30m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'








crc32_zip_entry(){
    unzip -v $1 $2 | sed -n 4p| awk '{ print $7}'
}

Enter(){
    read -r -p 'Enter'
}

test_is_in_listing(){
    local vp=$1
    [[ $vp == *@SOURCE.TXT ]] && return
    find ${vp%/*} | grep  -F /${vp##*/} |sed 's|^|test_is_in_listing |1' || echo -e "\n${ANSI_FG_RED} MISSING   find ${vp%/*} | grep -q -F /${vp##*/}    $ANSI_RESET\n"
}

test_checksum(){
    local repeats=$1  crc32="$2" vp=$3  c
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
            prompt_error
        fi
        ((i)) && rm -v "$vp"
    done
}


test_pattern(){
    local  pattern="$1"  vp="$2"
    echo  "'$pattern' in $ANSI_FG_BLUE$vp$ANSI_RESET   ... "
    [[ $vp == *SOURCE.TXT ]] && strings $vp
    local result="${ANSI_FG_RED}FAILED"
    strings "$vp" | grep "$pattern" && result="${ANSI_FG_GREEN}OK"
    echo "$result$ANSI_RESET"
    test_is_in_listing $vp
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

print_src(){
    echo; head -v  "$1@SOURCE.TXT"; echo
    local pattern="${2:-}"
    [[ -n $pattern ]] &&  test_pattern $pattern        "$p@SOURCE.TXT"
}

prompt_error(){
    read -r -p  "$ANSI_FG_RED Error $ANSI_RESET  Press Enter" </dev/tty
}



ask_remove_converted(){
    read -r -p "Remove $MODI/zipsfs/c? [Y/n] "
    if [[ -z $REPLY || ${REPLY,,} == *y*  ]]; then
        rm -v -r "$MODI/zipsfs/c";
        sleep 1
    fi
}


assure_mountpoint_db(){
    local m=~/.ZIPsFS/db/$1
    mountpoint $m && ls -d $MNT/zipsfs/~1/db  && return 0
    echo "${ANSI_FG_RED}Error:${ANSI_RESET}   ZIPsFS_testfiles_start_ZIPsFS.sh needs to be run with option -f to mount the databases"
    return 1
}


MNT='mnt'
MODI=~/tmp/ZIPsFS/testing/modifications
REMOTE1=~/tmp/ZIPsFS/testing/remote1
TMPDIR=~/tmp/ZIPsFS/tmp
mkdir -p $MODI $REMOTE1 $TMPDIR
WITH_FTP_DIRS=0
STORE=''
VERBOSE=''
while getopts 'vdfsm:' o; do
    case $o in
        m) MNT=$OPTARG;;
        f) WITH_FTP_DIRS=1;;
        s) STORE=//s-mcpb-ms03.charite.de/fulnas1/1; ls -l -d $STORE || STORE='';;
        d) rm -v -r "/home/_cache/$USER/ZIPsFS/modifications";;
        v) VERBOSE=-v;;
        *) echo '$0 Invalid option '$o;exit 1;;
    esac
done
shift $((OPTIND-1))
! ls -d $MNT && read -r -p 'Error. Specify -m mnt '
