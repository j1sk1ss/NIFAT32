#ifndef CHECKSUM_H_
#define CHECKSUM_H_
#ifdef __cplusplus
extern "C" {
#endif

typedef unsigned int checksum_t;
checksum_t crc32(unsigned int init, const unsigned char* buf, int len);
checksum_t murmur3_x86_32(const unsigned char* key, unsigned int len, unsigned int seed);

#ifdef __cplusplus
}
#endif
#endif