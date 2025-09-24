#!/usr/bin/env bash
set -u

readonly DIR=~/test/ZIPsFS/installation
readonly U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip

main(){
    mkdir -p $DIR || return
    cd $DIR || return
    rm -r ZIPsFS-main main.zip 2>/dev/null
    wget -N $U || return
    unzip -o main.zip || return
    ls -l
    ./ZIPsFS-main/src/ZIPsFS.compile.sh
}
main
