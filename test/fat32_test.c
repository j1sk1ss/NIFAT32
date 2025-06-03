#include "../nifat32.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>

#define DISK_PATH   "nifat32.img"
#define SECTOR_SIZE 512

static int disk_fd = 0;

int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
}

int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE + offset) > 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\n");
        return EXIT_FAILURE;
    }

    int count = atoi(argv[1]);
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

    int handled_errors = 0;
    int unhundled_errors = 0;

    const char* target_file = "test.txt";
    char target_fatname[128] = { 0 };
    name_to_fatname(target_file, target_fatname);

    const char* data = "Hello there! This is a custom data!\n123345786780-764234][']\t\t\n\r\bqqq`12";
    int data_size = strlen(data) + 1;

    while (count-- > 0) {
        ci_t ci = NIFAT32_open_content(target_fatname);
        if (ci < 0) {
            handled_errors++;
            fprintf(stderr, "Can't open content!\n");
            continue;
        }

        int write_corrupted = 0;
        if (!NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)data, data_size)) {
            handled_errors++;
            write_corrupted = 1;
            fprintf(stderr, "Can't write content!\n");
        }

        int read_cottupted = 0;
        unsigned char buffer[512] = { 0 };
        if (!NIFAT32_read_content2buffer(ci, 0, (buffer_t)buffer, 512)) {
            handled_errors++;
            read_cottupted = 1;
            fprintf(stderr, "Can't write content!\n");
        }

        if (!write_corrupted && !read_cottupted) {
            if (strcmp(data, buffer)) {
                unhundled_errors++;
            }
        }
    }

    return EXIT_SUCCESS;
}
