#ifndef RIPEMD160_H_
#define RIPEMD160_H_
#ifdef __cplusplus
extern "C" {
#endif

#include "str.h"

#define ROTL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

#define F(x, y, z) ((x) ^ (y) ^ (z))
#define G(x, y, z) (((x) & (y)) | (~(x) & (z)))
#define H(x, y, z) (((x) | ~(y)) ^ (z))
#define I(x, y, z) (((x) & (z)) | ((y) & ~(z)))
#define J(x, y, z) ((x) ^ ((y) | ~(z)))

#define R_LEFT(a, b, c, d, e, func, k, m, s) do { \
    unsigned int temp = ROTL((a) + func((b), (c), (d)) + (m) + (k), (s)) + (e); \
    (a) = (e); \
    (e) = (d); \
    (d) = ROTL((c), 10); \
    (c) = (b); \
    (b) = temp; \
} while(0)

#define R_RIGHT(ap, bp, cp, dp, ep, func, k, m, s) do { \
    unsigned int temp = ROTL((ap) + func((bp), (cp), (dp)) + (m) + (k), (s)) + (ep); \
    (ap) = (ep); \
    (ep) = (dp); \
    (dp) = ROTL((cp), 10); \
    (cp) = (bp); \
    (bp) = temp; \
} while(0)

typedef unsigned int ripemd160_t[5];

int ripemd160_cmp(ripemd160_t f, ripemd160_t s);
int ripemd160(const unsigned char* msg, unsigned int length, ripemd160_t hash);

#ifdef __cplusplus
}
#endif
#endif