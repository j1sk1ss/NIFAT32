#include "nifat32_test.h"

nifat32_timer_t r, w;

int main() {
    if (!setup_nifat32(NULL)) return EXIT_FAILURE;
    fat_data_t fd;
    NIFAT32_get_fs_data(&fd);
    add_time2timer(MEASURE_TIME_US({
        if (!read_fat(123, &fd)) return EXIT_FAILURE;
    }), &r);
    add_time2timer(MEASURE_TIME_US({
        if (!write_fat(124, FAT_CLUSTER_BAD, &fd)) return EXIT_FAILURE;
    }), &r);
    fprintf(stdout, "FAT read time:   %.6f µs\n", get_avg_timer(&r));
    fprintf(stdout, "FAT write time:  %.6f µs\n", get_avg_timer(&w));
    return EXIT_SUCCESS;
}
