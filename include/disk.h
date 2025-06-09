#ifndef DISK_H_
#define DISK_H_

#include <stddef.h>
#include "threading.h"
#include "str.h"
#include "mm.h"

typedef int sector_offset_t;
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

/*
Setup disk ubstraction layer.

Params: 
- read - IO read function on specific platform.
- write - IO write function on specific platform.
- sector_size - Platform sector size.

Return 1 if setup success.
Return 0 if something goes wrong.
*/
int DSK_setup(
    int (*read)(sector_addr_t, sector_offset_t, unsigned char*, int), 
    int (*write)(sector_addr_t, sector_offset_t, const unsigned char*, int), 
    int sector_size
);

/*
Read one sector from disk with io functions.
Note: Will claim area for read lock.
[Thread-safe]

Params:
- sa - Sector address, e.g. sector index.
- buffer - Pointer to buffer where function will safe data from disk.
- buff_size - Buffer size.
              Note: Should be greater or equals to sector size.
              Note 2: If buffer size lower then sector size, data will 
              shrink to this size without error.

Return 1 if io read success.
Return 0 if io error.
*/
int DSK_read_sector(sector_addr_t sa, unsigned char* buffer, int buff_size);

/*
Read sequence of sectors with offset from disk with io functions.
Note: Will claim area for read lock.
[Thread-safe]

Params:
- sa - Start sector address, e.g. sector index.
- buffer - Pointer to buffer where function will safe data from disk.
- buff_size - Buffer size.
              Note: Should be greater or equals to sector size.
              Note 2: If buffer size lower then sector size, data will 
              shrink to this size without error.
- sc - Sectors count.

Return 1 if io read success.
Return 0 if io error.
*/
int DSK_readoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* buffer, int buff_size, int sc);

/*
Write data from data buffer to sector on disk via disk io functions.
Note: Will claim area for write lock.
[Thread-safe]

Params:
- sa - Sector address, e.g. sector index.
- data - Pointer to buffer where placed data for write operation.
- data_size - Data size for write.
              Note: If data larger then sector size, function won't shrink it.

Return 1 if io write success.
Return 0 if io error.        
*/
int DSK_write_sector(sector_addr_t sa, const unsigned char* data, int data_size);

/*
Write data from data buffer to sectors sequence on disk via disk io functions.
Note: Will claim area for write lock.
[Thread-safe]

Params:
- sa - Sector address, e.g. sector index.
- data - Pointer to buffer where placed data for write operation.
- data_size - Data size for write.
              Note: If data larger then sector size, function won't shrink it.
- sc - Sectors count.

Return 1 if io write success.
Return 0 if io error.     
*/
int DSK_writeoff_sectors(sector_addr_t sa, sector_offset_t offset, const unsigned char* data, int data_size, int sc);

/*
Return sector size.
*/
int DSK_get_sector_size();

#endif