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

# Data collection
For instance, that's how to evaluate how the boot time depends on boot sector index:
```bash
python3 test/nifat32_tests.py \ 
  --new-image \                             # create a new image for each run
  --formatter formatter/ \                  # image creation tool path
  --tests-folder test/ \                    # where test scripts are placed
  --root-folder . \                         # where is the nifat32 root folder
  --clean \                                 # clean binaries after each run
  --test-specific test_nifat32_boot \       # we run the specific boot test
  --bs-count 32 \                           # we build the image with 32 boot sectors
  --collect-param b-bs-count \              # we iterate the b-bs-count parameter (bad boot sectors)
  --collect-from 0 \                        # we collect output from the initial state
  --collect-to 32 \                         # up to max value
  --collect-step 1 \                        # with the step equals to 1
  --collect-output broken_boot_results.txt  # and save it in broken_boot_results.txt
```

or here's how to evaluate read/write time to the FAT depends on FAT count:
```bash
python3 test/nifat32_tests.py \                 
  --new-image \                                 # create a new image for each run
  --formatter formatter/ \                      # image creation tool path
  --tests-folder test/ \                        # where test scripts are placed
  --root-folder . \                             # where is the nifat32 root folder
  --clean \                                     # clean binaries after each run
  --test-specific test_nifat32_cluster_time \   # we run the specific cluster time test
  --collect-param fat-count \                   # we iterate the fat-count parameter
  --collect-from 1 \                            # we collect output from the initial state
  --collect-to 16 \                             # up to max value
  --collect-step 1 \                            # with the step equals to 1
  --collect-output fat_count_results.txt        # and save it in fat_count_results.txt
```

## Auto data collection
To collect several data results, use the bash script in the directory:
```bash
chmod +x collector.sh
# For FAT evalueation
./collector.sh fat 30 --from 1 --to 16 --step 1
# For bootsector evaluation
./collector.sh bs 30 --from 1 --to 32 --step 1 --broken-bs-fixed 0
```
