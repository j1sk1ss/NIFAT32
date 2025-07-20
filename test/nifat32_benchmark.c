#include "nifat32_test.h"

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

nifat32_timer_t create_timer;
nifat32_timer_t write_timer;
nifat32_timer_t read_timer;
nifat32_timer_t rename_timer;
nifat32_timer_t copy_timer;
nifat32_timer_t delete_timer;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\nUsage:%s <count>\n", argv[0]);
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
    
    unsigned char buffer[sizeof(test_val_t)] = { 0 };
    memset(data.val1, 0, 128);
    strncpy(data.val1, "Test data from structure! Hello there from structure, I guess..", 128);

    int offset = 0;
    while (count-- > 0) {
        const char* target_file = filenames[count % 18];
        char target_fatname[128] = { 0 };
        path_to_fatnames(target_file, target_fatname);

        /* Create target file */
        ci_t ci;
        add_time2timer(MEASURE_TIME_US({
            ci = NIFAT32_open_content(NO_RCI, target_fatname, MODE(R_MODE | W_MODE | CR_MODE, FILE_TARGET));
            if (ci < 0) return EXIT_FAILURE;
        }), &create_timer);

        /* Write test data to file */
        add_time2timer(MEASURE_TIME_US({
            NIFAT32_write_buffer2content(ci, offset, (const_buffer_t)&data, sizeof(test_val_t));
        }), &write_timer);

        /* Read and check test data from file */
        add_time2timer(MEASURE_TIME_US({
            if (!nifate32_read_and_compare(ci, offset, (buffer_t)buffer, 143, (const_buffer_t)&data, sizeof(test_val_t), SUCCESS)) {
                return EXIT_FAILURE;
            }
        }), &read_timer);

        /* Rename file */
        char name_buffer[12] = { 0 };
        _get_name(name_buffer);
        name_to_fatname(name_buffer, target_fatname);
        cinfo_t info = { .type = STAT_FILE };
        strcpy(info.full_name, target_fatname);
        add_time2timer(MEASURE_TIME_US({
            NIFAT32_change_meta(ci, &info);
        }), &rename_timer);

        /* Create another file */
        _get_name(name_buffer);
        name_to_fatname(name_buffer, target_fatname);
        ci_t dst_ci;
        add_time2timer(MEASURE_TIME_US({
            dst_ci = NIFAT32_open_content(NO_RCI, target_fatname, MODE(W_MODE | CR_MODE, FILE_TARGET));
        }), &create_timer);

        /* Copy data from source file to new file */
        add_time2timer(MEASURE_TIME_US({
            NIFAT32_copy_content(ci, dst_ci, DEEP_COPY);
        }), &copy_timer);

        /* Read and check data from new copied file */
        add_time2timer(MEASURE_TIME_US({
            if (!nifate32_read_and_compare(dst_ci, offset, (buffer_t)buffer, 143, (const_buffer_t)&data, sizeof(test_val_t), SUCCESS)) {
                return EXIT_FAILURE;
            }
        }), &read_timer);

        /* Delete copied file */
        add_time2timer(MEASURE_TIME_US({
            NIFAT32_delete_content(dst_ci);
        }), &delete_timer);

        /* Close source file */
        NIFAT32_close_content(ci);
    }

    NIFAT32_unload();

    time_t now = time(NULL);
    char time_str[20] = { 0 };
    struct tm* tm_info = localtime(&now);
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", tm_info);
    fprintf(
        stdout, "[%s] Hundled error count: %i\tUnhundled error count: %i\n", 
        time_str, handled_errors, unhundled_errors
    );
    fprintf(stdout, "Total files: %i, with size=%iMB\n", _id, (4096 * _id) / (1024 * 1024));

    fprintf(stdout, "\n==== Performance Summary ====\n");
    fprintf(stdout, "Avg create time: %.2f µs\n", get_avg_timer(&create_timer));
    fprintf(stdout, "Avg write time:  %.2f µs\n", get_avg_timer(&write_timer));
    fprintf(stdout, "Avg read time:   %.2f µs\n", get_avg_timer(&read_timer));
    fprintf(stdout, "Avg rename time: %.2f µs\n", get_avg_timer(&rename_timer));
    fprintf(stdout, "Avg copy time:   %.2f µs\n", get_avg_timer(&copy_timer));
    fprintf(stdout, "Avg delete time: %.2f µs\n", get_avg_timer(&delete_timer));
    fprintf(stdout, "=============================\n\n\n");

    return EXIT_SUCCESS;
}
