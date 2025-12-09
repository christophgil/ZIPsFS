#!/usr/bin/env bash
set -u


readonly DIR=~/test/ZIPsFS/installation
readonly U=https://github.com/christophgil/ZIPsFS/archive/refs/heads/main.zip



main(){
    rm -r ~/test/ZIPsFS/installation
    [[ -d $DIR ]] && return 1
    mkdir -p $DIR || return 1
    cd $DIR || return 1
    rm -r ZIPsFS-main main.zip 2>/dev/null
    wget -N  $U
    unzip main.zip

pwd
    ls -l
    ./ZIPsFS-main/src/ZIPsFS.compile.sh

}
main
