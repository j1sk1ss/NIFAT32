#include "nifat32_test.h"

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

    long total_open_time_us  = 0;
    long total_write_time_us = 0;
    long total_read_time_us  = 0;

    int open_ops  = 0;
    int write_ops = 0;
    int read_ops  = 0;

#pragma endregion

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\n");
        return EXIT_FAILURE;
    }

    if (!setup_nifat32(64 * 1024 * 1024)) {
        fprintf(stderr, "setup_nifat32() error!\n");
        return EXIT_FAILURE;        
    }

    int count = atoi(argv[1]);
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
        }

        open_ops++;
        ci_t ci;
        total_open_time_us += MEASURE_TIME_US({
            ci = NIFAT32_open_content(NO_RCI, target_fatname, MODE(R_MODE | W_MODE | CR_MODE, FILE_TARGET));
        });

        if (ci < 0) {
            unhundled_errors++;
            fprintf(stderr, "Can't open content, but this content should be presented or created!\n");
            continue;
        }

        write_ops++;
        int written = 0;
        unsigned char buffer[sizeof(test_val_t)] = { 0 };
        total_write_time_us += MEASURE_TIME_US({
            written = NIFAT32_write_buffer2content(ci, offset, (const_buffer_t)&data, sizeof(test_val_t));
        });

        read_ops++;
        int readden = 0;
        total_read_time_us += MEASURE_TIME_US({
            if (!nifate32_read_and_compare(ci, offset, (buffer_t)buffer, 143, (const_buffer_t)&data, sizeof(test_val_t), 1)) {
                return EXIT_FAILURE;
            }
        });

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
