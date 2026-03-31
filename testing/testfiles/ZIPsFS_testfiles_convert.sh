#!/usr/bin/env bash
set -u

# cargo install csv2parquet

src=${BASH_SOURCE[0]}
source ${src%/*}/ZIPsFS_testfiles_inc.sh

EE='png jpg gif'

file_empty_or_not_exist(){
		[[ -s $1 ]] && return 1
		[[ -f $1 ]] && rm -v "$1"
		return 0
}

mk_test_img(){
		local ext base=/test_fileconvert/banner
		local base_rp=$REMOTE1$base
		mkdir -p ${base_rp%/*}
		for ext in $EE; do
				local f=$base_rp.$ext
				file_empty_or_not_exist $f && convert  -pointsize 64 "label:$ext file" $f
				ls -l $f >&2
		done
		echo $base
}


mk_parquet(){
		local vp=/test_fileconvert/small.parquet
		local rp=$REMOTE1$vp
		mkdir -p ${rp%/*}
		file_empty_or_not_exist $rp &&  echo -e 'a,b,c\n1,2,3\n2,3,4'  >$rp.orig  &&  set -x && csv2parquet $rp.orig $rp && set +x
		echo ${vp}
}



mk_pdf(){
		local vp_base=test_fileconvert/my_latex
		local rp_base=$REMOTE1/$vp_base
		echo "rp_base:  $rp_base.pdf" >&2
		if file_empty_or_not_exist "$rp_base.pdf"; then
				echo "Going to create $rp_base.pdf" >&2
				tr @ '\n' <<<   '\documentclass{article}@\begin{document}@Hello, world!@\end{document}@' >$rp_base.tex
				! pushd ${rp_base%/*} >&2 && prompt_error
				set -x
				pdflatex $rp_base.tex >&2
				set +x
				popd >&2
				ls -l $rp_base.pdf >&2
		else
				echo "Already exists $rp_base.pdf" >&2
		fi
		echo $vp_base.pdf
}


main(){


		ask_remove_converted
		if false; then
				read -r -p "Remove $REMOTE1/test_fileconvert? [y/N] "
				if [[ ${REPLY,,} == *y*  ]]; then
						rm -v -r "$REMOTE1/test_fileconvert";
						sleep 1
				fi
		fi
		mkdir -p {$MNT,$REMOTE1}/test_fileconvert
		ls -l -t  {$MNT,$REMOTE1}/test_fileconvert



		echo "${ANSI_INVERSE}pdf 2 txt$ANSI_RESET"
		local f=$MNT/zipsfs/c/$(mk_pdf)
		! ls -l  $f && prompt_error


		echo "Going to type $f.txt ..." >&2
		! head -v $f.txt && prompt_error



		! head -v $f.txt && prompt_error
		! ls -l  $f && prompt_error

		echo "${ANSI_INVERSE}Scale images$ANSI_RESET"
		local ext base=$(mk_test_img)
		for ext in $EE; do
				local f=$MNT/zipsfs/c$base.scale25%.$ext
				set -x; rm $f  2>/dev/null; set +x
				! picterm $f && prompt_error
				! picterm $f && prompt_error
		done

		echo "${ANSI_INVERSE}ocr$ANSI_RESET"
		local f=$MNT/zipsfs/c$base.png.ocr.eng.txt

		set -x; rm $f  2>/dev/null; set +x
		! head -v $f && prompt_error
		! head -v $f && prompt_error


		echo "${ANSI_INVERSE}parquet$ANSI_RESET"
		local vp=$(mk_parquet)
		local f=$MNT/zipsfs/c$vp.tsv
		set -x; rm $f 2>/dev/null; set +x
		! ls -l $f && prompt_error
		! head -v $f && prompt_error
		! head -v $f && prompt_error




}





main "$@"
# echo "${ANSI_INVERSE}Second Pass ${ANSI_RESET}"
# main "$@"
