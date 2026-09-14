/* log.c */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdarg.h>
#include "log.h"
#include "config.h"

static int g_log_fd = -1;

void log_init(void)
{
    g_log_fd = open("/dev/kmsg", O_WRONLY);
}

void dbgf(const char *fmt, ...)
{
    if (g_log_fd < 0) return;

    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (n > 0) {
        size_t len = (size_t)n < sizeof(buf) ? (size_t)n : sizeof(buf) - 1;
        write(g_log_fd, buf, len);
    }
}

void die(const char *reason)
{
    dbgf(LOGPFX "die: %s -- triggering kernel panic\n", reason);
    if (g_log_fd >= 0) close(g_log_fd);
    _exit(0);
}
