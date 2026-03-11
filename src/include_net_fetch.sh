#!/usr/bin/env bash
set -u
SRC=${BASH_SOURCE[0]}
DIR=${SRC%/*}
while read a; do    eval "${a#$}"; done  <${SRC%/*}/include_net_fetch_common.ps1 #FILTER_OUT
DIR=/var/Users/cgille/tmp/ZIPsFS/mnt/ZIPsFS/n                                    #FILTER_OUT
cc=("$c0" "$c1" "$c2" "$c3")
main(){
		while true; do
				echo
				local url='' choice
				while [[ -z $url ]]; do
						echo
						local i=0
						for c in "${cc[@]}"; do
								echo " ($i) $c"
								((i++))
						done
						read -r -p "$XCHOICE" choice
						[[ -z $choice ]] && exit 0
						choice=${choice%% }
						choice=${choice## }
						[[ $choice == '0' ]] && { xdg-open $URL_UP;continue;}
						[[ -n ${cc[$choice]:-} ]] && choice="${cc[$choice]:-}"
						local e
						for e in .fasta .fasta.gz; do
								[[ $choice == *$e\ * ]] && url="$FTP_EXPASY${choice%$e *}$e"
						done
				done
				echo -e "\nurl=$url"
				[[ $url == *.gz &&  $url != *.tar.gz ]] && echo && read -r -p "$XGZ" && [[ ${REPLY,,} != *y* ]] &&	url=${url%.gz}
				local html=${url//:/,}.html
				html=$DIR/_UPDATE_/${html//\//,}
				if [[ -s $html ]]; then
						XHTML
						local out="$(grep -a $'\t' $html |sed 's|<[^>]\+>||g')"
						echo "$out"  |column -ts $'\t'
						echo
						local path=$DIR/$(grep -i  $'\tRelative' <<< "$out" |cut -f 1|sed 's|.*/||1')
						[[ -s $path ]] && echo $ANSI_FG_GREEN || echo $ANSI_FG_RED
						stat -c "%n   %s Bytes$ANSI_RESET" $path
				else
						echo "${ANSI_FG_RED}File $html does not exist.$ANSI_RESET"
				fi
				read -r -p "Done. $XRST"
		done
}
main
