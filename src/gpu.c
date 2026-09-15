/* src/gpu.c */
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include "gpu.h"
#include "config.h"
#include "log.h"

/*
 * Mali kbase ioctl numbers:
 * Midgard/Bifrost/Valhall kbase header specifications
 */
#define KBASE_IOCTL_TYPE 0x80

struct kbase_ioctl_version_check {
    unsigned short major;
    unsigned short minor;
};

#define KBASE_IOCTL_VERSION_CHECK \
    _IOWR(KBASE_IOCTL_TYPE, 0, struct kbase_ioctl_version_check)

struct kbase_ioctl_set_flags {
    unsigned int create_flags;
};

#define KBASE_IOCTL_SET_FLAGS \
    _IOW(KBASE_IOCTL_TYPE, 1, struct kbase_ioctl_set_flags)

struct gpu_status g_gpu = {0};

static int mknod_mali0(void)
{
    int major = 10, minor = 0; /* Default misc major */
    int fd = open(MALI_SYSFS_DEV, O_RDONLY);
    if (fd >= 0) {
        char buf[32];
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0) {
            buf[n] = '\0';
            sscanf(buf, "%d:%d", &major, &minor);
        }
    }

    if (mknod(MALI_DEV_NODE, S_IFCHR | 0666, makedev((unsigned)major, (unsigned)minor)) < 0) {
        if (errno != EEXIST) {
            dbgf(LOGPFX "mknod %s failed: %d (%s)\n", MALI_DEV_NODE, errno, strerror(errno));
            return 0;
        }
    }
    return 1;
}

int gpu_init(void)
{
    g_gpu.is_available = 0;
    g_gpu.fd = -1;

    mknod_mali0();

    g_gpu.fd = open(MALI_DEV_NODE, O_RDWR | O_CLOEXEC);
    if (g_gpu.fd < 0) {
        dbgf(LOGPFX "Mali GPU open(%s) failed: %s\n", MALI_DEV_NODE, strerror(errno));
        return 0;
    }

    /* Handshake with Mali kbase driver */
    struct kbase_ioctl_version_check ver = {0, 0};
    if (ioctl(g_gpu.fd, KBASE_IOCTL_VERSION_CHECK, &ver) < 0) {
        dbgf(LOGPFX "Mali ioctl(VERSION_CHECK) failed: %s\n", strerror(errno));
    } else {
        g_gpu.major_version = ver.major;
        g_gpu.minor_version = ver.minor;
        dbgf(LOGPFX "ARM Mali kbase driver active! DDK Version: %u.%u\n", ver.major, ver.minor);
    }

    /* Set default context flags to bring core power domains out of sleep */
    struct kbase_ioctl_set_flags flags = {0};
    ioctl(g_gpu.fd, KBASE_IOCTL_SET_FLAGS, &flags);

    g_gpu.is_available = 1;
    return 1;
}

void gpu_kick_keepalive(void)
{
    if (g_gpu.fd >= 0) {
        /* Keep GPU clock domain and PM alive */
        struct kbase_ioctl_version_check ver = {0, 0};
        ioctl(g_gpu.fd, KBASE_IOCTL_VERSION_CHECK, &ver);
    }
}

void gpu_cleanup(void)
{
    if (g_gpu.fd >= 0) {
        close(g_gpu.fd);
        g_gpu.fd = -1;
    }
    g_gpu.is_available = 0;
}
