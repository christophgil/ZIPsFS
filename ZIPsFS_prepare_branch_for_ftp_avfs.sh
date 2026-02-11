#!/usr/bin/env bash
set -u
readonly MNTavfs=~/mnt/avfs_for_Z
readonly DBavfs=~/.ZIPsFS/DBavfs

echo 'U N D E R    C O N S T R U C T I O N  ' >&2

cat<<EOF
This script prepares a branch $DBavfs
Add  @preload $DBavfs to the  ZIPsFS   command line:
Example:

   ZIPsFS  root1 root2    //$DBavfs  @preload @preload-gz

EOF


:<<EOF
Curlftpfs vs AVFS
=================
Note: AVFS copies FTP files much faster than Curlftpfs. However, this is compensated by using curl for copying.

Unfortunately, $DBavfs/ebi  and ~/mnt/avfs_for_Z/#ftp:ftp.ebi.ac.uk  eventually becomes inaccessible.

Note that symbolic links are generally not expanded in ZIPsFS.  However, those with targets
containing the string pattern /#ftp: are expanded.
This is pattern is recognized by a customizable function which enables symlink expansion for specific paths.

EOF

mkdir $MNTavfs $DBavfs
go(){
    local t="$MNTavfs/#ftp:$1" l=$DBavfs/$2
    set -x
    rm "$l" 2>/dev/null
    ! ln -sfn $t $l  && exit 111
    set +x
}
! mountpoint $MNTavfs && echo "Please run:    tmux rename-window ${MNTavfs##*/};     avfsd  -o large_read -f $MNTavfs" >&2 && exit 1


go ftp.pride.ebi.ac.uk/pride/data/archive          pride
go massive-ftp.ucsd.edu                            massive
go ftp.ensembl.org/pub                             ensembl
go ftp.ncbi.nlm.nih.gov                            ncbi
go ftp.ebi.ac.uk/pub                               ebi
