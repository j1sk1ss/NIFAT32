/*
License:
    MIT License. See LICENSE file in project root.
    Copyright (c) 2025 Nikolay

Description:
    Persistent error storage and error registration helpers.

Dependencies:
    - std/hamming.h - Encoded error data helpers.
    - std/errcodes.h - Error code enumeration.
    - src/disk.h - Sector I/O primitives.
    - src/fatinfo.h - FAT filesystem metadata.
*/

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

#define ERRORS_MULTIPLIER 10986542U
#define GET_ERRORSSECTOR(n, ts) (((((n) + 23) * ERRORS_MULTIPLIER) >> 9) % (ts - 2))

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
