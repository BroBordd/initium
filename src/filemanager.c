/* src/filemanager.c */
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "filemanager.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"

#define MAX_ENTRIES 128

struct file_entry {
    char name[128];
    int is_dir;
    long long size;
};

static struct file_entry g_entries[MAX_ENTRIES];
static int g_entry_count = 0;
static float g_fm_scroll_y = 0.0f;
static char g_cur_path[512] = "/";
static fm_mode_t g_mode = FM_MODE_BROWSE;
static fm_pick_callback_t g_on_pick = NULL;

static void scan_directory(const char *path)
{
    g_entry_count = 0;
    g_fm_scroll_y = 0.0f;

    DIR *dir = opendir(path);
    if (!dir) return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && g_entry_count < MAX_ENTRIES) {
        if (strcmp(ent->d_name, ".") == 0) continue;

        struct file_entry *e = &g_entries[g_entry_count++];
        snprintf(e->name, sizeof(e->name), "%s", ent->d_name);

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", strcmp(path, "/") == 0 ? "" : path, ent->d_name);

        struct stat st;
        if (stat(full, &st) == 0) {
            e->is_dir = S_ISDIR(st.st_mode);
            e->size = st.st_size;
        } else {
            e->is_dir = (ent->d_type == DT_DIR);
            e->size = 0;
        }
    }
    closedir(dir);
}

void fm_init(fm_mode_t mode, const char *start_path, fm_pick_callback_t on_pick)
{
    g_mode = mode;
    g_on_pick = on_pick;
    snprintf(g_cur_path, sizeof(g_cur_path), "%s", (start_path && start_path[0]) ? start_path : "/");
    scan_directory(g_cur_path);
}

void fm_scroll_delta(float delta_y)
{
    g_fm_scroll_y += delta_y;
    if (g_fm_scroll_y < 0.0f) g_fm_scroll_y = 0.0f;
    float max_scroll = (float)(g_entry_count * (BUTTON_HEIGHT + BUTTON_GAP) - 600);
    if (max_scroll < 0.0f) max_scroll = 0.0f;
    if (g_fm_scroll_y > max_scroll) g_fm_scroll_y = max_scroll;
}

void fm_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    const char *title = (g_mode == FM_MODE_PICKER) ? "SELECT BINARY TO EXECUTE" : "FILE EXPLORER";
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14, title, FONT_SIZE_TITLE, COLOR_TITLE_TXT);

    char path_buf[512];
    snprintf(path_buf, sizeof(path_buf), "Path: %s", g_cur_path);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 66, path_buf, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    int start_y = topbar_h + 20 - (int)g_fm_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    for (int i = 0; i < g_entry_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound)
            continue;

        struct file_entry *e = &g_entries[i];

        unsigned int border = e->is_dir ? COLOR_TOPBAR_ACCENT : COLOR_CARD_BORDER;
        fb_draw_card(UI_PADDING_X, card_y, card_w, BUTTON_HEIGHT, COLOR_CARD_BG, border);

        /* Vector Icon Rendering (Folders vs Binaries vs Documents) */
        if (e->is_dir) {
            vector_draw_icon(UI_PADDING_X + 24, card_y + 36, 48, VEC_ICON_FOLDER, COLOR_CHECK_ON);
        } else if (g_mode == FM_MODE_PICKER || strstr(e->name, ".sh") || strstr(e->name, ".bin")) {
            vector_draw_icon(UI_PADDING_X + 24, card_y + 36, 48, VEC_ICON_BINARY, COLOR_TOPBAR_ACCENT);
        } else {
            vector_draw_icon(UI_PADDING_X + 24, card_y + 36, 48, VEC_ICON_FILE, COLOR_SUBTITLE_TXT);
        }

        font_draw_text(UI_PADDING_X + 90, card_y + 24, e->name, FONT_SIZE_BODY, COLOR_CARD_TXT);

        char sub[64];
        if (e->is_dir)
            snprintf(sub, sizeof(sub), "Directory");
        else
            snprintf(sub, sizeof(sub), "%lld bytes", e->size);
        font_draw_text(UI_PADDING_X + 90, card_y + 70, sub, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);
    }
}

void fm_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    int start_y = topbar_h + 20 - (int)g_fm_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    for (int i = 0; i < g_entry_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound)
            continue;

        if (x >= UI_PADDING_X && x <= UI_PADDING_X + card_w &&
            y >= card_y && y <= card_y + BUTTON_HEIGHT) {
            trigger_vibration();
            struct file_entry *e = &g_entries[i];

            char target[1024];
            snprintf(target, sizeof(target), "%s/%s",
                     strcmp(g_cur_path, "/") == 0 ? "" : g_cur_path, e->name);

            if (e->is_dir) {
                snprintf(g_cur_path, sizeof(g_cur_path), "%s", target);
                scan_directory(g_cur_path);
            } else if (g_mode == FM_MODE_PICKER && g_on_pick) {
                g_on_pick(target);
            }
            break;
        }
    }
}
