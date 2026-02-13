#include <std/checksum.h>

static inline unsigned int _rotl32(unsigned int x, char r) {
    return (x << r) | (x >> (32 - r));
}

static inline unsigned int _fmix32(unsigned int h) {
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

checksum_t murmur3_x86_32(const unsigned char* key, unsigned int len, unsigned int seed) {
    const unsigned int c1 = 0xcc9e2d51;
    const unsigned int c2 = 0x1b873593;

    const int nblocks = len / 4;
    unsigned int h1 = seed;

    const unsigned int* blocks = (const unsigned int *)(key);
    for (int i = 0; i < nblocks; i++) {
        unsigned int k1 = blocks[i];

        k1 *= c1;
        k1 = _rotl32(k1, 15);
        k1 *= c2;

        h1 ^= k1;
        h1 = _rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    const unsigned char* tail = (const unsigned char*)(key + nblocks * 4);
    unsigned int k1 = 0;
    switch (len & 3) {
        case 3: k1 ^= tail[2] << 16; __attribute__((fallthrough));
        case 2: k1 ^= tail[1] << 8;  __attribute__((fallthrough));
        case 1: k1 ^= tail[0];
                k1 *= c1;
                k1 = _rotl32(k1, 15);
                k1 *= c2;
                h1 ^= k1;
    }

    h1 ^= len;
    h1 = _fmix32(h1);
    return h1;
}