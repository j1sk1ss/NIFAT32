#ifndef ERRORS_H_
#define ERRORS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <std/hamming.h>
#include <std/errcodes.h>
#include <src/disk.h>
#include <src/fatinfo.h>

#define PACK_INFO_ENTRY(f, c) (((unsigned int)(f) << 16) | ((unsigned int)(c) & 0xFFFF))
#define GET_FIRST_ERROR(e)    ((unsigned short)((e) >> 16))
#define GET_LAST_ERROR(e)     ((unsigned short)((e) & 0xFFFF))

#define ERRORS_MULTIPLIER 98776542U
#define GET_ERRORSSECTOR(n, ts) (((((n) + 34) * ERRORS_MULTIPLIER) >> 17) % (ts - 12))

typedef struct {
    unsigned short first_error;
    unsigned short current;
    unsigned short last_error;
} errors_t;

int errors_setup(fat_data_t* fi);
int errors_register_error(error_code_t code, fat_data_t* fi);
error_code_t errors_last_error(fat_data_t* fi);

#ifdef __cplusplus
}
#endif
#endif