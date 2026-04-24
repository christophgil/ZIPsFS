#!/usr/bin/env bash
set -u
src=${BASH_SOURCE[0]}
inc=ZIPsFS_testfiles_inc.sh
source ${src%/*}/$inc
#STORE=//s-mcpb-ms03.charite.de/fulnas1/1/
STORE=''

FTP_DIRS=''
((WITH_FTP_DIRS)) && FTP_DIRS=$(~/git_projects/ZIPsFS/ZIPsFS_prepare_branch_for_ftp.sh)


[[ -n ${TMUX:-} ]] && tmux set-environment LANG en_US.UTF-8
[[ -n ${TMUX:-} ]] && tmux set-environment LC_ALL=en_US.UTF-8

run_ZIPsFS(){
        rm ~/.ZIPsFS/_home_cgille_tmp_ZIPsFS_mnt/cachedir/*.cache
        mountpoint $MNT && umount $MNT
        mountpoint $MNT && umount -l $MNT
        mountpoint $MNT 2>&1 |grep 'connected' &&  sudo umount $MNT
        mountpoint $MNT && return

        [[ -n $STORE ]] && ! ls -d $STORE && read -t 3 -r -p "Directory $STORE not found " && STORE=''
        set -x
        fuseopt='allow_other,attr_timeout=13,entry_timeout=13,negative_timeout=13,ac_attr_timeout=13'
        ~/git_projects/ZIPsFS/ZIPsFS  "$@" $VERBOSE -k -l 33GB  -c rule  "$MODI/" /$REMOTE1/  $FTP_DIRS $STORE : -o $fuseopt    $MNT
        set +x
        # ~/git_projects/ZIPsFS/ZIPsFS  "$@" -k -l 33GB  -c rule  "" /$REMOTE1/  $FTP_DIRS $STORE : -o 'allow_other,attr_timeout=13,entry_timeout=13,negative_timeout=13,ac_attr_timeout=13'    $MNT
}
run_ZIPsFS
