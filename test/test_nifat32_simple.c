#include "nifat32_test.h"

int main() {
    if (!setup_nifat32(NULL)) return EXIT_FAILURE;
    ci_t ci = nifat32_open_test(
        NO_RCI, 
        "test_directory/sub_directory/test.txt", 
        (CR_MODE | W_MODE | R_MODE | FILE_TARGET), 
        SUCCESS
    );

    if (ci < 0) {
        fprintf(stderr, "ci < 1!\n");
        return EXIT_FAILURE;
    }
    
    const char sentence[] = "Hello world, It's me again!\n";
    char data_to_work[8096] = { 0 };
    for (int i = 0; i < (int)(sizeof(data_to_work) / sizeof(sentence)); i++) {
        memcpy(data_to_work + (i * sizeof(sentence)), sentence, sizeof(sentence));
    }

    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&data_to_work, sizeof(data_to_work));
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&data_to_work, sizeof(data_to_work), SUCCESS)) return EXIT_FAILURE;
    return EXIT_SUCCESS;
}
