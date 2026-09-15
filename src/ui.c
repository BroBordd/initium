#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include "ui.h"
#include "mount.h"
#include "filemanager.h"
#include "terminal.h"
#include "sysmon.h"
#include "processes.h"
#include "recents.h"
#include "bench3d.h"
#include "power.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"
#include "log.h"

static current_view_t g_active_view = VIEW_HOMESCREEN;
static int g_exit_requested = 0;

/* API */
static char g_toast_msg[128];
static int g_toast_timer = 0;
static char g_alert_title[64];
static char g_alert_msg[256];
static int g_alert_active = 0;

/* FPS counter */
static int g_ui_fps = 0;
static unsigned long g_ui_frames = 0;
static struct timespec g_ui_last_time;

#define TOTAL_PAGES 2
static int g_cur_page = 0;
static float g_page_scroll_x = 0.0f;
static float g_target_scroll_x = 0.0f;

struct app_shortcut {
    const char *title;
    vector_icon_t icon;
    unsigned int icon_color;
    current_view_t target_view;
    int is_action;
};

static const struct app_shortcut PAGE0_APPS[6] = {
    {"Execute",  VEC_ICON_BINARY,   0x06B6D4, VIEW_FILEMANAGER, 1},
    {"Storage",  VEC_ICON_STORAGE,  0x10B981, VIEW_MOUNT,       0},
    {"Terminal", VEC_ICON_TERMINAL, 0x8B5CF6, VIEW_TERMINAL,    0},
    {"Files",    VEC_ICON_FOLDER,   0xF59E0B, VIEW_FILEMANAGER, 0},
    {"3D Bench", VEC_ICON_3D,       0xEC4899, VIEW_BENCH3D,     0},
    {"Sensors",  VEC_ICON_SENSOR,   0x06B6D4, VIEW_SYSMON,      0}
};

static const struct app_shortcut PAGE1_APPS[3] = {
    {"Processes",VEC_ICON_PROCESSES,0x6366F1, VIEW_PROCESSES,   0},
    {"Power",    VEC_ICON_POWER,    0xEF4444, VIEW_POWER,       0},
    {"Recents",  VEC_ICON_RECENTS,  0x38BDF8, VIEW_RECENTS,     0}
};

int ui_is_exit_requested(void) { return g_exit_requested; }
current_view_t ui_get_current_view(void) { return g_active_view; }
void ui_return_to_home(void) { g_active_view = VIEW_HOMESCREEN; kb_hide(); }

void ui_show_toast(const char *msg) {
    snprintf(g_toast_msg, sizeof(g_toast_msg), "%s", msg);
    g_toast_timer = 120;
}

void ui_show_alert(const char *title, const char *msg) {
    snprintf(g_alert_title, sizeof(g_alert_title), "%s", title);
    snprintf(g_alert_msg, sizeof(g_alert_msg), "%s", msg);
    g_alert_active = 1;
}

static char g_exec_path[256];
static void on_args_submit(const char *args) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "%s %s", g_exec_path, args);
    g_active_view = VIEW_TERMINAL;
    terminal_execute_command(cmd);
}
static void on_executable_picked(const char *path) {
    snprintf(g_exec_path, sizeof(g_exec_path), "%s", path);
    kb_show("args: ", on_args_submit);
}

void ui_init(void) {
    g_active_view = VIEW_HOMESCREEN;
    clock_gettime(CLOCK_MONOTONIC, &g_ui_last_time);
    terminal_init();
    kb_init();
    bench3d_init();
    power_init();
}

static void draw_ring(int cx, int cy, int r, int thickness, float pct, unsigned int fg, unsigned int bg) {
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            float dist = sqrtf((x - cx)*(x - cx) + (y - cy)*(y - cy));
            if (dist >= r - thickness && dist <= r) {
                float angle = atan2f(x - cx, cy - y);
                if (angle < 0) angle += 2 * 3.14159265f;
                if (angle <= pct * 2 * 3.14159265f)
                    fb_write_pixel(x, y, fg);
                else
                    fb_write_pixel(x, y, bg);
            }
        }
    }
}

static void render_status_bar(void) {
    int bar_y = NOTCH_OFFSET_Y;
    fb_fill_rect(0, bar_y, g_fb.xres, STATUS_BAR_HEIGHT, COLOR_STATUSBAR_BG);
    fb_fill_rect(0, bar_y + STATUS_BAR_HEIGHT, g_fb.xres, 1, COLOR_NAVBAR_BORDER);

    char uptime_str[64];
    unsigned long hb = get_heartbeat_counter();
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);

    snprintf(uptime_str, sizeof(uptime_str), "%02d:%02d | %d FPS | UP %02lu:%02lu",
             info->tm_hour, info->tm_min, g_ui_fps, hb / 3600, (hb % 3600) / 60);
    font_draw_text(UI_PADDING_X, bar_y + 24, uptime_str, FONT_SIZE_STATUS, COLOR_SUBTITLE_TXT);

    int ring_cy = bar_y + STATUS_BAR_HEIGHT / 2;
    int ring_x = g_fb.xres - UI_PADDING_X - 160;
    
    // CPU Ring
    draw_ring(ring_x, ring_cy, 18, 4, 0.45f, 0x10B981, 0x1E293B);
    font_draw_text(ring_x - 14, ring_cy + 24, "CPU", 14.0f, COLOR_HINT_TXT);
    
    // RAM Ring
    ring_x += 50;
    draw_ring(ring_x, ring_cy, 18, 4, 0.60f, 0xF59E0B, 0x1E293B);
    font_draw_text(ring_x - 14, ring_cy + 24, "RAM", 14.0f, COLOR_HINT_TXT);

    vector_draw_icon(g_fb.xres - UI_PADDING_X - 50, bar_y + 22, 30, VEC_ICON_BATTERY, COLOR_SUBTITLE_TXT);
    font_draw_text(g_fb.xres - UI_PADDING_X - 10, bar_y + 24, "98%", FONT_SIZE_STATUS, COLOR_TITLE_TXT);
}

static void render_nav_bar(void) {
    int bar_y = g_fb.yres - NAV_BAR_HEIGHT;
    fb_fill_rect(0, bar_y, g_fb.xres, NAV_BAR_HEIGHT, COLOR_NAVBAR_BG);
    fb_fill_rect(0, bar_y, g_fb.xres, 2, COLOR_NAVBAR_BORDER);

    int third = g_fb.xres / 3;
    int icon_s = 36;

    vector_draw_icon((third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_BACK, COLOR_NAVBAR_ICON);
    vector_draw_icon(third + (third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_HOME, COLOR_NAVBAR_ICON);
    vector_draw_icon((third * 2) + (third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_RECENTS, COLOR_NAVBAR_ICON);
}

static void render_app_tile(int center_x, int top_y, const struct app_shortcut *app) {
    int isize = GRID_ICON_SIZE;
    int ix = center_x - (isize / 2);
    int iy = top_y;

    fb_draw_card(ix, iy, isize, isize, COLOR_CARD_BG, app->icon_color);
    int icon_draw_size = isize * 55 / 100;
    vector_draw_icon(ix + (isize - icon_draw_size)/2, iy + (isize - icon_draw_size)/2,
                     icon_draw_size, app->icon, app->icon_color);

    int tw = font_measure_text(app->title, FONT_SIZE_APP_LABEL);
    font_draw_text(center_x - (tw / 2), iy + isize + 24, app->title, FONT_SIZE_APP_LABEL, COLOR_TITLE_TXT);
}

static void render_homescreen(void) {
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);
    g_page_scroll_x += (g_target_scroll_x - g_page_scroll_x) * 0.22f;

    int top_margin = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 60;
    int col_width = g_fb.xres / GRID_COLS;
    int row_height = 180;

    fb_set_clip(0, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT, g_fb.xres,
                g_fb.yres - NOTCH_OFFSET_Y - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT);

    int p0_x_offset = -(int)g_page_scroll_x;
    for (int i = 0; i < 6; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        render_app_tile(p0_x_offset + (col * col_width) + (col_width / 2), top_margin + (row * row_height), &PAGE0_APPS[i]);
    }

    int p1_x_offset = g_fb.xres - (int)g_page_scroll_x;
    for (int i = 0; i < 3; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        render_app_tile(p1_x_offset + (col * col_width) + (col_width / 2), top_margin + (row * row_height), &PAGE1_APPS[i]);
    }

    fb_clear_clip();

    int dot_y = g_fb.yres - NAV_BAR_HEIGHT - 36;
    int dot_cx = g_fb.xres / 2;
    int dot_gap = 26;
    for (int p = 0; p < TOTAL_PAGES; p++) {
        int dx = dot_cx - ((TOTAL_PAGES - 1) * dot_gap / 2) + (p * dot_gap);
        unsigned int dot_color = (g_cur_page == p) ? COLOR_CARD_SEL_BORDER : COLOR_NAVBAR_ICON_DIM;
        fb_fill_rect(dx - 4, dot_y - 4, 8, 8, dot_color);
    }
}

void ui_render(void) {
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    g_ui_frames++;
    double elapsed = (now.tv_sec - g_ui_last_time.tv_sec) + (now.tv_nsec - g_ui_last_time.tv_nsec) / 1000000000.0;
    if (elapsed >= 0.5) {
        g_ui_fps = (int)(g_ui_frames / elapsed);
        g_ui_frames = 0;
        g_ui_last_time = now;
    }

    struct timespec r0, r1, r2, r3;
    clock_gettime(CLOCK_MONOTONIC, &r0);

    switch (g_active_view) {
    case VIEW_HOMESCREEN:  render_homescreen(); break;
    case VIEW_MOUNT:       mount_screen_render(); break;
    case VIEW_FILEMANAGER: fm_render(); break;
    case VIEW_TERMINAL:    terminal_render(); break;
    case VIEW_SYSMON:      sysmon_render(); break;
    case VIEW_PROCESSES:   processes_render(); break;
    case VIEW_RECENTS:     recents_render(); break;
    case VIEW_BENCH3D:     bench3d_render(); break;
    case VIEW_POWER:       power_render(); break;
    }

    clock_gettime(CLOCK_MONOTONIC, &r1);

    render_status_bar();
    render_nav_bar();

    clock_gettime(CLOCK_MONOTONIC, &r2);

    if (g_toast_timer > 0) {
        int w = font_measure_text(g_toast_msg, FONT_SIZE_SMALL) + 40;
        int x = (g_fb.xres - w) / 2;
        int y = g_fb.yres - NAV_BAR_HEIGHT - 80;
        fb_draw_card(x, y, w, 50, COLOR_CARD_BG, COLOR_TOPBAR_ACCENT);
        font_draw_text(x + 20, y + 14, g_toast_msg, FONT_SIZE_SMALL, COLOR_TITLE_TXT);
        g_toast_timer--;
    }

    if (g_alert_active) {
        int w = 400;
        int h = 200;
        int x = (g_fb.xres - w) / 2;
        int y = (g_fb.yres - h) / 2;
        fb_draw_card(x, y, w, h, COLOR_CARD_BG, COLOR_CARD_EXIT_BORDER);
        font_draw_text(x + 20, y + 20, g_alert_title, FONT_SIZE_BODY, COLOR_TITLE_TXT);
        font_draw_text(x + 20, y + 70, g_alert_msg, FONT_SIZE_SMALL, COLOR_SUBTITLE_TXT);
        fb_draw_card(x + 20, y + 130, w - 40, 50, COLOR_CARD_SEL_BG, COLOR_CARD_SEL_BORDER);
        int tw = font_measure_text("OK", FONT_SIZE_SMALL);
        font_draw_text(x + w/2 - tw/2, y + 146, "OK", FONT_SIZE_SMALL, COLOR_TITLE_TXT);
    }

    fb_present();

    clock_gettime(CLOCK_MONOTONIC, &r3);

    /* Perf instrumentation: split ui_render() itself into view-draw,
     * status/nav bar, and present, to find where the fixed ~50ms/frame
     * cost lives. Logged every 60 frames via dbgf/dmesg. Remove once
     * the bottleneck is found. */
    static unsigned long acc_view = 0, acc_bars = 0, acc_present = 0;
    static unsigned long acc_samples = 0;
    acc_view    += (unsigned long)((r1.tv_sec - r0.tv_sec) * 1000000000L + (r1.tv_nsec - r0.tv_nsec));
    acc_bars    += (unsigned long)((r2.tv_sec - r1.tv_sec) * 1000000000L + (r2.tv_nsec - r1.tv_nsec));
    acc_present += (unsigned long)((r3.tv_sec - r2.tv_sec) * 1000000000L + (r3.tv_nsec - r2.tv_nsec));
    acc_samples++;
    if (acc_samples >= 60) {
        dbgf(LOGPFX "perf: ui_render split: view=%luus  bars=%luus  present(incl.toast/alert)=%luus  view_active=%d\n",
             acc_view / acc_samples / 1000,
             acc_bars / acc_samples / 1000,
             acc_present / acc_samples / 1000,
             (int)g_active_view);
        acc_view = 0; acc_bars = 0; acc_present = 0; acc_samples = 0;
    }
}

void ui_handle_drag_x(float delta_x) {
    if (g_active_view == VIEW_HOMESCREEN) {
        g_target_scroll_x += delta_x;
        if (g_target_scroll_x < 0.0f) g_target_scroll_x = 0.0f;
        if (g_target_scroll_x > (float)(g_fb.xres * (TOTAL_PAGES - 1)))
            g_target_scroll_x = (float)(g_fb.xres * (TOTAL_PAGES - 1));
    } else if (g_active_view == VIEW_BENCH3D) {
        bench3d_rotate_drag(-delta_x, 0.0f);
    }
}

void ui_on_page_swipe_end(void) {
    if (g_active_view == VIEW_HOMESCREEN) {
        int target_page = (int)((g_target_scroll_x + (g_fb.xres / 2)) / g_fb.xres);
        if (target_page < 0) target_page = 0;
        if (target_page >= TOTAL_PAGES) target_page = TOTAL_PAGES - 1;
        g_cur_page = target_page;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_handle_scroll_y(float delta_y) {
    if (g_active_view == VIEW_FILEMANAGER) fm_scroll_delta(delta_y);
    else if (g_active_view == VIEW_PROCESSES) processes_scroll(delta_y);
    else if (g_active_view == VIEW_MOUNT) mount_screen_scroll(delta_y);
    else if (g_active_view == VIEW_BENCH3D) bench3d_rotate_drag(0.0f, -delta_y);
}

void ui_handle_tap(int x, int y) {
    if (g_alert_active) {
        int w = 400; int h = 200;
        int ax = (g_fb.xres - w) / 2; int ay = (g_fb.yres - h) / 2;
        if (x >= ax + 20 && x <= ax + w - 20 && y >= ay + 130 && y <= ay + 180) {
            g_alert_active = 0;
            trigger_vibration();
        }
        return;
    }

    if (y >= g_fb.yres - NAV_BAR_HEIGHT) {
        trigger_vibration();
        int third = g_fb.xres / 3;
        if (x < third) {
            if (g_active_view != VIEW_HOMESCREEN) g_active_view = VIEW_HOMESCREEN;
        } else if (x < third * 2) {
            g_active_view = VIEW_HOMESCREEN;
        } else {
            recents_init();
            g_active_view = VIEW_RECENTS;
        }
        return;
    }

    switch (g_active_view) {
    case VIEW_MOUNT:       mount_screen_handle_touch(x, y, 1); return;
    case VIEW_FILEMANAGER: fm_handle_touch(x, y, 1); return;
    case VIEW_TERMINAL:    terminal_handle_touch(x, y, 1); return;
    case VIEW_SYSMON:      sysmon_handle_touch(x, y, 1); return;
    case VIEW_PROCESSES:   processes_handle_touch(x, y, 1); return;
    case VIEW_RECENTS:     recents_handle_touch(x, y, 1); return;
    case VIEW_BENCH3D:     bench3d_handle_touch(x, y, 1); return;
    case VIEW_POWER:       power_handle_touch(x, y, 1); return;
    default: break;
    }

    if (g_active_view == VIEW_HOMESCREEN) {
        int top_margin = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 60;
        int col_width = g_fb.xres / GRID_COLS;
        int row_height = 180;
        int count = (g_cur_page == 0) ? 6 : 3;
        const struct app_shortcut *apps = (g_cur_page == 0) ? PAGE0_APPS : PAGE1_APPS;

        for (int i = 0; i < count; i++) {
            int col = i % GRID_COLS;
            int row = i / GRID_COLS;
            int cx = (col * col_width) + (col_width / 2);
            int cy = top_margin + (row * row_height);
            int hit_box = GRID_ICON_SIZE + 24;

            if (x >= cx - (hit_box/2) && x <= cx + (hit_box/2) &&
                y >= cy - 10 && y <= cy + hit_box + 20) {
                trigger_vibration();
                if (apps[i].is_action == 1) {
                    fm_init(FM_MODE_PICKER, "/system/bin", on_executable_picked);
                    g_active_view = VIEW_FILEMANAGER;
                } else {
                    if (apps[i].target_view == VIEW_MOUNT) mount_screen_init();
                    if (apps[i].target_view == VIEW_BENCH3D) bench3d_init();
                    if (apps[i].target_view == VIEW_FILEMANAGER) fm_init(FM_MODE_BROWSE, "/", NULL);
                    g_active_view = apps[i].target_view;
                }
                break;
            }
        }
    }
}

void ui_on_volume_up(void) {
    trigger_vibration();
    if (g_active_view == VIEW_HOMESCREEN) {
        g_cur_page = (g_cur_page - 1 + TOTAL_PAGES) % TOTAL_PAGES;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_on_volume_down(void) {
    trigger_vibration();
    if (g_active_view == VIEW_HOMESCREEN) {
        g_cur_page = (g_cur_page + 1) % TOTAL_PAGES;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_on_power_key(void) {
    trigger_vibration();
    if (g_active_view != VIEW_HOMESCREEN) {
        g_active_view = VIEW_HOMESCREEN;
    }
}
