#ifndef HAMMING_H_
#define HAMMING_H_

#define GET_BIT(b, i) ((b >> i) & 1)
#define SET_BIT(n, i, v) (v ? (n | (1 << i)) : (n & ~(1 << i)))
#define TOGGLE_BIT(b, i) (b ^ (1 << i))

typedef unsigned char byte_t;
typedef unsigned short encoded_t;
typedef unsigned short decoded_t;

/*
Unpack memory function should decode src pointed data from hamming 15,11 (With error correction).
P.S. Before usage, allocate dst memory with size, same as count of elements in src.

Params:
- src - Source encoded data.
- dst - Destination for decoded data.
- l - Element count.

Return pointer to dst.
*/
void* unpack_memory(const encoded_t* src, byte_t* dst, int l);

/*
Pack memory function should encode src pointed data to hamming 15,11.
P.S. Before usage, allocate dst memory with size, same as count of elements in src.

Params:
- src - Source encoded data.
- dst - Destination for decoded data.
- l - Element count.

Return pointer to dst.
*/
void* pack_memory(const byte_t* src, decoded_t* dst, int l);

#endif