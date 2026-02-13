#ifndef NIFAT32_TEST_
#define NIFAT32_TEST_

#include "../nifat32.h"
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/time.h>

typedef struct {
    long total;
    int ops;
} nifat32_timer_t;

static inline int add_time2timer(long time, nifat32_timer_t* t) {
    t->ops++;
    t->total += time;
    return 1;
}

static inline int reset_timer(nifat32_timer_t* t) {
    t->ops   = 0;
    t->total = 0;
    return 1;
}

static inline double get_avg_timer(nifat32_timer_t* t) {
    if (!t->ops) return 0;
    return t->total / (double)t->ops;
}

static inline long _current_time_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000000L) + tv.tv_usec;
}

#define MEASURE_TIME_US(block) ({ \
    long _start = _current_time_us(); \
    block; \
    long _end = _current_time_us(); \
    _end - _start; \
})

char* disk_path = "nifat32.img";
#define SET_PATH(path) disk_path = path

int sector_size = 512;
#define SET_SECTOR_SIZE(size) sector_size = size

int disk_fd = -1;

static inline int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    if (!buff_size) return 1;
    return pread(disk_fd, buffer, buff_size, sa * sector_size + offset) > 0;
}

static inline int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    if (!data_size) return 1;
    return pwrite(disk_fd, data, data_size, sa * sector_size + offset) > 0;
}

static int _mock_fprintf_(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vfprintf(stdout, fmt, args);
    va_end(args);
    return result;
}

static inline int _mock_vfprintf_(const char* fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
}

static int setup_nifat32() {
    disk_fd = open(disk_path, O_RDWR);
    if (disk_fd < 0) {
        fprintf(stderr, "%s not found!\n", disk_path);
        return 0;
    }

    nifat32_params_t params = { 
        .bs_num    = 0, 
#ifdef V_SIZE
        .ts        = V_SIZE / sector_size,
        .jc        = J_COUNT,
        .bs_count  = BS_COUNT,
#else
        .ts        = (64 * 1024 * 1024) / sector_size, 
        .jc        = 0, 
        .bs_count  = 5,
#endif
        .fat_cache = CACHE, 
        .disk_io   = {
            .read_sector  = _mock_sector_read_,
            .write_sector = _mock_sector_write_,
            .sector_size  = sector_size
        },
        .logg_io   = {
            .fd_fprintf  = _mock_fprintf_,
            .fd_vfprintf = _mock_vfprintf_
        }
    };

    if (!NIFAT32_init(&params)) {
        fprintf(stderr, "NIFAT32_init() error!\n");
        close(disk_fd);
        return 0;
    }

    return 1;
}

static int destroy_nifat32() {
    int ret = close(disk_fd);
    disk_fd = -1;
    return ret;
}

#define SUCCESS 1
#define FAILURE 0

static inline ci_t nifat32_open_test(ci_t rci, char* path, unsigned char mode, char exp) {
    ci_t ci = NIFAT32_open_content(rci, path, mode);
    if (ci < 0 && exp) {
        fprintf(stderr, "ERROR! NIFAT32_open_content return -> %i, rci=%i, path=%s, mode=%u\n", ci, rci, path, mode);
        return -1;
    }

    return ci;
}

static inline int nifate32_read_and_compare_alloc(ci_t ci, int off, const_buffer_t dst, int size, char exp) {
    buffer_t src = (buffer_t)malloc(size);
    int readden = NIFAT32_read_content2buffer(ci, off, src, size);
    if (memcmp(src, dst, size) && exp) {
        fprintf(stderr, "ERROR! NIFAT32_read_content2buffer return unexpected result!\n");
        fprintf(stderr, "\n Source data [%i]: ", size);
        for (int i = 0; i < size; i++) printf("0x%x(%c) ", dst[i], dst[i]);
        fprintf(stderr, "\n Read result [%i]: ", readden);
        for (int i = 0; i < readden; i++) printf("0x%x(%c) ", src[i], src[i]);
        free(src);
        return 0;
    }

    free(src);
    return 1;
}

static inline int nifate32_read_and_compare(
    ci_t ci, int off, buffer_t src, int src_size, const_buffer_t dst, int dst_size, char exp
) {
    int readden = NIFAT32_read_content2buffer(ci, off, src, src_size);
    if (memcmp(src, dst, dst_size) && exp) {
        fprintf(stderr, "ERROR! NIFAT32_read_content2buffer return unexpected result!\n");
        fprintf(stdout, "\n Source data [%i]: ", dst_size);
        for (int i = 0; i < dst_size; i++) printf("0x%x ", dst[i]);
        fprintf(stdout, "\n Read result [%i]: ", readden);
        for (int i = 0; i < readden; i++) printf("0x%x ", src[i]);
        return 0;
    }

    return 1;
}

#endif