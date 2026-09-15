/* src/ui.c */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "ui.h"
#include "mount.h"
#include "filemanager.h"
#include "terminal.h"
#include "sysmon.h"
#include "recents.h"
#include "bench3d.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"

static current_view_t g_active_view = VIEW_HOMESCREEN;
static int g_exit_requested = 0;

/* AOSP Launcher Paging Engine */
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

/* Page 0 Apps */
static const struct app_shortcut PAGE0_APPS[6] = {
    {"Execute",  VEC_ICON_BINARY,   0x3B82F6, VIEW_FILEMANAGER, 1},
    {"Storage",  VEC_ICON_STORAGE,  0x10B981, VIEW_MOUNT,       0},
    {"Terminal", VEC_ICON_TERMINAL, 0x8B5CF6, VIEW_TERMINAL,    0},
    {"Files",    VEC_ICON_FOLDER,   0xF59E0B, VIEW_FILEMANAGER, 0},
    {"3D Bench", VEC_ICON_3D,       0xEC4899, VIEW_BENCH3D,     0},
    {"Sensors",  VEC_ICON_SENSOR,   0x06B6D4, VIEW_SYSMON,      0}
};

/* Page 1 Apps */
static const struct app_shortcut PAGE1_APPS[3] = {
    {"Recents",  VEC_ICON_RECENTS,  0x6366F1, VIEW_RECENTS,     0},
    {"Reboot",   VEC_ICON_POWER,    0xF43F5E, VIEW_HOMESCREEN,  2},
    {"Poweroff", VEC_ICON_POWER,    0xEF4444, VIEW_HOMESCREEN,  3}
};

int ui_is_exit_requested(void)
{
    return g_exit_requested;
}

current_view_t ui_get_current_view(void)
{
    return g_active_view;
}

void ui_return_to_home(void)
{
    g_active_view = VIEW_HOMESCREEN;
    kb_hide();
}

static void on_executable_picked(const char *path)
{
    g_active_view = VIEW_TERMINAL;
    terminal_execute_command(path);
}

void ui_init(void)
{
    g_active_view = VIEW_HOMESCREEN;
    g_cur_page = 0;
    g_page_scroll_x = 0.0f;
    g_target_scroll_x = 0.0f;
    g_exit_requested = 0;
    terminal_init();
    kb_init();
    bench3d_init();
}

static void render_status_bar(void)
{
    int bar_y = NOTCH_OFFSET_Y;
    fb_fill_rect(0, bar_y, g_fb.xres, STATUS_BAR_HEIGHT, COLOR_STATUSBAR_BG);
    fb_fill_rect(0, bar_y + STATUS_BAR_HEIGHT, g_fb.xres, 1, COLOR_NAVBAR_BORDER);

    char uptime_str[64];
    unsigned long hb = get_heartbeat_counter();
    snprintf(uptime_str, sizeof(uptime_str), "UP %02lu:%02lu:%02lu",
             hb / 3600, (hb % 3600) / 60, hb % 60);
    font_draw_text(UI_PADDING_X, bar_y + 14, uptime_str, FONT_SIZE_STATUS, COLOR_SUBTITLE_TXT);

    vector_draw_icon(g_fb.xres - UI_PADDING_X - 110, bar_y + 12, 30, VEC_ICON_BATTERY, COLOR_SUBTITLE_TXT);
    font_draw_text(g_fb.xres - UI_PADDING_X - 70, bar_y + 14, "98%", FONT_SIZE_STATUS, COLOR_TITLE_TXT);
}

static void render_nav_bar(void)
{
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

/* Renders an individual app icon with squircle container and text */
static void render_app_tile(int center_x, int top_y, const struct app_shortcut *app)
{
    int isize = GRID_ICON_SIZE;
    int ix = center_x - (isize / 2);
    int iy = top_y;

    /* Squircle container card */
    fb_draw_card(ix, iy, isize, isize, COLOR_CARD_BG, app->icon_color);

    /* Center Vector Icon */
    int icon_draw_size = isize * 55 / 100;
    vector_draw_icon(ix + (isize - icon_draw_size)/2, iy + (isize - icon_draw_size)/2,
                     icon_draw_size, app->icon, app->icon_color);

    /* Text Label Centered Below */
    int text_offset_x = center_x - ((int)strlen(app->title) * 7);
    font_draw_text(text_offset_x, iy + isize + 16, app->title, FONT_SIZE_APP_LABEL, COLOR_TITLE_TXT);
}

/* AOSP Workspace: 3 Columns x 3 Rows with Horizontal Swipe Transition */
static void render_homescreen(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    /* Spring interpolation for smooth 60 FPS page slides */
    g_page_scroll_x += (g_target_scroll_x - g_page_scroll_x) * 0.22f;

    int top_margin = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 60;
    int col_width = g_fb.xres / GRID_COLS;
    int row_height = 180;

    /* Viewport Scissor for horizontal paging */
    fb_set_clip(0, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT, g_fb.xres,
                g_fb.yres - NOTCH_OFFSET_Y - STATUS_BAR_HEIGHT - NAV_BAR_HEIGHT);

    /* --- PAGE 0 --- */
    int p0_x_offset = -(int)g_page_scroll_x;
    for (int i = 0; i < 6; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int cx = p0_x_offset + (col * col_width) + (col_width / 2);
        int cy = top_margin + (row * row_height);
        render_app_tile(cx, cy, &PAGE0_APPS[i]);
    }

    /* --- PAGE 1 --- */
    int p1_x_offset = g_fb.xres - (int)g_page_scroll_x;
    for (int i = 0; i < 3; i++) {
        int col = i % GRID_COLS;
        int row = i / GRID_COLS;
        int cx = p1_x_offset + (col * col_width) + (col_width / 2);
        int cy = top_margin + (row * row_height);
        render_app_tile(cx, cy, &PAGE1_APPS[i]);
    }

    fb_clear_clip();

    /* Paging Dots Indicator (● ○) */
    int dot_y = g_fb.yres - NAV_BAR_HEIGHT - 36;
    int dot_cx = g_fb.xres / 2;
    int dot_gap = 26;

    for (int p = 0; p < TOTAL_PAGES; p++) {
        int dx = dot_cx - ((TOTAL_PAGES - 1) * dot_gap / 2) + (p * dot_gap);
        unsigned int dot_color = (g_cur_page == p) ? COLOR_CARD_SEL_BORDER : COLOR_NAVBAR_ICON_DIM;
        fb_fill_rect(dx - 4, dot_y - 4, 8, 8, dot_color);
    }
}

void ui_render(void)
{
    switch (g_active_view) {
    case VIEW_HOMESCREEN:  render_homescreen(); break;
    case VIEW_MOUNT:       mount_screen_render(); break;
    case VIEW_FILEMANAGER: fm_render(); break;
    case VIEW_TERMINAL:    terminal_render(); break;
    case VIEW_SYSMON:      sysmon_render(); break;
    case VIEW_RECENTS:     recents_render(); break;
    case VIEW_BENCH3D:     bench3d_render(); break;
    }

    render_status_bar();
    render_nav_bar();

    fb_present();
}

void ui_handle_drag_x(float delta_x)
{
    if (g_active_view == VIEW_HOMESCREEN) {
        g_target_scroll_x += delta_x;
        if (g_target_scroll_x < 0.0f) g_target_scroll_x = 0.0f;
        if (g_target_scroll_x > (float)(g_fb.xres * (TOTAL_PAGES - 1)))
            g_target_scroll_x = (float)(g_fb.xres * (TOTAL_PAGES - 1));
    } else if (g_active_view == VIEW_BENCH3D) {
        bench3d_rotate_drag(-delta_x, 0.0f);
    }
}

void ui_on_page_swipe_end(void)
{
    if (g_active_view == VIEW_HOMESCREEN) {
        /* Snap to nearest page */
        int target_page = (int)((g_target_scroll_x + (g_fb.xres / 2)) / g_fb.xres);
        if (target_page < 0) target_page = 0;
        if (target_page >= TOTAL_PAGES) target_page = TOTAL_PAGES - 1;

        g_cur_page = target_page;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_handle_scroll_y(float delta_y)
{
    if (g_active_view == VIEW_FILEMANAGER)
        fm_scroll_delta(delta_y);
    else if (g_active_view == VIEW_RECENTS)
        recents_scroll(delta_y);
    else if (g_active_view == VIEW_MOUNT)
        mount_screen_scroll((int)(delta_y / 30.0f));
    else if (g_active_view == VIEW_BENCH3D)
        bench3d_rotate_drag(0.0f, -delta_y);
}

/* Dispatched ONLY when user releases finger without dragging (Touch Slop OK) */
void ui_handle_tap(int x, int y)
{
    /* 1. Bottom Navigation Bar */
    if (y >= g_fb.yres - NAV_BAR_HEIGHT) {
        trigger_vibration();
        int third = g_fb.xres / 3;
        if (x < third) {
            /* Back Button */
            if (g_active_view != VIEW_HOMESCREEN)
                g_active_view = VIEW_HOMESCREEN;
        } else if (x < third * 2) {
            /* Home Button */
            g_active_view = VIEW_HOMESCREEN;
        } else {
            /* Recents Button */
            recents_init();
            g_active_view = VIEW_RECENTS;
        }
        return;
    }

    /* 2. Route to Active Subview */
    switch (g_active_view) {
    case VIEW_MOUNT:       mount_screen_handle_touch(x, y, 1); return;
    case VIEW_FILEMANAGER: fm_handle_touch(x, y, 1); return;
    case VIEW_TERMINAL:    terminal_handle_touch(x, y, 1); return;
    case VIEW_SYSMON:      sysmon_handle_touch(x, y, 1); return;
    case VIEW_RECENTS:     recents_handle_touch(x, y, 1); return;
    case VIEW_BENCH3D:     bench3d_handle_touch(x, y, 1); return;
    default: break;
    }

    /* 3. Homescreen Icon Grid Tap Detection */
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
                    /* Execute binary picker */
                    fm_init(FM_MODE_PICKER, "/system/bin", on_executable_picked);
                    g_active_view = VIEW_FILEMANAGER;
                } else if (apps[i].is_action == 2 || apps[i].is_action == 3) {
                    /* Power / Exit Panic */
                    g_exit_requested = 1;
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

void ui_on_volume_up(void)
{
    trigger_vibration();
    if (g_active_view == VIEW_HOMESCREEN) {
        g_cur_page = (g_cur_page - 1 + TOTAL_PAGES) % TOTAL_PAGES;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_on_volume_down(void)
{
    trigger_vibration();
    if (g_active_view == VIEW_HOMESCREEN) {
        g_cur_page = (g_cur_page + 1) % TOTAL_PAGES;
        g_target_scroll_x = (float)(g_cur_page * g_fb.xres);
    }
}

void ui_on_power_key(void)
{
    trigger_vibration();
    if (g_active_view != VIEW_HOMESCREEN) {
        g_active_view = VIEW_HOMESCREEN;
    }
}
