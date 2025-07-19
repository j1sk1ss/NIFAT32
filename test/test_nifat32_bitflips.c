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
    "root/test.txt", "root1/test1.txt",
    "root/asd.txt", "root7/test1.txt",
    "root2/rer.txt", "root5/test1.txt",
    "root65/iei.txt", "root0/test1.txt"
};

int main() {
    if (!setup_nifat32(64 * 1024 * 1024)) return EXIT_FAILURE;
    fprintf(stdout, "Creating files...\n");
    for (int i = 0; i < 8; i++) {
        ci_t ci = nifat32_open_test(NO_RCI, paths[i], (CR_MODE | R_MODE | W_MODE | FILE_TARGET), SUCCESS);
        test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
        NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t));
        if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
        NIFAT32_close_content(ci);
    }

    fprintf(stdout, "Test started...\n");
    long long int i = 0;
    while (i++) {
        ci_t ci = nifat32_open_test(NO_RCI, paths[i % 8], (R_MODE | NO_TARGET), SUCCESS);
        test_data_t tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
        if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
        if (!(i % 1000)) {
            ci_t rci = nifat32_open_test(NO_RCI, NULL, DF_MODE, SUCCESS);
            NIFAT32_repair_content(rci, 1);
            NIFAT32_repair_bootsectors();
            NIFAT32_close_content(rci);
        }

        NIFAT32_close_content(ci);
    }
    
    return EXIT_SUCCESS;
}
