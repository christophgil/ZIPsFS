#! /usr/bin/env bash
source $(dirname -- $0)/include_copied_paths.sh #FILTER_OUT

print_instructions
get_copied "$@"| awk '!seen[$0]++' |while read -r path; do
		[[ -z $path ]] && continue
		echo "$ANSI_INVERSE ====== $path ======$ANSI_RESET"
		head   $path | strings
		echo
done
