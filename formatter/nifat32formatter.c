#include "nifat32formatter.h"

static int      _write_bs(int, uint32_t, uint32_t);
static int      _write_journals(int, uint32_t);
static int      _write_errors(int, uint32_t);
static int      _write_fats(int, fat_table_t, uint32_t, uint32_t);
static int      _initialize_fat(fat_table_t, uint32_t, uint32_t);
static int      _write_root_directory(int, uint32_t, uint32_t);
static int      _create_directory(int, fat_table_t, uint32_t*, uint32_t, uint32_t, int*);
static int      _copy_files_to_fs(int, const char*, fat_table_t, uint32_t, uint32_t, uint32_t);
static int      _copy_file(int, FILE*, fat_table_t, uint32_t*, size_t*, uint32_t, uint32_t, int*);
static void     _to_83_name(const char*, char*);
static uint32_t _calculate_fat_size(uint32_t);

static opt_t opt = { 
    .spc    = SECTORS_PER_CLUSTER,
    .v_size = DEFAULT_VOLUME_SIZE,
    .fc     = FAT_COUNT,
    .bsbc   = BS_BACKUPS,
    .jc     = JOURNALS_BACKUPS,
    .ec     = ERRORS_COUNT
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

    if (!_write_journals(fd, total_sectors)) {
        fprintf(stderr, "Error writing journal sector\n");
        close(fd);
        return EXIT_FAILURE;
    }

    if (!_write_errors(fd, total_sectors)) {
        fprintf(stderr, "Error writing errors sector\n");
        close(fd);
        return EXIT_FAILURE;
    }

    memset(fat_table, FAT_ENTRY_FREE, total_clusters * sizeof(uint32_t));
    _initialize_fat(fat_table, total_sectors, total_clusters);
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

    if (!_write_fats(fd, fat_table, fat_size, total_sectors)) {
        fprintf(stderr, "Error writting FAT tables\n");
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

    
    static inline unsigned int _rotl32(unsigned int x, char r) {
        return (x << r) | (x >> (32 - r));
    }

    static inline unsigned int _fmix32(unsigned int h) {
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

    unsigned int _murmur3_x86_32(const unsigned char* key, unsigned int len, unsigned int seed) {
        const unsigned int c1 = 0xcc9e2d51;
        const unsigned int c2 = 0x1b873593;

        const int nblocks = len / 4;
        unsigned int h1 = seed;

        const unsigned int* blocks = (const unsigned int *)(key);
        for (int i = 0; i < nblocks; i++) {
            unsigned int k1 = blocks[i];

            k1 *= c1;
            k1 = _rotl32(k1, 15);
            k1 *= c2;

            h1 ^= k1;
            h1 = _rotl32(h1, 13);
            h1 = h1 * 5 + 0xe6546b64;
        }

        const unsigned char* tail = (const unsigned char*)(key + nblocks * 4);
        unsigned int k1 = 0;
        switch (len & 3) {
            case 3: k1 ^= tail[2] << 16;
            case 2: k1 ^= tail[1] << 8;
            case 1: k1 ^= tail[0];
                    k1 *= c1;
                    k1 = _rotl32(k1, 15);
                    k1 *= c2;
                    h1 ^= k1;
        }

        h1 ^= len;
        h1 = _fmix32(h1);
        return h1;
    }

#pragma endregion

static int _write_bs(int fd, uint32_t total_sectors, uint32_t fat_size) {
    nifat32_bootsector_t bs = { 0 };
    bs.bootjmp[0] = 0xEB;
    bs.bootjmp[1] = 0x58;
    bs.bootjmp[2] = 0x90;
    memcpy(bs.oem_name, "NIFAT 32", 8);
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

    nifat32_ext32_bootsector_t ext = { 0 };
    ext.boot_signature   = 0x5A;
    ext.table_size_32    = fat_size;
    ext.root_cluster     = ROOT_DIR_CLUSTER;
    ext.drive_number     = 0x80;
    ext.boot_signature   = 0x29;
    ext.volume_id        = 0x12345678;
    memcpy(ext.volume_label, "ROOT_LABEL ", 11);
    memcpy(ext.fat_type_label, "NIFAT32 ", 8);
    ext.checksum = 0;
    ext.checksum = _murmur3_x86_32((uint8_t*)&ext, sizeof(ext), 0);

    memcpy(&bs.extended_section, &ext, sizeof(ext));
    bs.checksum = 0;
    bs.checksum = _murmur3_x86_32((uint8_t*)&bs, sizeof(bs), 0);
    fprintf(stdout, "Bootstruct checksum: %u, ext. section: %u\n", bs.checksum, ext.checksum);

    int encoded_size = sizeof(encoded_t) * sizeof(nifat32_bootsector_t);
    encoded_t* encoded_bs = (encoded_t*)malloc(encoded_size);
    if (!encoded_bs) return 0;

    uint8_t padding[BYTES_PER_SECTOR] = { 0 };
    for (int i = 0; i < opt.bsbc; i++) {
        unsigned int ca = GET_BOOTSECTOR(i, total_sectors);
        _pack_memory((unsigned char*)&bs, encoded_bs, sizeof(nifat32_bootsector_t));
        if (pwrite(fd, padding, sizeof(padding), ca * BYTES_PER_SECTOR) != sizeof(padding)) return 0;
        if (pwrite(fd, encoded_bs, encoded_size, ca * BYTES_PER_SECTOR) != encoded_size) return 0;
        fprintf(stdout, "[i=%i] encoded bootsector has been written at ca=%u/%u!\n", i, ca, total_sectors);
    }

    free(encoded_bs);
    return 1;
}

static int _write_journals(int fd, uint32_t ts) {
    for (int i = 0; i < opt.jc; i++) {
        uint8_t buffer[BYTES_PER_SECTOR * opt.spc];
        memset(buffer, 0, BYTES_PER_SECTOR * opt.spc);
        uint32_t sector = GET_JOURNALSECTOR(i, ts);
        if (pwrite(fd, buffer, sizeof(buffer), sector * BYTES_PER_SECTOR) != sizeof(buffer)) return 0;
        fprintf(stdout, "[i=%i] viped area for journals has been written at sa=%u -> %u!\n", i, sector, sector + opt.spc);
    }

    return 1;
}

static int _write_errors(int fd, uint32_t ts) {
    for (int i = 0; i < opt.ec; i++) {
        uint8_t buffer[BYTES_PER_SECTOR * opt.spc];
        memset(buffer, 0, BYTES_PER_SECTOR * opt.spc);
        uint32_t sector = GET_ERRORSSECTOR(i, ts);
        if (pwrite(fd, buffer, sizeof(buffer), sector * BYTES_PER_SECTOR) != sizeof(buffer)) return 0;
        fprintf(stdout, "[i=%i] viped area for errors has been written at sa=%u -> %u!\n", i, sector, sector + opt.spc);
    }

    return 1;
}

/* Generate first clusters, reserve clusters for bootstructs */
static int _initialize_fat(uint32_t* fat_table, uint32_t ts, uint32_t tc) {
    fat_table[0] = FAT_ENTRY_RESERVED | (0xF8 << 24);
    fat_table[1] = FAT_ENTRY_END | (0xF8 << 24);
    fat_table[2] = FAT_ENTRY_END | (0xF8 << 24);
    uint32_t cluster_for_backup = 0;

    /* Bootsector */
    for (int i = 0; i < opt.bsbc; i++) {
        uint32_t sector = GET_BOOTSECTOR(i, ts);
        cluster_for_backup = sector / opt.spc;
        if (cluster_for_backup < tc) {
            fat_table[cluster_for_backup] = FAT_ENTRY_RESERVED | (0xF8 << 24);
        }
    }

    /* FATs */
    for (int i = 0; i < opt.fc; i++) {
        long long total_reserved = tc * sizeof(uint32_t) * sizeof(encoded_t);
        uint32_t sa = RESERVED_SECTORS + GET_FATSECTOR(i, ts);
        cluster_for_backup = sa / opt.spc;

        int c = 0;
        for (; cluster_for_backup < tc && total_reserved > 0; cluster_for_backup++, c++) {
            fat_table[cluster_for_backup] = FAT_ENTRY_RESERVED | (0xF8 << 24);
            total_reserved -= opt.spc * BYTES_PER_SECTOR;
        }

        printf("Reserved %u for FAT %i\n", c, i);
    }

    /* Journals */
    for (int i = 0; i < opt.jc; i++) {
        uint32_t sector = GET_JOURNALSECTOR(i, ts);
        cluster_for_backup = sector / opt.spc;
        if (cluster_for_backup < tc) {
            fat_table[cluster_for_backup] = FAT_ENTRY_RESERVED | (0xF8 << 24);
        }
    }

    /* Errors */
    for (int i = 0; i < opt.ec; i++) {
        uint32_t sector = GET_ERRORSSECTOR(i, ts);
        cluster_for_backup = sector / opt.spc;
        if (cluster_for_backup < tc) {
            fat_table[cluster_for_backup] = FAT_ENTRY_RESERVED | (0xF8 << 24);
        }
    }

    return 1;
}

static int _write_fats(int fd, fat_table_t fat_table, uint32_t fat_size, uint32_t ts) {
    uint32_t fat_bytes = fat_size * BYTES_PER_SECTOR * sizeof(encoded_t);
    unsigned char* encoded_fat = (unsigned char*)malloc(fat_bytes);
    if (!encoded_fat) return 0;

    memset(encoded_fat, 0, fat_bytes);
    _pack_memory((unsigned char*)fat_table, (unsigned short*)encoded_fat, fat_size * BYTES_PER_SECTOR);
    
    for (int i = 0; i < opt.fc; i++) {
        uint32_t sa = RESERVED_SECTORS + GET_FATSECTOR(i, ts);
        if (pwrite(fd, encoded_fat, fat_bytes, sa * BYTES_PER_SECTOR) != fat_bytes) return 0;
        printf("Encoded (Hamming) FAT written at sa=%u/%u\n", sa, ts);
    }

    free(encoded_fat);
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
            root_dir[entry_count].name_hash  = _murmur3_x86_32(root_dir[entry_count].file_name, sizeof(root_dir[entry_count].file_name), 0);
            root_dir[entry_count].attributes = 0x10;
            root_dir[entry_count].cluster    = start_cluster;
            root_dir[entry_count].file_size  = 0;
            root_dir[entry_count].checksum   = _murmur3_x86_32((uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]), 0);
            fprintf(stdout, "%s checksum: %u\n", entry->d_name, root_dir[entry_count].checksum);
            entry_count++;
        }
        else if (S_ISREG(st.st_mode)) {
            FILE* src_file = fopen(full_path, "rb");
            if (!src_file) {
                perror("Error opening source file");
                continue;
            }

            size_t file_size = 0;
            uint32_t start_cluster = 0;
            if (!_copy_file(fd, src_file, fat_table, &start_cluster, &file_size, data_start, total_clusters, &last_alloc)) {
                fclose(src_file);
                continue;
            }

            fclose(src_file);

            _to_83_name(entry->d_name, (char*)root_dir[entry_count].file_name);
            root_dir[entry_count].name_hash  = _murmur3_x86_32(root_dir[entry_count].file_name, strlen((char*)root_dir[entry_count].file_name), 0);
            root_dir[entry_count].attributes = 0x20;
            root_dir[entry_count].cluster    = start_cluster;
            root_dir[entry_count].file_size  = file_size;
            root_dir[entry_count].checksum   = 0;
            root_dir[entry_count].checksum   = _murmur3_x86_32((uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]), 0);
            fprintf(stdout, "%s checksum: %u\n", entry->d_name, root_dir[entry_count].checksum);
            entry_count++;
        }
    }

    root_dir[entry_count].file_name[0] = ENTRY_END;
    root_dir[entry_count].name_hash  = _murmur3_x86_32((uint8_t*)root_dir[entry_count].file_name, sizeof(root_dir[entry_count].file_name), 0);
    root_dir[entry_count].attributes = 0;
    root_dir[entry_count].cluster    = 0;
    root_dir[entry_count].file_size  = 0;
    root_dir[entry_count].checksum   = 0;
    root_dir[entry_count].checksum   = _murmur3_x86_32((uint8_t*)&root_dir[entry_count], sizeof(root_dir[entry_count]), 0);
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
    int fd, fat_table_t fat_table, uint32_t* start_cluster, uint32_t data_start, uint32_t total_clusters, int* last_alloc
) {
    for (int i = *last_alloc; i < total_clusters; i++) {
        if (fat_table[i] == FAT_ENTRY_FREE) {
            fat_table[i] = FAT_ENTRY_END;
            *last_alloc = i + 1;
            *start_cluster = i;

            directory_entry_t entries[3] = { 0 };
            _to_83_name(".", (char*)entries[0].file_name);
            entries[0].name_hash  = _murmur3_x86_32(entries[0].file_name, sizeof(entries[0].file_name), 0);
            entries[0].attributes = 0x10;
            entries[0].cluster    = i;
            entries[0].checksum   = 0;
            entries[0].checksum   = _murmur3_x86_32((uint8_t*)&entries[0], sizeof(entries[0]), 0);
            fprintf(stdout, ". checksum: %u\n", entries[0].checksum);

            _to_83_name("..", (char*)entries[1].file_name);
            entries[1].name_hash  = _murmur3_x86_32(entries[1].file_name, sizeof(entries[1].file_name), 0);
            entries[1].attributes = 0x10;
            entries[1].cluster    = 0;
            entries[1].checksum   = 0;
            entries[1].checksum   = _murmur3_x86_32((uint8_t*)&entries[1], sizeof(entries[1]), 0);
            fprintf(stdout, ".. checksum: %u\n", entries[1].checksum);

            entries[2].file_name[0] = ENTRY_END;
            entries[2].name_hash  = _murmur3_x86_32(entries[2].file_name, sizeof(entries[2].file_name), 0);
            entries[2].attributes = 0x10;
            entries[2].cluster    = 0;
            entries[2].checksum   = 0;
            entries[2].checksum   = _murmur3_x86_32((uint8_t*)&entries[2], sizeof(entries[2]), 0);
            fprintf(stdout, "END checksum: %u\n", entries[1].checksum);

            encoded_t* root_dirs = (encoded_t*)malloc(sizeof(entries) * sizeof(encoded_t));
            if (!root_dirs) {
                return 0;
            }

            _pack_memory((unsigned char*)entries, root_dirs, sizeof(entries));
            off_t cluster_offset = data_start + (i - ROOT_DIR_CLUSTER) * CLUSTER_SIZE;
            if (
                pwrite(
                    fd, root_dirs, sizeof(entries) * sizeof(encoded_t), cluster_offset
                ) != sizeof(entries) * sizeof(encoded_t)
            ) {
                free(root_dirs);
                return 0;
            }

            free(root_dirs);
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