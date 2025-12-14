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
if false; then
    readonly MNTavfs=~/mnt/avfs_for_Z
    readonly DBavfs=~/.ZIPsFS/DBavfs
    mkdir $MNTavfs $DBavfs
    mk_symlink_avfs(){
        local t="$MNTavfs/#ftp:$1" l=$DBavfs/$2
        set -x
        rm "$l" 2>/dev/null
        ! ln -sfn $t $l  && exit 111
        set +x
    }
    ! mountpoint $MNTavfs && echo "Please  tmux rename-window ${MNTavfs##*/};     avfsd  -o large_read -f $MNTavfs" >&2 && exit 1
fi
# ========================================


readonly DBcurlftpfs=~/.ZIPsFS/DBcurlftpfs
mkdir -p $DBcurlftpfs


mount_curlftpfs(){
    local url=$1 mnt=$DBcurlftpfs/$2
    mkdir -p $mnt

    ## Data loading is slow. ZIPsFS will use curl instead. Therefore it needs to know the URL.
    echo ftp://$url > $mnt.URL
    mountpoint $mnt && return
    curlftpfs  $url  $mnt
#    read -p "Enter"
}

go(){
    mount_curlftpfs  "$@"
    false &&         mk_symlink_avfs "$@"
}


go ftp.pride.ebi.ac.uk/pride/data/archive          pride
go massive-ftp.ucsd.edu                            massive
go ftp.ensembl.org/pub                             ensembl
go ftp.ncbi.nlm.nih.gov                            ncbi
go ftp.ebi.ac.uk/pub/databases/Pfam                pfam
go ftp.ebi.ac.uk/pub/databases/Rfam                Pfam
for d in AllTheBacteria GO alphafold pdb Pfam Rfam uniprot; do
    go ftp.ebi.ac.uk/pub/databases/$d  $d
done
}
