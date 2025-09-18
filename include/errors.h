#ifndef ERRORS_H_
#define ERRORS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <disk.h>
#include <fatinfo.h>
#include <hamming.h>
#include <errcodes.h>

#define ERRORS_MULTIPLIER 98776542U
#define GET_ERRORSSECTOR(n, ts) (((((n) + 34) * ERRORS_MULTIPLIER) >> 17) % (ts - 12))

typedef struct {
    error_code_t code;
} error_t;

int ERR_register_error(error_code_t code);
int ERR_last_error(error_t* b);

#ifdef __cplusplus
}
#endif
#endif