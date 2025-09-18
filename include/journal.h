#ifndef JOURNAL_H_
#define JOURNAL_H_
#ifdef __cplusplus
extern "C" {
#endif

#include <str.h>
#include <fat.h>
#include <disk.h>
#include <cluster.h>
#include <fatinfo.h>
#include <logging.h>
#include <checksum.h>
#include <threading.h>

#define JOURNAL_MULTIPLIER 12345625789U
#define GET_JOURNALSECTOR(n, ts) (((((n) + 63) * JOURNAL_MULTIPLIER) >> 15) % (ts - 128))

typedef struct {
    unsigned char  file_name[11];
    checksum_t     name_hash;
    unsigned char  attributes;
    cluster_addr_t rca; // root cluster
    cluster_addr_t dca; // data cluster
    unsigned int   file_size;
    checksum_t     checksum;
} __attribute__((packed)) unsqueezed_entry_t;

typedef struct {
    unsigned char  file_name[11];
    unsigned char  attributes;
    cluster_addr_t rca;
    cluster_addr_t dca;
    unsigned int   file_size;
} __attribute__((packed)) squeezed_entry_t;

#define NO_OP   0x00
#define ADD_OP  0x01
#define EDIT_OP 0x02
#define DEL_OP  0x03

typedef struct {
    unsigned char    op;
    cluster_addr_t   ca;
    unsigned char    offset;
    squeezed_entry_t entry;
    checksum_t       checksum;
} __attribute__((packed)) journal_entry_t;

/*
Restore all saved actions from journal. Will restore cluster_size / sizeof(journal_entry_t) actions.
Params:
- fi - FS data.

Return 1 if restore success.
Return 0 if restore failed.
*/
int restore_from_journal(fat_data_t* fi);

/*
Add entry operation to journal.
Params:
- op - Operation type (ADD_OP, DEL_OP, EDIT_OP, NO_OP (reserved)).
- ca - Cluster where entry placed.
- offset - Offset of entry (For EDIT_OP).
- entry - Result entry. What we expect to see on disk.
- fi - FS data.

Return journal entry index.
Return 0 if something goes wrong.
*/
int journal_add_operation(unsigned char op, cluster_addr_t ca, int offset, unsqueezed_entry_t* entry, fat_data_t* fi);

/*
Mark journal entry as solved.
Params:
- index - Journal entry index.
- fi - FS data.

Return 1 if mark success.
Return 0 if something goes wrong.
*/
int journal_solve_operation(int index, fat_data_t* fi);

#ifdef __cplusplus
}
#endif
#endif