/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    Checksum type and Murmur3 x86 32-bit checksum function.

Dependencies:
    - None.
*/

#ifndef CHECKSUM_H_
#define CHECKSUM_H_
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int checksum_t;
checksum_t nft32_murmur3_x86_32(const unsigned char* key, unsigned int len, unsigned int seed);

#ifdef __cplusplus
}
#endif
#endif
