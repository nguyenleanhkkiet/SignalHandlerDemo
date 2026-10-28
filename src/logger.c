#include "logger.h"

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static int log_fd = -1;

int logger_init(const char *path)
{
    if (path == NULL) {
        errno = EINVAL;
        return -1;
    }
    int saved_errno = errno;
    do {
        log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    } while (log_fd < 0 && errno == EINTR);
    if (log_fd < 0) return -1;
    errno = saved_errno;
    return 0;
}

int logger_close(void)
{
    if (log_fd < 0) return 0;
    int fd = log_fd;
    log_fd = -1;
    if (close(fd) < 0) return -1;
    return 0;
}

static int logger_vwrite(log_level_t level, const char *format, va_list args)
{
    int incoming_errno = errno;
    if (log_fd < 0 || format == NULL ||
        (level != LOG_LEVEL_INFO && level != LOG_LEVEL_ERROR)) {
        errno = log_fd < 0 ? EBADF : EINVAL;
        return -1;
    }

    char message[1024];
    char line[1280];
    struct timespec now;
    struct tm local;
    if (clock_gettime(CLOCK_REALTIME, &now) < 0) return -1;
    if (localtime_r(&now.tv_sec, &local) == NULL) {
        errno = EOVERFLOW;
        return -1;
    }
    int message_length = vsnprintf(message, sizeof(message), format, args);
    if (message_length < 0) return -1;
    const char *level_name = level == LOG_LEVEL_INFO ? "INFO" : "ERROR";
    int length = snprintf(line, sizeof(line), "%04d-%02d-%02d %02d:%02d:%02d.%03ld [%s] [pid=%ld] %s\n",
                          local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
                          local.tm_hour, local.tm_min, local.tm_sec, now.tv_nsec / 1000000,
                          level_name, (long)getpid(), message);
    if (length < 0) return -1;
    if ((size_t)length >= sizeof(line)) length = (int)sizeof(line) - 1;

    struct flock lock = {.l_type = F_WRLCK, .l_whence = SEEK_SET, .l_start = 0, .l_len = 0};
    while (fcntl(log_fd, F_SETLKW, &lock) < 0) {
        if (errno != EINTR) return -1;
    }

    int write_errno = 0;
    size_t written = 0;
    while (written < (size_t)length) {
        ssize_t amount = write(log_fd, line + written, (size_t)length - written);
        if (amount > 0) {
            written += (size_t)amount;
        } else if (amount < 0 && errno == EINTR) {
            continue;
        } else {
            write_errno = amount == 0 ? EIO : errno;
            break;
        }
    }
    lock.l_type = F_UNLCK;
    int unlock_result;
    do {
        unlock_result = fcntl(log_fd, F_SETLK, &lock);
    } while (unlock_result < 0 && errno == EINTR);
    if (write_errno != 0) {
        errno = write_errno;
        return -1;
    }
    if (unlock_result < 0) return -1;
    errno = incoming_errno;
    return 0;
}

int logger_log(log_level_t level, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = logger_vwrite(level, format, args);
    va_end(args);
    return result;
}

int logger_info(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = logger_vwrite(LOG_LEVEL_INFO, format, args);
    va_end(args);
    return result;
}

int logger_error(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    int result = logger_vwrite(LOG_LEVEL_ERROR, format, args);
    va_end(args);
    return result;
}
