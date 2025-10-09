# Install ZIPsFS on OpenIndiana

OpenIndiana is an Illumos distribution and a descendant of OpenSolaris


pkg update
pkg install tmux fuse libzip pkg:/metapackages/build-essential
pkg install tmux fuse libzip build-essential




##  Compile ZIPsFS

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




<!-- OpenSolaris -->
<!-- * https://artemis.sh/2022/03/07/pkgsrc-openindiana-illumos.html -->
<!-- * https://www.openindiana.org/packages/ -->
<!-- pkg publisher -->
<!-- To add a publisher to your system (requires privileges): -->
<!-- pkg set-publisher -g http://path/to/repo_uri publisher -->


/var/pkg/cache
pkg set-property flush-content-cache-on-success True
https://github.com/jurikm/illumos-fusefs/raw/master/lib/libfuse-20100615.tgz
