#!/usr/bin/env bash

! clang --version && echo -e ' Please install clang.\n See https://apt.llvm.org/\n See https://github.com/llvm/llvm-project/releases \n sudo apt-get install clang'
[[ -s ZIPsFS ]] && rm -v ZIPsFS
{
    cat ZIPsFS_stat_queue.c ZIPsFS.c ZIPsFS_cache.c  ZIPsFS_debug.c ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
    cat  ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
}> ZIPsFS.h

dir=$PWD
cd ~ # Otherwise the logs contain relative paths

if grep -n  '^ *[a-z].*) *LOCK_N(' $(find $dir -name '*.c');then
    echo 'Error: LOCK_N(...) requires curly braces'
    exit 1
fi
as=''
if false;then
    gcc -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -o $dir/ZIPsFS
else
    echo 'export PATH=/usr/lib/llvm-10/bin/:$PATH' > $dir/set_path.sh
    source $dir/set_path.sh
    as="-fsanitize=address  -fno-omit-frame-pointer"
    export PATH=/usr/lib/llvm-10/bin/:$PATH
    clang -Wno-string-compare -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip  -o $dir/ZIPsFS
fi
ls -l -h $dir/ZIPsFS{.h,}
