#include <std/logging.h>

static log_io_t _log_io = {
    .fd_fprintf  = NULL,
    .fd_vfprintf = NULL
};

int LOG_setup(
    int (*fd_fprintf)(const char*, ...),
    int (*fd_vfprintf)(const char*, va_list)
) {
    _log_io.fd_fprintf  = fd_fprintf;
    _log_io.fd_vfprintf = fd_vfprintf;
    return 1;
}

static int _write_log(const char* level, const char* file, int line, const char* message, va_list args) {
    if (!_log_io.fd_fprintf || !_log_io.fd_vfprintf) return 0;
    if (!level)   level   = "(null)";
    if (!file)    file    = "(null)";
    if (!message) message = "(null)";
    _log_io.fd_fprintf("[%s] (%s:%i) ", level, file, line);
    _log_io.fd_vfprintf(message, args);
    _log_io.fd_fprintf("\n");
    return 1;
}

int log_message(const char* level, const char* file, int line, const char* message, ...) {
    va_list args;
    va_start(args, message);
    _write_log(level, file, line, message, args);
    va_end(args);
    return 1;
}