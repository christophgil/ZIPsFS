#!/usr/bin/env bash
[[ -s ZIPsFS ]] && rm -v ZIPsFS

# makeheaders ZIPsFS.c
sed -n 's|^\(static .*) *\){$|\1;|p' ZIPsFS.c > ZIPsFS.h

if true;then




if false;then
    gcc -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -lsqlite3 -o ZIPsFS
else
    clang -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g   $PWD/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -lsqlite3 -o ZIPsFS
fi
ls -l -h ZIPsFS.h ZIPsFS
fi
