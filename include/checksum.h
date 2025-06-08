#ifndef CHECKSUM_H_
#define CHECKSUM_H_

typedef unsigned int checksum_t;
checksum_t crc32(unsigned int init, const unsigned char* buf, int len);

#endif