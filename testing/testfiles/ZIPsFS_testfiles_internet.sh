#!/usr/bin/env bash
set -u

source ${BASH_SOURCE%/*}/ZIPsFS_testfiles_inc.sh

main(){
    echo "${ANSI_INVERSE}Test downloaded internet files $ANSI_RESET"
    test_pattern    $MNT/zipsfs/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README ' warranties '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,README ' warranties '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE  ' License '
    test_pattern    $MNT/zipsfs/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml 'keywordList'
}
main "$@"


if askRemoveYn; then
    rm -v -r -f "$MODI/zipsfs/n/";
    main
fi
