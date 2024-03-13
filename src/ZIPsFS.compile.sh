#!/usr/bin/env bash

# https://apt.llvm.org/
[[ -s ZIPsFS ]] && rm -v ZIPsFS



# makeheaders ZIPsFS.c
#cat ZIPsFS_stat_queue.c ZIPsFS.c ZIPsFS_cache.c  ZIPsFS_debug.c ZIPsFS_log.c  |     sed -n 's|^\(static .*) *\){$|\1;|p'  > ZIPsFS.h
{
    cat ZIPsFS_stat_queue.c ZIPsFS.c ZIPsFS_cache.c  ZIPsFS_debug.c ZIPsFS_log.c  | sed -n 's|^\(static .*)\) *{$|\1;|p'
    cat  ZIPsFS_log.c | grep '^#define .*\*TO_HEADER\*'
}> ZIPsFS.h

#/*TO_HEADER*/





dir=$PWD
cd ~ # Otherwise the logs contain relative paths

if grep -n  '^ *[a-z].*) *LOCK(' $(find $dir -name '*.c');then
    echo 'Error: LOCK(...) requires curly braces'
    exit 1
fi
# https://github.com/llvm/llvm-project/releases
if false;then
    gcc -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -o $dir/ZIPsFS
else

    echo 'export PATH=/usr/lib/llvm-10/bin/:$PATH' > $dir/set_path.sh
    source $dir/set_path.sh

    #    as="-fsanitize=address -fsanitize-ignorelist=$dir/sanitize-ignorelist.txt  -fno-omit-frame-pointer"
    #     -fsanitize=memory not combinable with =address
    #      This does not work export ASAN_SYMBOLIZER_PATH=/usr/bin/llvm-symbolizer-12
        as="-fsanitize=address  -fno-omit-frame-pointer"
    export PATH=/usr/lib/llvm-10/bin/:$PATH

    clang --version
    clang -Wno-string-compare -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip  -o $dir/ZIPsFS
fi
ls -l -h $dir/ZIPsFS{.h,}
