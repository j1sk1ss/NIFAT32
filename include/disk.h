#ifndef DISK_H_
#define DISK_H_

#include <stddef.h>

typedef unsigned int sector_offset_t;
typedef unsigned int sector_addr_t;

typedef struct {
    int (*read_sector)(sector_addr_t sector, unsigned char* buffer, int buff_size);
    int (*write_sector)(sector_addr_t sector, unsigned char* data, int data_size);
    int sector_size;
} io_t;

int DSK_setup(
    int (*read)(sector_addr_t, unsigned char*, int), 
    int (*write)(sector_addr_t, unsigned char*, int), 
    int sector_size
);

int DSK_read_sector(sector_addr_t sa, unsigned char* buffer, int buff_size);
int DSK_read_sectors(sector_addr_t sa, unsigned char* buffer, int buff_size, int sc);
int DSK_readoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* buffer, int buff_size, int sc);
int DSK_write_sector(sector_addr_t sa, unsigned char* data, int data_size);
int DSK_write_sectors(sector_addr_t sa, unsigned char* data, int data_size, int sc);
int DSK_writeoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* data, int data_size, int sc);

#endif