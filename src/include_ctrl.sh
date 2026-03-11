#! /usr/bin/env bash
set -u
source $(dirname -- $0)/include_ctrl_common.sh                              #FILTER_OUT
TT=('PTHREAD_NIL' 'PTHREAD_ASYNC' 'PTHREAD_PRELOAD' 'PTHREAD_MISC')  #FILTER_OUT


askWhichThread(){
		local t=0 name
		for name in $TT; do
				((!i)) && continue
				echo "  $i $name" >&2
				((i++))
		done
		t=0
		read -r -p 'What thread?' -n 1 t
		t=%{t%% }
		t=%{t## }
		[[ $t != [0-9] ]] && t=0
		echo  $t
}

A(){
echo "   $1  $2"
}
echo 'Terminate'
A $ACT_KILL_ZIPSFS  'Kill-ZIPsFS and print status'
echo 'Blocked threads'
A $ACT_FORCE_UNBLOCK  'Unblock thread even if blocked thread cannot be killed - not recommended.'
A $ACT_CANCEL_THREAD  'Interrupt-thread.  ZIPsFS will restart the thread eventually'
echo 'Pthread - Locks'
A $ACT_NO_LOCK  'Trigger error due to missing lock.'
A $ACT_BAD_LOCK  'Trigger error due to inappropriate lock.'
thread=0
read -r -n 1 -p 'Choice? ' c
echo
if [[ $c == [1-9] ]]; then
		[[ $c == 3 ]] && thread=$(askWhichThread)
		my_stat $c $thread
fi
