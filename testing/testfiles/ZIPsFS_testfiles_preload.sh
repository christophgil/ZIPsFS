#!/usr/bin/env bash
set -u

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh
declare -A CC=([gzip]=gz [bzip2]=bz2 [xz]=xz [lrz]=lrz  [compress]=Z)
VP=txt/numbers.txt
RP=$REMOTE1/$VP


go1(){
		local opt=$1 c=$2  vp=${VP}
		local sfx=${CC[$c]}
		[[ -n $sfx ]] && vp=${vp%.txt} && vp+=_$sfx.txt.$sfx

		local p=$MNT/ZIPsFS/lr/${vp%.$sfx}
		case $opt in
				-m) local f=$REMOTE1/$vp
						[[ ! -s $f ]] && $c -c $RP >$f
						ls -l $f;;
				-d) set -x
						rm -v $p 2>/dev/null
						set +x;;
				-r) echo "${ANSI_INVERSE}Test preload $p$ANSI_RESET"
						rm $p 2>/dev/null
						test_checksum 1 8DC4565D $MNT/$VP
						print_src $p testing
						test_checksum 1 8DC4565D $p
						print_src $p /${MODI##*/}/;;
		esac
}

go(){
		local opt=$1
		shift
		for c in "${!CC[@]}"; do
				go1 $opt $c
		done
}

main(){
		[[ ! -s $RP ]] && mkdir -p ${RP%/*} && seq 1000 >$RP
		go -m
		read -r -p "Remove $MODI/ZIPsFS/n/? [Y/n] "
		[[ ${REPLY,,} != *n*  ]] && go -d
		go -r
}


main "$@"
