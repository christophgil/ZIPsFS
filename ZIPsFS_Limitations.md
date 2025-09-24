## LIMITATIONS

### Hard Links

Hard links are not supported, though symlinks are fully functional.

### Deleting Files

Files can only be deleted if their physical location resides in the first source. Files located in
other branches are accessed in a read-only mode, and deletion of these files would require a
mechanism to remove them from the system, which is currently not implemented.

If you require this functionality, please submit a feature request.

### Reading and Writing

Simultaneous reading and writing of a file using the same file descriptor will only function
correctly for files stored in the writable source.
