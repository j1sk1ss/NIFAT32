/* gcc-14 -DERROR_LOGS test/fat32_test.c nifat32.c src/* std/* -o test/fat32_test */
#include "../nifat32.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define DISK_PATH   "nifat32.img"
#define SECTOR_SIZE 512

static int disk_fd = 0;

int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
}

int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE + offset) > 0;
}

typedef struct {
    char  val1[128];
    char  val2;
    short val3;
    int   val4;
    long  val5;
} __attribute__((packed)) test_val_t;

typedef struct {
    char* fullname;
    char* name;
    char* ext;
} filename_t;

static filename_t filenames[] = {
    {
        .fullname = "test.txt",
        .name = "test",
        .ext = "txt"
    },
    {
        .fullname = "asd.bin",
        .name = "asd",
        .ext = "bin"
    },
    {
        .fullname = "123.dr",
        .name = "123",
        .ext = "dr"
    },
    {
        .fullname = "terrr",
        .name = "terrr",
        .ext = ""
    },
    {
        .fullname = "test1.txt",
        .name = "test1",
        .ext = "txt"
    },
    {
        .fullname = "test2.txt",
        .name = "test2",
        .ext = "txt"
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\n");
        return EXIT_FAILURE;
    }

    disk_fd = open(DISK_PATH, O_RDWR);
    if (disk_fd < 0) {
        fprintf(stderr, "%s not found!\n", DISK_PATH);
        return EXIT_FAILURE;
    }

    int count = atoi(argv[1]);
    if (!DSK_setup(_mock_sector_read_, _mock_sector_write_, SECTOR_SIZE)) {
        fprintf(stderr, "DSK_setup() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    #define DEFAULT_VOLUME_SIZE (64 * 1024 * 1024)
    if (!NIFAT32_init(0, DEFAULT_VOLUME_SIZE / SECTOR_SIZE)) {
        fprintf(stderr, "NIFAT32_init() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    int handled_errors = 0;
    int unhundled_errors = 0;

    test_val_t data = {
        .val2 = 0xA1,
        .val3 = 0xF34,
        .val4 = 0xDEA,
        .val5 = 0xDEAD
    };
    
    strncpy(data.val1, "Test data from structure! Hello there from structure, I guess..", 128);

    int offset = 0;
    while (count-- > 0) {
        const char* target_file = filenames[count % 6].fullname;
        char target_fatname[128] = { 0 };
        name_to_fatname(target_file, target_fatname);

        if (!(count % 1000)) {
            time_t now = time(NULL);
            struct tm* tm_info = localtime(&now);
            char time_str[20];
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            fprintf(
                stdout, "[%s] Hundled error count: %i\tUnhundled error count: %i\n", 
                time_str, handled_errors, unhundled_errors
            );
        }

        if (!NIFAT32_content_exists(target_file)) {
            handled_errors++;
            cinfo_t file = { .type = STAT_FILE };
            str_memcpy(file.file_name, filenames[count % 6].name, strlen(filenames[count % 6].name) + 1);
            str_memcpy(file.file_extension, filenames[count % 6].ext, strlen(filenames[count % 6].ext) + 1);
            name_to_fatname(target_file, file.full_name);
            if (!NIFAT32_put_content(PUT_TO_ROOT, &file)) {
                handled_errors++;
                fprintf(stderr, "File creation error!\n");
                continue;
            }
        }

        ci_t ci = NIFAT32_open_content(target_fatname);
        if (ci < 0) {
            unhundled_errors++;
            fprintf(stderr, "Can't open content, but this content should be presented or created!\n");
            continue;
        }

        unsigned char buffer[8192] = { 0 };
        NIFAT32_write_buffer2content(ci, offset, (const_buffer_t)&data, sizeof(test_val_t));
        NIFAT32_read_content2buffer(ci, offset, (buffer_t)buffer, 8192);

        offset += sizeof(test_val_t);
        if (!memcmp((const_buffer_t)&data, (const_buffer_t)buffer, sizeof(test_val_t))) {
            unhundled_errors++;
        }

        NIFAT32_close_content(ci);
    }

    return EXIT_SUCCESS;
}
