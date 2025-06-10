#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include "nifat32.h"

typedef enum {
    CD,
    READ,
    WRITE,
    LS,
    RM,
    MKDIR,
    MKFILE,
    UNKNOWN
} cmd_t;

cmd_t _get_cmd(const char* input) {
    if (!strcmp(input, "cd"))          return CD;
    else if (!strcmp(input, "read"))   return READ;
    else if (!strcmp(input, "write"))  return WRITE;
    else if (!strcmp(input, "ls"))     return LS;
    else if (!strcmp(input, "rm"))     return RM;
    else if (!strcmp(input, "mkdir"))  return MKDIR;
    else if (!strcmp(input, "mkfile")) return MKFILE;
    return UNKNOWN;
}

#define DISK_PATH   "nifat32.img"
#define SECTOR_SIZE 512

static int disk_fd = 0;
int _mock_sector_read_(sector_addr_t sa, sector_offset_t offset, buffer_t buffer, int buff_size) {
    return pread(disk_fd, buffer, buff_size, sa * SECTOR_SIZE + offset) > 0;
}

int _mock_sector_write_(sector_addr_t sa, sector_offset_t offset, const_buffer_t data, int data_size) {
    return pwrite(disk_fd, data, data_size, sa * SECTOR_SIZE + offset) > 0;
}

int main(int argc, char* argv[]) {
    disk_fd = open(DISK_PATH, O_RDWR);
    if (disk_fd < 0) {
        fprintf(stderr, "%s not found!\n", DISK_PATH);
        return EXIT_FAILURE;
    }

    if (!DSK_setup(_mock_sector_read_, _mock_sector_write_, SECTOR_SIZE)) {
        fprintf(stderr, "DSK_setup() error!\n");
        close(disk_fd);
        return EXIT_FAILURE;
    }

    #define DEFAULT_VOLUME_SIZE (64 * 1024 * 1024)
    if (!NIFAT32_init(0, DEFAULT_VOLUME_SIZE / SECTOR_SIZE)) {
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
upper:
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
            
            case MKFILE: {
                const char* file_name = cmds[1];
                const char* file_ext  = cmds[2];
                
                cinfo_t file_info = { .type = STAT_FILE };
                str_memcpy(file_info.file_name, file_name, strlen(file_name) + 1);
                str_memcpy(file_info.file_extension, file_ext, strlen(file_ext) + 1);

                char fullname[128] = { 0 };
                sprintf(fullname, "%s.%s", file_name, file_ext);
                name_to_fatname(fullname, file_info.full_name);

                ci_t root_ci = PUT_TO_ROOT;
                if (strlen(current_path) > 1) {
                    root_ci = NIFAT32_open_content(current_path, DF_MODE);
                    if (root_ci < 0) {
                        close(disk_fd);
                        return EXIT_FAILURE;
                    }
                }

                NIFAT32_put_content(root_ci, &file_info, NO_RESERVE);
                break;
            }

            case MKDIR: {
                const char* name = cmds[1];
                
                cinfo_t dir_info = { .type = STAT_DIR };
                str_memcpy(dir_info.file_name, name, strlen(name) + 1);
                name_to_fatname(name, dir_info.full_name);

                ci_t root_ci = PUT_TO_ROOT;
                if (strlen(current_path) > 1) {
                    root_ci = NIFAT32_open_content(current_path, DF_MODE);
                    if (root_ci < 0) {
                        close(disk_fd);
                        return EXIT_FAILURE;
                    }
                }

                NIFAT32_put_content(root_ci, &dir_info, NO_RESERVE);
                break;
            }

            case RM: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(path_buffer, DF_MODE);
                if (ci >= 0) NIFAT32_delete_content(ci);
                else printf("Content not found!\n");
                break;
            }

            case READ: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(path_buffer, DF_MODE);
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
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(path_buffer, DF_MODE);
                if (ci >= 0) {
                    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)cmds[2], 512);
                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;
            
            case LS: {
                ci_t ci = -1;
                if (strlen(current_path) > 1) ci = NIFAT32_open_content(current_path, DF_MODE);
                else ci = NIFAT32_open_content(".", DF_MODE);
                if (ci >= 0) {
                    unsigned char cluster_data[4096] = { 0 };
                    NIFAT32_read_content2buffer(ci, 0, (buffer_t)cluster_data, 4096);

                    unsigned char decoded[2048] = { 0 };
                    unpack_memory((encoded_t*)cluster_data, decoded, 2048);

                    unsigned int entries = (4096 / sizeof(short)) / sizeof(directory_entry_t);
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

                break;
            }
            
            default: break;
        }
    }

    return EXIT_SUCCESS;
}
