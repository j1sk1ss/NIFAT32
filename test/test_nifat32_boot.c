#include "nifat32_test.h"

nifat32_timer_t boot;

int main() {
    add_time2timer(MEASURE_TIME_US({
        if (!setup_nifat32()) { return EXIT_FAILURE; }
    }), &boot);
    fprintf(stdout, "Boot time: %.2f µs\n", get_avg_timer(&boot));
    return EXIT_SUCCESS;
}
