/*
Write read test in SEU MEU context. Will write to default file a structure, then read this data with comparasion.
During this test file system will repair all files.
*/
#include "nifat32_test.h"

typedef struct {
    char val1[32];
    unsigned short val2;
} test_data_t;

char* paths[] = {
    "test/test.txt",  "test1/test1.txt",
    "test/asd.txt",   "test7/test1.txt",
    "test/rer.txt",   "test5/test1.txt",
    "test65/iei.txt", "test0/test1.txt"
};

int main() {
#ifndef NO_CREATION
    if (!setup_nifat32(NULL)) return EXIT_FAILURE;
    fprintf(stdout, "Creating files...\n");
    for (int i = 0; i < 8; i++) {
        ci_t ci = nifat32_open_test(NO_RCI, paths[i], MODE(CR_MODE | R_MODE | W_MODE, FILE_TARGET), SUCCESS);
        test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
        NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t));
        if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) {
            fprintf(stderr, "Incorrect read for written data! Parg=%s, Code: 1\n", paths[i]);
            return EXIT_FAILURE;
        }

        NIFAT32_close_content(ci);
    }

    fprintf(stdout, "Test started...\n");
    long long int i = 1;
    while (i++ < 9999) {
        ci_t ci = nifat32_open_test(NO_RCI, paths[i % 8], MODE(R_MODE, NO_TARGET), SUCCESS);
        test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
        if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) {
            fprintf(stderr, "Incorrect read for written data! Path=%s, Code: 2\n", paths[i % 8]);
            return EXIT_FAILURE;
        }

        if (!(i % 1000)) {
            ci_t rci = nifat32_open_test(NO_RCI, NULL, DF_MODE, SUCCESS);
            NIFAT32_repair_content(rci, 1);
            NIFAT32_repair_bootsectors();
            NIFAT32_close_content(rci);
        }

        NIFAT32_close_content(ci);
    }

    NIFAT32_unload();
#else /* NO_CREATION */
    ci_t ci = NIFAT32_open_content(NO_RCI, NULL, DF_MODE);
    NIFAT32_repair_content(ci, 1);
    NIFAT32_repair_bootsectors();
    NIFAT32_close_content(ci);
#endif
    return EXIT_SUCCESS;
}
