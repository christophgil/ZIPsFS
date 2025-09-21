MOTIVATION:
==========

   How does ZIPsFS cope with I/O errors and blocks of upstream file systems?
   Problems in the upstream file systems can be simulated with the FUSE file system passthrough_simulate_error.



COMPILATION:
============

   Edit and configure
   Run passthrough_simulate_error.compile.sh to compile.

   Two executables will be generated:

      ~/compiled/passthrough_simulate_error: FUSE file system with conditional I/O errors and blocks


      ~/compiled/passthrough_simulate_error_ctrl: Activate and deactivate I/O errors and blocks in the FUSE file system




USAGE:
======


  1. Start the fUSE file system in foreground:

         ~/compiled/passthrough_simulate_error -f <empty-folder-as-mount-point>

  2. Release all blocks

         ~/compiled/passthrough_simulate_error_ctrl -u


  3. Start ZIPsFS:

     ./ZIPsFS_start.sh


  4. List files in the virtual file system

      ~/tmp/ZIPsFS/mnt/


     Test the virtual file system with I/O errors and blocks activated.
     Activate I/O errors using ~/compiled/passthrough_simulate_error_ctrl with the Options

             -r  Single error on read()
             -o  Single error on open()
             -a  Single error on stat()

             -R  Block on read()
             -O  Block on open()
             -A  Block on stat()

    In combination with option -u, these blocks and errors are removed.





NOTE:
=====

  The file passthrough_simulate_error.c is derived from fuse-3.16.2/example/passthrough.c

  The modifications can be shown with

    grep -e '\bcg_' -e passthrough_simulate_error     passthrough_simulate_error.c
