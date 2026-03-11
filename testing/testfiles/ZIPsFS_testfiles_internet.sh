#!/usr/bin/env bash
set -u

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh

main(){


		echo "${ANSI_INVERSE}Test downloaded internet files $ANSI_RESET"
		read -r -p "Remove $MODI/ZIPsFS/n/? [Y/n] "
		if [[ -z $REPLY || ${REPLY,,} == *y*  ]]; then
				rm -v -r -f "$MODI/ZIPsFS/n/";
				sleep 1
		fi

		test_pattern 2 ' warranties '  $MNT/ZIPsFS/n/https,,,ftp.uniprot.org,pub,databases,uniprot,README


		test_pattern 2 ' License '     $MNT/ZIPsFS/n/ftp,,,ftp.uniprot.org,pub,databases,uniprot,LICENSE
		test_pattern 2 'keywordList'   $MNT/ZIPsFS/n/ftp,,,ftp.ebi.ac.uk,pub,databases,uniprot,current_release,knowledgebase,complete,docs,keywlist.xml

}
main "$@"
