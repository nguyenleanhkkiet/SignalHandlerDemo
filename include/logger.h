#ifndef LOGGER_H
#define LOGGER_H

#include <stdarg.h>

typedef enum {
    LOG_LEVEL_INFO,
    LOG_LEVEL_ERROR
} log_level_t;

/* Return 0 on success; return -1 on failure and set errno. */
int logger_init(const char *path);
int logger_log(log_level_t level, const char *format, ...);
int logger_info(const char *format, ...);
int logger_error(const char *format, ...);
int logger_close(void);

#endif
