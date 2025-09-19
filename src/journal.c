#include <journal.h>

#ifndef NIFAT32_RO
static int __write_journal__(int index, journal_entry_t* entry, fat_data_t* fi, int journal) {
    decoded_t entry_buffer[sizeof(journal_entry_t)] = { 0 };
    cluster_offset_t entry_offset = index * sizeof(entry_buffer);
    pack_memory((const byte_t*)entry, entry_buffer, sizeof(journal_entry_t));
    if (
        !DSK_writeoff_sectors(
            GET_JOURNALSECTOR(journal, fi->total_sectors), entry_offset, (const_buffer_t)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not write journal to journal sector!");
        errors_register_error(JOURNAL_WRITE_ERROR, fi);
        return 0;
    }

    return 1;    
} 

static int _write_journal(int index, journal_entry_t* entry, fat_data_t* fi) {
    print_debug("_write_journal(index=%i)", index);
    for (int i = 0; i < fi->journals_count; i++) __write_journal__(index, entry, fi, i);
    return 1;
}

static int __read_journal__(int index, journal_entry_t* entry, fat_data_t* fi, int journal) {
    decoded_t entry_buffer[sizeof(journal_entry_t)] = { 0 };
    cluster_offset_t entry_offset = index * sizeof(entry_buffer);
    if (
        !DSK_readoff_sectors(
            GET_JOURNALSECTOR(journal, fi->total_sectors), entry_offset, (buffer_t)entry_buffer, sizeof(entry_buffer), 1
        )
    ) {
        print_error("Could not read journal from journal sector!");
        errors_register_error(JOURNAL_READ_ERROR, fi);
        return 0;
    }

    unpack_memory((const decoded_t*)entry_buffer, (byte_t*)entry, sizeof(journal_entry_t));
    return 1;    
} 

static int _read_journal(int index, journal_entry_t* entry, fat_data_t* fi) {
    print_debug("_read_journal(index=%i)", index);
    int wrong    = -1;
    int val_freq = 0;
    int copy_pos = -1;
    checksum_t journal_checksum = -1;
    for (int i = 0; i < fi->journals_count; i++) {
        journal_entry_t curr;
        __read_journal__(index, &curr, fi, i);
        if (curr.checksum == journal_checksum) val_freq++;
        else {
            val_freq--;
            wrong++;
        }

        if (val_freq < 0) {
            journal_checksum = curr.checksum;
            copy_pos = i;
            val_freq = 0;
        }
    }

    if (copy_pos < 0) return 0;
    __read_journal__(copy_pos, entry, fi, index);
    if (wrong > 0) {
        print_warn("Journal wrong value at index=%u. Fixing to val=%u...", index, journal_checksum);
        _write_journal(index, entry, fi);
    }

    return 1;
}

static int _squeeze_entry(unsqueezed_entry_t* src, squeezed_entry_t* dst) {
    dst->attributes = src->attributes;
    dst->rca        = src->rca;
    dst->dca        = src->dca;
    dst->file_size  = src->file_size;
    str_memcpy(dst->file_name, src->file_name, sizeof(src->file_name));
    return 1;
}

static int _unsqueeze_entry(squeezed_entry_t* src, unsqueezed_entry_t* dst) {
    dst->attributes = src->attributes;
    dst->rca        = src->rca;
    dst->dca        = src->dca;
    dst->file_size  = src->file_size;
    str_memcpy(dst->file_name, src->file_name, sizeof(src->file_name));
    dst->name_hash = murmur3_x86_32((const_buffer_t)dst->file_name, sizeof(dst->file_name), 0);
    dst->checksum  = murmur3_x86_32((const_buffer_t)dst, sizeof(unsqueezed_entry_t), 0);
    return 1;
}
#endif

static unsigned char _journal_index = 0;

int restore_from_journal(fat_data_t* fi) {
#ifndef NIFAT32_RO
    if (!fi->journals_count) return 0;
    for (
        _journal_index = 0; 
        _journal_index < (fi->cluster_size / (sizeof(journal_entry_t) * sizeof(encoded_t))); 
        _journal_index++
    ) {
        journal_entry_t entry;
        _read_journal(_journal_index, &entry, fi);
        if (entry.op == NO_OP) continue;

        unsqueezed_entry_t restored;
        _unsqueeze_entry(&entry.entry, &restored);
        print_warn("Found unsolved journal entry! op=%i, name=%s, ca=%u, offset=%i", entry.op, restored.file_name, entry.ca, entry.offset);
        
        switch (entry.op) {
            case DEL_OP: {
                decoded_t entry_buffer[sizeof(unsqueezed_entry_t)] = { 0 };
                writeoff_cluster(entry.ca, entry.offset * sizeof(entry_buffer), (const_buffer_t)entry_buffer, sizeof(entry_buffer), fi);
                break;
            }
            case ADD_OP:
            case EDIT_OP: {
                decoded_t entry_buffer[sizeof(unsqueezed_entry_t)] = { 0 };
                pack_memory((const byte_t*)&restored, entry_buffer, sizeof(unsqueezed_entry_t));
                writeoff_cluster(entry.ca, entry.offset * sizeof(entry_buffer), (const_buffer_t)entry_buffer, sizeof(entry_buffer), fi);
                break;
            }
            default: break;
        }

        entry.op = NO_OP;
        _write_journal(_journal_index, &entry, fi);
    }

    _journal_index = 0;
    return 1;
#else
    return 1;
#endif
}

lock_t _journal_lock = NULL_LOCK;

int journal_add_operation(unsigned char op, cluster_addr_t ca, int offset, unsqueezed_entry_t* entry, fat_data_t* fi) {
#ifndef NIFAT32_RO
    if (!fi->journals_count) return 0;
    print_debug("journal_add_operation(op=%u, ca=%u, offset=%i)", op, ca, offset);
    
    int delay = 100;
    int entry_index = 0;
    while (delay-- > 0) {
        if (THR_require_write(&_journal_lock, get_thread_num())) {
            entry_index = _journal_index++ % (fi->cluster_size / (sizeof(journal_entry_t) * sizeof(encoded_t)));
            journal_entry_t curr;
            _read_journal(entry_index, &curr, fi);
            THR_release_write(&_journal_lock, get_thread_num());
            if (curr.op == NO_OP) break;
            else print_warn("Journal entry occupied. op=%i", curr.op);
        }
    }
    
    if (delay <= 0) return -1;

    journal_entry_t j_entry = { .ca = ca, .offset = offset, .op = op };
    _squeeze_entry(entry, &j_entry.entry);
    _write_journal(entry_index, &j_entry, fi);
    
    return entry_index;
#else
    return -1;
#endif
}

int journal_solve_operation(int index, fat_data_t* fi) {
#ifndef NIFAT32_RO
    if (!fi->journals_count || index < 0) return 0;
    print_debug("journal_solve_operation(index=%i)", index);
    journal_entry_t solved = { 0x00 };
    return _write_journal(index, &solved, fi);
#else
    return 1;
#endif
}