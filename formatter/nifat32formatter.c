#include "nifat32formatter.h"

static int write_bs(int, uint32_t, uint32_t);
static int write_fats(int, uint32_t*, uint32_t, uint32_t);
static int initialize_fat(uint32_t*, uint32_t);
static int write_root_directory(int, uint32_t, uint32_t);
static int copy_files_to_fs(int, const char*, uint32_t*, uint32_t, uint32_t, uint32_t);
static int copy_file(int, FILE*, uint32_t*, uint32_t*, size_t*, uint32_t, uint32_t, int*);
static void to_83_name(const char*, char*);
static uint32_t calculate_fat_size(uint32_t);

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s (-s <source_path> | --empty) -o <output_path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int is_empty = 0;
    const char* src_path = NULL;
    const char* output_path = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], EMPTY_FLAG)) {
            is_empty = 1;
        }
        else if (!strcmp(argv[i], SOURCE_OPTION)) {
            if (i + 1 < argc) src_path = argv[++i];
            else {
                fprintf(stderr, "Error: Source path required after -s\n");
                return EXIT_FAILURE;
            }
        } 
        else if (!strcmp(argv[i], SAVE_OPTION)) {
            if (i + 1 < argc) output_path = argv[++i];
            else {
                fprintf(stderr, "Error: Output path required after -o\n");
                return EXIT_FAILURE;
            }
        }
    }

    if (!output_path) {
        fprintf(stderr, "Error: Output path not specified\n");
        return EXIT_FAILURE;
    }

    if (!is_empty && !src_path) {
        fprintf(stderr, "Error: Source path required for non-empty volume\n");
        return EXIT_FAILURE;
    }

    int fd = open(output_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("Error creating output file");
        return EXIT_FAILURE;
    }

    if (ftruncate(fd, DEFAULT_VOLUME_SIZE)) {
        perror("Error setting volume size");
        close(fd);
        return EXIT_FAILURE;
    }

    uint32_t total_sectors  = DEFAULT_VOLUME_SIZE / BYTES_PER_SECTOR;
    uint32_t fat_size       = calculate_fat_size(total_sectors);
    uint32_t total_clusters = (total_sectors - RESERVED_SECTORS - FAT_COUNT * fat_size) / SECTORS_PER_CLUSTER;
    uint32_t data_start     = (RESERVED_SECTORS + FAT_COUNT * fat_size) * BYTES_PER_SECTOR;

    if (!write_bs(fd, total_sectors, fat_size)) {
        fprintf(stderr, "Error writing boot sector\n");
        close(fd);
        return EXIT_FAILURE;
    }

    uint32_t* fat_table = calloc(total_clusters, sizeof(uint32_t));
    if (!fat_table) {
        fprintf(stderr, "Memory allocation error\n");
        close(fd);
        return EXIT_FAILURE;
    }

    initialize_fat(fat_table, total_clusters);
    if (!write_fats(fd, fat_table, fat_size, total_clusters)) {
        fprintf(stderr, "Error writing FAT tables\n");
        free(fat_table);
        close(fd);
        return EXIT_FAILURE;
    }

    if (!write_root_directory(fd, data_start, fat_size)) {
        fprintf(stderr, "Error initializing root directory\n");
        free(fat_table);
        close(fd);
        return EXIT_FAILURE;
    }

    if (!is_empty) {
        if (!copy_files_to_fs(fd, src_path, fat_table, fat_size, data_start, total_clusters)) {
            fprintf(stderr, "Error copying files\n");
            free(fat_table);
            close(fd);
            return EXIT_FAILURE;
        }
        printf("Created volume with files from '%s' at '%s'\n", src_path, output_path);
    } 
    else {
        printf("Created empty FAT32 volume at '%s'\n", output_path);
    }

    if (!write_fats(fd, fat_table, fat_size, total_clusters)) {
        fprintf(stderr, "Error updating FAT tables\n");
    }

    free(fat_table);
    close(fd);
    return EXIT_SUCCESS;
}

static uint32_t calculate_fat_size(uint32_t total_sectors) {
    uint32_t data_sectors = total_sectors - RESERVED_SECTORS;
    uint32_t fat_size = 0;
    uint32_t prev_fat_size;
    
    do {
        prev_fat_size = fat_size;
        uint32_t clusters = (data_sectors - FAT_COUNT * fat_size) / SECTORS_PER_CLUSTER;
        fat_size = (clusters * 4 + BYTES_PER_SECTOR - 1) / BYTES_PER_SECTOR;
    } while (fat_size != prev_fat_size);
    return fat_size;
}

static int write_bs(int fd, uint32_t total_sectors, uint32_t fat_size) {
    fat_BS_t bs = {0};
    fat_extBS_32_t ext = {0};
    uint8_t padding[BYTES_PER_SECTOR - sizeof(fat_BS_t)] = {0};

    bs.bootjmp[0] = 0xEB;
    bs.bootjmp[1] = 0x58;
    bs.bootjmp[2] = 0x90;
    memcpy(bs.oem_name, "MSWIN4.1", 8);
    bs.bytes_per_sector = BYTES_PER_SECTOR;
    bs.sectors_per_cluster = SECTORS_PER_CLUSTER;
    bs.reserved_sector_count = RESERVED_SECTORS;
    bs.table_count = FAT_COUNT;
    bs.root_entry_count = 0;
    bs.total_sectors_16 = 0;
    bs.media_type = 0xF8;
    bs.sectors_per_track = 63;
    bs.head_side_count = 255;
    bs.hidden_sector_count = 0;
    bs.total_sectors_32 = total_sectors;

    ext.boot_signature = 0x5A;
    ext.table_size_32 = fat_size;
    ext.root_cluster = ROOT_DIR_CLUSTER;
    ext.fat_info = 1;
    ext.backup_BS_sector = 6;
    ext.drive_number = 0x80;
    ext.boot_signature = 0x29;
    ext.volume_id = 0x12345678;
    memcpy(ext.volume_label, "NO NAME    ", 11);
    memcpy(ext.fat_type_label, "FAT32   ", 8);

    memcpy(bs.extended_section, &ext, sizeof(ext));
    if (lseek(fd, 0, SEEK_SET) != 0) return 0;
    if (write(fd, &bs, sizeof(bs)) != sizeof(bs)) return 0;
    if (write(fd, padding, sizeof(padding)) != sizeof(padding)) return 0;
    if (lseek(fd, 6 * BYTES_PER_SECTOR, SEEK_SET) != 6 * BYTES_PER_SECTOR) return 0;
    if (write(fd, &bs, sizeof(bs)) != sizeof(bs)) return 0;
    if (write(fd, padding, sizeof(padding)) != sizeof(padding)) return 0;

    return 1;
}

static int initialize_fat(uint32_t* fat_table, uint32_t total_clusters) {
    fat_table[0] = FAT_ENTRY_RESERVED | (0xF8 << 24);
    fat_table[1] = FAT_ENTRY_END;
    fat_table[2] = FAT_ENTRY_END;
    return 1;
}

static int write_fats(int fd, uint32_t* fat_table, uint32_t fat_size, uint32_t total_clusters) {
    uint32_t fat_bytes = fat_size * BYTES_PER_SECTOR;
    uint8_t* fat_buffer = calloc(1, fat_bytes);
    if (!fat_buffer) return 0;

    memcpy(fat_buffer, fat_table, total_clusters * sizeof(uint32_t));
    off_t fat1_offset = RESERVED_SECTORS * BYTES_PER_SECTOR;
    if (lseek(fd, fat1_offset, SEEK_SET) != fat1_offset) {
        free(fat_buffer);
        return 0;
    }

    if (write(fd, fat_buffer, fat_bytes) != fat_bytes) {
        free(fat_buffer);
        return 0;
    }

    off_t fat2_offset = fat1_offset + fat_bytes;
    if (lseek(fd, fat2_offset, SEEK_SET) != fat2_offset) {
        free(fat_buffer);
        return 0;
    }

    if (write(fd, fat_buffer, fat_bytes) != fat_bytes) {
        free(fat_buffer);
        return 0;
    }

    free(fat_buffer);
    return 1;
}

static int write_root_directory(int fd, uint32_t data_start, uint32_t fat_size) {
    off_t root_dir_offset = data_start + (ROOT_DIR_CLUSTER - 2) * CLUSTER_SIZE;
    uint8_t empty_dir[CLUSTER_SIZE] = {0};
    if (lseek(fd, root_dir_offset, SEEK_SET) != root_dir_offset) return 0;
    if (write(fd, empty_dir, CLUSTER_SIZE) != CLUSTER_SIZE) return 0;
    return 1;
}

static int copy_files_to_fs(
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
        if (stat(full_path, &st) != 0 || !S_ISREG(st.st_mode)) 
            continue;

        FILE* src_file = fopen(full_path, "rb");
        if (!src_file) {
            perror("Error opening source file");
            continue;
        }

        uint32_t start_cluster = 0;
        size_t file_size = 0;
        if (!copy_file(fd, src_file, fat_table, &start_cluster, &file_size, data_start, total_clusters, &last_alloc)) {
            fclose(src_file);
            continue;
        }

        fclose(src_file);

        to_83_name(entry->d_name, (char*)root_dir[entry_count].file_name);
        root_dir[entry_count].attributes = 0x20;
        root_dir[entry_count].high_bits  = (start_cluster >> 16) & 0xFFFF;
        root_dir[entry_count].low_bits   = start_cluster & 0xFFFF;
        root_dir[entry_count].file_size  = file_size;
        entry_count++;
    }

    closedir(dir);

    if (lseek(fd, root_dir_offset, SEEK_SET) != root_dir_offset) {
        free(root_dir);
        return 0;
    }
    
    if (write(fd, root_dir, CLUSTER_SIZE) != CLUSTER_SIZE) {
        free(root_dir);
        return 0;
    }

    free(root_dir);
    return 1;
}

static int copy_file(
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
                break;
            }
        }

        if (cluster_found == -1) {
            fprintf(stderr, "No free clusters available\n");
            return 0;
        }

        fat_table[cluster_found] = FAT_ENTRY_END;
        if (prev_cluster != -1) fat_table[prev_cluster] = cluster_found;
        else *start_cluster = cluster_found;

        off_t cluster_offset = data_start + (cluster_found - 2) * CLUSTER_SIZE;
        if (lseek(fd, cluster_offset, SEEK_SET) != cluster_offset) return 0;
        if (write(fd, buffer, bytes_read) != bytes_read) return 0;

        *file_size += bytes_read;
        prev_cluster = cluster_found;
    }

    return 1;
}

static void to_83_name(const char* name, char* out) {
    char base[9] = {0};
    char ext[4] = {0};
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