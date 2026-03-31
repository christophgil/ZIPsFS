#!/usr/bin/env bash
set -u


:<<EOF
This script prepares a branch ~/.ZIPsFS/DBcurlftpfs.
Give  this in ZIPsFS  at the command line:

     ZIPsFS  <root-path-1>   --preload $HOME/.ZIPsFS/DBcurlftpfs

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
    local url=$1 root=$DBcurlftpfs/$db
    local mnt=$root
    local exclude=${3:-}

    if ((DO_UMOUNT)); then
        set -x
        $DO_DRYRUN fusermount -u $mnt
        set +x
        return
    fi
    if ((DO_UMOUNT_SUDO)); then
        set -x
        $DO_DRYRUN sudo umount -f $mnt || $DO_DRYRUN sudo umount -l $mnt
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
    # We use /usr/bin/curl for copying. Here we also give a timeout.
    echo ftp://$url >$mnt.URL
    echo Going to test mountpoint $mnt >&2
    mountpoint $mnt >&2 && return
    set -x
    curlftpfs  $url  $mnt -o cache=yes,cache_timeout=999,noforget,allow_other
    set +x
}


readonly DBcurlftpfs=~/.ZIPsFS/db
mkdir -p $DBcurlftpfs
CLIPARAS=''

DO_UMOUNT=0
DO_UMOUNT_SUDO=0
DO_DRYRUN=''
while getopts 'nuU' o; do
    case $o in
        n) DO_DRYRUN=':';;
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



echo 'Add the following CLI parameter to the commandline of ZIPsFS:' >&2
echo $CLIPARAS
