#!/usr/bin/env bash
set -u
SRC=${BASH_SOURCE[0]}
ROOT1=~/tmp/ZIPsFS/root1

prepare(){
    mkdir -p $ROOT1
    local z=$ROOT1/brukertimstof_test.d.Zip;
    if [[ ! -s $z ]]; then
        local a aa='analysis.tdf analysis.tdf_bin'
        for a in $aa; do
            echo "This is $a" > $ROOT1/$a
        done
        pushd ${z%/*}; zip $z $aa; popd
        pushd $ROOT1; rm $aa; popd
    fi
    ls -l $z
}
main(){

    mnt=~/tmp/ZIPsFS/mnt
    ME=~/mnt/simulate_error
    mkdir -p $mnt
    mountpoint $mnt && umount -l $mnt >/dev/null
    mountpoint  $mnt 2>&1 | grep 'not connected' &&  sudo umount $mnt
    tmux rename-window   ZIPsFS
    root2=/$ME/$(realpath $ROOT1)

    echo root1=$ROOT1; ls $ROOT1
    echo root2=$root2; ls $root2
    sleep 1
    echo Going to run ZIPsFS
    set -x
    # /$root2

    ${SRC%/*}/../..//ZIPsFS -k -l 33GB -c rule /slow3/Users/cgille/ZIPsFS/modifications/    "$root2/"  "$ROOT1/"    :   $mnt
    # ${SRC%/*}/../..//ZIPsFS -k -l 33GB -c rule    $root2   :   $mnt

    set +x
}
prepare
main "$@"
