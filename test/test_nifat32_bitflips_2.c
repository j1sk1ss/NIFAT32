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
    if (!setup_nifat32(NULL)) return EXIT_FAILURE;
    fprintf(stdout, "Opening files...\n");
    for (int i = 0; i < 8; i++) {
        ci_t ci = nifat32_open_test(NO_RCI, paths[i], MODE(R_MODE, NO_TARGET), SUCCESS);
        if (ci < 0) return EXIT_FAILURE;
        NIFAT32_close_content(ci);
    }

    return EXIT_SUCCESS;
}
