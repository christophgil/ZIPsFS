#!/usr/bin/env bash
source $(dirname -- $0)/include_ctrl_common.sh                                                       #FILTER_OUT
menu(){ echo '  0=CLEAR_ALL_CACHES  1=CLEAR_DIRCACHE  2=CLEAR_ZIPINLINE_CACHE  3=CLEAR_STATCACHE';} #FILTER_OUT
ACT_CLEAR_CACHE=6                                                                                   #FILTER_OUT


menu

cache='';
[[ $# == 0 ]] && read -r -p 'What cache?' -n 1 cache
for c in "${@}" $cache; do
		[[ $c == [0-9] ]] && my_stat 6 $ACT_CLEAR_CACHE;
done
