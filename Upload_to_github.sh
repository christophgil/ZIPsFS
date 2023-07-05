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

# CURRENT STATE

Usable.

However, we are still fixing minor bugs.

# MOTIVATION

We use closed-source proprietary Windows software and shared libraries for reading experimental data from different types of mass spectrometry machines.
Unfortunately, the sheer amount of file data brought our high-performance Windows server to a stand still.
Currently we are using  Wine on Ubuntu which works much better for large files.
Most data is archived in a read-only WORM file system for years.

To reduce the number of individual files,  files of one record are bundled in one ZIP.
We hoped that the data inside the ZIP was easily accessible using  pipes or process substitution. Unfortunately, this is not the case  for the proprietary  software we are using.
Another approach, mounting all ZIP files before starting the computation worked well in the past.

ZIPsFS has been developed to solve the following  problems:

- Recently  the size of our experiments and thus the number of ZIP files grew enormously.
  Mounting thousands of individual ZIP files at a time is not feasible.

- Some proprietary  software for mass spectrometry software requires write access for the volume where the files are loaded from.

- Some of the  files are  not read  sequentially. Instead data is read  from different file positions. This is particularly  inefficient
  for compressed  ZIP entries. The worst case is a  negative i.e. backward  seek operation.
  It  will result in a new remote request from the beginning of the member content.

- Experimental records are first stored in an intermediate storage as a ZIP file. Later after verification, these ZIP files are then moved to the WORM file system which acts as a final secure archive.
  Consequently there are different places to look for a particular file.

- With one  proprietary shared library to read the data, millions of redundant requests to the file system occur. This
  is usually not a  problem  when records are stored locally as flat files.  However, for remote file access  or mounted ZIP files there is a strong   performance loss.

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
