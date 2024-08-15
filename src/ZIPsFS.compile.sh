#!/usr/bin/env bash

########################
### Please customize ###
########################

WITH_SANITIZER=0
IPATHS='-I . -I/usr/local/include/fuse3  -I/usr/local/include -I/opt/local/include -I/usr/pkg/include'
LPATHS='-L/usr/pkg/lib -L/usr/local/lib -L/opt/local/lib'

CCOMPILER=CC
clang --version && CCOMPILER=clang

export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[100;1m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'

########################
### CLI Parameters   ###
########################
press_ctrl_c(){
    read -r -p  'Press Ctrl-C'
}

while getopts 'sg' o;do
    case $o in
        s) WITH_SANITIZER=0;;
        g) CCOMPILER=gcc;;
        *) echo "wrong_option $o"; press_ctrl_c;;
    esac
done
shift $((OPTIND-1))
echo "WITH_SANITIZER:$WITH_SANITIZER  CCOMPILER:$CCOMPILER"
DIR=${BASH_SOURCE[0]%/*}
TEMP=~/tmp/ZIPsFS/compilation
! mkdir -p $TEMP && press_ctrl_c


detect_fuse_version(){
    local exe=$TEMP/print_fuse_version p
    [[ -d /usr/pkg/lib &&  $OSTYPE == *netbsd* ]] && export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/pkg/lib
    for p in -lfuse3 -lfuse2 -lfuse1 -lfuse; do
        rm $exe 2>/dev/null
        if $CCOMPILER $IPATHS $LPATHS $p $DIR/print_fuse_version.c -o  $exe 2>$TEMP/try_compile$p.log && [[ -s $exe ]];then
            if ! $exe;then
                echo "${ANSI_FG_RED}Problem$ANSI_RESET running $exe"
                press_ctrl_c
            else
                echo $p
            fi
            return 0
        fi
    done
    echo "${ANSI_FG_RED}Problem $ANSI_RESET compiling with libfuse. Is it install? Possiple package names 'libfuse3-dev', 'ibfuse3', 'libfuse-dev' or  'ibfuse'" >&2
    press_ctrl_c
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
    $CCOMPILER $src -o ${src%.c} 2>$TEMP/try_compile_without-lexecinfo.log && echo 'Not require option -lexecinfo' >&2  && return 0
    $CCOMPILER $src -lexecinfo -o ${src%.c} 2>$TEMP/try_compile_with-lexecinfo.log && echo 'Do require option -lexecinfo' >&2 && echo ' -lexecinfo' && return 0
    echo "${ANSI_FG_RED}Problem $ANSI_RESET  compiling function backtrace_symbols()."
    press_ctrl_c
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
    exe=${DIR%/*}/ZIPsFS
    [[ -s $exe ]] && rm -v $exe
    {
        cat $DIR/ZIPsFS_stat_queue.c $DIR/ZIPsFS.c $DIR/ZIPsFS_cache.c  $DIR/ZIPsFS_debug.c $DIR/ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
        cat $DIR/ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
    }> $DIR/ZIPsFS.h


    ! cd ~ && press_ctrl_c # Otherwise the logs contain relative paths

    sanitize=''
    LL="-lpthread -lm -lzip $LFUSE $(print_linker_option_execinfo)"
    ##    [[ $OSTYPE == *freebsd* ]] && LL+=' -lexecinfo'
    NOWARN="-Wno-string-compare"
    {

        if [[ $CCOMPILER == clang ]];then
            export PATH=/usr/lib/llvm-10/bin/:$PATH
            sanitize=''
            ((WITH_SANITIZER)) && sanitize="-fsanitize=address  -fno-omit-frame-pointer"
            export PATH=/usr/lib/llvm-10/bin/:$PATH
        else
            NOWARN+=" -Wno-format-truncation"
        fi
        set -x
        $CCOMPILER  $NOWARN  -DHAVE_CONFIG_H $IPATHS  -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $sanitize  $DIR/ZIPsFS.c $LPATHS $LL $version   -o $exe
        set +x
    } # 2>&1 | HL_ERROR
    if ls -l -h $exe;then
        echo "$ANSI_FG_GREEN Success $ANSI_RESET"
    else
        echo "$ANSI_FG_RED Failed $ANSI_RESET"
    fi
    echo LL=$LL
    echo sanitize=$sanitize
    echo "See logs in $TEMP"
    if [[ -s $exe ]];then
        echo 'Suggest testing: '
        [[ $OSTYPE == *freebsd* ]] && "If it might not work, try as root"
        echo "$DIR/ZIPsFS_testing.sh $exe"
    fi

}

main "$@"
