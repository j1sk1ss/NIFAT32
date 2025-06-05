#include "../include/logging.h"

static int _write_log(const char* level, const char* file, int line, const char* message, va_list args) {
    if (!level)   level   = "(null)";
    if (!file)    file    = "(null)";
    if (!message) message = "(null)";
    fd_fprintf("[%s] (%s:%i) ", level, file, line);
    fd_vfprintf(message, args);
    fd_fprintf("\n");
    return 1;
}

int log_message(const char* level, const char* file, int line, const char* message, ...) {
    va_list args;
    va_start(args, message);
    _write_log(level, file, line, message, args);
    va_end(args);
    return 1;
}