# Testing
For testing purposes, here is a set of tests. </br>

**How to run?** </br>
```
chmod +x run_tests.sh index_test.sh benchmark.sh
```

# Benchmark
Benchamrking (Avg. time for creating, reading, writing, deleting, renaming and copying):
```
./benchmark.sh --new-image <count>
```
- --new-image - Will create new nifat32 image with formatter tool.
- --del-bin - Will clean directory after test.
- Files limit. Test will create this file amount.

# Index test
Tests how indexing affects performance (average time to open and read files):
```
 ./file_test.sh --new-image 100
```
- --new-image - Will create new nifat32 image with formatter tool.
- --del-bin - Will clean directory after test.
- Files limit. Test will create this file amount.

# Main testing
Runs general tests to ensure correct behavior of the file system:
```
./run_tests.sh --new-image
```
- --new-image - Will create new nifat32 image with formatter tool.
- --ndel-bin - Will save build data after test.
- --check - Special type of execution after bitflip injection.
- Files limit. Test will create this file amount.
