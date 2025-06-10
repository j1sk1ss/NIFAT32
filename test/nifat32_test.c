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
    if (!buff_size) return 1;
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
}

int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    if (!data_size) return 1;
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
        .fullname = "root/test.txt",
        .name = "test",
        .ext = "txt"
    },
    {
        .fullname = "root/tdir/asd.bin",
        .name = "asd",
        .ext = "bin"
    },
    {
        .fullname = "root/dir/dir2/dir3/123.dr",
        .name = "123",
        .ext = "dr"
    },
    {
        .fullname = "root/terrr/terrr",
        .name = "terrr",
        .ext = ""
    },
    {
        .fullname = "root/test1.txt",
        .name = "test1",
        .ext = "txt"
    },
    {
        .fullname = "root/test2.txt",
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
        .val2 = 0x1A,
        .val3 = 0xF34,
        .val4 = 0xDEA,
        .val5 = 0xDEAD
    };
    
    memset(data.val1, 0, 128);
    strncpy(data.val1, "Test data from structure! Hello there from structure, I guess..", 128);

    int offset = 0;
    while (count-- > 0) {
        const char* target_file = filenames[count % 6].fullname;
        char target_fatname[128] = { 0 };
        path_to_fatnames(target_file, target_fatname);

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

        ci_t ci = NIFAT32_open_content(target_fatname, MODE(CR_MODE, FILE_MODE));
        if (ci < 0) {
            unhundled_errors++;
            fprintf(stderr, "Can't open content, but this content should be presented or created!\n");
            continue;
        }

        unsigned char buffer[sizeof(test_val_t)] = { 0 };
        NIFAT32_write_buffer2content(ci, offset, (const_buffer_t)&data, sizeof(test_val_t));
        NIFAT32_read_content2buffer(ci, offset, (buffer_t)buffer, sizeof(test_val_t));

        offset += sizeof(test_val_t);
        if (memcmp((const_buffer_t)&data, (const_buffer_t)buffer, sizeof(test_val_t))) {
            unhundled_errors++;

            printf("\n Source data: ");
            for (int i = 0; i < sizeof(test_val_t); i++) {
                printf("0x%x ", ((const_buffer_t)&data)[i]);
            }

            printf("\n Read result: ");
            for (int i = 0; i < sizeof(test_val_t); i++) {
                printf("0x%x ", buffer[i]);
            }

            fflush(stdout);
            while (1);
        }

        NIFAT32_close_content(ci);
    }

    return EXIT_SUCCESS;
}
