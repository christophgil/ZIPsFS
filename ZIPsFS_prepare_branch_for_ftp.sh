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


readonly DBcurlftpfs=~/.ZIPsFS/DB
mkdir -p $DBcurlftpfs
CLIPARAS=''

go(){
    local db=$2
    local url=$1 root=$DBcurlftpfs/$db
    local mnt=$root
    local exclude=${3:-}

    if ((DO_UMOUNT)) && mountpoint $mnt >&2; then
        set -x
        fusermount -u $mnt
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
# statfs() is used to check periodically whether remote paths are not frozen.
# The statfs property takes two numbers and optionally a path:
#   1.  How often check response with statvfs
#   2.  Max waiting time until we say that folder is not responding
#   3.  Optional the path to check. If not provided,  then the root-path is taken
statfs=10 300 $root
# The next line improves performance.
path-prefix=/DB
#
preload
preload-gz
EOF
        [[ -n $exclude ]] && echo "path-deny-patterns=$exclude" >&2
    } >$root.ZIPsFS.properties
    # We use /usr/bin/curl for copying. Here we also give a timeout.
    echo ftp://$url >$mnt.URL
    mountpoint $mnt >&2 && return
    curlftpfs  $url  $mnt -o cache=yes,cache_timeout=999,noforget
}


DO_UMOUNT=0
[[ ${1:-} == -u ]] && DO_UMOUNT=1

go massive-ftp.ucsd.edu                            massive
# read -p aaaaaaaaaaa
go ftp.pride.ebi.ac.uk/pride/data/archive          pride
go ftp.ensembl.org/pub                             ensembl
go ftp.ncbi.nlm.nih.gov                            ncbi
# The folder /pub/ebi/databases/pdb/data/structures/all contains all PDB entries and reading this huge directory needs to be avoided.
go ftp.ebi.ac.uk/pub                               ebi  ::/DB/ebi/databases/pdb/data/structures/all:/Note/that/you/can/exclude/several/paths:/colon/separates/paths::
go ftp.pdbj.org/pub                                pdbj ::/DB/pdbj/pdb/data/structures/all
#go localhost                                       localhost


echo 'Add the following CLI parameter to the commandline of ZIPsFS:' >&2
echo $CLIPARAS
