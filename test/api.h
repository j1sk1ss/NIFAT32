#ifndef TESTER_H_
#define TESTER_H_

#include "../nifat32.h"

#define fs_open(path, mode) NIFAT32_open_content(NO_RCI, path, mode)
#define fs_close(fd) NIFAT32_close_content(fd)
#define fs_read(fd, off, buff, size) NIFAT32_read_content2buffer(fd, off, buff, size)
#define fs_write(fd, off, buff, size) NIFAT32_write_buffer2content(fd, off, buff, size)
#define fs_copy(fd_src, fd_dst) NIFAT32_copy_content(fd_src, fd_dst, DEEP_COPY)
#define fs_delete(fd) NIFAT32_delete_content(fd)

#endif