#!/usr/bin/env bash

IS_CLANG=1
WITH_SANITIZER=0

while getopts 'sg' o;do
    case $o in
        s) WITH_SANITIZER=0;;
        g) IS_CLANG=0;;
        *) echo "wrong_option $o"; return;;
    esac
done
shift $((OPTIND-1))
read -t 10 -r -p "WITH_SANITIZER=$WITH_SANITIZER  IS_CLANG=$IS_CLANG"



IS_APPLE=0
IS_NETBSD=0
[[ $OSTYPE == darwin* ]] && IS_APPLE=1
[[ $OSTYPE == netbsd* ]] && IS_NETBSD=1
[[ $OSTYPE == freebsd* ]] && IS_FREEBSD=1

((IS_CLANG)) && ! clang --version && IS_CLANG=0 && echo -e ' Please install clang.\n See https://apt.llvm.org/\n See https://github.com/llvm/llvm-project/releases \n sudo apt-get install clang'

dir=${BASH_SOURCE[0]%/*}
[[ -s $dir/../ZIPsFS ]] && rm -v $dir/../ZIPsFS
{
    cat $dir/ZIPsFS_stat_queue.c $dir/ZIPsFS.c $dir/ZIPsFS_cache.c  $dir/ZIPsFS_debug.c $dir/ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
    cat $dir/ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
}> $dir/ZIPsFS.h


cd ~ || read -r -p Enter # Otherwise the logs contain relative paths

# if grep -n  '^ *[a-z].*) *LOCK_N(' $(find $dir -name '*.c');then
#     echo 'Error: LOCK_N(...) requires curly braces'
#     exit 1
# fi
as=''
I='-I . -I/usr/local/include/fuse3  -I/usr/local/include -I/opt/local/include -I/usr/pkg/include'
HL_ERROR(){
    grep --color=always -i 'error\|$'
}
LL='-lpthread -lm -lzip -lfuse'
LP='-L/usr/pkg/lib -L/usr/local/lib -L/opt/local/lib'
! ((IS_APPLE || IS_NETBSD)) && LL+=3
NOWARN="-Wno-format-truncation -Wno-string-compare"
((IS_FREEBSD)) && LL+=' -lexecinfo'
{
    if ((IS_CLANG));then
        echo using clang
        echo 'export PATH=/usr/lib/llvm-10/bin/:$PATH' > $dir/set_path.sh
        source $dir/set_path.sh
        as=''
        ((WITH_SANITIZER)) && as="-fsanitize=address  -fno-omit-frame-pointer"
        export PATH=/usr/lib/llvm-10/bin/:$PATH
        clang  $NOWARN  -DHAVE_CONFIG_H $I  -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $dir/ZIPsFS.c $LP $LL  -o $dir/../ZIPsFS
    else
        echo Using gcc
        gcc  $NOWARN -DHAVE_CONFIG_H  $I  -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  $dir/ZIPsFS.c   $LP $LL -o $dir/../ZIPsFS
    fi
} # 2>&1 | HL_ERROR
ls -l -h $dir/ZIPsFS.h  $dir/../ZIPsFS
echo LL=$LL
echo as=$as
echo WITH_SANITIZER=$WITH_SANITIZER
