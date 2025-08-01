/*
Write read test. Will write to default file a structure, then read this data with comparasion.
*/
#include "nifat32_test.h"

typedef struct {
    char val1[32];
    unsigned short val2;
} test_data_t;

int main() {
#ifndef NO_CREATION
    if (!setup_nifat32()) return EXIT_FAILURE;
    ci_t ci = nifat32_open_test(NO_RCI, "root99/test.txt", (CR_MODE | W_MODE | R_MODE | FILE_TARGET), SUCCESS);
    test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t));
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
#endif

    return EXIT_SUCCESS;
}
