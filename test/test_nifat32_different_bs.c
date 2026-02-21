/* Check how long it takes to start a NIFAT32 instance and how BS copies affect this.
*/
#include "nifat32_test.h"

nifat32_timer_t init_timer;

int main() {
    nifat32_params_t p;
    setup_nifat32(&p);
    destroy_nifat32();

    for (int i = 0; i < p.bs_count; i++) {
        reset_timer(&init_timer);
        for (int j = 0; j < 1024; j++) {
            p.bs_num = 0;
            add_time2timer(MEASURE_TIME_US({
                if (!setup_nifat32(&p)) return EXIT_FAILURE;
            }), &init_timer);     
            destroy_nifat32();
        }

        char buffer[512] = { 0 };
#ifdef V_SIZE
        int ts = V_SIZE / DSK_get_sector_size();
#else
        int ts = (64 * 1024 * 1024) / DSK_get_sector_size();
#endif

        pwrite(disk_fd, buffer, sizeof(buffer), GET_BOOTSECTOR(i, ts) * DSK_get_sector_size());
        printf("Average init time with %i/%i BS is %.2f µs\n", p.bs_num, p.bs_count, get_avg_timer(&init_timer));
    }

    return EXIT_SUCCESS;
}
