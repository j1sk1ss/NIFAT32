#include "../include/disk.h"

static disk_io_t _disk_io = {
    .read_sector  = NULL,
    .write_sector = NULL,
    .sector_size  = 512
};

static io_thread_t _io_guard = { .lock = NULL_LOCK };

static int _lock_area(sector_addr_t sa, int size, int ro) {
    if (!THR_require_write(&_io_guard.lock, get_thread_num())) return 0;
    int found = -1, locked = 0;
    for (int i = 0; i < IO_THREADS_MAX; i++) {
        if (_io_guard.areas[i].count == -1) {
            if (found == -1) found = i;
            continue;
        }

        sector_addr_t a_start = _io_guard.areas[i].start;
        sector_addr_t a_end   = a_start + _io_guard.areas[i].count;
        sector_addr_t b_start = sa;
        sector_addr_t b_end   = sa + size;
        if ((a_start < b_end) && (b_start < a_end)) {
            if (!ro) {
                locked = 1;
                break;
            } 
            else {
                if (!_io_guard.areas[i].ro) {
                    locked = 1;
                    break;
                }
            }
        }
    }

    if (!locked && found != -1) {
        _io_guard.areas[found].start = sa;
        _io_guard.areas[found].count = size;
        _io_guard.areas[found].ro    = ro;
        THR_release_write(&_io_guard.lock, get_thread_num());
        return 1;
    }

    THR_release_write(&_io_guard.lock, get_thread_num());
    return 0;
}

static int _unlock_area(sector_addr_t sa, int size) {
    if (THR_require_write(&_io_guard.lock, get_thread_num())) {
        for (int i = 0; i < IO_THREADS_MAX; i++) {
            if (_io_guard.areas[i].start == sa && _io_guard.areas[i].count == size) {
                _io_guard.areas[i].count = -1;
                _io_guard.areas[i].start = 0;
                _io_guard.areas[i].ro    = 0;
                break;
            }
        }

        THR_release_write(&_io_guard.lock, get_thread_num());
        return 1;
    }
    
    return 0;
}

int DSK_setup(
    int (*read)(sector_addr_t, sector_offset_t, unsigned char*, int), 
    int (*write)(sector_addr_t, sector_offset_t, const unsigned char*, int), 
    int sector_size
) {
    _disk_io.read_sector  = read;
    _disk_io.write_sector = write;
    _disk_io.sector_size  = sector_size;
    for (int i = 0; i < IO_THREADS_MAX; i++) {
        _io_guard.areas[i].count = -1;
        _io_guard.areas[i].start = 0;
        _io_guard.areas[i].ro    = 0;
    }

    return 1;
}

int DSK_read_sector(sector_addr_t sa, unsigned char* buffer, int buff_size) {
    print_debug("DSK_read_sector(sa=%u, size=%i)", sa, buff_size);
    if (_lock_area(sa, 1, READ_LOCK)) {
        int read_result = _disk_io.read_sector(sa, 0, buffer, buff_size);
        _unlock_area(sa, 1);
        return read_result;
    }
    else {
        print_error("Can't read-lock area sa=%u sc=1", sa);
    }

    return 0;
}

int DSK_readoff_sectors(sector_addr_t sa, sector_offset_t offset, unsigned char* buffer, int buff_size, int sc) {
    print_debug("DSK_readoff_sectors(sa=%u, offset=%u, size=%i, sc=%i)", sa, offset, buff_size, sc);
    if (_lock_area(sa, sc, READ_LOCK)) {
        int total_readden = 0;
        for (int i = 0; i < sc && buff_size > 0; i++) {
            if (offset > _disk_io.sector_size) offset -= _disk_io.sector_size;
            else {
                int read_size = buff_size > _disk_io.sector_size - offset ? _disk_io.sector_size - offset : buff_size;
                if (!_disk_io.read_sector(sa + i, offset, buffer + total_readden, read_size)) {
                    print_error("Disk read IO error! addr=%u, off=%u, read_size=%i", sa + i, offset, read_size);
                    _unlock_area(sa, sc);
                    return 0;
                }

                total_readden += read_size;
                buff_size -= read_size;
                offset = 0;
            }
        }

        _unlock_area(sa, sc);
        return 1;
    }
    else {
        print_error("Can't read-lock area sa=%u sc=%i", sa, sc);
    }

    return 0;
}

int DSK_write_sector(sector_addr_t sa, const unsigned char* data, int data_size) {
    print_debug("DSK_write_sector(sa=%u, size=%i)", sa, data_size);
    if (_lock_area(sa, 1, WRITE_LOCK)) {
        int write_result = _disk_io.write_sector(sa, 0, data, data_size);
        _unlock_area(sa, 1);
        return write_result;
    }
    else {
        print_error("Can't write-lock area sa=%u sc=1", sa);
    }

    return 0;
}

int DSK_writeoff_sectors(sector_addr_t sa, sector_offset_t offset, const unsigned char* data, int data_size, int sc) {
    print_debug("DSK_writeoff_sectors(sa=%u, offset=%u, size=%i, sc=%i)", sa, offset, data_size, sc);
    if (_lock_area(sa, sc, WRITE_LOCK)) {
        int total_written = 0;
        for (int i = 0; i < sc && data_size > 0; i++) {
            if (offset > _disk_io.sector_size) offset -= _disk_io.sector_size;
            else {
                int write_size = data_size > _disk_io.sector_size - offset ? _disk_io.sector_size - offset : data_size;
                if (!_disk_io.write_sector(sa + i, offset, data + total_written, write_size)) {
                    print_error("Disk write IO error! addr=%u, off=%u, write_size=%i", sa + i, offset, write_size);
                    _unlock_area(sa, sc);
                    return 0;
                }
                
                total_written += write_size;
                data_size -= write_size;
                offset = 0;
            }
        }

        _unlock_area(sa, sc);
        return 1;
    }
    else {
        print_error("Can't write-lock area sa=%u sc=%i", sa, sc);
    }

    return 0;
}

int DSK_copy_sectors(sector_addr_t src, sector_addr_t dst, int sc, unsigned char* buffer, int buff_size) {
    if (_lock_area(dst, sc, WRITE_LOCK)) {
        int copy_result = 0;
        for (int i = 0; i < sc; i++) {
            copy_result += _disk_io.read_sector(src, i, buffer, buff_size);
            copy_result += _disk_io.write_sector(dst, i, buffer, buff_size);
        }

        _unlock_area(dst, sc);
        return copy_result;
    }
    else {
        print_error("Can't read-lock area sa=%u sc=1", dst);
    }

    return 0;
}

int DSK_get_sector_size() {
    return _disk_io.sector_size;
}
