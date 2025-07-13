#include "../include/journal.h"

static int __write_journal__(int index, journal_entry_t* entry, fat_data_t* fi, int journal) {
    cluster_offset_t entry_offset = index * sizeof(journal_entry_t);
    decoded_t entry_buffer[sizeof(journal_entry_t)] = { 0 };
    pack_memory((const byte_t*)entry, entry_buffer, sizeof(journal_entry_t));
    if (
        !DSK_writeoff_sectors(
            GET_JOURNALSECTOR(journal, fi->total_sectors), entry_offset % fi->bytes_per_sector, 
            (const unsigned char*)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not write journal to journal sector!");
        return 0;
    }

    return 1;    
} 

static int _write_journal(int index, journal_entry_t* entry, fat_data_t* fi) {
    for (int i = 0; i < fi->journals_count; i++) __write_journal__(index, entry, fi, i);
    return 1;
}

static int __read_journal__(int index, journal_entry_t* entry, fat_data_t* fi, int journal) {
    cluster_offset_t entry_offset = index * sizeof(journal_entry_t);
    decoded_t entry_buffer[sizeof(journal_entry_t)] = { 0 };
    if (
        !DSK_readoff_sectors(
            GET_JOURNALSECTOR(journal, fi->total_sectors), entry_offset % fi->bytes_per_sector, 
            (const unsigned char*)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not read journal from journal sector!");
        return 0;
    }

    unpack_memory(entry_buffer, (const byte_t*)entry, sizeof(journal_entry_t));
    return 1;    
} 

static int _read_journal(int index, journal_entry_t* entry, fat_data_t* fi) {
    // read and choose correct (by checksum) entry in journal
    return 1;
}

static int _squeeze_entry(directory_entry_t* src, squeezed_entry_t* dst) {
    dst->attributes = src->attributes;
    dst->cluster    = src->cluster;
    dst->file_size  = src->file_size;
    str_memcpy(dst->file_name, src->file_name, sizeof(src->file_name));
    return 1;
}

static int _unsqueeze_entry(squeezed_entry_t* src, directory_entry_t* dst) {
    dst->attributes = src->attributes;
    dst->cluster    = src->cluster;
    dst->file_size  = src->file_size;
    str_memcpy(dst->file_name, src->file_name, sizeof(src->file_name));
    dst->name_hash = crc32(0, (const_buffer_t)dst->file_name, 11);
    dst->checksum  = crc32(0, (const_buffer_t)dst, sizeof(directory_entry_t));
    return 1;
}

static unsigned char _journal_index = 0;

int restore_from_journal() {
    // iterate from entire journal and solve all unsolved operations
    return 1;
}

int journal_add_operation(unsigned char op, cluster_addr_t ca, int offset, directory_entry_t* entry, fat_data_t* fi) {
    journal_entry_t j_entry = { .ca = ca, .offset = offset, .op = op };
    _squeeze_entry(entry, &j_entry.entry);
    int entry_index = _journal_index++ % (fi->cluster_size / (sizeof(journal_entry_t) * sizeof(encoded_t)));
    _write_journal(entry_index, &j_entry, fi);
    return entry_index;    
}

int journal_solve_operation(int index, fat_data_t* fi) {
    journal_entry_t solved = { .op = 0x00 };
    return _write_journal(index, &solved, fi);
}