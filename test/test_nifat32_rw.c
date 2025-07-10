/*
Write read test. Will write to default file a structure, then read this data with comparasion.
*/
#include "nifat32_test.h"

typedef struct {
    char val1[32];
    unsigned short val2;
} test_data_t;

int main() {
    if (!setup_nifat32(64 * 1024 * 1024)) return EXIT_FAILURE;
    ci_t ci = nifat32_open_test(NO_RCI, "root1/test.txt", (CR_MODE | DF_MODE | FILE_TARGET), SUCCESS);
    test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t));
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
