## ZIPsFS Configuration

Configuration files, identified
by the prefix **ZIPsFS_configuration**, are written in C. Any changes require recompilation and a
restart of ZIPsFS to take effect.

With the -s option, the updated ZIPsFS can seamlessly replace running instances without disrupting the virtual file system.

To illustrate how this works, let MNT represent the apparent mount point of the FUSE file system.
Suppose we are in the parent directory of MNT, enabling the use of relative paths.
Users access files through this apparent mount point, but in reality, MNT is a symbolic link to the actual mount point.
The real mount point is not directly accessed by users, as it changes each time a new instance of ZIPsFS is launched.

For example, assume the obsolete ZIPsFS instance is mounted at ./.mountpoints/MNT/1.
When a new instance replaces it, it may use any empty directory as   mount point. ZIPsFS must be started with the following command line  option:

    -s MNT

Once the new instance is running, the symbolic link is updated to point to the new mount
location. From the user's perspective, nothing changes - the apparent mount point remains MNT. To
ensure uninterrupted access, the obsolete instance should remain active for a short period to allow
ongoing file operations to complete.

If MNT  is within an exported  SAMBA or NFS path the real mount points should be in the exported file tree as well.
Include into */etc/samba/smb.conf*:

    follow symlinks = yes
