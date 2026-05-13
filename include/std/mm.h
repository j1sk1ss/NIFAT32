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
    This file contains main tools for working with FS memory manager.

Dependencies:
    - stddef.h - For NULL.
    - logging.h - Logging tools.
    - threading.h - Locks for linked list allocation.
*/

#ifndef MM_H_
#define MM_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/str.h>
#include <std/null.h>
#include <std/logging.h>
#include <std/threading.h>

#if defined(NO_DEFAULT_MM_MANAGER) && !defined(NON_DEFAULT_MM_MANAGER)
#define NON_DEFAULT_MM_MANAGER
#endif

#ifndef ALLOC_BUFFER_SIZE
#define ALLOC_BUFFER_SIZE   512000
#endif
#define ALIGNMENT           8  
#define MM_BLOCK_MAGIC      0xC07DEL
#define NO_OFFSET           0

typedef struct mm_block {
    unsigned int     magic;
    unsigned int     size;
    unsigned char    free;
    struct mm_block* next;
} mm_block_t;

typedef struct {
    int   (*init)();
    void* (*malloc)(unsigned long);
    void  (*free)(void*);
} mm_manager_t;

#define DEFAULT_MM_MANAGER NULL, NULL, NULL
/*
Setup basic memory manager. If you provide 'DEFAULT_MM_MANAGER' value, will
use default functions. Otherwise don't forget to build FS with the 'NO_DEFAULT_MM_MANAGER' flag.
This flag will exclude this manager from the final executable.
Params:
    - `init` - Any initial routine. Can be 'NULL'.
    - `malloc` - Malloc function that should return an address for a data,
    - `free` - Free function.

Returns 1 if init was succeess.
*/
int nft32_setup_mm_manager(int (*init)(), void* (*malloc)(unsigned long), void (*free)(void*));

/*
Init first memory block in memory manager.

Return -1 if something goes wrong.
Return 1 if success init.
*/
int nft32_mm_init();

/*
Allocate memory block.
[Thread-safe]

Params:
    - size - Memory block size.

Return NULL if can't allocate memory.
Return pointer to allocated memory.
*/
void* nft32_malloc_s(unsigned long size);

/*
Free allocated memory.
[Thread-safe]

Params:
    - ptr - Pointer to allocated data.

Return -1 if something goes wrong.
Return 1 if free success.
*/
void nft32_free_s(void* ptr);

#ifdef __cplusplus
}
#endif
#endif
