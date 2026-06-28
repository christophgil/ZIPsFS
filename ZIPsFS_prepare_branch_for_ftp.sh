#!/usr/bin/env bash
set -u

export ANSI_FG_GREEN=$'\e[32m' ANSI_FG_RED=$'\e[31m' ANSI_FG_MAGENTA=$'\e[35m'  ANSI_FG_BLUE=$'\e[34;1m'  ANSI_FG_YELLOW=$'\e[33m' ANSI_FG_WHITE=$'\e[37m'
export ANSI_INVERSE=$'\e[7m' ANSI_BOLD=$'\e[1m' ANSI_UNDERLINE=$'\e[4m' ANSI_RESET=$'\e[0m'

readonly METHOD_RCLONE=1
readonly METHOD_CURLFTP=2
########################################################
# This Script mounts common bioinformatics resources   #
#                                                      #
# OPTIONS                                              #
#   -u Umount                                          #
#   -t Test mounted file systems                       #
#   -n Dryrun                                          #
#                                                      #
# CONFIGURATION                                        #
#   You may specify whether to use rclone or curlftpfs #
########################################################
# readonly METHOD=$METHOD_RCLONE
if [[ -z ${METHOD:-} ]]; then
    version=$(curlftpfs -V 2>&1)
    if rclone --version >&2; then
        METHOD=$METHOD_RCLONE
    elif [[ ${version,,} == fuse ]]; then
        METHOD=$METHOD_CURLFTP
    else
        echo "${BASH_SOURCE[0]}: ${ANSI_FG_RED}Neither rclone nor curlftpfs is installed. Aborting ... $ANSI_RESET" >&2
        exit 1
    fi
fi
#####################
# END CONFIGURATION #
#####################







readonly PARENT=~/.ZIPsFS/db


:<<EOF
This script prepares a branch $PARENT.
Give  this in ZIPsFS  at the command line:

     ZIPsFS  <root-path-1>   --preload $HOME/.ZIPsFS/PARENT

EOF


:<<EOF
Not using AVFS
==============
Note: AVFS copies FTP files much faster than Curlftpfs.
Unfortunately, constantly loosing some branches like #ftp:ftp.ebi.ac.uk/pub/databases/ gets lost after a while.
Giving up on AVFS for now because the connection
Compensating slower file transfer of Curlftpfs by using wget or curl within ZIPsFS for copying.
EOF
# ========================================



go(){
    local db=$2
    local url=$1 root=$PARENT/$db
    local mnt=$root
    local exclude=${3:-}

    if ((DO_UMOUNT)); then
        set -x
        mountpoint $mnt && $DO_DRYRUN fusermount -u $mnt >&2
        set +x
        return
    fi
    if ((DO_TEST)); then
        mountpoint $mnt && ls -l $mnt | head -n 4
        echo
    fi

    if ((DO_UMOUNT_SUDO)); then
        set -x
        $DO_DRYRUN sudo umount -f $mnt  >&2 || $DO_DRYRUN sudo umount -l $mnt >&2
        set +x
        return
    fi
    mkdir -p $mnt
    # Leading double slash - makes it a "remote" branch.
    # Leading triple slash - would activate timeout. However, timeout slows down long dir listings.
    # We can rather set timeout in Curlftpfs in ftpfs.c, main()-function:  curl_easy_setopt(easy,CURLOPT_TIMEOUT,1000);
    CLIPARAS+=" /$root "
    {
        cat  <<EOF
# statfs() is used to check periodically whether the remote path probe_path is not frozen.
#   1.  probe_path
#   2.  probe_path-timeout
# probe_path=$root
probe_path_response_ttl=4
probe_path_timeout=44
preload=1
path_prefix=/db
decompression=gz,bz2
EOF
        [[ -n $exclude ]] && echo "path_deny=$exclude"
    } >$root.ZIPsFS.properties
    # We use /usr/bin/curl for copying. This is twice as fast than rclone and much faster than curlftpfs. This also allows us to give  timeout.
    echo ftp://$url >$mnt.URL
    echo "Going to test mountpoint $mnt ..." >&2
    mountpoint $mnt >/dev/null && echo "$ANSI_FG_GREEN YES$ANSI_RESET" >&2  && return
    echo "$ANSI_FG_RED Not yet$ANSI_RESET" >&2
    set -x
    local host=${url%%/*}    path=''
    [[ $url == */* ]] && path=/${url#*/}
    case $METHOD in
        $METHOD_CURLFTP)   $DO_DRYRUN curlftpfs  $url  $mnt -o cache=yes,cache_timeout=999,noforget,allow_other >&2;;
        $METHOD_RCLONE)
            #url=ftp.pride.ebi.ac.uk/pride/data/archive
            #--vfs-cache-mode off --vfs-read-chunk-size-limit 1G --buffer-size 64M --vfs-cache-max-size 50G
            local more_opt=''
            [[ $db == 'massive' ]] && more_opt='--ftp-explicit-tls  --no-check-certificate --ftp-no-check-certificate'
            local performance='--vfs-cache-max-age 24h --vfs-read-chunk-size 64M '
            $DO_DRYRUN rclone mount :ftp,host=$host,user=anonymous,pass=iWUjcuIO8HMn21fi0vaYzouElo1s0gKX_Q,protocol=ftp:$path  $mnt --vfs-cache-mode full --config ''  $more_opt --daemon
            ;;
    esac
    set +x
}



mkdir -p $PARENT
CLIPARAS=''

DO_UMOUNT=0
DO_UMOUNT_SUDO=0
DO_DRYRUN=''
DO_TEST=0
while getopts 'nuUt' o; do
    case $o in
        n) DO_DRYRUN=':';;
        t) DO_TEST=1;;
        u) DO_UMOUNT=1;;
        U) DO_UMOUNT_SUDO=1;;
        *) echo "wrong option $o" >&2; exit 1 ;;
    esac
done
shift $((OPTIND-1))

echo DO_DRYRUN=$DO_DRYRUN >&2
if false; then
    go ftp.ensembl.org/pub                             ensembl
    go ftp.ncbi.nlm.nih.gov                            ncbi
    go ftp.expasy.org                                 uniprot
fi
go massive-ftp.ucsd.edu                            massive
go ftp.pride.ebi.ac.uk/pride/data/archive          pride
go ftp.uniprot.org                                 uniprot

# ## The folder /pub/ebi/databases/pdb/data/structures/all contains all PDB entries and reading this huge directory needs to be avoided.
go ftp.ebi.ac.uk/pub                               ebi  '::/db/ebi/databases/pdb/data/structures/all:/Note/that/you/can/exclude/several/paths:/colon/separates/paths::'
go ftp.pdbj.org/pub                                pdbj '::/db/pdbj/pdb/data/structures/all'

if ((DO_UMOUNT)); then
    pids=$(pgrep -u $USER -f "\\brclone\\b.*$PARENT/";  pgrep -u $USER -f "\\bcurlftpfs\\b.*$PARENT/")
    [[ -n $pids ]] && echo Remaining PIDs && ps u -p $pids && echo sudo umount $pids >&2
fi
echo 'Add the following CLI parameter to the commandline of ZIPsFS:' >&2
echo $CLIPARAS
# lftp -e "set ftp:ssl-allow false" massive-ftp.ucsd.edu
