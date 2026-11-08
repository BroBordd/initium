#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <errno.h>
#include "mount.h"
#include "framebuffer.h"
#include "font.h"
#include "hardware.h"
#include "config.h"
#include "log.h"

#define MAX_PARTS 48

struct part_info {
    char name[32];
    char devpath[128];
    char mountpoint[128];
    int is_mounted;
};

static struct part_info g_parts[MAX_PARTS];
static int g_part_count = 0;
static float g_mount_scroll_y = 0.0f;

static int is_path_mounted(const char *devpath, const char *mountpoint)
{
    FILE *fp = fopen("/proc/mounts", "r");
    if (!fp) return 0;

    char dev[256], mnt[256], type[64], opts[256];
    int dummy1, dummy2;

    while (fscanf(fp, "%255s %255s %63s %255s %d %d",
                  dev, mnt, type, opts, &dummy1, &dummy2) == 6) {
        if (strcmp(dev, devpath) == 0 || strcmp(mnt, mountpoint) == 0) {
            fclose(fp);
            return 1;
        }
    }
    fclose(fp);
    return 0;
}

static void add_partition(const char *name, const char *devpath)
{
    if (g_part_count >= MAX_PARTS) return;
    if (strncmp(name, "loop", 4) == 0 || strncmp(name, "ram", 3) == 0) return;

    for (int i = 0; i < g_part_count; i++) {
        if (strcmp(g_parts[i].name, name) == 0) return;
    }

    struct part_info *p = &g_parts[g_part_count++];
    snprintf(p->name, sizeof(p->name), "%s", name);
    snprintf(p->devpath, sizeof(p->devpath), "%s", devpath);

    if (strcmp(name, "system") == 0 || strcmp(name, "system_a") == 0 || strcmp(name, "system_b") == 0)
        snprintf(p->mountpoint, sizeof(p->mountpoint), "/system");
    else if (strcmp(name, "vendor") == 0 || strcmp(name, "vendor_a") == 0 || strcmp(name, "vendor_b") == 0)
        snprintf(p->mountpoint, sizeof(p->mountpoint), "/vendor");
    else if (strcmp(name, "userdata") == 0 || strcmp(name, "data") == 0)
        snprintf(p->mountpoint, sizeof(p->mountpoint), "/data");
    else
        snprintf(p->mountpoint, sizeof(p->mountpoint), "/mnt/%s", name);

    p->is_mounted = is_path_mounted(p->devpath, p->mountpoint);
}

static void scan_partitions(void)
{
    g_part_count = 0;

    mkdir("/dev/block", 0755);
    mkdir("/dev/block/by-name", 0755);

    /* 1. Primary method: scan /sys/class/block to discover GPT partition names (PARTNAME) */
    DIR *blkdir = opendir("/sys/class/block");
    if (blkdir) {
        struct dirent *ent;
        while ((ent = readdir(blkdir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            if (strncmp(ent->d_name, "loop", 4) == 0 || strncmp(ent->d_name, "ram", 3) == 0) continue;

            char uevent_path[256];
            snprintf(uevent_path, sizeof(uevent_path), "/sys/class/block/%s/uevent", ent->d_name);

            FILE *ufp = fopen(uevent_path, "r");
            if (!ufp) continue;

            int major = -1, minor = -1;
            char partname[64] = "";
            char devname[64] = "";
            char line[128];

            while (fgets(line, sizeof(line), ufp)) {
                size_t len = strlen(line);
                if (len > 0 && line[len - 1] == '\n') line[len - 1] = '\0';

                if (strncmp(line, "MAJOR=", 6) == 0) major = atoi(line + 6);
                else if (strncmp(line, "MINOR=", 6) == 0) minor = atoi(line + 6);
                else if (strncmp(line, "PARTNAME=", 9) == 0) snprintf(partname, sizeof(partname), "%s", line + 9);
                else if (strncmp(line, "DEVNAME=", 8) == 0) snprintf(devname, sizeof(devname), "%s", line + 8);
            }
            fclose(ufp);

            if (devname[0] == '\0') snprintf(devname, sizeof(devname), "%s", ent->d_name);

            if (major >= 0 && minor >= 0) {
                char node_path[128];
                snprintf(node_path, sizeof(node_path), "/dev/block/%s", devname);
                mknod(node_path, S_IFBLK | 0660, makedev((unsigned)major, (unsigned)minor));

                if (partname[0] != '\0') {
                    char byname_path[128];
                    snprintf(byname_path, sizeof(byname_path), "/dev/block/by-name/%s", partname);
                    unlink(byname_path);
                    symlink(node_path, byname_path);

                    add_partition(partname, byname_path);

                    /* Provide non-slot alias for slot-based A/B partitions */
                    if (strcmp(partname, "vendor_a") == 0 || strcmp(partname, "vendor_b") == 0) {
                        char alias[128];
                        snprintf(alias, sizeof(alias), "/dev/block/by-name/vendor");
                        unlink(alias);
                        symlink(node_path, alias);
                        add_partition("vendor", alias);
                    }
                    if (strcmp(partname, "system_a") == 0 || strcmp(partname, "system_b") == 0) {
                        char alias[128];
                        snprintf(alias, sizeof(alias), "/dev/block/by-name/system");
                        unlink(alias);
                        symlink(node_path, alias);
                        add_partition("system", alias);
                    }
                } else {
                    add_partition(devname, node_path);
                }
            }
        }
        closedir(blkdir);
    }

    /* 2. Traditional by-name / mapper scan */
    const char *dirs[] = {
        "/dev/block/by-name",
        "/dev/block/bootdevice/by-name",
        "/dev/block/mapper"
    };

    for (size_t d = 0; d < sizeof(dirs)/sizeof(dirs[0]); d++) {
        DIR *dir = opendir(dirs[d]);
        if (!dir) continue;

        struct dirent *ent;
        while ((ent = readdir(dir)) != NULL) {
            if (ent->d_name[0] == '.') continue;
            char devpath[128];
            snprintf(devpath, sizeof(devpath), "%s/%s", dirs[d], ent->d_name);
            add_partition(ent->d_name, devpath);
        }
        closedir(dir);
    }

    /* 3. Fallback scan /proc/partitions */
    FILE *fp = fopen("/proc/partitions", "r");
    if (fp) {
        char line[128];
        while (fgets(line, sizeof(line), fp)) {
            int major, minor;
            long long blocks;
            char partname[64];
            if (sscanf(line, "%d %d %lld %63s", &major, &minor, &blocks, partname) == 4) {
                char devpath[128];
                snprintf(devpath, sizeof(devpath), "/dev/block/%s", partname);
                mknod(devpath, S_IFBLK | 0660, makedev((unsigned)major, (unsigned)minor));
                add_partition(partname, devpath);
            }
        }
        fclose(fp);
    }
}

static void toggle_mount(struct part_info *p)
{
    if (p->is_mounted) {
        if (umount2(p->mountpoint, MNT_DETACH) == 0 || umount(p->mountpoint) == 0) {
            p->is_mounted = 0;
        }
    } else {
        mkdir("/mnt", 0755);
        mkdir(p->mountpoint, 0755);

        const char *fs_types[] = {"ext4", "erofs", "f2fs", "vfat"};
        int is_ro = (strcmp(p->mountpoint, "/system") == 0 || strcmp(p->mountpoint, "/vendor") == 0);
        unsigned long flags = MS_NOATIME | (is_ro ? MS_RDONLY : 0);

        for (size_t i = 0; i < sizeof(fs_types)/sizeof(fs_types[0]); i++) {
            if (mount(p->devpath, p->mountpoint, fs_types[i], flags, NULL) == 0) {
                p->is_mounted = 1;
                break;
            }
            if (!(flags & MS_RDONLY) &&
                mount(p->devpath, p->mountpoint, fs_types[i], flags | MS_RDONLY, NULL) == 0) {
                p->is_mounted = 1;
                break;
            }
        }
    }
}

void handle_firmware_requests(void)
{
    DIR *dir = opendir("/sys/class/firmware");
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        if (strcmp(ent->d_name, "timeout") == 0) continue;

        char fw_name[128];
        snprintf(fw_name, sizeof(fw_name), "%s", ent->d_name);

        char loading_path[256], data_path[256];
        snprintf(loading_path, sizeof(loading_path), "/sys/class/firmware/%s/loading", fw_name);
        snprintf(data_path, sizeof(data_path), "/sys/class/firmware/%s/data", fw_name);

        const char *fw_dirs[] = {
            "/vendor/firmware",
            "/vendor/firmware_mnt/image",
            "/system/etc/firmware",
            "/etc/firmware",
            "/firmware/image"
        };

        char file_path[512];
        int fw_src_fd = -1;
        for (size_t i = 0; i < sizeof(fw_dirs)/sizeof(fw_dirs[0]); i++) {
            snprintf(file_path, sizeof(file_path), "%s/%s", fw_dirs[i], fw_name);
            fw_src_fd = open(file_path, O_RDONLY);
            if (fw_src_fd >= 0) break;
        }

        int lfd = open(loading_path, O_WRONLY);
        if (lfd < 0) {
            if (fw_src_fd >= 0) close(fw_src_fd);
            continue;
        }

        if (fw_src_fd >= 0) {
            write(lfd, "1\n", 2);
            int dfd = open(data_path, O_WRONLY);
            if (dfd >= 0) {
                char buf[4096];
                ssize_t nr;
                while ((nr = read(fw_src_fd, buf, sizeof(buf))) > 0) {
                    write(dfd, buf, (size_t)nr);
                }
                close(dfd);
                write(lfd, "0\n", 2);
                dbgf(LOGPFX "firmware: successfully loaded %s from %s\n", fw_name, file_path);
            } else {
                write(lfd, "-1\n", 3);
            }
            close(fw_src_fd);
        } else {
            /* Firmware not found: abort sysfs wait immediately instead of blocking driver probe */
            write(lfd, "-1\n", 3);
            dbgf(LOGPFX "firmware: %s not found, aborted sysfs wait\n", fw_name);
        }
        close(lfd);
    }
    closedir(dir);
}

void mount_critical_early(void)
{
    /* 1. Cap firmware timeout to 1 second so probe requests never block the kernel */
    int to_fd = open("/sys/class/firmware/timeout", O_WRONLY);
    if (to_fd >= 0) {
        write(to_fd, "1\n", 2);
        close(to_fd);
        dbgf(LOGPFX "sysfs firmware timeout configured to 1s\n");
    }

    /* 2. Populate block device nodes from sysfs and mount /vendor */
    scan_partitions();
    for (int i = 0; i < g_part_count; i++) {
        if ((strcmp(g_parts[i].name, "vendor") == 0 ||
             strcmp(g_parts[i].name, "vendor_a") == 0 ||
             strcmp(g_parts[i].name, "vendor_b") == 0) && !g_parts[i].is_mounted) {
            toggle_mount(&g_parts[i]);
            if (g_parts[i].is_mounted) {
                dbgf(LOGPFX "early mounted %s on %s (%s)\n", g_parts[i].name, g_parts[i].mountpoint, g_parts[i].devpath);
                break;
            }
        }
    }

    /* 3. Tell the kernel to search /vendor/firmware directly for request_firmware() */
    int fw_fd = open("/sys/module/firmware_class/parameters/path", O_WRONLY);
    if (fw_fd >= 0) {
        write(fw_fd, "/vendor/firmware", strlen("/vendor/firmware"));
        close(fw_fd);
        dbgf(LOGPFX "configured firmware_class.path to /vendor/firmware\n");
    }

    /* 4. Provide standard filesystem symlinks */
    mkdir("/lib", 0755);
    mkdir("/etc", 0755);
    unlink("/lib/firmware");
    unlink("/etc/firmware");
    symlink("/vendor/firmware", "/lib/firmware");
    symlink("/vendor/firmware", "/etc/firmware");

    /* 5. Fulfill or abort any already-pending sysfs firmware requests */
    handle_firmware_requests();
}

void mount_screen_init(void)
{
    g_mount_scroll_y = 0.0f;
    scan_partitions();
}

void mount_screen_scroll(float delta_y)
{
    g_mount_scroll_y += delta_y;
}

void mount_screen_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "STORAGE & PARTITIONS", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64,
                   "Dynamic Mount Subsystem // Tap to Toggle", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    float max_scroll = (float)(g_part_count * (BUTTON_HEIGHT + 14) - (g_fb.yres - topbar_h - NAV_BAR_HEIGHT));
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (g_mount_scroll_y < 0.0f) g_mount_scroll_y *= 0.75f;
    else if (g_mount_scroll_y > max_scroll) g_mount_scroll_y = max_scroll + (g_mount_scroll_y - max_scroll) * 0.75f;

    int start_y = topbar_h + 20 - (int)g_mount_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    fb_set_clip(0, topbar_h + 4, g_fb.xres, bottom_bound - topbar_h - 4);

    for (int i = 0; i < g_part_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + 14);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound) continue;

        struct part_info *p = &g_parts[i];
        fb_draw_card(UI_PADDING_X, card_y, card_w, BUTTON_HEIGHT, COLOR_CARD_BG, COLOR_CARD_BORDER);

        int check_color = p->is_mounted ? COLOR_CHECK_ON : COLOR_CHECK_OFF;
        fb_draw_card(UI_PADDING_X + 24, card_y + 36, 48, 48, COLOR_CANVAS, check_color);
        if (p->is_mounted) font_draw_text(UI_PADDING_X + 38, card_y + 44, "X", FONT_SIZE_BODY, COLOR_CHECK_ON);

        char title_buf[64];
        snprintf(title_buf, sizeof(title_buf), "%s", p->name);
        font_draw_text(UI_PADDING_X + 90, card_y + 22, title_buf, FONT_SIZE_BODY, COLOR_CARD_TXT);

        char sub_buf[128];
        snprintf(sub_buf, sizeof(sub_buf), "%s -> %s (%s)",
                 p->devpath, p->mountpoint, p->is_mounted ? "MOUNTED" : "UNMOUNTED");
        font_draw_text(UI_PADDING_X + 90, card_y + 68, sub_buf, FONT_SIZE_SUBTITLE,
                       p->is_mounted ? COLOR_CHECK_ON : COLOR_SUBTITLE_TXT);
    }
    fb_clear_clip();
}

void mount_screen_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    int start_y = topbar_h + 20 - (int)g_mount_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    for (int i = 0; i < g_part_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + 14);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound) continue;

        if (x >= UI_PADDING_X && x <= UI_PADDING_X + card_w &&
            y >= card_y && y <= card_y + BUTTON_HEIGHT) {
            trigger_vibration();
            toggle_mount(&g_parts[i]);
            break;
        }
    }
}
