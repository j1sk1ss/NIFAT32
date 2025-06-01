#include "../include/disk.h"

static io_t disk_io = {
    .read_sector  = NULL,
    .write_sector = NULL,
    .sector_size  = 512
};

int DSK_setup(
    int (*read)(sector_addr_t, unsigned char*, int), 
    int (*write)(sector_addr_t, unsigned char*, int), 
    int sector_size
) {
    disk_io.read_sector  = read;
    disk_io.write_sector = write;
    disk_io.sector_size  = sector_size;
    return 1;
}

int DSK_read_sector(sector_addr_t sa, unsigned char* buffer, int buff_size) {
    return disk_io.read_sector(sa, buffer, buff_size);
}

int DSK_read_sectors(sector_addr_t sa, unsigned char* buffer, int buff_size, int sc) {
    for (int i = 0; i < sc && buff_size > 0; i++) {
        int read_size = buff_size > disk_io.sector_size ? disk_io.sector_size : buff_size;
        if (!disk_io.read_sector(sa + i, buffer + (i * disk_io.sector_size), read_size)) {
            return 0;
        }

        buff_size -= read_size;
    }

    return 1;
}

int DSK_readoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* buffer, int buff_size, int sc) {
    return 1;
}

int DSK_write_sector(sector_addr_t sa, unsigned char* data, int data_size) {
    return disk_io.write_sector(sa, data, data_size);
}

int DSK_write_sectors(sector_addr_t sa, unsigned char* data, int data_size, int sc) {
    for (int i = 0; i < sc && data_size > 0; i++) {
        int write_size = data_size > disk_io.sector_size ? disk_io.sector_size : data_size;
        if (!disk_io.read_sector(sa + i, data + (i * disk_io.sector_size), write_size)) {
            return 0;
        }
        
        data_size -= write_size;
    }

    return 1;
}

int DSK_writeoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* data, int data_size, int sc) {
    return 1;
}
