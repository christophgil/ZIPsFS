#!/usr/bin/env bash
export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[100;1m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m' ANSI_RESET=$'\e[0m'


## Specify root directory here.  Test file trees will be written here
BASE=~/test/ZIPsFS/small_test

ROOT2=readonly2
ROOTS="readonly1 $ROOT2 readonly3"

set -u

mk_test_zip(){
    local z=$BASE/$ROOT2/my_zip_file.zip
    if [[ ! -s $z ]];then
        mk_test_files $BASE/zip zipentry-
        cd $BASE/zip || return
        zip -r $z *
    fi
}

mk_test_files(){
    local root=$1 pfx=$2
    local r subdir1 subdir2 f
    for r in $ROOTS; do
        for subdir1 in a b; do
            for subdir2 in x y; do
                for f in 1 2 3 4 5 6;do
                    local f=$root/$r/$subdir1/$subdir2/${pfx}root_${root##*/}_file_$f.txt
                    [[ -f $f ]] && continue
                    mkdir -p ${f%/*}
                    echo This is file $f >$f
                    ls -l $f
                done
            done
        done
    done
    tree $BASE/reado*
}


main(){
    mk_test_files $BASE ''
    mk_test_zip
    local exe_from_para=${1:-}
    local m=$BASE/mnt
    mkdir -p $m $BASE/writable
    echo "Maybe $m is still mounted - going to umount  ..."

    JUNKPARA='';  [[ $OSTYPE == *netbsd* ]] &&  JUNKPARA="$m $m"  ## Otherwise error Note enough para
    fusermount -u $m $JUNKPARA



    #    $exe $BASE/writable $ROOTS : -f -o allow_other $m
    local rr=''
    for r in $ROOTS;do rr+=" $BASE/$r"; done

    local exe=$exe_from_para
    if [[ -z $exe ]];then
        exe=~/git_projects/ZIPsFS/ZIPsFS
        [[ ! -s $exe ]] && exe=~/ZIPsFS/ZIPsFS
        [[ ! -s $exe ]] && echo 'Missing argument: executable of ZIPsFS' && return 1
        echo "Using default exe $exe"
    fi
    if [[ ! -s $exe ]];then
        echo "$exe (executable of ZIPsFS) does not exist"
        echo "Please pass path of ZIPsFS executable as argument."
    else
        echo "Going to mount ZIPsFS on $m. This folder is still empty. Observe this folder now!"
        read -r -p  "Press enter to continue"

        set +x
        if [[ $OSTYPE == *netbsd* ]];then
            if ldd $exe |fgrep 'not found'; then
                echo "${ANSI_FG_RED}Shared libs not found.$ANSI_RESET Therefore setting LD_LIBRARY_PATH:"
                echo '    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/pkg/lib'
                read -r -p  "Press enter to continue"
                export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:/usr/pkg/lib
            fi
        fi
        $exe  $BASE/writable $rr : -f  $m
    fi
    return

}

main "$@"
