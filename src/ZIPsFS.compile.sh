#!/usr/bin/env bash

########################
### Please customize ###
########################

WITH_SANITIZER=0
IPATHS='-I . -I/usr/local/include/fuse3  -I/usr/local/include -I/opt/local/include -I/usr/pkg/include'
LPATHS='-L/usr/pkg/lib -L/usr/local/lib -L/opt/local/lib'

CCOMPILER=CC
clang --version && CCOMPILER=clang
########################
### CLI Parameters   ###
########################

while getopts 'sg' o;do
    case $o in
        s) WITH_SANITIZER=0;;
        g) CCOMPILER=gcc;;
        *) echo "wrong_option $o"; return;;
    esac
done
shift $((OPTIND-1))
echo "WITH_SANITIZER:$WITH_SANITIZER  CCOMPILER:$CCOMPILER"
DIR=${BASH_SOURCE[0]%/*}
TEMP=~/tmp/ZIPsFS/compilation
mkdir -p $TEMP || read -r -p  "Error"

export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_RESET=$'\e[0m'
detect_fuse_version(){
    local exe=$TEMP/print_fuse_version p
    for p in -lfuse3 -lfuse2 -lfuse1 -lfuse; do
        rm $exe 2>/dev/null
        if $CCOMPILER $IPATHS $LPATHS $p $DIR/print_fuse_version.c -o  $exe && [[ -s $exe ]];then
            $exe
            echo $p
            return 0
        fi
    done
    return 1
}

print_linker_option_execinfo(){
    local src=$TEMP/test_lib_execinfo.c
    cat << EOF > $src
#include <execinfo.h>
int main(int argc, char *argv[]){
    backtrace_symbols(0,0);
}
EOF
    $CCOMPILER $src -o ${src%.c} && echo "Not require option -lexecinfo">&2 && return 0
    $CCOMPILER $src -lexecinfo -o ${src%.c} && "Do require option -lexecinfo">&2 && echo ' -lexecinfo' && return 0
    return 1
}

HL_ERROR(){
    grep --color=always -i 'error\|$'
}

find_bugs(){
    if grep -n  '^ *[a-z].*) *LOCK_N(' $(find $DIR -name '*.c');then
        echo 'Error: LOCK_N(...) requires curly braces'
        exit 1
    fi
}

main(){
    local version
    ! version=$(detect_fuse_version) && echo "$ANSI_FG_RED Failed to compile tiny test program with libfuse $ANSI_RESET"  && return 1
    echo version: $version
    [[ -s $DIR/../ZIPsFS ]] && rm -v $DIR/../ZIPsFS
    {
        cat $DIR/ZIPsFS_stat_queue.c $DIR/ZIPsFS.c $DIR/ZIPsFS_cache.c  $DIR/ZIPsFS_debug.c $DIR/ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
        cat $DIR/ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
    }> $DIR/ZIPsFS.h


    cd ~ || read -r -p Enter # Otherwise the logs contain relative paths

    as=''
    LL="-lpthread -lm -lzip $LFUSE $(print_linker_option_execinfo)"
    ##    [[ $OSTYPE == freebsd* ]] && LL+=' -lexecinfo'
    NOWARN="-Wno-string-compare"
    {

        if [[ $CCOMPILER == clang ]];then
            echo using clang
            export PATH=/usr/lib/llvm-10/bin/:$PATH
            as=''
            ((WITH_SANITIZER)) && as="-fsanitize=address  -fno-omit-frame-pointer"
            export PATH=/usr/lib/llvm-10/bin/:$PATH
        else
            NOWARN+=" -Wno-format-truncation"
        fi
        set -x
        $CCOMPILER  $NOWARN  -DHAVE_CONFIG_H $IPATHS  -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $DIR/ZIPsFS.c $LPATHS $LL $version   -o $DIR/../ZIPsFS
        set +x
    } # 2>&1 | HL_ERROR
    if ls -l -h $DIR/../ZIPsFS;then
        echo "$ANSI_GREEN Success $ANSI_RESET"
    else
        echo "$ANSI_FG_RED Failed $ANSI_RESET"
    fi
    echo LL=$LL
    echo as=$as
    echo WITH_SANITIZER=$WITH_SANITIZER
}

main "$@"
