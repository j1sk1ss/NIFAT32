/*
License:
    MIT License

    Copyright (c) 2025 Nikolay 

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to deal
    in the Software without restriction, including without limitation the rights
    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
    copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in all
    copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
    SOFTWARE.

Description:
    This file contains main tools for working with disk / flash etc.

Dependencies:
    - logging.h - Logging tools.
    - threading.h - Locks for IO operations.
*/

#ifndef DISK_H_
#define DISK_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <null.h>
#include <logging.h>
#include <threading.h>

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
} disk_io_t;

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
Copy sector to destination sector.
Note: copy buffer should be greater or equals to sector size.
Params:
- src - Source start sector.
- dst - Destination start sector.
- sc - Sector count.
- buffer - Copy buffer.
- buff_size - Copy buffer size.

Return count of readden and written bytes.
*/
int DSK_copy_sectors(sector_addr_t src, sector_addr_t dst, int sc, unsigned char* buffer, int buff_size);

/*
Return sector size.
*/
int DSK_get_sector_size();

#ifdef __cplusplus
}
#endif
#endif