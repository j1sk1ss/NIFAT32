#ifndef STR_H_
#define STR_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/null.h>

/*
Memory special functions. 
*/
void* nft32_str_memcpy(void* destination, const void* source, unsigned int num);
void* nft32_str_memset(void* pointer, unsigned char value, unsigned int num);
int nft32_str_memcmp(const void* firstPointer, const void* secondPointer, unsigned int num);

/*
String special functions.
*/
char* nft32_str_strncpy(char* dst, const char* src, int n);
int nft32_str_strncmp(const char* str1, const char* str2, unsigned int n);
unsigned int nft32_str_strlen(const char* str);
char* nft32_str_strcpy(char* dst, const char* src);

/*
ctype special functions.
*/
int nft32_str_uppercase(char* str);

#ifdef __cplusplus
}
#endif
#endif