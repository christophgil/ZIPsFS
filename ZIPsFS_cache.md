## File content cache

ZIPsFS optionally supports caching specific files and ZIP entries entirely in RAM, allowing data segments to
be served from memory in any order.
This feature significantly improves performance for software that performs random-access reads for remote files and for
ZIP entries.

The ***-l*** option sets an upper limit on memory usage for the ZIP RAM cache.
When available memory runs low, ZIPsFS can either pause,  proceed without caching file data or just ignore the
memory restriction depending on the configuration.
These caching behaviors - such as which files to cache and how to handle memory pressure - are defined in the configuration.


## File attribute cache

Additional caching mechanisms are designed to accelerate file listing in large directories for ZIP entries.





## Data Integrity for ZIP Entries

For ZIP entries loaded entirely into RAM:
ZIPsFS performs CRC checksum validation.
Any detected inconsistencies are logged, helping to detect corruption or transmission errors.
