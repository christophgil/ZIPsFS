# Install ZIPsFS for MacOSX


Download and install MacPorts https://www.macports.org/
Download and install macFUSE https://osxfuse.github.io/

Open terminal and run:

    sudo port install libzip bindfs wget unzip tmux lynx

Load the kernel. Give the correct OSX version, here 15.

    sudo kextload /Library/Filesystems/macfuse.fs/Contents/Extensions/15/macfuse.kext && echo Success

An error box might pop up:

  The system extension required for mounting macFUSE volumes could not be loaded.  Please open the
  Security & Privacy System Preferences pane, go to the General preferences and allow loading system
  software from developer "Benjamin Fleischer". A restart might be required before the system
  extension can be loaded.
  Then try mounting the volume again.

Maybe you need to restart and try again.


To check whether fuse is working try bindfs it  is a simple FUSE file system.

    mkdir -p ~/mnt/test_bindfs
    bindfs -f ~ ~/mnt/test_bindfs

The -f option means that bindfs runs in foreground.
In another terminal check whether you see the content of the home directory at the mount point.

     ls ~/mnt/test_bindfs


## MacOSX - Install ZIPsFS

Finnally run

     src/ZIPsFS.compile.sh
