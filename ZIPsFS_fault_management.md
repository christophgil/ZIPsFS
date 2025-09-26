# Fault Management for Remote File Access

Accessing remote files inherently carries a higher risk of failure. Requests may either:

 - Fail immediately with an error code, or

 - Block indefinitely, causing potential hangs.

In many FUSE file systems, a blocking access can render the entire virtual file system unresponsive.
ZIPsFS addresses this with built-in fault management for remote branches.

Remote roots in ZIPsFS are specified using a double-slash prefix, similar to UNC paths (//server/share/...).
Each remote branch is isolated in terms of fault handling and threading and has its own thread pool, ensuring faults in one do not affect others.
To avoid blocking the main file system thread, remote file operations are executed asynchronously in dedicated worker threads.

## Timeouts

ZIPsFS remains responsive even if a remote file access hangs.
The fuse thread delegates the file operation to another thread and waits for its completion.
After the configurable timeout it gives up.

## Duplicated file paths

For redundantly stored files (i.e., available on multiple branches), another branch may take over
transparently if one fails or becomes unresponsive.


## Blocked worker threads
The worker thread may block permanently. In this case it can be killed automatically and restarted. However killing this thread sometimes does not work.

If the stalled thread cannot be terminated, ZIPsFS will not create a new thread.
To check whether all threads are responding, activate logging. For details see

    ~/.ZIPsFS/.../log_flags.conf.readme

This is best resolved by restarting ZIPsFS without interrupting ongoing file accesses.









## Debug Options

### The ZIPsFS option  **-T**

Checks whether ZIPsFS can generate and print a backtrace in case of errors or crashes.  This feature
elies on external tools to translate memory addresses into source code locations: On Linux and
FreeBSD, it uses addr2line, typically located in /usr/bin/.  On macOS, it uses the atos tool
instead.  Ensure these tools are installed and accessible in your system's PATH for backtraces to
work correctly.

See ZIPsFS.compile.sh for activation of sanitizers.
