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
    - entry.h - For directry_entry_t structure.
    - threading.h - Locks for table write.
    - fat.h - Cluster typedef.
*/

#ifndef CTABLE_H_
#define CTABLE_H_

#include "mm.h"
#include "fat.h"
#include "null.h"
#include "entry.h"
#include "threading.h"

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
    union {
        directory_t directory;
        file_t      file;
    };
    
    cluster_addr_t parent_cluster;
    cluster_addr_t data_cluster;
    directory_entry_t meta;
    content_type_t content_type;
} content_t;

#define NOT_PRESENT 0x00
#define STAT_FILE   0x01
#define STAT_DIR    0x02

typedef struct {
    char full_name[12];
    char file_name[8];
    char file_extension[4];
    int  type;
    int  size;
} cinfo_t;

int ctable_init();
ci_t alloc_ci();
int setup_content(ci_t ci, int is_dir, const char* name83, cluster_addr_t root, cluster_addr_t data, directory_entry_t* meta);
cluster_addr_t get_content_data_ca(const ci_t ci);
unsigned int get_content_size(const ci_t ci);
cluster_addr_t get_content_root_ca(const ci_t ci);
const char* get_content_name(const ci_t ci);
int stat_content(const ci_t ci, cinfo_t* info);
int destroy_content(ci_t ci);

#endif