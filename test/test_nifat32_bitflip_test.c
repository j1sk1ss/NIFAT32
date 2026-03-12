#include "nifat32_test.h"

int main() {
    if (!setup_nifat32(NULL)) {
        close(disk_fd);
        return EXIT_FAILURE;
    }

    ci_t ci = NIFAT32_open_content(NO_RCI, "dir1/dir2/dir3/file.txt", MODE(R_MODE | W_MODE, FILE_TARGET));
    if (ci < 0) {
        close(disk_fd);
        return EXIT_FAILURE;
    }
    
    close(disk_fd);
    return EXIT_SUCCESS;
}
