#include "../nifat32.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

#define DISK_PATH   "nifat32.img"
#define SECTOR_SIZE 512

#pragma region [Mock IO]

    static int disk_fd = 0;

    int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
        if (!buff_size) return 1;
        return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
    }

    int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
        if (!data_size) return 1;
        return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE + offset) > 0;
    }

    static int _mock_fprintf(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        int result = vfprintf(stdout, fmt, args);
        va_end(args);
        return result;
    }

    static int _mock_vfprintf(const char* fmt, va_list args) {
        return vfprintf(stdout, fmt, args);
    }

#pragma endregion

#pragma region [Data]

    static int _id = 1;
    static const char* _get_name(char* buffer, int id) {
        snprintf(buffer, 12, "%06d.pg", id < 0 ? _id++ : id);
        return buffer;
    }

#pragma endregion

#pragma region [Timings]

    long total_open_time_us = 0;
    int open_ops = 0;

    static long _current_time_us() {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (tv.tv_sec * 1000000L) + tv.tv_usec;
    }

#pragma endregion

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\n");
        return EXIT_FAILURE;
    }

    if (!LOG_setup(_mock_fprintf, _mock_vfprintf)) {
        fprintf(stderr, "LOG_setup() error!\n");
        close(disk_fd);
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
    nifat32_params params = { .bs_num = 0, .ts = DEFAULT_VOLUME_SIZE / SECTOR_SIZE, .fat_cache = CACHE };
    if (!NIFAT32_init(&params)) {
        fprintf(stderr, "NIFAT32_init() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    fprintf(stdout, "Creating %i files in root directory...\n", count);
    while (count-- > 0) {
        char target_fatname[128] = { 0 };
        char name_buffer[12] = { 0 };
        _get_name(name_buffer, -1);
        name_to_fatname(name_buffer, target_fatname);

        const char* data = "Default data from default file. Nothing interesting here.";
        ci_t ci = NIFAT32_open_content(target_fatname, MODE(W_MODE | CR_MODE, FILE_TARGET));
        if (ci >= 0) {
            NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)data, 58);
            NIFAT32_close_content(ci);
        }
    }

    srand(time(NULL));
    fprintf(stdout, "Performing tests...\n");
    for (int i = 0; i < _id; i++) {
        char target_fatname[128] = { 0 };
        char name_buffer[12] = { 0 };
        _get_name(name_buffer, rand() % (_id + 1));
        name_to_fatname(name_buffer, target_fatname);

        char buffer[512] = { 0 };
        long start = _current_time_us();
        ci_t ci = NIFAT32_open_content(target_fatname, MODE(R_MODE, FILE_TARGET));
        long end = _current_time_us();
        total_open_time_us += (end - start);
        open_ops++;

        if (ci >= 0) {
            NIFAT32_read_content2buffer(ci, 0, (buffer_t)buffer, 58);
            NIFAT32_close_content(ci);
        }
    }

    fprintf(stdout, "\n==== Performance Summary ====\n");
    if (open_ops) fprintf(stdout, "Avg open time:  %.2f µs\n", total_open_time_us / (double)open_ops);
    fprintf(stdout, "=============================\n\n\n");

    NIFAT32_unload();
    return EXIT_SUCCESS;
}
