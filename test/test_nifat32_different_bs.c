/*
Write read test. Will write to default file a structure, then read this data with comparasion.
*/
#include "nifat32_test.h"

nifat32_timer_t init_timer;

int main() {
    for (int i = 0; i < 15; i++) {
        reset_timer(&init_timer);
        add_time2timer(MEASURE_TIME_US({
            if (!setup_nifat32()) return EXIT_FAILURE;
        }), &init_timer);
        printf("Average init time with %i BS is %.2f µs\n", i, get_avg_timer(&init_timer));
        
        char buffer[512] = { 0 };
#ifdef V_SIZE
        int ts = V_SIZE / DSK_get_sector_size();
#else
        int ts = (64 * 1024 * 1024) / DSK_get_sector_size();
#endif
        pwrite(disk_fd, buffer, sizeof(buffer), GET_BOOTSECTOR(i, ts) * DSK_get_sector_size());
        destroy_nifat32();
    }

    return EXIT_SUCCESS;
}
