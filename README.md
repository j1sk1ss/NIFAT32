# NiFAT32
The NiFAT32 project is FAT32 file system with a modification which improves overall performance under SEU (Single Event Upset) and MEU (Multiple Event Upsets). This project can be used as a library, as an independent executable and as a system for an IoT device. 

## Navigation
- [Easy start (Library)](#easy-start-library)
  - [Initialization](#initialization)
  - [Open a content entry](#open-a-content-entry)
  - [Create a new content entry](#create-a-new-content-entry)
  - [Edit a content entry](#edit-a-content-entry)
  - [Write a content entry](#write-a-content-entry)
  - [Read a content entry](#read-a-content-entry)
  - [Get content information](#get-content-information)
  - [Truncate a content entry](#truncate-a-content-entry)
  - [Index a directory](#index-a-directory)
  - [Copy a content entry](#copy-a-content-entry)
  - [Delete a content entry](#delete-a-content-entry)
  - [Repair operations](#repair-operations)
  - [File system information and errors](#file-system-information-and-errors)
  - [Closing file system](#closing-file-system)
- [Helpers](#helpers)
  - [FAT names](#fat-names)
- [Porting to IoT platforms](#porting-to-iot-platforms)
  - [What should be copied](#what-should-be-copied)
  - [What should be provided by platform](#what-should-be-provided-by-platform)
  - [Memory manager](#memory-manager)
  - [Image creation](#image-creation)
  - [Recommended compile flags](#recommended-compile-flags)
- [Build](#build)
  - [Library](#library)
  - [Formatter](#formatter)
  - [Unix executable](#unix-executable)
- [Testing](#testing)
- [Features](#features)

## Easy start (Library)
### Initialization
To load a NiFAT32 instance you will need to invoke the `NIFAT32_init()` function with boot parameters:
```c
nifat32_params_t params;
if (!NIFAT32_init(&params)) {
    return EXIT_FAILURE;
}
```

Parameters are the next:
| Parameter | Full name | Possible values |
|-|-|-|
| fat_cache | Status for the FAT cache system | <b>NO_CACHE</b> (There is no FAT cache), </br> <b>CACHE</b> (There is a classic 'lazy' (on load) cache), </br> <b>CACHE + HARD_CACHE</b> (There is a cache which will load entire table at the start) |
| bs_num | Boot sectors number (Service field, do not change) | 0 | 
| bs_count | Boot sectors count | >= 1 |
| ts | Total sectors count in the image (You can get this value by dividing the total size of the image in bytes with the sector size in bytes) | >= 1 |
| jc | Journal sectors count | >= 0 |
| ec | Error storage sectors count | >= 0 |
| disk_io | Disk IO function pointers | - |
| logg_io | Logging IO function pointers | - |

### Open a content entry
NiFAT32 names directories and files as content entries. To open an existed content you will need to invoke the `NIFAT32_open_content` function with the `root_ci` parameter (Can be `NO_RCI`) which represents the content index where we're searching the content (If it receives the `NO_RCI` value, it will search in the entire image). Also you will need to get a name of the target content (or a path) and set a mode. There are several modes in the system:

| Parameter | Description |
|-|-|
| DF_MODE | Will open a content in Read + Write mode |
| R_MODE | Will open a content only in Read mode |
| W_MODE | Will open a content only in Write mode |
| CR_MODE | Will tell to the system that we need to create all content entries that don't exist. <br> This mode is used with NO_TARGET, FILE_TARGET and DIR_TARGET options. This will say which type is the last part in the path ('cause we can't say there is a directory or a file at the path's tale). |

Let's open the `hello.txt` that is present in the `dir` directory. To accomplish this, we can either use the full path, or open the directory first. The second approach is usefull if a directory is placed deep in the file system, and we use it really often.
```c
ci_t ci = NIFAT32_open_content(NO_RCI, "DIR/HELLO   TXT", MODE(R_MODE, NO_TARGET)); // The last name in the path *must* be in the 8.3 format! Use in-built helpers to achive this.
if (ci >= 0) {
    // The target file (Read-only) is opened with the `ci` index.
}
```

### Create a new content entry
To create a new content entry, you will need to use the `NIFAT32_put_content`. This function accepts the `root_ci` (Root content index, something like the file descriptor), content information (see the table below), and reserce cluster count. </br>
Content information is a structure that is presented below:
| Parameter | Full name | Possible values |
|-|-|-|
| full_name | All 11 characters (Name + Extention) | A string from 0 bytes length to 11 bytes length. |
| name | First 8 bytes from the name (Optional for creation) | A string from 0 bytes length to 8 bytes length. |
| extention | Last 3 bytes from the name (Optional for creation) | A string from 0 bytes length to 3 bytes length. |
| size | Content size (For directories always 0) | >= 0 |
| type | Content entry type | <b>STAT_FILE</b> - content is a file </br> <b>STAT_DIR</b> - content is a directory |

The `root_ci` value sets the destination where the new content will be stored. If there is no directories in the file system instance, this value can be obtained from the opn function with `NO_RCI`. For instance let's make a new directory in an empty file system instance:
```c
ci_t rci = NIFAT32_open_content(NO_RCI, NULL, MODE(W_MODE, NO_TARGET));
if (rci >= 0) {
    cinfo_t info = { .type = STAT_DIR, .full_name = "TDIR       " };
    NIFAT32_put_content(rci, &info, NO_RESERVE);
    NIFAT32_close_content(rci);
}
```

There is a less complex way of how to create a file or a directory. The `NIFAT32_open_content` as it mentioned above can create all non-existed files/directories with the `CR_MODE` flag. In a nutshell the code above can be re-written:
```c
ci_t new_directory = NIFAT32_open_content(NO_RCI, "TDIR       ", MODE(CR_MODE | R_MODE | W_MODE, DIR_TARGET));
if (new_directory >= 0) {
    // The directory is created and opened with the `new_directory` index.
}
```

P.S.: *One flaw in the second approach is that we can't set the reserved count of clusters. The `NIFAT32_put_content` function can pre-allocate a chain for data which can help with de-fragmentation in future.*

### Edit a content entry
To perform this operation you will need to invoke the `NIFAT32_change_meta` function which accepts the `ci` of the target content and a new information (cinfo_t). Let's rename the directory above and change its type to file.
```c
// ci_t new_directory = ...
cinfo_t info = { .full_name = "TFILE   TXT", .size = 1, .type = STAT_FILE };
NIFAT32_change_meta(new_directory, &info);
```

### Write a content entry
To write a data to a content entry, you need to use the `NIFAT32_write_buffer2content` function which accepts the content index of a content entry, offset, the data and size of the data.
```c
// ci_t new_directory = ... (now this is tfile.txt)
if (NIFAT32_write_buffer2content(new_directory, 0, "Hello world!", 12) == 12) {
}
```

### Read a content entry
To read a data from a content entry, you need to use the `NIFAT32_read_content2buffer` function which accepts the content index of a content entry, offset, the output buffer and size of the output buffer.
```c
char buffer[64];
// ci_t new_directory = ... (now this is tfile.txt)
if (NIFAT32_read_content2buffer(new_directory, 0, buffer, sizeof(buffer)) == 12) {
}
```

### Get content information
To get a content meta information you will need to use the `NIFAT32_stat_content` function. It accepts the content index and fills the `cinfo_t` structure.
```c
// ci_t new_directory = ... (now this is tfile.txt)
cinfo_t stat;
if (NIFAT32_stat_content(new_directory, &stat)) {
    // stat.full_name contains 8.3 name
    // stat.size contains a content size
    // stat.type can be STAT_FILE or STAT_DIR
}
```

### Truncate a content entry
The `NIFAT32_truncate_content` function changes the occupied size of a file and saves data in the result clusters. The content entry should be opened in Write mode.
```c
// ci_t new_directory = ... (now this is tfile.txt)
if (NIFAT32_truncate_content(new_directory, 0, 6)) {
    // The file was truncated to 6 bytes.
}
```

### Index a directory
NiFAT32 can index a directory to improve search speed. This is usefull for big directories where the same directory is used many times. To perform this operation, use the `NIFAT32_index_content` function.
```c
ci_t dir = NIFAT32_open_content(NO_RCI, "TDIR       ", DF_MODE);
if (dir >= 0) {
    NIFAT32_index_content(dir);
    NIFAT32_close_content(dir);
}
```

### Copy a content entry
The NiFAT32 file system can create a shallow copy of a content or a deep copy. The shallow copy addresses the problem of backup meta information in terms of SEU presents, that's why I strongly suggest to use it for your files if your system will encounter SEU. To perform this you will need to invoke the `NIFAT32_copy_content` which accepts target and source content indexes (You will need to create a dummy placeholder for a copy) and copy type (`DEEP_COPY` or `SHALLOW_COPY`).
```c
// ci_t new_directory = ... (now this is tfile.txt)
ci_t copy = NIFAT32_open_content(NO_RCI, "TDIR       ", MODE(CR_MODE | R_MODE | W_MODE, DIR_TARGET));
if (copy >= 0) {
    NIFAT32_copy_content(new_directory, copy, SHALLOW_COPY);
}
```

### Delete a content entry
To delete a content entry (and erase all data, which means delete data in files and delete files and directories recursively in directories) you will need to use the `NIFAT32_delete_content` function. This function accepts only one parameter - content index. 
```c
// ci_t new_directory = ... (now this is tfile.txt)
NIFAT32_delete_content(copy);
NIFAT32_delete_content(new_directory);
```

### Repair operations
NiFAT32 stores important structures with noise-immune encoding and checksum validation. For manual restoration there are two public functions:

| Function | Description |
|-|-|
| NIFAT32_repair_bootsectors | Rebuilds and rewrites all bootsector copies from information which is already loaded in RAM. |
| NIFAT32_repair_content | Reads, corrects and writes directory entries inside a directory content. Can work recursively. |

Example:
```c
ci_t root = NIFAT32_open_content(NO_RCI, NULL, DF_MODE);
if (root >= 0) {
    NIFAT32_repair_content(root, 1);
    NIFAT32_close_content(root);
}

NIFAT32_repair_bootsectors();
```

### File system information and errors
To get loaded file system information, use the `NIFAT32_get_fs_data` function. The output structure contains sector size, cluster size, FAT count, FAT size, root cluster, journals count and other base values.
```c
fat_data_t fs;
if (NIFAT32_get_fs_data(&fs)) {
    // fs.cluster_size is bytes_per_sector * sectors_per_cluster
}
```

The last registered error can be requested with `NIFAT32_get_last_error`. If there is no registered error, the function will return `-1`.
```c
error_code_t code = NIFAT32_get_last_error();
if (code >= 0) {
    // Handle or print the error code.
}
```

### Closing file system
When you don't need the current NiFAT32 instance anymore, invoke `NIFAT32_unload`. This function unloads FAT cache and destroys the content table.
```c
NIFAT32_unload();
```

## Helpers
### FAT names
NiFAT32 works with FAT 8.3 names internally. If you have a regular name or a path, you can convert it with helpers from `std/fatname.h`.

| Function | Example |
|-|-|
| nft32_name_to_fatname | `hello.txt` -> `HELLO   TXT` |
| nft32_fatname_to_name | `HELLO   TXT` -> `HELLO.TXT` |
| nft32_path_to_fatnames | `root/dir/file.txt` -> `ROOT/DIR/FILE    TXT` |
| nft32_path_to_83 | Converts a path to 8.3 in the same memory. |
| nft32_extract_name | Extracts the last name from a path. |
| unpack_83_name | Splits `NAME    EXT` to name and extention. |

Example:
```c
char fat_path[128] = { 0 };
nft32_path_to_fatnames("root/dir/hello.txt", fat_path);

ci_t ci = NIFAT32_open_content(NO_RCI, fat_path, DF_MODE);
```

## Porting to IoT platforms
NiFAT32 doesn't use POSIX or Unix calls inside the core library directly. The platform-dependent part is passed through `nifat32_params_t`, so for a microcontroller or another IoT platform you need to copy the core files and provide several small functions for storage, logging and memory.

### What should be copied
For a minimal port you need the next files and folders:

| Path | Description |
|-|-|
| nifat32.c | Main public API implementation. |
| nifat32.h | Main public API header. |
| src/ | Core FAT, cluster, entry, journal, cache and error-storage logic. |
| std/ | Small standard helpers used by the file system. |
| include/ | Public and internal headers for `src/` and `std/`. |

The next files are optional for the target platform:

| Path | Description |
|-|-|
| formatter/ | Host-side image creation tool. Usually built on PC before flashing/copying an image to device storage. |
| unix_nifat32.c | Unix example shell above the API. It is useful as a reference, but shouldn't be copied to MCU firmware. |
| test/ | Tests and benchmark tools. |
| graphs/ | Documentation images and collected results. |

### What should be provided by platform
The platform should provide disk IO functions. These functions work with logical sectors. The `sa` parameter is a sector address, `offset` is an offset inside this sector, and `buff_size` / `data_size` is a count of bytes.

```c
static int my_read_sector(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    // Read buff_size bytes from storage address: sa * SECTOR_SIZE + offset.
    // Return 1 if read was success, otherwise return 0.
}

static int my_write_sector(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    // Write data_size bytes to storage address: sa * SECTOR_SIZE + offset.
    // Return 1 if write was success, otherwise return 0.
}
```

Logging is optional. If you don't need logs, you can pass `NULL` callbacks and disable log flags during build.

```c
static int my_fprintf(const char* fmt, ...) {
    // Print to UART/SWO/RTT or ignore.
}

static int my_vfprintf(const char* fmt, va_list args) {
    // Print to UART/SWO/RTT or ignore.
}
```

After that, fill `nifat32_params_t` with platform values:

```c
#define SECTOR_SIZE 512
#define IMAGE_SIZE_BYTES (64 * 1024 * 1024)

nifat32_params_t params = {
    .bs_num    = 0,
    .bs_count  = 5,
    .ts        = IMAGE_SIZE_BYTES / SECTOR_SIZE,
    .jc        = 2,
    .ec        = 0,
    .fat_cache = CACHE,
    .disk_io   = {
        .read_sector  = my_read_sector,
        .write_sector = my_write_sector,
        .sector_size  = SECTOR_SIZE
    },
    .logg_io   = {
        .fd_fprintf  = my_fprintf,
        .fd_vfprintf = my_vfprintf
    },
    .mm_manager = { DEFAULT_MM_MANAGER }
};

if (!NIFAT32_init(&params)) {
    // Mount error.
}
```

P.S.: *The `bs_count`, `ts`, `jc`, sector size and FAT count should match the image that was created by the formatter.*

### Memory manager
By default NiFAT32 uses a small static memory manager from `std/mm.c`. Its buffer size can be changed with `ALLOC_BUFFER_SIZE`.

```bash
make ALLOC_BUFFER_SIZE=262144
```

If your platform already has a heap, RTOS allocator or a region allocator, you can provide your own memory manager and build without the default one:

```c
static int my_mm_init() {
    return 1;
}

static void* my_malloc(unsigned int size) {
    // Return a memory block or NULL.
}

static int my_free(void* ptr) {
    // Free memory and return 1 if success.
}

nifat32_params_t params = {
    // ...
    .mm_manager = {
        .init   = my_mm_init,
        .malloc = my_malloc,
        .free   = my_free
    }
};
```

Build flag:
```bash
make NO_DEFAULT_MM_MANAGER=1
```

### Image creation
The common way for IoT usage is to create an image on the host machine and then put it to the target storage (SD card, SPI flash, QSPI flash, eMMC and so on).

```bash
cd formatter
make
./formatter -o ../nifat32.img --volume-size 64 --spc 8 --fc 4 --bsbc 5 --jc 2
```

If you need initial files in the image, pass a source folder:
```bash
./formatter -o ../nifat32.img -s ../data --volume-size 64 --spc 8 --fc 4 --bsbc 5 --jc 2
```

After image creation write the image to the device storage with your usual flashing tool or storage writer. In firmware use the same geometry values in `nifat32_params_t`.

### Recommended compile flags
For read-only devices, firmware update partitions or images that shouldn't be changed from the target, use:

```bash
make NIFAT32_RO=1
```

For small systems I suggest disabling debug and IO logs:
```bash
make DEBUG_LOGS=0 IO_LOGS=0 LOGGING_LOGS=0
```

For platforms with their own allocator:
```bash
make NO_DEFAULT_MM_MANAGER=1
```

For platforms with no allocator and a fixed memory budget:
```bash
make ALLOC_BUFFER_SIZE=131072
```

## Build
### Library
To build the shared library, run:
```bash
make
```

The output file will be placed at:
```bash
builds/nifat32.so
```

By default the library is built with logs enabled. You can disable some log groups through Make variables:
```bash
make DEBUG_LOGS=0 IO_LOGS=0 MEM_LOGS=0
```

There are several compile flags for changing library behaviour:

| Make variable | C macro | Description |
|-|-|-|
| NIFAT32_RO | NIFAT32_RO | Builds the library in Read-Only mode. Write operations, content creation, copy, truncate and delete won't change the image. |
| - | NO_HEAP | Exclude from an instance any code which involves heap usage (mallocs) |
| - | NIFAT32_NO_ECACHE | Excludes from an instance all code for indexation. Operation index content won't do anything | 
| - | NO_FAT_CACHE | Excludes from an instance all code for fat caching |
| - | NO_FAT_MAP | Excludes from an instance all code for fat map |
| NO_DEFAULT_MM_MANAGER | NO_DEFAULT_MM_MANAGER | Excludes the default built-in memory manager. Use it when you provide your own `mm_manager` functions in `nifat32_params_t`. |
| ALLOC_BUFFER_SIZE | ALLOC_BUFFER_SIZE | Changes the static buffer size for the default memory manager. This value is used only when the default manager is enabled. |

Examples:
```bash
make NIFAT32_RO=1
make NO_DEFAULT_MM_MANAGER=1
make ALLOC_BUFFER_SIZE=1048576
```

You can combine these flags with logging options:
```bash
make NIFAT32_RO=1 ALLOC_BUFFER_SIZE=262144 DEBUG_LOGS=0
```

P.S.: *The source also understands `NON_DEFAULT_MM_MANAGER` as an old name for `NO_DEFAULT_MM_MANAGER`. For new builds it is better to use `NO_DEFAULT_MM_MANAGER`.*

### Formatter
The formatter creates a NiFAT32 image and can optionally copy files from a source directory to the image.
```bash
cd formatter
make
./formatter -o ../nifat32.img
```

There are several formatter options:

| Option | Description | Default value |
|-|-|-|
| -o | Output image path | Required |
| -s | Source directory which should be copied to the image | Empty image |
| --volume-size | Volume size in MB | 64 |
| --spc | Sectors per cluster | 8 |
| --fc | FAT copies count | 4 |
| --bsbc | Bootsector copies count | 5 |
| --b-bsbc | Count of broken bootsector copies for debug/testing | 0 |
| --jc | Journal sectors count | 2 |

Example:
```bash
./formatter -o ../nifat32.img -s ../data --volume-size 128 --spc 8 --fc 4 --bsbc 5 --jc 2
```

### Unix executable
The `unix_nifat32.c` file is a simple interactive executable above the library API. It can be used as a small shell for a NiFAT32 image.
```bash
gcc unix_nifat32.c nifat32.c src/*.c std/*.c -Iinclude -o unix_nifat32
./unix_nifat32 nifat32.img 64 512 5 2
```

Arguments are the next:

| Argument | Description |
|-|-|
| argv[1] | Path to NiFAT32 image |
| argv[2] | Image size in MB |
| argv[3] | Sector size |
| argv[4] | Bootsectors count |
| argv[5] | Journal sectors count |

The executable supports commands like `cd`, `ls`, `mkdir`, `mkfile`, `read`, `write`, `trunc`, `cp`, `mv`, `rm`, `frename`, `rs` and `get_le`.

## Testing
For testing purposes, there is a Python runner in the `test` folder:
```bash
python3 test/nifat32_tests.py --new-image --formatter formatter --tests-folder test --root-folder . --clean --test-type default
```

You can run bitflip tests with an injector scenario:
```bash
python3 test/nifat32_tests.py --new-image --formatter formatter --tests-folder test --root-folder . --clean --test-type bitflip --injector-scenario test/injector_scenario.txt
```

More information about tests and data collection is placed in `test/README.md`.

## Features
The project provides:

| Feature | Description |
|-|-|
| FAT32-like layout | The file system keeps the FAT-style content model, clusters and directory entries. |
| Noise-immune bootsectors | Bootsector copies are encoded and physically decompressed across the image. |
| FAT copies with voting | FAT reads can use several FAT copies and synchronize them after mismatch detection. |
| Directory entry protection | Directory entries contain checksum and hash fields. |
| Optional FAT cache | Lazy cache and hard cache modes are supported. |
| Journals | Journal sectors can be used during initialization for restoration. |
| Error storage | The file system can store and return registered error codes. |
| Platform IO abstraction | Disk and logging functions are passed through `nifat32_params_t`, so the library can be ported to Unix, embedded systems or another environment. |
| Custom memory manager | Memory operations can be provided by the platform through `mm_manager`. |

P.S.: *NiFAT32 is not a drop-in replacement for a system FAT32 boot partition. Bootsectors and FAT tables are encoded and placed with NiFAT32-specific rules, so the reader should use this library or compatible logic.*
