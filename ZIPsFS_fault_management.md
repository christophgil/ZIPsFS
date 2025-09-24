## Fault Management for Remote File Access

Accessing remote files inherently carries a higher risk of failure. Requests may either:

 - Fail immediately with an error code, or

 - Block indefinitely, causing potential hangs.

In many FUSE file systems, a blocking access can render the entire virtual file system unresponsive.
ZIPsFS addresses this with built-in fault management for remote branches.

Remote roots in ZIPsFS are specified using a double-slash prefix, similar to UNC paths (//server/share/...).
Each remote branch is isolated in terms of fault handling and threading and has its own thread pool, ensuring faults in one do not affect others.
To avoid blocking the main file system thread, remote file operations are executed asynchronously in dedicated worker threads.

ZIPsFS remains responsive even if a remote file access hangs.  For redundantly stored files (i.e.,
available on multiple branches), another branch may take over transparently if one fails or becomes
unresponsive.

If a thread becomes unresponsive, ZIPsFS will try to terminate the stalled thread after a timeout.
As soon as the old thread does not exist any more, a new thread is started, attempting to restore
functionality to the affected branch.

If the stalled thread cannot be terminated, ZIPsFS will not create a new thread.
To check whether all threads are responding, activate logging. For details see

    ~/.ZIPsFS/.../log_flags.conf.readme

This is best resolved by restarting ZIPsFS without interrupting ongoing file accesses.

Another possibility is to start a new thread irrespectively of the still existing blocked thread.
There is a shell script ***ZIPsFS_CTRL.sh*** for this in ***~/.ZIPsFS/***.  When the blocked thread
which had been scheduled for termination wakes up, it will be terminated by the system.  However,
there is no guaranty that termination will be perforemd immediately.  For a short time, the two
threads may be active concurrently with  undefined behaviour and the risk of segmentation faults.



## Debug Options

### The ZIPsFS  **-T**

Checks whether ZIPsFS can generate and print a backtrace in case of errors or crashes.  This feature
elies on external tools to translate memory addresses into source code locations: On Linux and
FreeBSD, it uses addr2line, typically located in /usr/bin/.  On macOS, it uses the atos tool
instead.  Ensure these tools are installed and accessible in your system's PATH for backtraces to
work correctly.

See ZIPsFS.compile.sh for activation of sanitizers.
