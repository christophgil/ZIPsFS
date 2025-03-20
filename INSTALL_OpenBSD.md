# Install ZIPsFS on OpenBSD

    pkg_add fuse-zip libzip

[//]: # pkg_add  rsync lynx
[//]: # pkg_info -Q    ## Search

Then run

    ZIPsFS.compile.sh

It was not possible to run ZIPsFS and other fuse filesystems  as non-root.
If you know how this can be done please tell me.
