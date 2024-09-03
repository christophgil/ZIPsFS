#!/usr/bin/env bash

########################
### Please customize ###
########################
set -u
WITH_SANITIZER=0
IPATHS='-I . -I/usr/local/include/fuse3  -I/usr/local/include -I/opt/local/include -I/usr/pkg/include'
LPATHS='-L/usr/pkg/lib -L/usr/local/lib -L/opt/local/lib'

CCOMPILER=CC
clang --version && CCOMPILER=clang



export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m' ANSI_FG_GRAY=$'\e[30;1m' ANSI_FG_BLUE=$'\e[34;1m' ANSI_FG_BLACK=$'\e[100;1m' ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'


try_compile(){
    local success=1 name=$1  includes=$2  main=$3  opt=${4:-}
    local x=$TEMP/$name
    local c=$x.c
    [[ ! -s $c ]] && echo -e "$includes\nint main(int argc,char *argv[]){\n $main; }\n" >$c.$$.tmp && mv $c.$$.tmp $c
    ! { $CCOMPILER $opt $c -o $x  2>&1; }  >$x.log && success=0
    echo $success
    return $((success==0))
}

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
IS_CG=0
[[ $(hostname) == s-mcpb-ms03.charite.de ]] && IS_CG=1 && WITH_SANITIZER=1  # && CCOMPILER=gcc
SRC=${BASH_SOURCE[0]}
DIR=${SRC%/*}
TEMP=~/tmp/ZIPsFS/compilation
! mkdir -p $TEMP && press_ctrl_c

for f in $TEMP/*;do
    [[ -e $f && $SRC -nt $f ]] && rm "$f"
done

detect_fuse_version(){
    local p
    [[ -d /usr/pkg/lib &&  $OSTYPE == *netbsd* ]] && export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:/usr/pkg/lib
    for p in -lfuse3 -lfuse2 -lfuse1 -lfuse; do
        local x=$TEMP/fuse_version$p
        if try_compile ${x##*/} \
                       $'#include <stdio.h>\n#define _FILE_OFFSET_BITS 64\n#define FUSE_USE_VERSION 33\n#include <fuse.h>' \
                       'printf("-DFUSE_MAJOR_V=%d -DFUSE_MINOR_V=%d",FUSE_MAJOR_VERSION,FUSE_MINOR_VERSION); '  \
                       "$IPATHS $LPATHS $p" >/dev/null; then
            if ! $x;then
                echo "${ANSI_FG_RED}Problem$ANSI_RESET running $x" >&2
                press_ctrl_c
            else
                echo " $p"
            fi
            return 0
        fi
    done
    echo "${ANSI_FG_RED}Problem $ANSI_RESET compiling with libfuse. Is it install? Possiple package names 'libfuse3-dev', 'ibfuse3', 'libfuse-dev' or  'ibfuse'" >&2
    press_ctrl_c
    return 1
}
print_linker_option_execinfo(){
    local p
    [[ -d /usr/pkg/lib &&  $OSTYPE == *netbsd* ]] && export LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-}:/usr/pkg/lib
    for p in -lexecinfo ''; do
        if try_compile "backtrace$p" '#include <execinfo.h>' 'backtrace_symbols(0,0);' $p >/dev/null;then
            echo $p
            return 0
        fi
    done
    return 1
    echo "${ANSI_FG_RED}Problem $ANSI_RESET  compiling function backtrace_symbols() in $x.c."
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
    ((IS_CG)) && ! cd && return 1 # Otherwise the logs contain relative paths
    ! try_compile 'empty' '' '' >/dev/null && echo "Cannot compile simple file $TEMP/empty.c" && return 1
    local has_atos=0 has_addr2line=0 LL="-lpthread -lm -lzip  "  has_backtrace=0  sanitize=''  NOWARN="-Wno-string-compare" fuse_version
    addr2line -H >/dev/null 2>/dev/null && has_addr2line=1
    atos -h 2>/dev/null && has_atos=1
    LL+=" $(print_linker_option_execinfo) " && has_backtrace=1
    ! fuse_version="$(detect_fuse_version) " && echo "$ANSI_FG_RED Failed to compile tiny test program with libfuse $ANSI_RESET"  && return 1
    LL+=" -l${fuse_version##*-l} "
    local opts=-D"HAS_ADDR2LINE=$has_addr2line "-D"HAS_ATOS=$has_atos "-D"HAS_BACKTRACE=$has_backtrace ${fuse_version% -l*} "
    opts+=-D"HAS_EXECVPE=$(try_compile execvpe '#include <unistd.h>' 'execvpe("",NULL,NULL);' -Werror) "
    opts+=-D"HAS_UNDERSCORE_ENVIRON=$(try_compile us_environ '#include <unistd.h>' 'extern char **_environ; char **ee=_environ;') "
    opts+=-D"HAS_ST_MTIM=$(try_compile st_mtim '#include <sys/stat.h>' 'struct stat st;  st.st_mtim=st.st_mtim; ') "
    opts+=-D"HAS_POSIX_FADVISE=$(try_compile posix_fadvise '#include <fcntl.h>' 'posix_fadvise(0,0,0,POSIX_FADV_DONTNEED);') "
    local x=${DIR%/*}/ZIPsFS
    rm  "$x" 2>/dev/null
    [[ -s $x ]] && echo "${ANSI_RED}Warning, $x exists $ANSI_RESET"
    {
        cat $DIR/ZIPsFS_stat_queue.c $DIR/ZIPsFS.c $DIR/ZIPsFS_cache.c  $DIR/ZIPsFS_debug.c $DIR/ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
        cat $DIR/ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
    }> $DIR/ZIPsFS.h
    if [[ $CCOMPILER == clang ]];then
 #       export ASAN_OPTIONS='suppressions=ZIPsFS_asan.supp,detect_leaks=0,check_initialization_order=0,use-after-return=0,detect_stack_use_after_return=0'
#                export ASAN_OPTIONS="suppressions=$HOME/git_projects/ZIPsFS/src/ZIPsFS_asan.supp"
    ((IS_CG)) && export PATH=/usr/lib/llvm-20/bin/:$PATH
        ((WITH_SANITIZER)) && sanitize="-fsanitize=address  -fno-omit-frame-pointer  "
    else
        NOWARN+=" -Wno-format-truncation"
        ((WITH_SANITIZER)) && sanitize='-fsanitize=address -static-libasan'
    fi

    local c=${x}_compilation.sh
    echo "# This file has been created with $SRC"$'\n\n'$CCOMPILER $NOWARN $opts $IPATHS -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $sanitize $DIR/ZIPsFS.c $LPATHS $LL  -o $x |tee $c
    chmod +x $c
    press_ctrl_c
    . $c
    more $c |grep -v '^#'
    if ls -l -h $x;then
        echo "$ANSI_FG_GREEN Success $ANSI_RESET" $'\n\nSuggest testing:\n'$DIR/ZIPsFS_testing.sh $x'$\n'
        [[ $OSTYPE == *freebsd* ]] && echo 'If ZIPsFS does not work, try as root'
    else
        echo "$ANSI_FG_RED Failed $ANSI_RESET"
    fi




}
main "$@"
