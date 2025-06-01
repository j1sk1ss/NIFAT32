/*
This is the test environment for NIFAT32. Here we use UNIX syscalls for mock working with disk.
Save in mind, that here it takes a lot of time to perform a number of syscalls for read, open write etc.
On embedded system it will much faster.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include "nifat32.h"

#define DISK_PATH   "nifat32.img"
#define SECTOR_SIZE 512

static int disk_fd = 0;

int _mock_sector_read_(sector_addr_t sa, unsigned char* buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE) > 0;
}

int _mock_sector_write_(sector_addr_t sa, const unsigned char* data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE) > 0;
}

int main(int argc, char* argv[]) {
    disk_fd = open(DISK_PATH, O_RDWR);
    if (disk_fd < 0) {
        fprintf(stderr, "%s not found!\n", DISK_PATH);
        return EXIT_FAILURE;
    }

    if (!DSK_setup(_mock_sector_read_, _mock_sector_write_, SECTOR_SIZE)) {
        fprintf(stderr, "DSK_setup() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    if (!NIFAT32_init()) {
        fprintf(stderr, "NIFAT32_init() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    const char* test_file = "test2.tst";
    char fatname_buffer[13] = { 0 };
    name_to_fatname(test_file, fatname_buffer);

    if (!FAT_content_exists(fatname_buffer)) {
        fprintf(stderr, "File %s not found!\n", fatname_buffer);
        return EXIT_FAILURE;
    }
    else {
        fprintf(stdout, "File %s found!\n", fatname_buffer);
    }

    unsigned char content[512] = { 0 };
    fprintf(stdout, "Trying to open file: %s\n", fatname_buffer);
    ci_t ci = FAT_open_content(fatname_buffer);
    if (ci < 0) {
        fprintf(stderr, "Can't open file %s!\n", fatname_buffer);
        return EXIT_FAILURE;
    }

    cinfo_t info;
    FAT_stat_content(ci, &info);
    fprintf(stdout, "Opened content name: %s.%s\n", info.file_name, info.file_extension);

    fprintf(stdout, "Trying to read content from file: %s\n", info.file_name);
    if (FAT_read_content2buffer(ci, 0, content, 512) < 0) {
        fprintf(stderr, "Can't read file %s!\n", fatname_buffer);
        FAT_close_content(ci);
        return EXIT_FAILURE;
    }

    printf("Content data: %s\n", content);
    FAT_close_content(ci);

    close(disk_fd);
    return EXIT_SUCCESS;
}
