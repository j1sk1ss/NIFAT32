#ifndef ERRORS_H_
#define ERRORS_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <errcodes.h>

#define ERRORS_MULTIPLIER  98776542U
#define GET_ERRORSSECTOR(n, ts)  (((((n) + 34) * ERRORS_MULTIPLIER) >> 17) % (ts - 12))

#ifdef __cplusplus
}
#endif
#endif