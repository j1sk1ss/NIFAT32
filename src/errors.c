#include <errors.h>

static errors_t _errors = {
    .first_error = 1,
    .current     = 1,
    .last_error  = 1
};

static int __write_error__(int index, error_code_t c, fat_data_t* fi, int copy_index) {
    decoded_t entry_buffer[sizeof(error_code_t)] = { 0 };
    unsigned int entry_offset = index * sizeof(entry_buffer);
    pack_memory((const byte_t*)&c, entry_buffer, sizeof(error_code_t));
    if (
        !DSK_writeoff_sectors(
            GET_ERRORSSECTOR(copy_index, fi->total_sectors), entry_offset, 
            (const unsigned char*)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not write error to errors sector!");
        return 0;
    }

    return 1;    
} 

static int _write_error(int index, error_code_t c, fat_data_t* fi) {
    print_debug("_write_error(index=%i, c=%i)", index, c);
    for (int i = 0; i < fi->errors_count; i++) __write_error__(index, c, fi, i);
    return 1;
}

static int __read_error__(int index, error_code_t* c, fat_data_t* fi, int copy_index) {
    decoded_t entry_buffer[sizeof(error_code_t)] = { 0 };
    unsigned int entry_offset = index * sizeof(entry_buffer);
    if (
        !DSK_readoff_sectors(
            GET_ERRORSSECTOR(copy_index, fi->total_sectors), entry_offset, 
            (unsigned char*)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not read error from errors sector!");
        return 0;
    }

    unpack_memory((const decoded_t*)entry_buffer, (byte_t*)c, sizeof(error_code_t));
    return 1;
} 

static int _read_error(int index, error_code_t* c, fat_data_t* fi) {
    print_debug("_read_error(index=%i)", index);
    int wrong    = -1;
    int val_freq = 0;
    int copy_pos = -1;
    error_code_t error_code = -1;
    for (int i = 0; i < fi->errors_count; i++) {
        error_code_t curr = 0;
        if (!__read_error__(index, &curr, fi, i)) continue;
        if (curr == error_code) val_freq++;
        else {
            val_freq--;
            wrong++;
        }

        if (val_freq < 0) {
            error_code = curr;
            copy_pos = i;
            val_freq = 0;
        }
    }

    if (copy_pos < 0) return 0;
    if (wrong > 0) {
        print_warn("Errors wrong value at index=%u. Fixing to val=%u...", index, error_code);
        _write_error(index, error_code, fi);
    }

    *c = error_code;
    return 1;
}

static int _dump_info(errors_t* i, fat_data_t* fi) {
    print_debug("_dump_info(fe=%i, le=%i)", _errors.first_error, _errors.last_error);
    unsigned int body = PACK_INFO_ENTRY(_errors.first_error, _errors.last_error);
    if (_write_error(0, body, fi)) return 1;
    print_error("Error during errors info dump!");
    return 0;
}

static int _load_info(errors_t* i, fat_data_t* fi) {
    unsigned int body = 0;
    if (_read_error(0, &body, fi)) {
        i->first_error = GET_FIRST_ERROR(body);
        i->last_error  = GET_LAST_ERROR(body);
        if (!i->first_error) i->first_error = 1;
        if (!i->last_error)  i->last_error  = 1;
        i->current = i->first_error;
        print_debug("errors_load, body=%i, fe=%i, le=%i", body, i->first_error, i->last_error);
        return 1;
    }

    print_error("Error during errors info load!");
    return 0;
}

int errors_setup(fat_data_t* fi) {
    return _load_info(&_errors, fi);
}

static inline unsigned int _next_ring_index(unsigned int idx, fat_data_t* fi) {
    idx++;
    if ((idx * sizeof(error_code_t)) >= fi->cluster_size) idx = 1;
    return idx;
}

int errors_register_error(error_code_t code, fat_data_t* fi) {
    print_debug("errors_register_error(code=%i)", code);
    if (!_write_error(_errors.current, code, fi)) {
        return 0;
    }

    unsigned int next = _next_ring_index(_errors.current, fi);
    if (next == _errors.first_error) {
        _errors.first_error = _next_ring_index(_errors.first_error, fi);
    }

    _errors.current    = next;
    _errors.last_error = _errors.current;
    return _dump_info(&_errors, fi);
}

error_code_t errors_last_error(fat_data_t* fi) {
    print_debug("errors_last_error()");
    if (_errors.first_error == _errors.last_error) {
        print_warn("No errors!");
        return -1;
    }

    error_code_t c;
    if (!_read_error(_errors.first_error, &c, fi)) {
        return -1;
    }

    _errors.first_error = _next_ring_index(_errors.first_error, fi);
    _dump_info(&_errors, fi);
    return c;
}
