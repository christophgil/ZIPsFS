#!/usr/bin/env bash

set -u

source ${BASH_SOURCE%/*}/ZIPsFS_testfiles_inc.sh

VP=/test/test_underestimated_filesize/big_file.txt
mk_files(){
    local rp=$REMOTE1/$VP.gz
    mkdir -p ${rp%/*}
    if ! gunzip -c  $rp |grep -q LAST_LINE; then
        {
            dd if=/dev/zero of=/dev/stdout  bs=1024 count=4096
            echo
            echo Message at end of file.
            echo LAST_LINE
        } |gzip -c >$rp
    fi
    ls -l $rp
}

WITH_REMOVE=0
main(){
cat << EOF
${ANSI_UNDERLINE}Testing the call to fuse_invalidate_path().$ANSI_RESET
For dynamically generated files, reading may stop prematurely if stat() reports a file size
that is too small. The client will see a truncated file.

This is the case for this test file. Since it contains almost entirely zero bytes, the
compressed file is very small. ZIPsFS therefore initially estimates a file size that is
too small for the uncompressed file.

Solution: ZIPsFS calls fuse_invalidate_path(). Once the file has been inflated and its
actual size is known, the kernel is notified that the cached file attributes are invalid.

You can verify this behavior with the following experiment:
   Set WITH_FUSE_INVALIDATE_PATH to 0 and run this script again.
   The last line of the test file will not be visible because the estimated file size
   is too small.
EOF
    local path="$MNT/zipsfs/d/-/$VP"
    ((WITH_REMOVE)) && rm  "$path"
    mk_files
    ls -l $path
    local success="${ANSI_FG_RED}failed"
    grep -a --color LAST_LINE $path && success="${ANSI_FG_GREEN}OK"
    echo $success$ANSI_RESET

}
main

askRemoveYn && main
