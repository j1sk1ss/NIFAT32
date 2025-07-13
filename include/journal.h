#ifndef JOURNAL_H_
#define JOURNAL_H_

#include "str.h"
#include "fat.h"
#include "disk.h"
#include "entry.h"
#include "fatinfo.h"
#include "logging.h"
#include "checksum.h"
#include "threading.h"

#define JOURNAL_MULTIPLIER 12345625789U
#define GET_JOURNALSECTOR(n, ts) (((((n) + 63) * JOURNAL_MULTIPLIER) >> 15) % ts)

typedef struct {
    unsigned char  file_name[11];
    unsigned char  attributes;
    cluster_addr_t cluster;
    unsigned int   file_size;
} __attribute__((packed)) squeezed_entry_t;

typedef struct {
    unsigned char    op;
    cluster_addr_t   ca;
    unsigned char    offset;
    squeezed_entry_t entry;
    checksum_t       checksum;
} __attribute__((packed)) journal_entry_t;

int squeeze_entry(directory_entry_t* src, squeezed_entry_t* dst);
int unsqueeze_entry(squeezed_entry_t* src, directory_entry_t* dst);

#endif