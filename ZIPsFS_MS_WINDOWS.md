# MS-Windows


ZIPsFS can probably not be installed directly in MS-Windows.
It may can be installed in WSL.
The mountpoint can be exported as a SAMBA share.


## Files that are not listed in the parent are not accessible


In Windows files are not accessible when they are not listed in the parent folder.

A textfile can be formed By appending the suffix ***@SOURCE.TXT*** to a virtual file name which tells the real location of that file.

The physical file path, i.e., the actual storage location of a file, can be retrieved from a special
metadata file created by appending ***@SOURCE.TXT*** to the filename.

These virtual files will not be accessible in Windows.



### Microsoft-Windows Console Compatibility: External Queue Workaround

Some Windows command-line executables do not behave reliably when launched directly from compiled programs.
This issue stems from  Windows Console API which is used in long-running mass spectrometry CLI programs to implement progress reports.
Like traditional  escape sequences, the Windows Console API allows free cursor positioning.
In headless environments, i.e. ZIPsFS not started from a desktop environment,
respective  programs block unless without a  console device. A virtual  frame-buffer like ***xvfb*** can solve this issue.

Nevertheless, programs may still not be runnable using the UNIX fork() and exec() paradigm.
To work around this, ZIPsFS supports delegating such tasks to an external shell script.
When the special symbol ***PLACEHOLDER_EXTERNAL_QUEUE*** is specified instead of a direct executable path, ZIPsFS:

 - Pushes the task details to a queue.
 - Waits for the result.

The actual execution of these tasks is handled by the shell script ZIPsFS_autogen_queue.sh,
which must be started manually by the user. This script polls the queue and performs the requested conversions or operations.
Multiple instances of the script can run in parallel, allowing concurrent task handling.
