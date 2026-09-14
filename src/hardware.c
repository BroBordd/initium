/* hardware.c */
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <linux/watchdog.h>
#include "hardware.h"
#include "config.h"
#include "log.h"

static int g_watchdog_fd = -1;
static unsigned long g_heartbeat_counter = 0;

void init_power_and_signals(void)
{
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP,  SIG_IGN);
    signal(SIGTERM, SIG_IGN);
    signal(SIGINT,  SIG_IGN);

    int as_fd = open(AUTOSLEEP_SYSFS, O_WRONLY);
    if (as_fd >= 0) {
        write(as_fd, "off\n", 4);
        close(as_fd);
        dbgf(LOGPFX "disabled autosleep\n");
    }

    int wl_fd = open(WAKELOCK_SYSFS, O_WRONLY);
    if (wl_fd >= 0) {
        write(wl_fd, WAKELOCK_TAG, strlen(WAKELOCK_TAG));
        close(wl_fd);
        dbgf(LOGPFX "acquired wakelock: %s\n", WAKELOCK_TAG);
    }
}

void init_watchdog(void)
{
    int major = 10, minor = 130;
    int fd = open(WATCHDOG_SYS_DEV, O_RDONLY);
    if (fd >= 0) {
        char buf[32];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            sscanf(buf, "%d:%d", &major, &minor);
        }
    }

    mknod(WATCHDOG_DEV_NODE, S_IFCHR | 0600, makedev((unsigned)major, (unsigned)minor));
    g_watchdog_fd = open(WATCHDOG_DEV_NODE, O_WRONLY | O_NONBLOCK);
    if (g_watchdog_fd >= 0)
        dbgf(LOGPFX "watchdog opened on %s (%d:%d)\n", WATCHDOG_DEV_NODE, major, minor);
    else
        dbgf(LOGPFX "watchdog unavailable (errno=%d)\n", errno);
}

void kick_watchdog(void)
{
    g_heartbeat_counter++;
    if (g_watchdog_fd >= 0) {
        ioctl(g_watchdog_fd, WDIOC_KEEPALIVE, 0);
        write(g_watchdog_fd, "\0", 1);
    }
}

unsigned long get_heartbeat_counter(void)
{
    return g_heartbeat_counter;
}

void trigger_vibration(void)
{
    int fd = open(VIBRATOR_ENABLE_PATH, O_WRONLY);
    if (fd < 0)
        fd = open("/sys/devices/virtual/timed_output/vibrator/enable", O_WRONLY);

    if (fd >= 0) {
        write(fd, VIBRATOR_PULSE_LEN, strlen(VIBRATOR_PULSE_LEN));
        close(fd);
    }
}

int mknod_fb0(void)
{
    int fd = open(FB_SYSFS_DEV_FILE, O_RDONLY);
    if (fd < 0) return 0;

    char buf[64];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (n <= 0) return 0;

    int major = -1, minor = -1;
    if (sscanf(buf, "%d:%d", &major, &minor) != 2) return 0;

    mkdir("/dev/graphics", 0755);
    dev_t devno = makedev((unsigned)major, (unsigned)minor);
    if (mknod(FB_DEV_NODE, S_IFCHR | 0660, devno) < 0 && errno != EEXIST)
        return 0;

    return 1;
}

void setup_input_devnodes(void)
{
    mkdir("/dev/input", 0755);

    for (int i = 0; i <= 7; i++) {
        char devpath[64], syspath[64];
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        snprintf(syspath, sizeof(syspath), "/sys/class/input/event%d/dev", i);

        int major = 13, minor = 64 + i;
        int fd = open(syspath, O_RDONLY);
        if (fd >= 0) {
            char buf[32];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            close(fd);
            if (n > 0) {
                buf[n] = '\0';
                sscanf(buf, "%d:%d", &major, &minor);
            }
        }
        mknod(devpath, S_IFCHR | 0660, makedev((unsigned)major, (unsigned)minor));
    }
}

int *__errno_location(void);
int *__errno(void)
{
    return __errno_location();
}

void __assert_fail(const char *expr, const char *file, int line, const char *func);
void __assert2(const char *file, int line, const char *func, const char *expr)
{
    __assert_fail(expr, file, line, func);
}
