#include <std/str.h>

void* nft32_str_memcpy(void* __restrict destination, const void* __restrict source, unsigned int num) {
    unsigned int num_dwords = num / 4;
    unsigned int num_bytes  = num % 4;
    unsigned int* dest32 = (unsigned int*)destination;
    unsigned int* src32  = (unsigned int*)source;
    unsigned char* dest8 = ((unsigned char*)destination) + num_dwords * 4;
    unsigned char* src8  = ((unsigned char*)source) + num_dwords * 4;
    for (unsigned int i = 0; i < num_dwords; i++) dest32[i] = src32[i];
    for (unsigned int i = 0; i < num_bytes; i++) dest8[i] = src8[i];
    return destination;
}

void* nft32_str_memset(void* pointer, unsigned char value, unsigned int num) {
    unsigned int num_dwords = num / 4;
    unsigned int num_bytes  = num % 4;
    unsigned int* dest32 = (unsigned int*)pointer;
    unsigned char* dest8 = ((unsigned char*)pointer) + num_dwords * 4;
    unsigned char val8   = (unsigned char)value;
    unsigned int val32   = value | (value << 8) | (value << 16) | (value << 24);
    for (unsigned int i = 0; i < num_dwords; i++) dest32[i] = val32;
    for (unsigned int i = 0; i < num_bytes; i++)  dest8[i]  = val8;
    return pointer;
}

int nft32_str_memcmp(const void* firstPointer, const void* secondPointer, unsigned int num) {
    const unsigned char* u8Ptr1 = (const unsigned char *)firstPointer;
    const unsigned char* u8Ptr2 = (const unsigned char *)secondPointer;
    for (unsigned int i = 0; i < num; i++) {
        if (u8Ptr1[i] != u8Ptr2[i]) {
            return (int)u8Ptr1[i] - (int)u8Ptr2[i];
        }
    }
    
    return 0;
}

char* nft32_str_strncpy(char* dst, const char* src, int n) {
    int i = 0;
    while (i < n && src[i]) {
        dst[i] = src[i];
        i++;
    }

    while (i < n) {
        dst[i] = 0;
        i++;
    }

    return dst;
}

int nft32_str_strncmp(const char* str1, const char* str2, unsigned int n) {
    for (unsigned int i = 0; i < n; ++i) {
        if (str1[i] != str2[i] || (!str1[i] || !str2[i])) {
            return (unsigned char)str1[i] - (unsigned char)str2[i];
        }
    }

    return 0;
}

unsigned int nft32_str_strlen(const char* str) {
    unsigned int len = 0;
    while (*str) {
        ++len;
        ++str;
    }

    return len;
}

char* nft32_str_strcpy(char* dst, const char* src) {
    if (!nft32_str_strlen(src)) return NULL;
    int i = 0;
    while (src[i]) {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
    return dst;
}

static inline int _islower(int c) {
    return c >= 'a' && c <= 'z';
}

static inline int _toupper(int c) {
    if (_islower(c)) return c - 'a' + 'A';
    else return c;
}

int nft32_str_uppercase(char* str) {
    if (!str) return 0;
    for (int i = 0; str[i]; i++) {
        str[i] = _toupper(str[i]);
    }

    return 1;
}
