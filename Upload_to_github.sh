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
    # ZIPsFS - FUSE-based  overlay file system which expands  ZIP files

# MOTIVATION

We use closed-source proprietary Windows software and shared libraries for reading  experimental data from mass spectrometry machines.
Unfortunately, the sheer amount of file data brought our high-performance Windows server to a stand still.
Currently we are using  Wine on Ubuntu which works much better for large files.
Our data is archived in a read-only WORM file system. To reduce the number of individual files,  all files of one  record are bundled in one ZIP.

We hoped that we could use pipes or process substitution to access the data inside the ZIP, however this is not possible for the  software we are using.
However, mounting all ZIP files before starting the computation worked well in the past.

ZIPsFS has been developed to solve the following  problems:

- Recently  the size of our experiments and thus the number of ZIP files grew enormously.
  Mounting thousands of individual ZIP files at the same time is not feasible.

- Some proprietary  software for mass spectrometry software requires write access for the volume where the data is loaded from.

- Some of the  files are  not read  sequentially. Instead data is read  from varying file positions. This is particularly  inefficient
  for compressed  ZIP entries. The worst case is a  negative i.e. backward  seek operation.
  It  will result in a new remote request from the beginning of the member content.

- When experimental records  are archived the files are first stored in an  intermediate storage as a ZIP file. Later after verification, these ZIP files are moved to the WORM file system.
  Consequently there are different places where files could be located.

EOF
    echo
    cat $MD
}




main(){

    local msg="${1:-}"
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
