#include "nifat32formatter.h"

static int write_bs(int fd, size_t total_sectors) {
    fat_BS_t bs = { 0 };
    fat_extBS_32_t ext = { 0 };

    bs.bootjmp[0] = 0xEB;
    bs.bootjmp[1] = 0x58;
    bs.bootjmp[2] = 0x90;
    memcpy(bs.oem_name, "MSWIN4.1", 8);
    bs.bytes_per_sector      = BYTES_PER_SECTOR;
    bs.sectors_per_cluster   = SECTORS_PER_CLUSTER;
    bs.reserved_sector_count = RESERVED_SECTORS;
    bs.table_count           = FAT_COUNT;
    bs.media_type            = 0xF8;
    bs.sectors_per_track     = 63;
    bs.head_side_count       = 255;
    bs.total_sectors_32      = total_sectors;

    uint32_t fat_size = (total_sectors - RESERVED_SECTORS) / (FAT_COUNT + (SECTORS_PER_CLUSTER * 128) + 1);
    ext.table_size_32 = fat_size;
    ext.root_cluster  = ROOT_DIR_CLUSTER;
    ext.fat_info      = 1;
    ext.backup_BS_sector = 6;
    ext.drive_number     = 0x80;
    ext.boot_signature   = 0x29;
    ext.volume_id        = 0x12345678;

    memcpy(ext.volume_label, "NO NAME    ", 11);
    memcpy(ext.fat_type_label, "FAT32   ", 8);
    memcpy(bs.extended_section, &ext, sizeof(ext));

    write(fd, &bs, sizeof(bs));

    memcpy(bs.extended_section, &ext, sizeof(ext));
    write(fd, &bs, sizeof(bs));
    char padding[BYTES_PER_SECTOR - sizeof(bs)] = { 0 };
    write(fd, padding, sizeof(padding));

    return 1;
}

static int zero_fill(int fd, size_t total_bytes) {
    ftruncate(fd, total_bytes);
    return 1;
}

int alloc_cluster(uint32_t* fat, int* last) {
    for (int i = *last; i < FAT_ENTRIES; ++i) {
        if (fat[i] == 0) {
            fat[i] = 0x0FFFFFFF;
            *last = i + 1;
            return i;
        }
    }
    return -1;
}

static int copy_file_to_clusters(
    int fd, FILE* src, uint32_t* fat, int* cluster_cursor, uint32_t* start_cluster, size_t* file_size
) {
    char buffer[CLUSTER_SIZE];
    *file_size = 0;

    int prev_cluster = -1;
    while (1) {
        size_t bytes = fread(buffer, 1, CLUSTER_SIZE, src);
        if (bytes == 0) break;

        int cluster = alloc_cluster(fat, cluster_cursor);
        if (cluster == -1) {
            fprintf(stderr, "No space for cluster\n");
            return 0;
        }

        if (prev_cluster != -1) fat[prev_cluster] = cluster;
        else *start_cluster = cluster;

        off_t cluster_offset = DATA_REGION_OFFSET + cluster * CLUSTER_SIZE;
        lseek(fd, cluster_offset, SEEK_SET);
        write(fd, buffer, bytes);

        *file_size += bytes;
        prev_cluster = cluster;

        if (bytes < CLUSTER_SIZE) break;
    }

    return 1;
}

static int copy_files_to_fs(int fd, const char* folder_path) {
    DIR* dir = opendir(folder_path);
    if (!dir) { perror("opendir"); return 0; }

    struct dirent* entry;
    off_t dir_entry_offset = DATA_REGION_OFFSET;

    uint32_t fat[FAT_ENTRIES] = {0};
    fat[0] = fat[1] = 0x0FFFFFF8;

    int cluster_cursor = 2;

    while ((entry = readdir(dir))) {
        if (entry->d_type != DT_REG) continue;

        char full_path[512];
        snprintf(full_path, sizeof(full_path), "%s/%s", folder_path, entry->d_name);

        FILE* src = fopen(full_path, "rb");
        if (!src) { perror("fopen"); continue; }

        uint32_t start_cluster = 0;
        size_t size = 0;

        copy_file_to_clusters(fd, src, fat, &cluster_cursor, &start_cluster, &size);
        fclose(src);

        lseek(fd, dir_entry_offset, SEEK_SET);
        directory_entry_t ent = {0};
        memset(ent.file_name, ' ', 11);
        for (int i = 0; i < 11 && entry->d_name[i] && entry->d_name[i] != '.'; i++) {
            ent.file_name[i] = toupper(entry->d_name[i]);
        }
        ent.attributes = 0x20;
        ent.low_bits = start_cluster & 0xFFFF;
        ent.high_bits = (start_cluster >> 16) & 0xFFFF;
        ent.file_size = size;
        write(fd, &ent, sizeof(ent));

        dir_entry_offset += sizeof(directory_entry_t);
    }

    closedir(dir);
    for (int i = 0; i < FAT_COUNT; ++i) {
        lseek(fd, FAT_OFFSET + i * FAT_ENTRIES * sizeof(uint32_t), SEEK_SET);
        write(fd, fat, FAT_ENTRIES * sizeof(uint32_t));
    }

    return 1;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        fprintf(stdout, "Usage: %s (-s <path> / --empty) -o <path>\n", argv[0]);
        return EXIT_FAILURE;
    }

    int is_empty = 0;
    const char* path = NULL;
    const char* save_path = NULL;
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], EMPTY_FLAG)) is_empty = 1;
        else if (!strcmp(argv[i], SOURCE_OPTION)) {
            if (i + 1 < argc) path = argv[i + 1];
            else {
                fprintf(stderr, "Path required!\n");
                return EXIT_FAILURE;
            }
        }
        else if (!strcmp(argv[i], SAVE_OPTION)) {
            if (i + 1 < argc) save_path = argv[i + 1];
            else {
                fprintf(stderr, "Path required!\n");
                return EXIT_FAILURE;
            }
        }
    }

    int fd = open(save_path, O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    zero_fill(fd, DEFAULT_VOLUME_SIZE);
    size_t total_sectors = DEFAULT_VOLUME_SIZE / BYTES_PER_SECTOR;
    write_bs(fd, total_sectors);

    if (!is_empty) {
        copy_files_to_fs(fd, path);
        printf("Created volume with files from '%s' in '%s'\n", path, save_path);
    } 
    else {
        printf("Created empty FAT32 volume: %s\n", save_path);
    }

    close(fd);
    return EXIT_SUCCESS;
}