#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include "mount.h"
#include "framebuffer.h"
#include "font.h"
#include "hardware.h"
#include "config.h"

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

    if (strcmp(name, "system") == 0)
        snprintf(p->mountpoint, sizeof(p->mountpoint), "/system");
    else if (strcmp(name, "vendor") == 0)
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
                add_partition(partname, devpath);
            }
        }
        fclose(fp);
    }
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

static void toggle_mount(struct part_info *p)
{
    if (p->is_mounted) {
        if (umount2(p->mountpoint, MNT_DETACH) == 0 || umount(p->mountpoint) == 0) {
            p->is_mounted = 0;
        }
    } else {
        mkdir("/mnt", 0755);
        mkdir(p->mountpoint, 0755);

        const char *fs_types[] = {"ext4", "f2fs", "erofs", "vfat"};
        for (size_t i = 0; i < sizeof(fs_types)/sizeof(fs_types[0]); i++) {
            if (mount(p->devpath, p->mountpoint, fs_types[i], MS_NOATIME, NULL) == 0) {
                p->is_mounted = 1;
                break;
            }
        }
    }
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
