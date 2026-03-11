#!/usr/bin/env bash
get_copied(){
		if [[ $# != 0 ]]; then
				echo "Using $# shell parameters" >&2
				for a in "$@"; do
						[[ $a != /* ]] && echo -n $PWD/
						echo "$a"
				done
		elif ((IS_MACOSX)); then
				# New macs?
				osascript <<'EOF'
try
		set rawData to the clipboard
		if class of rawData is alias then
				set fileList to {rawData}
		else if class of rawData is list then
				set fileList to rawData
		else
				return
		end if

		repeat with f in fileList
				POSIX path of f
		end repeat
end try
EOF

				### Works 10.15
				pbpaste -Prefer NSFilenamesPboardType |plutil -p - |grep / |sed -e 's|[^"]*"||1' -e 's|".*||1'
		else
				local opt='-selection clipboard'
				local count=$(xclip $opt -o |wc -l)
				((count)) || opt=''
				set -x
				xclip $opt -o
				set +x
		fi
		echo
}




print_instructions(){

		how_select_files

		[[ $# == 0 ]] && read -r -p 'Press Enter to continue'
		echo
}

print_instructions; get_copied "$@"     #FILTER_OUT
