/*
unix_nifat32.c
This is a template file with simple implementation of file manage on nifat32.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "nifat32.h"

typedef enum {
    CP, MV, RM, MKDIR, MKFILE, FRENAME, // DDL
    READ, WRITE, TRUNC, // DML
    CD, LS, 
    UNKNOWN,
    RS, WS  // Debug sector functions
} cmd_t;

cmd_t _get_cmd(const char* input) {
    if (!strcmp(input, "cd"))           return CD;
    else if (!strcmp(input, "cp"))      return CP;
    else if (!strcmp(input, "mv"))      return MV;
    else if (!strcmp(input, "read"))    return READ;
    else if (!strcmp(input, "write"))   return WRITE;
    else if (!strcmp(input, "frename")) return FRENAME;
    else if (!strcmp(input, "ls"))      return LS;
    else if (!strcmp(input, "rm"))      return RM;
    else if (!strcmp(input, "mkdir"))   return MKDIR;
    else if (!strcmp(input, "mkfile"))  return MKFILE;
    else if (!strcmp(input, "trunc"))   return TRUNC;
    else if (!strcmp(input, "rs"))      return RS;
    else if (!strcmp(input, "ws"))      return WS;
    return UNKNOWN;
}

static int sector_size = 512;
static int disk_fd = 0;
static int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * sector_size + offset) > 0;
}

static int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * sector_size + offset) > 0;
}

static int _mock_fprintf_(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int result = vfprintf(stdout, fmt, args);
    va_end(args);
    return result;
}

static int _mock_vfprintf_(const char* fmt, va_list args) {
    return vfprintf(stdout, fmt, args);
}

/*
argv[1] - Path to nifat32 image
argv[2] - Image size
argv[3] - Sector size
argv[4] - bs_count
argv[5] - jc
*/
int main(int argc, char* argv[]) {
    disk_fd = open(argv[1], O_RDWR);
    if (disk_fd < 0) {
        fprintf(stderr, "%s not found!\n", argv[1]);
        return EXIT_FAILURE;
    }

    int v_size = atoi(argv[2]);
    sector_size = atoi(argv[3]);
    int bs = atoi(argv[4]);
    int jc = atoi(argv[5]);
    #define DEFAULT_VOLUME_SIZE (1024 * 1024)
    fprintf(stdout, "v_size=%i, sector_size=%i, bs=%i, jc=%i\n", v_size, sector_size, bs, jc);

    nifat32_params params = { 
        .bs_num    = 0, 
        .ts        = (v_size * DEFAULT_VOLUME_SIZE) / sector_size, 
        .fat_cache = CACHE | HARD_CACHE, 
        .jc        = jc, 
        .bs_count  = bs,
        .disk_io   = {
            .read_sector  = _mock_sector_read_,
            .write_sector = _mock_sector_write_,
            .sector_size  = sector_size
        },
        .logg_io   = {
            .fd_fprintf  = _mock_fprintf_,
            .fd_vfprintf = _mock_vfprintf_
        }
    };

    if (!NIFAT32_init(&params)) {
        fprintf(stderr, "NIFAT32_init() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    int alive = 1;
    char current_path[256] = { 0 };

    while (alive) {
        printf("%s > ", current_path);

        char buffer[256] = { 0 };
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = 0;

        int cmd_size = 0;
        char* cmds[20] = { NULL };
        char* token = strtok(buffer, " ");
        while (token != NULL) {
            cmds[cmd_size++] = token;
            token = strtok(NULL, " ");
        }

        switch (_get_cmd(cmds[0])) {
            case CD: {
                const char* path = cmds[1];
                if (!path) {
                    printf("cd: missing path\n");
                    break;
                }

                if (!strcmp(path, "..")) {
upper: {}
                    char* last_slash = strrchr(current_path, '/');
                    if (last_slash && last_slash != current_path) *last_slash = '\0';
                    else strcpy(current_path, "");
                } 
                else {
                    if (strlen(current_path) > 1 && strcmp(current_path, "/")) {
                        strcat(current_path, "/");
                    }

                    char fatbuffer[24] = { 0 };
                    strcpy(fatbuffer, path);
                    str_uppercase(fatbuffer);
                    strcat(current_path, fatbuffer);

                    if (!NIFAT32_content_exists(current_path)) {
                        goto upper;
                    }
                }
            }
            break;
            
            case CP: {
                const char* src = cmds[1];
                const char* dst = cmds[2];

                char src_path[128] = { 0 };
                char dst_path[128] = { 0 };

                strcpy(src_path, current_path);
                strcpy(dst_path, current_path);
                if (strlen(current_path) > 1 && strcmp(current_path, "/")) {
                    strcat(src_path, "/");
                    strcat(dst_path, "/");
                }
                
                strcat(src_path, src);
                strcat(dst_path, dst);

                char src_83[128] = { 0 };
                char dst_83[128] = { 0 };
                path_to_fatnames(src_path, src_83);
                path_to_fatnames(dst_path, dst_83);

                ci_t src_ci = NIFAT32_open_content(NO_RCI, src_83, DF_MODE);
                ci_t dst_ci = NIFAT32_open_content(NO_RCI, dst_83, DF_MODE);
                if (src_ci >= 0 && dst_ci >= 0) {
                    NIFAT32_copy_content(src_ci, dst_ci, DEEP_COPY);
                    NIFAT32_close_content(src_ci);
                    NIFAT32_close_content(dst_ci);
                }
            }
            break;

            case MV: {
                const char* src = cmds[1];
                const char* dst = cmds[2];

                char src_path[128] = { 0 };
                char dst_path[128] = { 0 };

                strcpy(src_path, current_path);
                strcpy(dst_path, current_path);
                if (strlen(current_path) > 1 && strcmp(current_path, "/")) {
                    strcat(src_path, "/");
                    strcat(dst_path, "/");
                }
                
                strcat(src_path, src);
                strcat(dst_path, dst);

                char src_83[128] = { 0 };
                char dst_83[128] = { 0 };
                path_to_fatnames(src_path, src_83);
                path_to_fatnames(dst_path, dst_83);

                ci_t src_ci = NIFAT32_open_content(NO_RCI, src_83, DF_MODE);
                ci_t dst_ci = NIFAT32_open_content(NO_RCI, dst_83, MODE(W_MODE | CR_MODE, FILE_TARGET));
                if (src_ci >= 0 && dst_ci >= 0) {
                    NIFAT32_copy_content(src_ci, dst_ci, DEEP_COPY);
                    NIFAT32_delete_content(src_ci);
                    NIFAT32_close_content(dst_ci);
                }

            }
            break;

            case MKFILE: {
                const char* file_name = cmds[1];
                const char* file_ext  = cmds[2];
                int reserve = NO_RESERVE;
                if (cmds[3]) reserve = atoi(cmds[3]);
                
                cinfo_t file_info = { .type = STAT_FILE };
                str_memcpy(file_info.file_name, file_name, strlen(file_name) + 1);
                str_memcpy(file_info.file_extension, file_ext, strlen(file_ext) + 1);

                char fullname[128] = { 0 };
                sprintf(fullname, "%s.%s", file_name, file_ext);
                name_to_fatname(fullname, file_info.full_name);

                ci_t root_ci;
                if (strlen(current_path) > 1) root_ci = NIFAT32_open_content(NO_RCI, current_path, DF_MODE);
                else root_ci = NIFAT32_open_content(NO_RCI, NULL, DF_MODE);
                if (root_ci < 0) fprintf(stderr, "Can't open file!\n");
                else {
                    NIFAT32_put_content(root_ci, &file_info, reserve);
                    NIFAT32_close_content(root_ci);
                }
            }
            break;

            case MKDIR: {
                const char* name = cmds[1];
                
                cinfo_t dir_info = { .type = STAT_DIR };
                str_memcpy(dir_info.file_name, name, strlen(name) + 1);
                name_to_fatname(name, dir_info.full_name);

                ci_t root_ci;
                if (strlen(current_path) > 1) root_ci = NIFAT32_open_content(NO_RCI, current_path, DF_MODE);
                else root_ci = NIFAT32_open_content(NO_RCI, NULL, DF_MODE);
                if (root_ci < 0) fprintf(stderr, "Can't open file!\n");
                else {
                    NIFAT32_put_content(root_ci, &dir_info, NO_RESERVE);
                    NIFAT32_close_content(root_ci);
                }
            }
            break;

            case FRENAME: {
                const char* name = cmds[1];
                char src_path[128] = { 0 };
                name_to_fatname(name, src_path);

                ci_t src = NIFAT32_open_content(NO_RCI, src_path, DF_MODE);
                if (src >= 0) {
                    const char* file_name = cmds[2];
                    const char* file_ext  = cmds[3];
                    cinfo_t file_info = { .type = STAT_FILE, .size = 12 };
                    str_memcpy(file_info.file_name, file_name, strlen(file_name) + 1);
                    str_memcpy(file_info.file_extension, file_ext, strlen(file_ext) + 1);

                    char fullname[128] = { 0 };
                    sprintf(fullname, "%s.%s", file_name, file_ext);
                    name_to_fatname(fullname, file_info.full_name);
                    NIFAT32_change_meta(src, &file_info);
                    NIFAT32_close_content(src);
                }
            }
            break;

            case RM: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(NO_RCI, path_buffer, DF_MODE);
                if (ci >= 0) NIFAT32_delete_content(ci);
                else printf("Content not found!\n");
            }
            break;

            case READ: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(NO_RCI, path_buffer, DF_MODE);
                if (ci >= 0) {
                    char content[512] = { 0 };
                    NIFAT32_read_content2buffer(ci, 0, (buffer_t)content, sizeof(content));
                    printf("%s\n", content);
                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;
            
            case WRITE: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                name_to_fatname(cmds[1], fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(NO_RCI, path_buffer, DF_MODE);
                if (ci >= 0) {
                    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)cmds[2], 512);
                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;
            
            case TRUNC: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                name_to_fatname(cmds[1], fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                int offset = atoi(cmds[2]);
                int size   = atoi(cmds[3]);
                ci_t ci = NIFAT32_open_content(NO_RCI, path_buffer, DF_MODE);
                if (ci >= 0) {
                    NIFAT32_truncate_content(ci, offset, size);
                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;

            case LS: {
                ci_t ci = -1;
                if (strlen(current_path) > 1) ci = NIFAT32_open_content(NO_RCI, current_path, DF_MODE);
                else ci = NIFAT32_open_content(NO_RCI, NULL, DF_MODE);
                if (ci >= 0) {
                    unsigned char cluster_data[8192] = { 0 };
                    NIFAT32_read_content2buffer(ci, 0, (buffer_t)cluster_data, 8192);

                    unsigned char decoded[4096] = { 0 };
                    unpack_memory((encoded_t*)cluster_data, decoded, sizeof(decoded));

                    unsigned int entries = (sizeof(cluster_data) / sizeof(short)) / sizeof(directory_entry_t);
                    directory_entry_t* entry = (directory_entry_t*)decoded;
                    for (unsigned int i = 0; i < entries; i++, entry++) {
                        if (entry->file_name[0] == ENTRY_END) break;
                        if (entry->file_name[0] != ENTRY_FREE) {
                            char name[128] = { 0 };
                            fatname_to_name((const char*)entry->file_name, name);
                            printf("%s\t%u\n", name, entry->file_size);
                        }
                    }

                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;
            
            case RS: {
                int sector = atoi(cmds[1]);
                char buffer[sector_size];
                DSK_readoff_sectors(sector, 0, buffer, sector_size, 1);
                printf("Data: [%s]\n", buffer);
                printf("Data (hex):\n");
                for (int i = 0; i < sector_size; i++) {
                    printf("%02X ", (unsigned char)buffer[i]);
                    if ((i + 1) % 16 == 0) printf("\n");
                }

                printf("\n");
            }
            break;

            default: break;
        }
    }

    NIFAT32_unload();
    return EXIT_SUCCESS;
}
