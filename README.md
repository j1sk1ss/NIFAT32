# NiFAT32
The NiFAT32 project is FAT32 file system with a modification which improves overall performance under SEU (Single Event Upset) and MEU (Multiple Event Upsets). This project can be used as a library, as an independent executable and as a system for an IoT device. 

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
cinfo_t info = { .type = STAT_DIR, .full_name = "TFILE   TXT", .size = 1, .type = STAT_FILE };
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
if (NIFAT32_write_buffer2content(new_directory, 0, buffer, sizeof(buffer)) == 12) {
}
```

### Copy a content entry
The NiFAT32 file system can create a shallow copy of a content or a deep copy. The shallow copy addresses the problem of backup meta information in terms of SEU presents, that's why I strongly suggest to use it for your files if your system will encounter SEU. To perform this you will need to invoke the `NIFAT32_copy_content` which accepts target and source content indexes (You will need to create a dummy placeholder for a copy) and copy type (`DEEP_COPY` or `SHALLOW_COPY`).
```c
// ci_t new_directory = ... (now this is tfile.txt)
ci_t copy = NIFAT32_open_content(NO_RCI, "TDIR       ", MODE(CR_MODE | R_MODE | W_MODE, DIR_TARGET));
if (copy >= 0) {
    NIFAT32_copy_content(new_directory, copy, SHALLOW_COPY);
    NIFAT32_close_content(copy);
}
```

### Delete a content entry
To delete a content entry (and erase all data, which means delete data in files and delete files and directories recursively in directories) you will need to use the `NIFAT32_delete_content` function. This function accepts only one parameter - content index. 
```c
// ci_t new_directory = ... (now this is tfile.txt)
NIFAT32_delete_content(copy);
NIFAT32_delete_content(new_directory);
```