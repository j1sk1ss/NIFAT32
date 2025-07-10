/*
Copy test create twi directories and file in one of them. Then try to copy source dir to destination.
*/
#include "nifat32_test.h"

typedef struct {
    char val1[32];
    unsigned short val2;
} test_data_t;

int main() {
    if (!setup_nifat32(64 * 1024 * 1024)) return EXIT_FAILURE;

    // Create target structure of directories and files
    ci_t ci = nifat32_open_test(NO_RCI, "root1/root2/test.txt", MODE(CR_MODE | W_MODE | R_MODE, FILE_TARGET), SUCCESS);
    test_data_t src_tval = { .val1 = "Hello test struct here!", .val2 = 0xDEAD };
    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&src_tval, sizeof(test_data_t));
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&src_tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    NIFAT32_close_content(ci);
    
    // Open source and destination contents
    ci_t src_ci = nifat32_open_test(NO_RCI, "root1", DF_MODE, SUCCESS);
    ci_t dst_ci = nifat32_open_test(NO_RCI, "root2", MODE(CR_MODE | W_MODE | R_MODE, DIR_TARGET), SUCCESS);
    if (!NIFAT32_copy_content(src_ci, dst_ci, DEEP_COPY)) return EXIT_FAILURE;
    NIFAT32_close_content(src_ci);
    NIFAT32_close_content(dst_ci);

    // Check if source data corrupted
    ci = nifat32_open_test(NO_RCI, "root1/root2/test.txt", DF_MODE, SUCCESS);
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&src_tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    NIFAT32_close_content(ci);

    // Check if source data wasn't copied
    ci = nifat32_open_test(NO_RCI, "root2/root2/test.txt", DF_MODE, SUCCESS);
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&src_tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    NIFAT32_close_content(ci);

    // Change source data
    ci = nifat32_open_test(NO_RCI, "root1/root2/test.txt", DF_MODE, SUCCESS);
    test_data_t dst_tval = { .val1 = "New data from struct!", .val2 = 0x1234 };
    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)&dst_tval, sizeof(test_data_t));
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&dst_tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    NIFAT32_close_content(ci);

    // Check if destination data changed
    ci = nifat32_open_test(NO_RCI, "root2/root2/test.txt", DF_MODE, SUCCESS);
    if (!nifate32_read_and_compare_alloc(ci, 0, (const_buffer_t)&src_tval, sizeof(test_data_t), SUCCESS)) return EXIT_FAILURE;
    NIFAT32_close_content(ci);

    return EXIT_SUCCESS;
}
