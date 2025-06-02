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

int _mock_sector_read_(sector_addr_t sa, unsigned int offset, unsigned char* buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
}

int _mock_sector_write_(sector_addr_t sa, unsigned int offset, const unsigned char* data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE + offset) > 0;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <path>\n", argv[0]);
        return EXIT_FAILURE;
    }

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

    /* Base file read / write test */
    {
        const char* test_file = argv[1];
        char fatname_buffer[13] = { 0 };
        name_to_fatname(test_file, fatname_buffer);

        if (!NIFAT32_content_exists(fatname_buffer)) {
            fprintf(stderr, "File %s not found!\n", fatname_buffer);
            close(disk_fd);
            return EXIT_FAILURE;
        }
        else {
            fprintf(stdout, "File %s found!\n", fatname_buffer);
        }

        unsigned char content[512] = { 0 };
        fprintf(stdout, "Trying to open file: %s\n", fatname_buffer);
        ci_t ci = NIFAT32_open_content(fatname_buffer);
        if (ci < 0) {
            fprintf(stderr, "Can't open file %s!\n", fatname_buffer);
            close(disk_fd);
            return EXIT_FAILURE;
        }

        cinfo_t info;
        NIFAT32_stat_content(ci, &info);
        fprintf(stdout, "Opened content name: %s.%s\n", info.file_name, info.file_extension);

        /* Reading test */
        {
            fprintf(stdout, "Trying to read content from file: %s\n", info.file_name);
            if (NIFAT32_read_content2buffer(ci, 0, content, 512) < 0) {
                fprintf(stderr, "Can't read file %s!\n", fatname_buffer);
                NIFAT32_close_content(ci);
                close(disk_fd);
                return EXIT_FAILURE;
            }

            fprintf(stdout, "Content data: %s\n", content);
        }

        /* Writing test */
        {
            fprintf(stdout, "Trying to write data to content\n");
            if (NIFAT32_write_buffer2content(ci, 5, (const buffer_t)"nax!", 5) < 0) {
                fprintf(stderr, "Can't write file %s!\n", fatname_buffer);
                NIFAT32_close_content(ci);
                close(disk_fd);
                return EXIT_FAILURE;
            }

            fprintf(stdout, "Writing complete!\n");
        }

        NIFAT32_close_content(ci);
    }

    /* File creating test */
    {
        const char* new_directory = "tdir";
        const char* new_file = "tfile.txt";
        fprintf(stdout, "Trying to create new file %s in directory %s\n", new_file, new_directory);

        cinfo_t dir_info = { .type = STAT_DIR };
        str_memcpy(dir_info.file_name, "tdir", 4);
        name_to_fatname("tdir", dir_info.full_name);

        cinfo_t file_info = { .type = STAT_FILE };
        str_memcpy(file_info.file_name, "tfile", 6);
        str_memcpy(file_info.file_extension, "txt", 4);
        name_to_fatname("tfile.txt", file_info.full_name);

        char path_buffer[100] = { 0 };
        name_to_fatname("root", path_buffer);

        ci_t root_ci = NIFAT32_open_content(path_buffer);
        if (root_ci < 0) {
            close(disk_fd);
            return EXIT_FAILURE;
        }

        NIFAT32_put_content(root_ci, &dir_info);
        NIFAT32_close_content(root_ci);

        name_to_fatname("root/tdir", path_buffer);
        ci_t tdir_ci = NIFAT32_open_content(path_buffer);
        if (tdir_ci < 0) {
            close(disk_fd);
            return EXIT_FAILURE;
        }

        NIFAT32_put_content(tdir_ci, &file_info);
        NIFAT32_close_content(tdir_ci);
    }

    /* Write to created file test */
    if (0) {
        char path_buffer[128] = { 0 };
        name_to_fatname("root/tdir/", path_buffer);
        name_to_fatname("tfile.txt", path_buffer + 10);
        ci_t ci = NIFAT32_open_content(path_buffer);
        if (ci < 0) {
            close(disk_fd);
            return EXIT_FAILURE;
        }

        if (NIFAT32_write_buffer2content(ci, 0, (const buffer_t)"Hello from new file!", 21) < 0) {
            NIFAT32_close_content(ci);
            close(disk_fd);
            return EXIT_FAILURE;
        }

        NIFAT32_close_content(ci);
    }

    /* Read from new file */
    if (0) {
        char path_buffer[128] = { 0 };
        name_to_fatname("root/tdir/", path_buffer);
        name_to_fatname("tfile.txt", path_buffer + 10);
        ci_t ci = NIFAT32_open_content(path_buffer);
        if (ci < 0) {
            close(disk_fd);
            return EXIT_FAILURE;
        }

        char content[512] = { 0 };
        if (NIFAT32_read_content2buffer(ci, 0, content, 512) < 0) {
            NIFAT32_close_content(ci);
            close(disk_fd);
            return EXIT_FAILURE;
        }

        fprintf(stdout, "Content from new file: %s\n", content);
        NIFAT32_close_content(ci);
    }

    /* Deleting file */
    if (0) {
        // char path_buffer[128] = { 0 };
        // name_to_fatname("root/tdir/", path_buffer);
        // name_to_fatname("tfile.txt", path_buffer + 10);
        // ci_t ci = NIFAT32_open_content(path_buffer);
        // if (ci < 0) {
        //     close(disk_fd);
        //     return EXIT_FAILURE;
        // }

        // NIFAT32_delete_content(ci);
    }

    close(disk_fd);
    return EXIT_SUCCESS;
}
