#ifndef MM_H_
#define MM_H_

#include <stddef.h>
#include "threading.h"
#include "logging.h"
#include "str.h"

#define ALLOC_BUFFER_SIZE   65536
#define ALIGNMENT           8  
#define MM_BLOCK_MAGIC      0xC07DEL
#define NO_OFFSET           0

#define GET_BIT(b, i) ((b >> i) & 1)
#define SET_BIT(n, i, v) (v ? (n | (1 << i)) : (n & ~(1 << i)))
#define TOGGLE_BIT(b, i) (b ^ (1 << i))

typedef struct mm_block {
    unsigned int magic;
    size_t size;
    unsigned char free;
    struct mm_block* next;
} mm_block_t;


/*
Init first memory block in memory manager.

Return -1 if something goes wrong.
Return 1 if success init.
*/
int mm_init();

/*
Allocate memory block.
[Thread-safe]

Params:
    - size - Memory block size.

Return NULL if can't allocate memory.
Return pointer to allocated memory.
*/
void* malloc_s(size_t size);

/*
Allocate memory block with offset.
[Thread-safe]

Params:
    - size - Memory block size.
    - offset - Minimum offset for memory block.

Return NULL if can't allocate memory.
Return pointer to allocated memory.
*/
void* malloc_off_s(size_t size, size_t offset);

/*
Realloc pointer to new location with new size.
Realloc took from https://github.com/j1sk1ss/CordellOS.PETPRJ/blob/Userland/src/kernel/memory/allocator.c#L138
[Thread-safe]

Params:
    - ptr - Pointer to old place.
    - elem - Size of new allocated area.

Return NULL if can't allocate data.
Return pointer to new allocated area.
*/
void* realloc_s(void* ptr, size_t elem);

/*
Free allocated memory.
[Thread-safe]

Params:
    - ptr - Pointer to allocated data.

Return -1 if something goes wrong.
Return 1 if free success.
*/
int free_s(void* ptr);

#endif