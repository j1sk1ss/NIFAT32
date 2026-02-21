/*
Journal test. Create, delete and edit entries only by journal.
*/
#include "nifat32_test.h"

typedef struct {
    char val1[32];
    unsigned short val2;
} test_data_t;

int main() {
#ifndef NO_CREATION
    if (!setup_nifat32(NULL)) return EXIT_FAILURE;
    ci_t rci = nifat32_open_test(NO_RCI, NULL, DF_MODE, 1);
    NIFAT32_close_content(rci);
#endif

    return EXIT_SUCCESS;
}
