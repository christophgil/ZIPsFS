#!/usr/bin/env bash


###################################
### Please change accordingly:  ###
###################################
PASSTHROUGH_SRC=/local/filesystem/fuse-3.16.2/example
I=-I/usr/local/include/fuse3
WITH_SANITIZER=0


! clang --version && echo -e ' Please install clang.\n See https://apt.llvm.org/\n See https://github.com/llvm/llvm-project/releases \n sudo apt-get install clang'

n=${0##*/}
n=${n%.compile.sh}
exe=~/compiled/$n
mkdir -p ${exe%/*}
[[ -s $exe ]] && rm -v $exe


cd ${BASH_SOURCE[0]%/*}

as=''
((WITH_SANITIZER)) &&   as="-fsanitize=address  -fno-omit-frame-pointer"
clang -Wno-string-compare -DHAVE_CONFIG_H -I. -iquote$PASSTHROUGH_SRC  $I -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $n.c -lfuse3 -lpthread -lm -lzip  -o $exe

ls -l -h $exe



exe=~/compiled/${n}_ctrl
clang ${n}_inc.c -o $exe
ls -l -h $exe
