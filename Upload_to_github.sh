#!/usr/bin/env bash
set -u
DIR="${BASH_SOURCE[0]}"
DIR=${DIR%/*}
MD=$DIR/ZIPsFS.1.md
MAN=$DIR/ZIPsFS.1
README_MD=$DIR/README.md

make_man(){
    pandoc $MD -s -t man > $MAN
}
make_readme(){
    cat <<EOF
    # ZIPsFS - Fuse-based  overlay file system which expands  ZIP files

# Motivation

We use closed-source proprietary Windows software and shared libraries for data conversion of high throughput experimental data. However,
the sheer amount of file data brought the high-performance Windows machine regularly to a stand still.
We ended up with Wine on Ubuntu which easily copes with the high amount of data.
Unfortunately, implementation and  user interface prevents usage of  UNIX techniques  to use  zipped files from our storage without creating intermediate files.
Furthermore some software demands write access to the file location.

We used to use zip-fuse to mount  ZIP files in the storage. Before starting computation, all required ZIP files were mounted.
Symbolic links solved the problem of demanded write access.
However, recently  the size of our experiments and thus the number of ZIP files grew, which rendered this method unusable.

Furthermore we are concerned about the health of our conventional hard disks since many  threads are simultaneously reading files of 2GB and more at different file positions.
There are also files which are sqlite3 databases. This leads to large numbers of seek operations which is inefficient for compressed and remote files.

ZIPsFS has been developed to solve all these problems.
EOF
    cat $MD
}




main(){

    local msg="${1:--}"
    echo mmmmmmmmmmmmmmmmmmmmmmmm $msg
    if [[ -z $msg ]]; then
        echo "Missing: commit message"
    else

        make_man
        make_readme > $README_MD

        git commit -a -m "$msg"
        git push origin
    fi
}


main "$@"
