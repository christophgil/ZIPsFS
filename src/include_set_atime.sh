#!/usr/bin/env bash

IS_TEST=0
SFX_ATIME=".magicSfxSetAccessTime"                      #FILTER_OUT
IS_TEST=1 #FILTER_OUT
function how_select_files(){ echo 'function  how_select_files';}      #FILTER_OUT
ASK_NOT_EXISTING='ASK_NOT_EXISTING'                     #FILTER_OUT
source $(dirname -- $0)/include_copied_paths.sh         #FILTER_OUT
main(){
		silent=0
		no_comput=0
		[[ ${1:-} == -i ]] && silent=1 && shift

		if ! ((silent)); then
				((IS_TEST)) || ask_how_long_ago
				local dd
				read -r -e -i 0 -p 'Days: ' dd
				dd=${dd,,}
				dd=${dd%% }
				local n=${dd//[^0-9-]/}
				[[ -z $n ]] && dd='0d' && n='0'
				[[ $n == "$dd" ]] && dd+='d'
				case $dd in
						*h) ((n*=1));;
						*d) ((n*=24));;
						*w) ((n*=24*7));;
						*m) ((n*=24*30));;
						*y) ((n*=24*365));;
				esac
				echo "dd: $dd   Hours: $n"
				if ! ((IS_TEST)); then
						ask_not_existing;
						read -r -p "$ASK_NOT_EXISTING"
						[[ ${REPLY,,} == y* ]] && no_comput=0
				fi
		fi
		print_instructions
		count=0
		while read -r f; do
				[[ -z $f ]] && continue
				echo "$ANSI_FG_BLUE Processing #$((count++)) $f ...$ANSI_RESET"
				! ((no_comput)) && { head "$f"|head -n 3|strings; cat $f.log 2>/dev/null; }
				if ! ls $f$SFX_ATIME$n; then

						now=$(date +%s)
						epoch=$((now-n*3600))
						touch=$(date --date=@$epoch +%Y%m%d%H%M.%S >/dev/null 2>&1 || date -r $epoch +%Y%m%d%H%M.%S)
						echo "now:$now epoch:$epoch touch:$touch:"
						set -x
						touch -a -t $touch "$f"
						set +x
				fi
		done  < <(get_copied "$@" | awk '!seen[$0]++')

		! ((count)) && echo $NO_FF
}
main "$@"
