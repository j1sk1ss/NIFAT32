/*
License:
    MIT License

    Copyright (c) 2025 Nikolay 

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Description:
    This file contains main tools for working with content table and ci.

Dependencies:
    - mm.h - FS memory manager.
    - null.h - NULL.
    - threading.h - Locks for table write.
    - fat.h - Cluster typedef.
    - entry.h - For directry_entry_t structure.
    - ecache.h - Ecache is saved in a content entry.
    - fatinfo.h - FAT information.
*/

#ifndef CTABLE_H_
#define CTABLE_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/mm.h>
#include <std/null.h>
#include <std/threading.h>
#include <src/fat.h>
#include <src/entry.h>
#include <src/ecache.h>
#include <src/fatinfo.h>

#define CONTENT_TABLE_SIZE 50

/* Content Index - ci */
typedef int ci_t;

typedef struct fat_file {
    char name[9];
    char extension[4];
} file_t;

typedef struct fat_directory {
    char name[12];
} directory_t;

typedef enum {
    CONTENT_TYPE_UNKNOWN,
    CONTENT_TYPE_FILE,
    CONTENT_TYPE_DIRECTORY,
    CONTENT_TYPE_EMPTY
} content_type_t;

typedef struct {
    ecache_t* root;
} content_index_t;

typedef struct {
    union {
        directory_t   directory;
        file_t        file;
    };
    
    content_index_t   index;          /* If this is a directory - Index data  */
    cluster_addr_t    parent_cluster; /* Claster where is the entry is placed */
    cluster_addr_t    data_cluster;   /* Head data claster of the entry       */
    directory_entry_t meta;           /* The entry                            */
    content_type_t    content_type;
    unsigned char     mode;           /* Open mode                            */
} content_t;

#define NOT_PRESENT 0x00 /* The entry hasn't allocated */
#define STAT_FILE   0x01 /* The entry is a file        */
#define STAT_DIR    0x02 /* The entry is a directory   */
typedef struct {
    char         full_name[12];
    char         file_name[8];
    char         file_extension[4];
    unsigned int size;
    int          type;
} cinfo_t;

/*
Load the basic information about the provided content.
Params:
    - `ci` - Content index.
    - `info` - Output information location.

Returns 1 if succeeds, otherwise will return 0.
*/
int stat_content(const ci_t ci, cinfo_t* info);

/*
Set the table entry information.
Params:
    - `ci` - Content index to setup.
    - `is_dir` - Is this entry a directory?
    - `name83` - 8.3 format filename.
    - `meta` - Entry meta information.
    - `mode` - Open mode.

Returns 1 if succeeds. Otherwise will return 0.
*/
int setup_content(
    ci_t ci, int is_dir, const char* __restrict name83, directory_entry_t* __restrict meta, unsigned char mode
);

/*
Set the table to zero.
Returns 1 if table is ready, otherwise will return 0.
*/
int ctable_init();

/*
Allocate a new one content idex.
Returns content id (>= 0) if succeeds. If the table is full,
or the table isn't allocated (yet?), will return -1.
*/
ci_t alloc_ci();

/*
Deallocate content by the provided content index.
Params:
    - `ci` - Content index.

Returns 1 if the content has deallocated. Otherwise will return 0.
*/
int destroy_content(ci_t ci);

/*
Get the data head field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the data cluster or 'FAT_CLUSTER_BAD' if something went wrong.
*/
cluster_addr_t get_content_data_ca(const ci_t ci);

/*
Set the data head field for an entry by the provided content index.
Params:
    - `ci` - Content index.
    - `ca` - Cluster address.

Returns 1 if succeeds, otherwise will return 0.
*/
int set_content_data_ca(const ci_t ci, cluster_addr_t ca);

/*
Get the data size field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the size of data or 0 if something went wrong.
*/
unsigned int get_content_size(const ci_t ci);

/*
Get the root field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the root cluster or 'FAT_CLUSTER_BAD' if something went wrong.
*/
cluster_addr_t get_content_root_ca(const ci_t ci);

/*
Get the content's name field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the name or NULL if something went wrong.
*/
const char* get_content_name(const ci_t ci);

/*
Get the mode field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the mode or 0 if something went wrong.
*/
unsigned char get_content_mode(const ci_t ci);

/*
Get the type field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the type or 'CONTENT_TYPE_UNKNOWN' if something went wrong.
*/
content_type_t get_content_type(const ci_t ci);

/*
Get the content's index tree field from an entry by the provided content index.
Params:
    - `ci` - Content index.

Returns the tree or NULL if something went wrong.
*/
ecache_t* get_content_ecache(const ci_t ci);

/*
Create index information.
Note: If this isn't a directory, will return 0.
Params:
    - `ci` - Directory content index.
    - `fi` - FAT information.

Returns 1 if a tree was created. Otherwise will return 0.
*/
int index_content(const ci_t ci, fat_data_t* fi);

#ifdef __cplusplus
}
#endif
#endif