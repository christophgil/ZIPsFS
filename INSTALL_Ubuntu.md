# Install ZIPsFS on Debian or Ubuntu

    apt-get update
    apt-get install fuse-zip   libfuse3-dev  libzip-dev  unzip lynx tmux
    addr2line -H || apt-get install binutils

    apt-get install gcc

or

    apt-get install clang

#  Compile ZIPsFS

To compile ZIPsFS, run

     ./ZIPsFS.compile.sh


# Trouble shooting Detect problems of fuse


If ZIPsFS does not work you need to exclude general problem of the  FUSE system.
This can be done by testing another FUSE file system like **sshfs** or **fuse-zip**.
The following shows how  fuse-zip can be tested. First it needs to be installed. On Debian or Ubuntu type

    sudo apt get install fuse-zip


In the following, an empty folder is created which serves as mount point. Then a ZIP file is made and mounted with fuse-zip.
Finally, the files at the mount point are shown.


    mkdir -p ~/test/fuse-zip/mnt
    zip --fifo  ~/test/fuse-zip/test.zip  <(date)
    fuse-zip  ~/test/fuse-zip/test.zip ~/test/fuse-zip/mnt
    ls -R  ~/test/fuse-zip/mnt






If this fails, and there is a permission problem, try as root.
