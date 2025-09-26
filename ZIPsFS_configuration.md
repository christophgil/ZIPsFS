## ZIPsFS Configuration

ZIPsFS can be customized:

 - optional features can be (de)-activated with  preprocessor macros "WITH_SOME_FEATURE"  which take the values 0 or 1.
 - Rules can be given
     - which files are cached in the main memory
     - which ZIP entries are inlined
 - Timeout values for accessing (remote) files
 - Automatic file conversions

Configuration files of ZIPsFS, are files written in the programming language C.
They have the prefix **ZIPsFS_configuration** and the suffix **.h** of **.c**.


ZIPsFS is customized for our needs - accessing archived high throughput data such that it can be
directly used for mass spectrometry software. These settings can be used as a sample to customize it
for other needs.


Changes require recompilation and take effect after restart of ZIPsFS.

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
