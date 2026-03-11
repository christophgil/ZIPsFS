#!/usr/bin/env bash
MNT=/var/Users/cgille/tmp/ZIPsFS/mnt #FILTER_OUT
CTRLSFX='CTRL_SFX'                   #FILTER_OUT
my_stat(){
		echo
		set -x
		stat --format %s /var/Users/cgille/tmp/ZIPsFS/mnt/${1}_${2:-0}_$CTRL_SFX
		set +x
}
