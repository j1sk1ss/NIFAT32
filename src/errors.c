#include <errors.h>

static int __write_error__(int index, error_code_t c, fat_data_t* fi, int journal) {
    decoded_t entry_buffer[sizeof(error_code_t)] = { 0 };
    unsigned int entry_offset = index * sizeof(entry_buffer);
    pack_memory((const byte_t*)c, entry_buffer, sizeof(error_code_t));
    if (
        !DSK_writeoff_sectors(
            GET_ERRORSSECTOR(journal, fi->total_sectors), entry_offset, 
            (const unsigned char*)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not write error to errors sector!");
        return 0;
    }

    return 1;    
} 

static int _write_error(int index, error_code_t c, fat_data_t* fi) {
    print_debug("_write_error(index=%i)", index);
    for (int i = 0; i < fi->journals_count; i++) __write_journal__(index, c, fi, i);
    return 1;
}

static int __read_error__(int index, error_code_t* c, fat_data_t* fi, int journal) {
    decoded_t entry_buffer[sizeof(error_code_t)] = { 0 };
    unsigned int entry_offset = index * sizeof(entry_buffer);
    if (
        !DSK_readoff_sectors(
            GET_ERRORSSECTOR(journal, fi->total_sectors), entry_offset, 
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
    print_debug("_read_journal(index=%i)", index);
    int wrong    = -1;
    int val_freq = 0;
    int copy_pos = -1;
    error_code_t error_code = -1;
    for (int i = 0; i < fi->journals_count; i++) {
        error_code_t curr;
        __read_error__(index, &curr, fi, i);
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

    return 1;
}

int ERR_register_error(error_code_t code) {
    return 1;
}

int ERR_last_error(error_t* b) {
    return 1;
}
