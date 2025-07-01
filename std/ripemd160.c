#include "../include/ripemd160.h"

static const unsigned int K[5]  = { 0x00000000, 0x5A827999, 0x6ED9EBA1, 0x8F1BBCDC, 0xA953FD4E };
static const unsigned int Kp[5] = { 0x50A28BE6, 0x5C4DD124, 0x6D703EF3, 0x7A6D76E9, 0x00000000 };
static const unsigned char R1[80] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 7, 4, 13, 1, 10, 6, 15, 3, 12, 0, 9, 5,
    2, 14, 11, 8, 3, 10, 14, 4, 9, 15, 8, 1, 2, 7, 0, 6, 13, 11, 5, 12, 1, 9, 11, 10, 0, 8, 12, 4,
    13, 3, 7, 15, 14, 5, 6, 2, 4, 0, 5, 9, 7, 12, 2, 10, 14, 1, 3, 8, 11, 6, 15, 13
};

static const unsigned char R2[80] = {
    5, 14, 7, 0, 9, 2, 11, 4, 13, 6, 15, 8, 1, 10, 3, 12, 6, 11, 3, 7, 0, 13, 5, 10, 14, 15, 8, 12,
    4, 9, 1, 2, 15, 5, 1, 3, 7, 14, 6, 9, 11, 8, 12, 2, 10, 0, 4, 13, 8, 6, 4, 1, 3, 11, 15, 0, 5,
    12, 2, 13, 9, 7, 10, 14, 12, 15, 10, 4, 1, 5, 8, 7, 6, 2, 13, 14, 0, 3, 9, 11
};

static const unsigned char S1[80] = {
    11, 14, 15, 12, 5, 8, 7, 9, 11, 13, 14, 15, 6, 7, 9, 8, 7, 6, 8, 13, 11, 9, 7, 15, 7, 12, 15,
    9, 11, 7, 13, 12, 11, 13, 6, 7, 14, 9, 13, 15, 14, 8, 13, 6, 5, 12, 7, 5, 11, 12, 14, 15, 14,
    15, 9, 8, 9, 14, 5, 6, 8, 6, 5, 12, 9, 15, 5, 11, 6, 8, 13, 12, 5, 12, 13, 14, 11, 8, 5, 6
};

static const unsigned char S2[80] = {
    8, 9, 9, 11, 13, 15, 15, 5, 7, 7, 8, 11, 14, 14, 12, 6, 9, 13, 15, 7, 12, 8, 9, 11, 7, 7, 12,
    7, 6, 15, 13, 11, 9, 7, 15, 11, 8, 6, 6, 14, 12, 13, 5, 14, 13, 13, 7, 5, 15, 5, 8, 11, 14, 14,
    6, 14, 6, 9, 12, 9, 12, 5, 15, 8, 8, 5, 12, 9, 12, 5, 14, 6, 8, 13, 6, 5, 15, 13, 11, 11
};

int ripemd160_cmp(ripemd160_t f, ripemd160_t s) {
    for (int i = 0; i < 5; i++) {
        if (f[i] != s[i]) return 0;
    }

    return 1;
}

int ripemd160(const unsigned char* msg, unsigned int length, ripemd160_t hash) {
    if (!msg || !length) return 0;

    ripemd160_t h = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    unsigned int new_length = ((length + 8) / 64 + 1) * 64;
    unsigned char padded[new_length];
    str_memcpy(padded, msg, length);

    padded[length] = 0x80;
    str_memset(padded + length + 1, 0, new_length - length - 9);
    unsigned long long bit_length = (unsigned long long)length * 8;
    str_memcpy(padded + new_length - 8, &bit_length, 8);

    for (unsigned int i = 0; i < new_length; i += 64) {
        unsigned int w[16];
        for (int k = 0; k < 16; k++) {
            w[k] = (unsigned int)padded[i + k*4] |
                   ((unsigned int)padded[i + k*4+1] << 8) |
                   ((unsigned int)padded[i + k*4+2] << 16) |
                   ((unsigned int)padded[i + k*4+3] << 24);
        }

        unsigned int a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
        unsigned int ap = a, bp = b, cp = c, dp = d, ep = e;

        for (int j = 0; j < 80; j++) {
            unsigned int f_index = j / 16;
            switch (f_index) {
                case 0: R_LEFT(a, b, c, d, e, F,  K[0], w[R1[j]], S1[j]); break;
                case 1: R_LEFT(a, b, c, d, e, G,  K[1], w[R1[j]], S1[j]); break;
                case 2: R_LEFT(a, b, c, d, e, H,  K[2], w[R1[j]], S1[j]); break;
                case 3: R_LEFT(a, b, c, d, e, I,  K[3], w[R1[j]], S1[j]); break;
                case 4: R_LEFT(a, b, c, d, e, J,  K[4], w[R1[j]], S1[j]); break;
            }

            switch (f_index) {
                case 0: R_RIGHT(ap, bp, cp, dp, ep, J,  Kp[0], w[R2[j]], S2[j]); break;
                case 1: R_RIGHT(ap, bp, cp, dp, ep, I,  Kp[1], w[R2[j]], S2[j]); break;
                case 2: R_RIGHT(ap, bp, cp, dp, ep, H,  Kp[2], w[R2[j]], S2[j]); break;
                case 3: R_RIGHT(ap, bp, cp, dp, ep, G,  Kp[3], w[R2[j]], S2[j]); break;
                case 4: R_RIGHT(ap, bp, cp, dp, ep, F,  Kp[4], w[R2[j]], S2[j]); break;
            }
        }

        unsigned int tmp = h[0];
        h[0] = h[1] + c + dp;
        h[1] = h[2] + d + ep;
        h[2] = h[3] + e + ap;
        h[3] = h[4] + a + bp;
        h[4] = tmp + b + cp;
    }

    str_memcpy(hash, h, sizeof(ripemd160_t));
    return 1;
}
