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
    UNKNOWN
} cmd_t;

cmd_t _get_cmd(const char* input) {
    if (!strcmp(input, "cd"))         return CD;
    else if (!strcmp(input, "read"))  return READ;
    else if (!strcmp(input, "write")) return WRITE;
    else if (!strcmp(input, "ls"))    return LS;
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
            
            case READ: {
                char path_buffer[256] = { 0 };
                strcpy(path_buffer, current_path);

                char fatbuffer[24] = { 0 };
                const char* path = cmds[1];
                name_to_fatname(path, fatbuffer);

                if (strlen(path_buffer) > 1 && strcmp(path_buffer, "/")) strcat(path_buffer, "/");
                strcat(path_buffer, fatbuffer);

                ci_t ci = NIFAT32_open_content(path_buffer);
                if (ci >= 0) {
                    char content[512] = { 0 };
                    NIFAT32_read_content2buffer(ci, 0, (buffer_t)content, 512);
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

                ci_t ci = NIFAT32_open_content(path_buffer);
                if (ci >= 0) {
                    NIFAT32_write_buffer2content(ci, 0, (const_buffer_t)cmds[2], 512);
                    NIFAT32_close_content(ci);
                }
                else {
                    printf("Content not found!\n");
                }
            }
            break;
            
            case LS:
            break;
            
            default: break;
        }
    }

    return EXIT_SUCCESS;
}
