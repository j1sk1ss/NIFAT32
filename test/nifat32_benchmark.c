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

    typedef struct {
        char  val1[128];
        char  val2;
        short val3;
        int   val4;
        long  val5;
    } __attribute__((packed)) test_val_t;

    static char* filenames[] = {
        "root/test.txt",   "root/tdir/asd.bin", "root/dir/dir2/dir3/123.dr", "root/terrr/terrr",
        "root/test1.txt",  "root/test2.txt",    "root/asd",                  "root/rt/fr/or/qwe/df/wd/lf/ls/ge/w/e/r/t/y/u/i/file.txt",
        "root/test_1.txt", "root/test_2.txt",   "root/test_3.txt",           "root/test_4.txt", "root/test_5.txt", "root/test_6.txt",
        "root/test_7.txt", "root/test_8.txt",   "root/test_9.txt",           "root/test_10.txt"
    };

    static int _id = 1;
    static const char* _get_name(char* buffer) {
        snprintf(buffer, 12, "%06d.pg", _id++);
        return buffer;
    }

#pragma endregion

#pragma region [Timings]

    long total_open_time_us = 0;
    long total_write_time_us = 0;
    long total_read_time_us = 0;

    int open_ops = 0;
    int write_ops = 0;
    int read_ops = 0;

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

    int handled_errors   = 0;
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
        const char* target_file = filenames[count % 18];
        char target_fatname[128] = { 0 };
        path_to_fatnames(target_file, target_fatname);

        if (!(count % 1000)) {
            time_t now = time(NULL);
            char time_str[20] = { 0 };
            struct tm* tm_info = localtime(&now);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
            fprintf(
                stdout, "[%s] Hundled error count: %i\tUnhundled error count: %i\n", 
                time_str, handled_errors, unhundled_errors
            );
            fprintf(stdout, "Total files: %i, with size=%iMB\n", _id, (offset * _id) / (1024 * 1024));

            fprintf(stdout, "\n==== Performance Summary ====\n");
            if (open_ops)  fprintf(stdout, "Avg open time:  %.2f µs\n", total_open_time_us / (double)open_ops);
            if (write_ops) fprintf(stdout, "Avg write time: %.2f µs\n", total_write_time_us / (double)write_ops);
            if (read_ops)  fprintf(stdout, "Avg read time:  %.2f µs\n", total_read_time_us / (double)read_ops);
            fprintf(stdout, "=============================\n\n\n");

            open_ops = write_ops = read_ops = 0;
            // name_to_fatname("root", target_fatname);
            // ci_t ci = NIFAT32_open_content(target_fatname, DF_MODE);
            // if (ci >= 0) NIFAT32_delete_content(ci);
        }

        long start = _current_time_us();
        ci_t ci = NIFAT32_open_content(NO_RCI, target_fatname, MODE(R_MODE | W_MODE | CR_MODE, FILE_TARGET));
        long end = _current_time_us();
        total_open_time_us += (end - start);
        open_ops++;

        if (ci < 0) {
            unhundled_errors++;
            fprintf(stderr, "Can't open content, but this content should be presented or created!\n");
            continue;
        }

        unsigned char buffer[sizeof(test_val_t)] = { 0 };

        start = _current_time_us();
        int written = NIFAT32_write_buffer2content(ci, offset, (const_buffer_t)&data, sizeof(test_val_t));
        end = _current_time_us();
        total_write_time_us += (end - start);
        write_ops++;

        start = _current_time_us();
        int readden = NIFAT32_read_content2buffer(ci, offset, (buffer_t)buffer, sizeof(test_val_t));
        end = _current_time_us();
        total_read_time_us += (end - start);
        read_ops++;

        if (memcmp((const_buffer_t)&data, (const_buffer_t)buffer, sizeof(test_val_t))) {
            unhundled_errors++;

            fprintf(stdout, "\n Source data [%i]: ", written);
            for (int i = 0; i < sizeof(test_val_t); i++) printf("0x%x ", ((const_buffer_t)&data)[i]);
            fprintf(stdout, "\n Read result [%i]: ", readden);
            for (int i = 0; i < sizeof(test_val_t); i++) printf("0x%x ", buffer[i]);
            
            fprintf(stdout, "Total files: %i, with size=%iMB\nOffset=%i\n", _id, (offset * _id) / (1024 * 1024), offset);
            fflush(stdout);
            return EXIT_FAILURE;
        }

        char name_buffer[12] = { 0 };
        _get_name(name_buffer);
        name_to_fatname(name_buffer, target_fatname);

        cinfo_t info = { .type = STAT_FILE };
        strcpy(info.full_name, target_fatname);
        
        NIFAT32_change_meta(ci, &info);
        NIFAT32_close_content(ci);
        if (!(count % 18)) offset += sizeof(test_val_t);
    }

    NIFAT32_unload();
    return EXIT_SUCCESS;
}
