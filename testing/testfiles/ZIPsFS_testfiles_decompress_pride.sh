#!/usr/bin/env bash

set -u

source ${BASH_SOURCE%/*}/ZIPsFS_testfiles_inc.sh



go1(){
    local vp=$1
    ((WITH_REMOVE)) && [[ -s $MNT/$vp ]] && rm -v $MNT/$vp
    test_pattern $MNT/$vp  'Sequence'
}
WITH_REMOVE=0
main(){
    go1 /db/pride/2005/08/PRD000004/PRIDE_Exp_Complete_Ac_666.xml

}
main

askRemoveYn && main
