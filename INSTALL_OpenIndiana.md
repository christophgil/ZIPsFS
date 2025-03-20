# Install ZIPsFS on OpenIndiana

OpenIndiana is an Illumos distribution and a descendant of OpenSolaris


pkg update
pkg install tmux fuse libzip pkg:/metapackages/build-essential
pkg install tmux fuse libzip build-essential



## Install ZIPsFS


## TroubleShooting

In my case ZIPsFS worked only as root.
I changed the permissions of /dev/fuse without success.





<!-- OpenSolaris -->
<!-- * https://artemis.sh/2022/03/07/pkgsrc-openindiana-illumos.html -->
<!-- * https://www.openindiana.org/packages/ -->
<!-- pkg publisher -->
<!-- To add a publisher to your system (requires privileges): -->
<!-- pkg set-publisher -g http://path/to/repo_uri publisher -->


/var/pkg/cache
pkg set-property flush-content-cache-on-success True
https://github.com/jurikm/illumos-fusefs/raw/master/lib/libfuse-20100615.tgz
Finnally run

     src/ZIPsFS.compile.sh
