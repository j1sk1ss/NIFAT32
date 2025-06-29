#ifndef RIPEMD160_H_
#define RIPEMD160_H_

#include "str.h"

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define F(x, y, z) ((x) ^ (y) ^ (z))
#define G(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define H(x, y, z) (((x) | ~(y)) ^ (z))
#define I(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define J(x, y, z) ((x) ^ ((y) | ~(z)))

#define R(a, b, c, d, e, func, k, m, s)        \
    a += func(b, c, d) + (m) + (k);            \
    a = ROTL(a, s) + e;                        \
    c = ROTL(c, 10);

typedef unsigned int ripemd160_t[5];

int ripemd160_cmp(ripemd160_t f, ripemd160_t s);
int ripemd160(const unsigned char* msg, unsigned int length, ripemd160_t hash);

#endif