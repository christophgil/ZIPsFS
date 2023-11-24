#!/usr/bin/env bash
[[ -s ZIPsFS ]] && rm -v ZIPsFS



# makeheaders ZIPsFS.c
cat ZIPsFS.c ZIPsFS_cache.c  ZIPsFS_debug.c  |     sed -n 's|^\(static .*) *\){$|\1;|p'  > ZIPsFS.h


for script in *.bash;do
    {
        echo -n '"'
        sed -e 's|$|\\n\\|1' $script
        echo -n '"'
    } >$script.inc
done

#read -p aaaaaaaaaa

dir=$PWD
cd ~ # Otherwise the logs contain relative paths


if false;then
    gcc -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -ggdb  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -o $dir/ZIPsFS
else

    as="-fsanitize=address -fno-omit-frame-pointer"
    #     -fsanitize=memory not combinable with =address
    # as='-fsanitize=thread -fno-omit-frame-pointer'
    # as=''
    ##    The leak detection is turned on by default on Linux

    clang -DHAVE_CONFIG_H -I. -I/usr/local/include/fuse3 -O0 -D_FILE_OFFSET_BITS=64 -rdynamic -g $as  $dir/ZIPsFS.c -lfuse3 -lpthread -L/usr/local/lib -lm -lzip -o $dir/ZIPsFS
fi
ls -l -h $dir/ZIPsFS{.h,}
