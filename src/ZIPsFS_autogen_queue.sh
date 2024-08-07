#!/usr/bin/env bash

#Archive:  /s-mcpb-ms03/CHA-CHA-RALSER-RAW/store/6600-tof2/Data/40-0010/20211001_TOF2_FA_254_40-0010_EURAC-MSBatch12_P05_H11.rawIdx.Zip 1.8 GB
#   /s-mcpb-ms03/union/test/is/ZIPsFS/a/6600-tof3/Data/s-mcpb-ms03/union/test/is/ZIPsFS/a/6600-tof3/Data/

#Archive: //s-mcpb-ms03/slow2/incoming/6600-tof3/Data/50-0086/20240229S_TOF3_FA_060_50-0086_OxoScan-MSBatch04_P08_H01.rawIdx.Zip 107 MB  wiff.scan 44 Byte
#raw rawIdx wiff
# for i in *.wiff.scan; do ls ${i%.scan} || rm -v "$i"; done
set -u
shopt -s nullglob
source ~/ref/sh/wine64_isolated.sh # Set up Wine
RUN_WINE="wine64_isolated /local/wine_prefix/wine_for_dia_convert3"
export NICE_WINE='time nice'
SOSRAW2WIFF_EXE='/home/cgille/wine/raw_convert/SOSRaw2Wiff_2023_10_25.exe'
SOS_DIR='/home/cgille/wine/raw_convert/SOSRaw2Wiff_Ver1.0'
RAW2WIFF_EXE='/home/cgille/wine/raw_convert/2020_11_raw2wiff.exe'
DIR_BASE=/slow3/Users/x/ZIPsFS/modifications/ZIPsFS/a
DIR_QUEUE=$DIR_BASE/.queue
DIR_TMP=$DIR_BASE/.temp
tmux_rename_window(){
    [[ -n ${1:-} ]] && tmux rename-window -t "$(tmux display-message -p '#I')" "$1"
}

#########################################################################
### The queue is recognized by the absence of command line arguments  ###
### Returns the number of instances                                   ###
#########################################################################
count_queue_running(){
    local msg=${1:-}
    local p=${BASH_SOURCE[0]} z
    p=${p##*/}
    z=$(pgrep -a -f "/$p$"| tee /dev/stderr |wc -l)
    ((z==0)) && echo "Is the queue running? Please start $0 without args as user $USER in a tmux window." >&2 && echo $msg >&2
    return $z
}
#########################################################################
### Generate .wiff.scan file using SOSRaw2WIF.exe or raw2wiff.exe     ###
### Important:  The wine process must not be redirected!              ###
### Calling this directly from ZIPsFS, lead to frozen FS and Zoombies ###
#########################################################################
cleanup_tmp(){
    echo "cleanup_tmp ..." >&2
    local d
    mkdir -p $DIR_TMP/trash
    for d in $DIR_TMP/*; do
        local pid=${d##*/}
        [[ ! -e $d ]] && continue
        if [[ /proc ]];then
            [[ -e /proc/$pid ]] && continue
        else
            ps -p $pid && continue
        fi
        rm -v $d/*.wiff.1.~idx2
        mv -v  $d $DIR_TMP/trash/
    done
    echo cleanup_tmp Done >&2
}
mk_wiff_scan(){
    local infile=$1 outfile=${2}
    local tmp=$DIR_TMP/$$ base=$infile
    mkdir -p $tmp || return 1
    base=${infile%.rawIdx}
    base=${base%.wiff2}
    base=${base%.wiff}
    base=${base%.wiff.scan}
    base=${base%.raw}
    local PFX=$'\e[45m MK SCAN\e[0m'
    echo $PFX "base: $base" >&2
    local e
    cd $tmp || return 1
    rm $tmp/*.wiff $tmp/*.wiff2 ./*.wiff.1.~idx2 $tmp/*.wiff.scan $tmp/*.raw $tmp/*.rawIdx  2>/dev/null
    for e in $base.{raw,rawIdx,wiff,wiff2};do
        [[ -s $e ]] && ln -v -f -s $e ./
    done
    echo $PFX Listing $tmp/ >&2 && ls -l >&2
    local b=${base##*/} cmd
    if [[ -s $base.rawIdx ]];then
        cmd="$RUN_WINE $RAW2WIFF_EXE  ./$b.raw ./$b.rawIdx ./$b.wiff"
    elif [[ -s $base.wiff2 ]];then
        cd $SOS_DIR || echo $PFX $'\e[41m Failed cd \e[0m' >&2
        cmd="$RUN_WINE $SOSRAW2WIFF_EXE $tmp/$b.raw"
    else
        echo $PFX $'\e[41m Missing '$base.rawIdx' or '$base.wiff2 $'\e[0m' >&2
        return 1
    fi
    echo $PFX "Going to run cd $PWD;  $cmd ..." >&2
    export WINEDEBUG=-all
    if $cmd;then
        sleep 10
        # Exit value not working. Typing Ctrl-C may leads to truncated file. Sleep allows for  Ctrl-C twice.
        echo $PFX "Return value $?" >&2
        local f
        echo $PFX "PWD=$PWD  b=$b" >&2
        for f in $b.wiff.scan ${b}TMP.wiff.scan ${b}TMP.wiff;do
            ! ls -l $f && continue
            [[ ! -s $f ]] && continue
            local o=${outfile%/*}/$f
            [[ $o == *TMP.wiff.scan ]] && o=${o%TMP.wiff.scan}.wiff.scan
            [[ $o == *TMP.wiff ]] && o=${o%TMP.wiff}.wiff
            echo $PFX "outfile=$outfile     o=$o" >&2
            [[ -s $o ]] && diff $f $o && continue
            mv -n -v -f  "$f"  "$o.$$.tmp"
            mv -n -v -f  "$o.$$.tmp"  "$o"
            chmod +x "$o" # A marker for cleanup
            chmod -w "$o"
            if [[ $o == *.wiff ]]; then
                #              ln -n -v $o $o.hardlink
                #                ln -n -v $o ${o%/*}/hardlink_${o##*/}.hardlink
                mkdir -p ${o%/*}/wiff && cp -v $o ${o%/*}/wiff/${o##*/}
                mkdir -p ${o%/*}/wiff_lnk && ln  -v $o ${o%/*}/wiff/${o##*/}
            fi
        done
        ls -l $outfile
    fi
}
##########################################
### Infinity loop process queue items  ###
##########################################
infinity_loop(){
    local interrupt=~/tmp/interrupt.d/ZIPsFS_autogen_queue
    mkdir ${interrupt%/*}; touch $interrupt
    while true;do
        local q infile count=0
        ls -l -t $DIR_QUEUE/*.q
        for q in $DIR_QUEUE/*.q;do
            [[ -s $interrupt ]] && break
            ((count++))
            read -r infile outfile <$q
            echo "#$count LOOP q: $q" >&2
            echo "#$count LOOP infile=$infile" >&2
            echo "#$count LOOP outfile=$outfile">&2
            [[ -z ${outfile:-} ]] && echo "#$count" $'\e[32m Outfile empty string  \e[0m' $infile >&2 && continue
            [[ ! -e $infile ]]    && echo "#$count" $'\e[41m Infile does not exist \e[0m' $infile >&2 && continue
            [[ -e $outfile  ]]    && echo "#$count" $'\e[32m Already exists        \e[0m' $outfile >&2 && continue
            ! mkdir -p ${outfile%/*}  && continue
            echo "case $q ... ">&2
            case $q in
                *.wiff.scan.q)
                    if mv -v $q $q.active;then
                        local cmd="mk_wiff_scan $infile $outfile"
                        echo '*************************************************************************** ' >&2
                        echo $'\e[7m' "#$count  Going to $cmd..." $'\e[0m' >&2
                        $cmd
                        [[ ! -s $outfile ]] && touch $outfile.failed
                    fi
                    ;;
            esac
            local z=FAILED
            [[ -s $outfile ]] && z=SUCCESS
            zip -j $DIR_QUEUE/$z.zip  $q.active && rm -v $q.active
        done
        read -r -t 10 -p "sleep 10 or press Enter" >&2
    done
}
#####################
### add_to_queue  ###
#####################
add_to_queue(){
    tmux_rename_window "add_to_queue"
    local f rel
    [[ -z ${DIR_ZIPSFS_MODIFICATIONS:-} ]] && echo "$RED_ERROR DIR_ZIPSFS_MODIFICATIONS not set" && return
    for f in "$@";do
        rel=${f##*/ZIPsFS/}
        [[ $f == "$rel" || ! -s $f ]] && continue
        rel=ZIPsFS/$rel
        local src='' dst=''
        case $f in
            *.wiff.scan) src=${f%.scan}; dst=$DIR_ZIPSFS_MODIFICATIONS/$rel;;
            *) continue;;
        esac
        [[ -s  $dst ]] && echo "$GREEN_ALREADY_EXISTS$dst" && continue
        local q=$DIR_QUEUE/${f##*/}.q
        [[ -s   $q ]]  && echo "$GREEN_ALREADY_EXISTS$q" && continue
        echo $src $dst >$q && echo "Written $q"
    done
}

#############
### main  ###
#############
main(){
    set -u
    shopt -s nullglob
    local OPTIND o
    mkdir -p  $DIR_QUEUE || return 1
    while getopts 'at' o;do
        case $o in
            a) shift $((OPTIND-1)); add_to_queue "$@"; return;;
            t) local w=6600-tof3/Data/50-0086/20240229S_TOF3_FA_060_50-0086_OxoScan-MSBatch04_P08_H01.wiff
               $0 /s-mcpb-ms03/union/test/is/ZIPsFS/a/$w /slow3/Users/x/ZIPsFS/modifications/ZIPsFS/a/$w.scan >$DIR_QUEUE/test.wiff.scan.q
               return;;
            *) echo wrong_option $o; return 1;;
        esac
    done
    shift $((OPTIND-1))

    case $# in
        0) tmux_rename_window "autogen"
           count_queue_running
           local instances=$?
           read -r -p "instances=$instances. Press enter"
           cleanup_tmp
           infinity_loop
           ;;
        2)local infile=$1 outfile=$2
          local f=$DIR_QUEUE/${outfile##*/}.q
          count_queue_running "Not going to add $outfile to queue" && return 1
          rm -v $outfile.failed 2>/dev/null
          echo $infile $outfile >$f
          echo Written $f >&2
          local count=0
          while [[ ! -s $outfile && ! -e $outfile.failed ]];do
              #((count%10==0)) &&
              count_queue_running "Stop waiting" && break
              read -r -t $((10+count++))  -p "sleep Waiting for $outfile" >&2
          done
          ls -l $outfile $outfile.failed >&2
          ;;
        *) echo "Wrong number of arguments $#" >&2
    esac
}



if [[ $(realpath -- ${BASH_SOURCE[0]}) == $(realpath -- $0) ]];then
    main "$@"
fi
