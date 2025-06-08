#ifndef CTABLE_H_
#define CTABLE_H_

#include <stddef.h>
#include "mm.h"
#include "entry.h"
#include "threading.h"

#define CONTENT_TABLE_SIZE 50

/* Content Index - ci */
typedef int ci_t;

typedef struct fat_file {
	char name[8];
	char extension[4];
} file_t;

typedef struct fat_directory {
	char name[11];
} directory_t;

typedef enum {
	CONTENT_TYPE_FILE,
	CONTENT_TYPE_DIRECTORY
} content_type_t;

typedef struct {
	union {
		directory_t* directory;
		file_t*      file;
	};
	
	cluster_addr_t parent_cluster;
	cluster_addr_t data_cluster;
	directory_entry_t meta;
	content_type_t content_type;
	lock_t lock;
} content_t;

#define NOT_PRESENT	0x00
#define STAT_FILE	0x01
#define STAT_DIR	0x02

typedef struct {
	char full_name[12];
	char file_name[8];
	char file_extension[4];
	int  type;
	int  size;
	unsigned short creation_time;
	unsigned short creation_date;
	unsigned short last_accessed;
	unsigned short last_modification_time;
	unsigned short last_modification_date;
} cinfo_t;

int ctable_init();
int unload_content_system(content_t* content);
content_t* create_content();
directory_t* create_directory();
file_t* create_file();
ci_t add_content2table(content_t* content);
int remove_content_from_table(ci_t ci);
content_t* get_content_from_table(const ci_t ci);

#endif