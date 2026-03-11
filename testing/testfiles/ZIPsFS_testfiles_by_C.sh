#!/usr/bin/env bash
set -u

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh

main(){


		echo "${ANSI_INVERSE}Test remember file size $ANSI_RESET"

		local f=$MNT/example_generated_file/example_generated_file.txt
		# shellcheck disable=SC2010
		ls ${f%/*/*} | grep example_generated_file || echo -n  "$ANSI_FG_RED ls mnt: Missing  example_generated_file  $ANSI_RESET"
		test_pattern 1 ' heap ' $f

		local size1=$(cat $f | wc -c)
		local size2=$(stat  --format '%s' $f)

		#    echo "diff: $((size1-size2))  size1:'$size1' size2: '$size2'"
		echo  -n "Testing hash-map of file sizes ... "

		if ((size1-size2)); then
				echo "$ANSI_FG_RED File size $f:    $size1 != $size2 $ANSI_RESET"
		else
				echo  "$ANSI_FG_GREEN OK  $size1 == $size2 $ANSI_RESET"
		fi


}
main "$@"
