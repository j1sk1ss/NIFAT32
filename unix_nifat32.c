#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "nifat32.h"

#define DISK_PATH   "disk_image.dsk"
#define SECTOR_SIZE 512

static int disk_fd = 0;

int _mock_sector_read_(sector_addr_t sa, unsigned char* buffer, int buff_size) {
    unsigned int offset = sa * SECTOR_SIZE;
    return pread(disk_fd, buffer, buff_size, offset);
}

int _mock_sector_write_(sector_addr_t sa, unsigned char* data, int data_size) {
    unsigned int offset = sa * SECTOR_SIZE;
    return pwrite(disk_fd, data, data_size, offset);
}

int main(int argc, char* argv[]) {
    disk_fd = open(DISK_PATH, O_RDWR);
    if (disk_fd < 0) {
        return EXIT_FAILURE;
    }

    if (!DSK_setup(_mock_sector_read_, _mock_sector_write_, SECTOR_SIZE)) {
        return EXIT_FAILURE;
    }

    if (!NIFAT32_init()) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
