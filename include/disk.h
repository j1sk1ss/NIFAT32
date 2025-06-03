#ifndef DISK_H_
#define DISK_H_

#include <stddef.h>
#include "threading.h"
#include "str.h"
#include "mm.h"

typedef unsigned int sector_offset_t;
typedef unsigned int sector_addr_t;

#define WRITE_LOCK 0
#define READ_LOCK  1
typedef struct {
    sector_addr_t start;
    int count;
    int ro;
} io_area_t;

#define IO_THREADS_MAX 50
typedef struct {
    io_area_t areas[IO_THREADS_MAX];
    lock_t lock;
} io_thread_t;

typedef struct {
    int (*read_sector)(sector_addr_t, sector_offset_t, unsigned char*, int);
    int (*write_sector)(sector_addr_t, sector_offset_t, const unsigned char*, int);
    int sector_size;
} io_t;

int DSK_setup(
    int (*read)(sector_addr_t, sector_offset_t, unsigned char*, int), 
    int (*write)(sector_addr_t, sector_offset_t, const unsigned char*, int), 
    int sector_size
);

int DSK_read_sector(sector_addr_t sa, unsigned char* buffer, int buff_size);
int DSK_readoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* buffer, int buff_size, int sc);
int DSK_write_sector(sector_addr_t sa, const unsigned char* data, int data_size);
int DSK_writeoff_sectors(sector_addr_t sa, sector_offset_t offset, const unsigned char* data, int data_size, int sc);
int DSK_get_sector_size();

#endif