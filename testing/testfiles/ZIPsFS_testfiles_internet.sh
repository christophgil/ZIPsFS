#!/usr/bin/env bash
set -u

source ${BASH_SOURCE%/*}/ZIPsFS_testfiles_inc.sh

main(){
    echo "${ANSI_INVERSE}Test downloaded internet files $ANSI_RESET"
    test_pattern    $MNT/zipsfs/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README ' warranties '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,README   ' warranties '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE  ' License '

    local f=$MNT/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml
    test_pattern    $f.gz 'keywordList'
    test_pattern    $f    'keywordList'

    local f=$MNT/zipsfs/n/ftp,,,ftp.expasy.org,databases,uniprot,current_release,knowledgebase,reference_proteomes,Viruses,UP001733680,UP001733680_2006137.fasta
    test_pattern    $f.gz ' protein '
    test_pattern    $f    ' protein '
}
main "$@"


if askRemoveYn; then
    rm -v -r -f "$MODI/zipsfs/n/";
    main
fi
