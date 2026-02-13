# Testing

For testing purposes, here is a set of tests. </br>

**How to run?** </br>
```
python3 test/nifat32_tests.py 
```

# Launching
Benchamrking (Avg. time for creating, reading, writing, deleting, renaming and copying):
```
python3 test/nifat32_tests.py --new-image --formatter <path> --tests-folder <path> --root-folder <path> --clean --test-type <type> --injector-scenario <path> --test-size <size>
```
- `--new-image` - Will create new nifat32 image with formatter tool.
- `--formatter` - Path to formatter folder
- `--tests-folder` - Path to folder with test_*.c files.
- `--root-folder` - Project root folder.
- `--clean` - Clean binaries after execution.
- `--test-type` - Test type.
- `--test-size` - Test size argument for every test.
- `--bs-count` - BootSectors count.

# Examples
Bitflip testing
```
python3 test/old/nifat32_tests.py --new-image --formatter formatter --tests-folder test/old --root-folder . --clean --test-type bitflip --injector-scenario test/injector_scenario.txt
```

Default testing
```
python3 test/old/nifat32_tests.py --new-image --formatter formatter --tests-folder test/old --root-folder . --clean --test-type default
```
