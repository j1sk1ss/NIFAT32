#include "nifat32_test.h"

static int _id = 1;
static const char* _get_raw_name(char* buffer, int id) {
    snprintf(buffer, 36, "%06d.pg", id < 0 ? _id++ : id);
    return buffer;
}

static const char* _get_name(char* buffer, int id) {
    snprintf(buffer, 36, "root/%06d.pg", id < 0 ? _id++ : id);
    return buffer;
}

nifat32_timer_t open_timer;

int main(int argc, char* argv[]) {
#ifndef NO_CREATION
    if (argc < 2) {
        fprintf(stderr, "Test count requiered!\n");
        return EXIT_FAILURE;
    }

    if (!setup_nifat32()) {
        fprintf(stderr, "setup_nifat32() error!\n");
        return EXIT_FAILURE;        
    }

    int count = atoi(argv[1]);
    fprintf(stdout, "Creating %i files in root directory...\n", count);
    const char* data = "Default data from default file. Nothing interesting here.";
    while (count-- > 0) {
        char target_fatname[128] = { 0 };
        char name_buffer[36] = { 0 };
        _get_name(name_buffer, -1);
        path_to_fatnames(name_buffer, target_fatname);

        ci_t ci = NIFAT32_open_content(NO_RCI, target_fatname, MODE(W_MODE | CR_MODE, FILE_TARGET));
        if (ci >= 0) {
            NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)data, 58);
            NIFAT32_close_content(ci);
        }
    }

    srand(time(NULL));
    fprintf(stdout, "Performing tests...\n");
    ci_t rci = NIFAT32_open_content(NO_RCI, "root", DF_MODE);

    long index_time_st = 0;
    fprintf(stdout, "\n==== Performance Summary ====\n");
    fprintf(stdout, "Index time:  %.2f µs\n", (MEASURE_TIME_US({index_time_st = NIFAT32_index_content(rci);})));

    for (int i = 0; i < _id; i++) {
        char target_fatname[128] = { 0 };
        char name_buffer[36] = { 0 };
        _get_raw_name(name_buffer, ((1 + rand()) % _id));
        path_to_fatnames(name_buffer, target_fatname);

        ci_t ci = -1;
        add_time2timer(MEASURE_TIME_US({
            ci = NIFAT32_open_content(rci, target_fatname, DF_MODE);
        }), &open_timer);

        if (ci >= 0) {
            char buffer[512] = { 0 };
            NIFAT32_read_content2buffer(ci, 0, (buffer_t)buffer, 512);
            if (memcmp(data, buffer, 58)) fprintf(stderr, "%.58s != %.58s!\n", data, buffer);
            NIFAT32_close_content(ci);
        } 
        else {
            fprintf(stderr, "[WARN] CAN'T OPEN %s!\n", target_fatname);
        }
    }

    NIFAT32_close_content(rci);
    fprintf(stdout, "Avg open time:  %.2f µs\n", get_avg_timer(&open_timer));
    fprintf(stdout, "=============================\n\n\n");

    NIFAT32_unload();
#endif

    return EXIT_SUCCESS;
}
