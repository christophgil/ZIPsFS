#!/usr/bin/env bash
[[ -s ZIPsFS ]] && rm -v ZIPsFS
makeheaders ZIPsFS.c

if false;then
    gcc -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lzip -lsqlite3 -o ZIPsFS
else
    clang -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g   $PWD/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lzip -lsqlite3 -o ZIPsFS
fi
ls -l -h ZIPsFS.h ZIPsFS
