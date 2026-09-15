#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <signal.h>
#include <ctype.h>
#include "processes.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"

#define MAX_PROCS 128

struct proc_card {
    int pid;
    char name[64];
    char state[16];
    long rss_pages;
};

static struct proc_card g_procs[MAX_PROCS];
static int g_proc_count = 0;
static float g_proc_scroll_y = 0.0f;

static void scan_running_processes(void)
{
    g_proc_count = 0;
    DIR *proc = opendir("/proc");
    if (!proc) return;

    struct dirent *ent;
    while ((ent = readdir(proc)) != NULL && g_proc_count < MAX_PROCS) {
        if (!isdigit(ent->d_name[0])) continue;

        int pid = 0;
        sscanf(ent->d_name, "%d", &pid);
        if (pid <= 0) continue;

        char stat_path[64];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", pid);

        FILE *fp = fopen(stat_path, "r");
        if (!fp) continue;

        char comm[64];
        char state_ch;
        if (fscanf(fp, "%*d (%63[^)]) %c", comm, &state_ch) >= 2) {
            struct proc_card *pc = &g_procs[g_proc_count++];
            pc->pid = pid;
            snprintf(pc->name, sizeof(pc->name), "%s", comm);
            snprintf(pc->state, sizeof(pc->state), "%c", state_ch);
            pc->rss_pages = 0; // Simplified for speed
        }
        fclose(fp);
    }
    closedir(proc);
}

void processes_init(void)
{
    g_proc_scroll_y = 0.0f;
    scan_running_processes();
}

void processes_scroll(float delta_y)
{
    g_proc_scroll_y += delta_y;
}

void processes_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "PROCESS VIEWER", FONT_SIZE_TITLE, COLOR_TITLE_TXT);

    char sub[64];
    snprintf(sub, sizeof(sub), "Kernel & Userspace Tasks (%d Running)", g_proc_count);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 66,
                   sub, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    float max_scroll = (float)(g_proc_count * (BUTTON_HEIGHT + BUTTON_GAP) - (g_fb.yres - topbar_h - NAV_BAR_HEIGHT));
    if (max_scroll < 0.0f) max_scroll = 0.0f;

    if (g_proc_scroll_y < 0.0f) {
        g_proc_scroll_y *= 0.75f;
    } else if (g_proc_scroll_y > max_scroll) {
        g_proc_scroll_y = max_scroll + (g_proc_scroll_y - max_scroll) * 0.75f;
    }

    int start_y = topbar_h + 20 - (int)g_proc_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    fb_set_clip(0, topbar_h + 4, g_fb.xres, bottom_bound - topbar_h - 4);

    for (int i = 0; i < g_proc_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound)
            continue;

        struct proc_card *p = &g_procs[i];

        fb_draw_card(UI_PADDING_X, card_y, card_w, BUTTON_HEIGHT, COLOR_CARD_BG, COLOR_CARD_BORDER);
        vector_draw_icon(UI_PADDING_X + 24, card_y + 36, 48, VEC_ICON_BINARY, COLOR_ACCENT_BLUE);

        char title[128];
        snprintf(title, sizeof(title), "%s (PID: %d)", p->name, p->pid);
        font_draw_text(UI_PADDING_X + 90, card_y + 22, title, FONT_SIZE_BODY, COLOR_TITLE_TXT);

        char meta[128];
        snprintf(meta, sizeof(meta), "State: %s", p->state);
        font_draw_text(UI_PADDING_X + 90, card_y + 68, meta, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

        int btn_w = 120;
        int btn_x = UI_PADDING_X + card_w - btn_w - 20;
        fb_draw_card(btn_x, card_y + 28, btn_w, 64, COLOR_CARD_EXIT_BG, COLOR_CARD_EXIT_BORDER);
        font_draw_text(btn_x + 30, card_y + 46, "KILL", FONT_SIZE_SMALL, 0xFFFFFF);
    }

    fb_clear_clip();
}

void processes_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    int start_y = topbar_h + 20 - (int)g_proc_scroll_y;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int bottom_bound = g_fb.yres - NAV_BAR_HEIGHT;

    for (int i = 0; i < g_proc_count; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        if (card_y + BUTTON_HEIGHT < topbar_h || card_y > bottom_bound)
            continue;

        int btn_w = 120;
        int btn_x = UI_PADDING_X + card_w - btn_w - 20;

        if (x >= btn_x && x <= btn_x + btn_w &&
            y >= card_y + 28 && y <= card_y + 92) {
            trigger_vibration();
            if (g_procs[i].pid > 1) {
                kill(g_procs[i].pid, SIGTERM);
                scan_running_processes();
            }
            break;
        }
    }
}
