#include "nifat32formatter.h"

static int _write_bs(int, uint32_t, uint32_t);
static int _write_fats(int, fat_table_t, uint32_t, uint32_t);
static int _initialize_fat(fat_table_t, uint32_t, uint32_t);
static int _write_root_directory(int, uint32_t, uint32_t);
static int _create_directory(int, fat_table_t, uint32_t*, uint32_t, uint32_t, int*);
static int _copy_files_to_fs(int, const char*, fat_table_t, uint32_t, uint32_t, uint32_t);
static int _copy_file(int, FILE*, fat_table_t, uint32_t*, size_t*, uint32_t, uint32_t, int*);
static void _to_83_name(const char*, char*);
static uint32_t _calculate_fat_size(uint32_t);

static opt_t opt = { 
    .spc    = SECTORS_PER_CLUSTER,
    .v_size = DEFAULT_VOLUME_SIZE,
    .fc     = FAT_COUNT,
    .bsbc   = BS_BACKUPS
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <args> -o <save_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    process_input(argc, argv, &opt);
    if (!opt.save_path) {
        fprintf(stderr, "Error: Output path not specified\n");
        return EXIT_FAILURE;
    }

    int fd = open(opt.save_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error creating output file");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, opt.v_size)) {
        perror("Error setting volume size");
        close(fd);
        return EXIT_FAILURE;
    }

    uint32_t total_sectors  = ((uint32_t)opt.v_size * 1024u * 1024u) / BYTES_PER_SECTOR;
    uint32_t fat_size       = _calculate_fat_size(total_sectors);
    uint32_t total_clusters = (total_sectors - RESERVED_SECTORS - opt.fc * fat_size) / opt.spc;
    uint32_t data_start     = (RESERVED_SECTORS + opt.fc * fat_size) * BYTES_PER_SECTOR;

    fat_table_t fat_table = (fat_table_t)malloc(total_clusters * sizeof(uint32_t));
    if (!fat_table) {
        fprintf(stderr, "Memory allocation error\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (!_write_bs(fd, total_sectors, fat_size)) {
        fprintf(stderr, "Error writing boot sector\n");
        close(fd);
        return EXIT_FAILURE;
    }

    _initialize_fat(fat_table, total_sectors, total_clusters);
    if (!_write_fats(fd, fat_table, fat_size, total_clusters)) {
        fprintf(stderr, "Error writing FAT tables\n");
        free(fat_table);
        close(fd);
        return EXIT_FAILURE;
    }

    if (!_write_root_directory(fd, data_start, fat_size)) {
        fprintf(stderr, "Error initializing root directory\n");
        free(fat_table);
        close(fd);
        return EXIT_FAILURE;
    }

    if (opt.source_path) {
        if (!_copy_files_to_fs(fd, opt.source_path, fat_table, fat_size, data_start, total_clusters)) {
            fprintf(stderr, "Error copying files\n");
            free(fat_table);
            close(fd);
            return EXIT_FAILURE;
        }

        printf("Created volume with files from '%s' at '%s'\n", opt.source_path, opt.save_path);
    } 
    else {
        printf("Created empty FAT32 volume at '%s'\n", opt.save_path);
    }

    if (!_write_fats(fd, fat_table, fat_size, total_clusters)) {
        fprintf(stderr, "Error updating FAT tables\n");
    }

    free(fat_table);
    close(fd);
    return EXIT_SUCCESS;
}

/* Sector count for FAT */
static uint32_t _calculate_fat_size(uint32_t total_sectors) {
    uint32_t data_sectors = total_sectors - RESERVED_SECTORS;
    
    uint32_t fat_size = 0;
    uint32_t prev_fat_size = 0;
    do {
        prev_fat_size = fat_size;
        uint32_t clusters = (data_sectors - opt.fc * fat_size) / opt.spc;
        fat_size = (clusters * sizeof(uint32_t) + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR;
    } while (fat_size != prev_fat_size);

    return fat_size;
}

#pragma region [SEU]

    static encoded_t _encode_hamming_15_11(decoded_t data) {
        encoded_t encoded = 0;
        encoded = SET_BIT(encoded, 2, GET_BIT(data, 0));
        encoded = SET_BIT(encoded, 4, GET_BIT(data, 1));
        encoded = SET_BIT(encoded, 5, GET_BIT(data, 2));
        encoded = SET_BIT(encoded, 6, GET_BIT(data, 3));
        encoded = SET_BIT(encoded, 8, GET_BIT(data, 4));
        encoded = SET_BIT(encoded, 9, GET_BIT(data, 5));
        encoded = SET_BIT(encoded, 10, GET_BIT(data, 6));
        encoded = SET_BIT(encoded, 11, GET_BIT(data, 7));
        encoded = SET_BIT(encoded, 12, GET_BIT(data, 8));
        encoded = SET_BIT(encoded, 13, GET_BIT(data, 9));
        encoded = SET_BIT(encoded, 14, GET_BIT(data, 10));

        byte_t p1 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 4) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 8) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 14);
        byte_t p2 = GET_BIT(encoded, 2) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
        byte_t p4 = GET_BIT(encoded, 4) ^ GET_BIT(encoded, 5) ^ GET_BIT(encoded, 6) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);
        byte_t p8 = GET_BIT(encoded, 8) ^ GET_BIT(encoded, 9) ^ GET_BIT(encoded, 10) ^ GET_BIT(encoded, 11) ^ GET_BIT(encoded, 12) ^ GET_BIT(encoded, 13) ^ GET_BIT(encoded, 14);

        encoded = SET_BIT(encoded, 0, p1);
        encoded = SET_BIT(encoded, 1, p2);
        encoded = SET_BIT(encoded, 3, p4);
        encoded = SET_BIT(encoded, 7, p8);
        return encoded;
    }

    static int _set_byte(unsigned short* ptr, int offset, unsigned char byte) {
        ptr[offset] = _encode_hamming_15_11((unsigned short)byte);
        return 1;
    }

    static void* _pack_memory(unsigned char* src, unsigned short* dst, int len) {
        for (int i = 0; i < len; i++) _set_byte(dst, i, src[i]);
        return (void*)dst;
    }

    static const unsigned int _crc32_table[] = {
        0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9,
        0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
        0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61,
        0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
        0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9,
        0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
        0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011,
        0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
        0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039,
        0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
        0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81,
        0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
        0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49,
        0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
        0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1,
        0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
        0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae,
        0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
        0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16,
        0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
        0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde,
        0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
        0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066,
        0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
        0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e,
        0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
        0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6,
        0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
        0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e,
        0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
        0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686,
        0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
        0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637,
        0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
        0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f,
        0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
        0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47,
        0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
        0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff,
        0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
        0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7,
        0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
        0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f,
        0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
        0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7,
        0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
        0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f,
        0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
        0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640,
        0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
        0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8,
        0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
        0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30,
        0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
        0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088,
        0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
        0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0,
        0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
        0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18,
        0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
        0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0,
        0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
        0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668,
        0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
    };

    static unsigned int _crc32(unsigned int init, const unsigned char* buf, int len) {
        unsigned int crc = init;
        while (len--) {
            crc = (crc << 8) ^ _crc32_table[((crc >> 24) ^ *buf) & 255];
            buf++;
        }

        return crc;
    }

#pragma endregion

static int _write_bs(int fd, uint32_t total_sectors, uint32_t fat_size) {
    fat_BS_t bs = { 0 };
    bs.bootjmp[0] = 0xEB;
    bs.bootjmp[1] = 0x58;
    bs.bootjmp[2] = 0x90;
    memcpy(bs.oem_name, "MSWIN4.1", 8);
    bs.bytes_per_sector      = BYTES_PER_SECTOR;
    bs.sectors_per_cluster   = opt.spc;
    bs.reserved_sector_count = RESERVED_SECTORS;
    bs.table_count           = opt.fc;
    bs.root_entry_count      = 0;
    bs.total_sectors_16      = 0;
    bs.media_type            = 0xF8;
    bs.sectors_per_track     = 63;
    bs.head_side_count       = 255;
    bs.hidden_sector_count   = 0;
    bs.total_sectors_32      = total_sectors;

    fat_extBS_32_t ext = { 0 };
    ext.boot_signature   = 0x5A;
    ext.table_size_32    = fat_size;
    ext.root_cluster     = ROOT_DIR_CLUSTER;
    ext.fat_info         = 1;
    ext.backup_BS_sector = 6;
    ext.drive_number     = 0x80;
    ext.boot_signature   = 0x29;
    ext.volume_id        = 0x12345678;
    memcpy(ext.volume_label, "ROOT_LABEL ", 11);
    memcpy(ext.fat_type_label, "NIFAT32 ", 8);
    ext.checksum = 0;
    ext.checksum = _crc32(0, (uint8_t*)&ext, sizeof(ext));

    memcpy(bs.extended_section, &ext, sizeof(ext));
    bs.checksum = 0;
    bs.checksum = _crc32(0, (uint8_t*)&bs, sizeof(bs));
    fprintf(stdout, "Bootstruct checksum: %u, ext. section: %u\n", bs.checksum, ext.checksum);

    int encoded_size = sizeof(encoded_t) * sizeof(fat_BS_t);
    encoded_t* encoded_bs = (encoded_t*)malloc(encoded_size);
    if (!encoded_bs) return 0;

    uint8_t padding[BYTES_PER_SECTOR] = { 0 };
    for (int i = 0; i < opt.bsbc; i++) {
        unsigned int ca = GET_BOOTSECTOR(i, total_sectors);
        _pack_memory((unsigned char*)&bs, encoded_bs, sizeof(fat_BS_t));
        if (pwrite(fd, padding, sizeof(padding), ca * BYTES_PER_SECTOR) != sizeof(padding)) return 0;
        if (pwrite(fd, encoded_bs, encoded_size, ca * BYTES_PER_SECTOR) != encoded_size) return 0;
        fprintf(stdout, "[i=%i] encoded bootsector has been written at ca=%u/%u!\n", i, ca, total_sectors);
    }

    free(encoded_bs);
    return 1;
}

/* Generate first clusters, reserve clusters for bootstructs */
static int _initialize_fat(uint32_t* fat_table, uint32_t total_sectors, uint32_t total_clusters) {
    fat_table[0] = FAT_ENTRY_RESERVED | (0xF8 << 24);
    fat_table[1] = FAT_ENTRY_END;
    fat_table[2] = FAT_ENTRY_END;

    uint32_t cluster_for_bs = 0;
    uint32_t cluster_for_backup;

    fat_table[cluster_for_bs] = FAT_ENTRY_RESERVED | (0xF8 << 24);
    for (int i = 0; i < opt.bsbc; i++) {
        unsigned int sector = GET_BOOTSECTOR(i, total_sectors);
        cluster_for_backup = sector / opt.spc;
        if (cluster_for_backup < total_clusters) {
            fat_table[cluster_for_backup] = FAT_ENTRY_RESERVED | (0xF8 << 24);
        }
    }

    return 1;
}

static int _write_fats(int fd, fat_table_t fat_table, uint32_t fat_size, uint32_t total_clusters) {
    off_t offset = RESERVED_SECTORS * BYTES_PER_SECTOR;
    uint32_t fat_bytes  = fat_size * BYTES_PER_SECTOR;
    for (int i = 0; i < opt.fc; i++) {
        if (pwrite(fd, fat_table, fat_bytes, offset) != fat_bytes) return 0;
        offset += fat_bytes;
    }

    return 1;
}

static int _write_root_directory(int fd, uint32_t data_start, uint32_t fat_size) {
    uint8_t empty_dir[CLUSTER_SIZE] = { 0 };
    off_t root_dir_offset = data_start + (ROOT_DIR_CLUSTER - 2) * CLUSTER_SIZE;
    if (pwrite(fd, empty_dir, CLUSTER_SIZE, root_dir_offset) != CLUSTER_SIZE) return 0;
    return 1;
}

static int _copy_files_to_fs(
    int fd, const char* folder_path, uint32_t* fat_table, uint32_t fat_size, uint32_t data_start, uint32_t total_clusters
) {
    DIR* dir = opendir(folder_path);
    if (!dir) {
        perror("Error opening source directory");
        return 0;
    }

    off_t root_dir_offset = data_start + (ROOT_DIR_CLUSTER - 2) * CLUSTER_SIZE;
    directory_entry_t* root_dir = calloc(CLUSTER_SIZE, 1);
    if (!root_dir) {
        closedir(dir);
        return 0;
    }

    if (lseek(fd, root_dir_offset, SEEK_SET) != root_dir_offset) {
        free(root_dir);
        closedir(dir);
        return 0;
    }

    if (read(fd, root_dir, CLUSTER_SIZE) != CLUSTER_SIZE) {
        free(root_dir);
        closedir(dir);
        return 0;
    }

    int entry_count = 0;
    struct dirent* entry;
    int last_alloc = 3;

    while ((entry = readdir(dir))) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) 
            continue;

        char full_path[1024] = { 0 };
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, entry->d_name);

        struct stat st;
        if (stat(full_path, &st) != 0) 
            continue;

        if (S_ISDIR(st.st_mode)) {
            uint32_t start_cluster = 0;
            if (!_create_directory(fd, fat_table, &start_cluster, data_start, total_clusters, &last_alloc)) {
                fprintf(stderr, "Failed to create directory: %s\n", full_path);
                continue;
            }

            _to_83_name(entry->d_name, (char*)root_dir[entry_count].file_name);
            root_dir[entry_count].attributes = 0x10;
            root_dir[entry_count].cluster    = start_cluster;
            root_dir[entry_count].file_size  = 0;
            root_dir[entry_count].checksum   = _crc32(0, (uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]));
            fprintf(stdout, "%s checksum: %u\n", entry->d_name, root_dir[entry_count].checksum);
            entry_count++;
        }
        else if (S_ISREG(st.st_mode)) {
            FILE* src_file = fopen(full_path, "rb");
            if (!src_file) {
                perror("Error opening source file");
                continue;
            }

            uint32_t start_cluster = 0;
            size_t file_size = 0;
            if (!_copy_file(fd, src_file, fat_table, &start_cluster, &file_size, 
                         data_start, total_clusters, &last_alloc)) {
                fclose(src_file);
                continue;
            }

            fclose(src_file);

            _to_83_name(entry->d_name, (char*)root_dir[entry_count].file_name);
            root_dir[entry_count].attributes = 0x20;
            root_dir[entry_count].cluster    = start_cluster;
            root_dir[entry_count].file_size  = file_size;
            root_dir[entry_count].checksum   = 0;
            root_dir[entry_count].checksum   = _crc32(0, (uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]));
            fprintf(stdout, "%s checksum: %u\n", entry->d_name, root_dir[entry_count].checksum);
            entry_count++;
        }
    }

    root_dir[entry_count].file_name[0] = ENTRY_END;
    root_dir[entry_count].attributes = 0;
    root_dir[entry_count].cluster    = 0;
    root_dir[entry_count].file_size  = 0;
    root_dir[entry_count].checksum   = 0;
    root_dir[entry_count].checksum   = _crc32(0, (uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]));
    fprintf(stdout, "END checksum: %u\n", root_dir[entry_count].checksum);
    entry_count++;

    closedir(dir);

    if (lseek(fd, root_dir_offset, SEEK_SET) != root_dir_offset) {
        free(root_dir);
        return 0;
    }
    
    unsigned short encoded_root_dir[CLUSTER_SIZE] = { 0 };
    _pack_memory((unsigned char*)root_dir, encoded_root_dir, CLUSTER_SIZE);
    if (write(fd, encoded_root_dir, CLUSTER_SIZE) != CLUSTER_SIZE) {
        free(root_dir);
        return 0;
    }

    free(root_dir);
    return 1;
}

static int _create_directory(
    int fd, uint32_t* fat_table, uint32_t* start_cluster, uint32_t data_start, uint32_t total_clusters, int* last_alloc
) {
    for (int i = *last_alloc; i < total_clusters; i++) {
        if (fat_table[i] == FAT_ENTRY_FREE) {
            fat_table[i] = FAT_ENTRY_END;
            *last_alloc = i + 1;
            *start_cluster = i;

            directory_entry_t entries[3] = { 0 };
            _to_83_name(".", (char*)entries[0].file_name);
            entries[0].attributes = 0x10;
            entries[0].cluster    = i;
            entries[0].checksum   = 0;
            entries[0].checksum   = _crc32(0, (uint8_t*)&entries[0], sizeof(entries[0]));
            fprintf(stdout, ". checksum: %u\n", entries[0].checksum);

            _to_83_name("..", (char*)entries[1].file_name);
            entries[1].attributes = 0x10;
            entries[1].cluster    = 0;
            entries[1].checksum   = 0;
            entries[1].checksum   = _crc32(0, (uint8_t*)&entries[1], sizeof(entries[1]));
            fprintf(stdout, ".. checksum: %u\n", entries[1].checksum);

            entries[2].file_name[0] = ENTRY_END;
            entries[2].attributes = 0x10;
            entries[2].cluster    = 0;
            entries[2].checksum   = 0;
            entries[2].checksum   = _crc32(0, (uint8_t*)&entries[2], sizeof(entries[2]));
            fprintf(stdout, "END checksum: %u\n", entries[1].checksum);

            off_t cluster_offset = data_start + (i - ROOT_DIR_CLUSTER) * CLUSTER_SIZE;
            if (pwrite(fd, entries, sizeof(entries), cluster_offset) != sizeof(entries)) return 0;
            return 1;
        }
    }

    return 0;
}

static int _copy_file(
    int fd, FILE* src_file, uint32_t* fat_table, uint32_t* start_cluster, 
    size_t* file_size, uint32_t data_start, uint32_t total_clusters, int* last_alloc
) {
    uint8_t buffer[CLUSTER_SIZE] = { 0 };
    *file_size = 0;
    int prev_cluster = -1;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, CLUSTER_SIZE, src_file)) > 0) {
        int cluster_found = -1;
        for (int i = *last_alloc; i < total_clusters; i++) {
            if (fat_table[i] == FAT_ENTRY_FREE) {
                cluster_found = i;
                *last_alloc = i + 1;
                fat_table[i] = FAT_ENTRY_END;
                break;
            }
        }

        if (cluster_found == -1) {
            fprintf(stderr, "No free clusters available\n");
            return 0;
        }

        if (prev_cluster != -1) {
            fat_table[prev_cluster] = cluster_found;
        } 
        else {
            *start_cluster = cluster_found;
        }

        off_t cluster_offset = data_start + (cluster_found - 2) * CLUSTER_SIZE;
        if (lseek(fd, cluster_offset, SEEK_SET) != cluster_offset) return 0;
        if (write(fd, buffer, bytes_read) != bytes_read) return 0;

        *file_size += bytes_read;
        prev_cluster = cluster_found;
    }

    return 1;
}

static void _to_83_name(const char* name, char* out) {
    char base[9] = { 0 };
    char ext[4]  = { 0 };
    const char* dot = strrchr(name, '.');

    if (dot && dot != name) {
        int base_len = dot - name;
        if (base_len > 8) base_len = 8;
        strncpy(base, name, base_len);
        
        int ext_len = strlen(dot + 1);
        if (ext_len > 3) ext_len = 3;
        strncpy(ext, dot + 1, ext_len);
    } 
    else {
        int name_len = strlen(name);
        if (name_len > 8) name_len = 8;
        strncpy(base, name, name_len);
    }

    for (int i = 0; i < 8; i++) out[i] = (i < strlen(base)) ? toupper(base[i]) : ' ';
    for (int i = 0; i < 3; i++) out[8 + i] = (i < strlen(ext)) ? toupper(ext[i]) : ' ';
}