## Logs

ZIPsFS typically runs as a foreground process.  To keep it active and monitor its output, it is
recommended to use a persistent terminal multiplexer such as tmux. This enables continuous
observation of all messages and facilitates long-running sessions.
Additional log files are stored in:

    ~/.ZIPsFS

For each mount point there are files specifying more  logs.

    log_flags.conf

See readme for details:

    log_flags.conf.readme


ZIPsFS dynamically generates an HTML status file within the virtual file system.
You can find it under the path: <Mount-Point>/ZIPsFS/
For example:

    ~/test/ZIPsFS/mnt/ZIPsFS/file_system_info.html

This file provides real-time information about the system’s current state.
