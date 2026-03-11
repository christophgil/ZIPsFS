#!/usr/bin/env bash

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh

test_zipinline(){
		echo "${ANSI_INVERSE}Test zip entry $ANSI_RESET"
		export GREP_COLORS='ms=01;32'
		local D=$1
		ls -l -d  $MNT/$D*
		sleep 2
		# shellcheck disable=SC2010
		if ls $MNT/$D | grep --color=always wiff && sleep 3 && ls $MNT/$D | grep --color=always wiff$; then
				echo -n  "$ANSI_FG_GREEN OK $ANSI_RESET"
		else
				echo -n  "$ANSI_FG_RED Error ZIP inline $ANSI_RESET"
				ls $MNT/$D
				Enter
		fi
}




main(){
		assure_mountpoint_db pride || return 1
		echo 'Running for the first time, you will soon see a file listing where the Zip files are not yet expanded'
		echo 'This will take some time because the files are remote'
		time test_zipinline /DB/pride/2025/07/PXD063487
		echo 'If you run the script a again after some time, the content of the  Zip files rather than the Zip-files themselfs will be shown.'
}
main "$@"
