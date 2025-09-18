#ifndef STR_H_
#define STR_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <null.h>

/*
Memory special functions. 
*/
void* str_memcpy(void* destination, const void* source, unsigned int num);
void* str_memset(void* pointer, unsigned char value, unsigned int num);
int str_memcmp(const void* firstPointer, const void* secondPointer, unsigned int num);

/*
String special functions.
*/
char* str_strncpy(char* dst, const char* src, int n);
int str_strcmp(const char* f, const char* s);
int str_strncmp(const char* str1, const char* str2, unsigned int n);
unsigned int str_strlen(const char* str);
char* str_strcpy(char* dst, const char* src);
char* str_strcat(char* dest, const char* src);

/*
ctype special functions.
*/
int str_toupper(int c);
int str_uppercase(char* str);

#ifdef __cplusplus
}
#endif
#endif