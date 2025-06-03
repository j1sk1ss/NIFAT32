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

Params:
    - size - Memory block size.

Return NULL if can't allocate memory.
Return pointer to allocated memory.
*/
void* malloc_s(size_t size);

/*
Allocate memory block with offset.

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

Params:
    - ptr - Pointer to old place.
    - elem - Size of new allocated area.

Return NULL if can't allocate data.
Return pointer to new allocated area.
*/
void* realloc_s(void* ptr, size_t elem);

/*
Free allocated memory.

Params:
    - ptr - Pointer to allocated data.

Return -1 if something goes wrong.
Return 1 if free success.
*/
int free_s(void* ptr);

/*
Hamming 15,11 code encoding.

Params:
    - data - Data to encode. Can be unsigned char or unsigned short with 11 data bits.

Return encoded unsigned short
*/
unsigned short encode_hamming_15_11(unsigned short data);

/*
Hamming 15,11 code decoding.

Params:
    - encoded - Encoded data from encode_hamming_15_11.

Return decoded char or 11 data bits unsigned short.
*/
unsigned short decode_hamming_15_11(unsigned short encoded);

/*
Unpack memory function should decode src pointed data from hamming 15,11 (With error correction).
P.S. Before usage, allocate dst memory with size, same as count of elements in src.

Params:
- src - Source encoded data.
- dst - Destination for decoded data.
- len - Bytes count.

Return pointer to dst.
*/
void* unpack_memory(unsigned short* src, unsigned char* dst, size_t len);

/*
Pack memory function should encode src pointed data to hamming 15,11.
P.S. Before usage, allocate dst memory with size, same as count of elements in src.

Params:
- src - Source encoded data.
- dst - Destination for decoded data.
- len - Bytes count.

Return pointer to dst.
*/
void* pack_memory(unsigned char* src, unsigned short* dst, size_t len);

#endif