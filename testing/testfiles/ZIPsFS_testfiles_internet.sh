#!/usr/bin/env bash
set -u

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh

main(){
    echo "${ANSI_INVERSE}Test downloaded internet files $ANSI_RESET"
    read -r -p "Remove $MODI/zipsfs/n/? [y/N] "
    if [[ ${REPLY,,} == *y*  ]]; then
        rm -v -r -f "$MODI/zipsfs/n/";
        sleep 1
    fi
    test_pattern    $MNT/zipsfs/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README ' warranties '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE  ' License '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml 'keywordList'

}
main "$@"
