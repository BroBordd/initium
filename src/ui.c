/* src/ui.c */
#include <stdio.h>
#include <string.h>
#include "ui.h"
#include "mount.h"
#include "filemanager.h"
#include "terminal.h"
#include "sysmon.h"
#include "recents.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"

static current_view_t g_active_view = VIEW_MAIN;
static int g_main_selected_idx = 0;
static int g_apps_selected_idx = 0;
static int g_exit_requested = 0;

int ui_is_exit_requested(void)
{
    return g_exit_requested;
}

current_view_t ui_get_current_view(void)
{
    return g_active_view;
}

void ui_return_to_main(void)
{
    g_active_view = VIEW_MAIN;
    kb_hide();
}

static void on_executable_picked(const char *path)
{
    g_active_view = VIEW_TERMINAL;
    terminal_execute_command(path);
}

void ui_init(void)
{
    g_active_view = VIEW_MAIN;
    g_main_selected_idx = 0;
    g_apps_selected_idx = 0;
    g_exit_requested = 0;
    terminal_init();
    kb_init();
}

/* Persistent System Status Bar at Top */
static void render_status_bar(void)
{
    int bar_y = NOTCH_OFFSET_Y;
    fb_fill_rect(0, bar_y, g_fb.xres, STATUS_BAR_HEIGHT, COLOR_STATUSBAR_BG);
    fb_fill_rect(0, bar_y + STATUS_BAR_HEIGHT, g_fb.xres, 1, COLOR_NAVBAR_BORDER);

    /* Watchdog Uptime Counter */
    char uptime_str[64];
    unsigned long hb = get_heartbeat_counter();
    snprintf(uptime_str, sizeof(uptime_str), "UP %02lu:%02lu:%02lu",
             hb / 3600, (hb % 3600) / 60, hb % 60);
    font_draw_text(UI_PADDING_X, bar_y + 14, uptime_str, FONT_SIZE_STATUS, COLOR_SUBTITLE_TXT);

    /* Battery & Status */
    vector_draw_icon(g_fb.xres - UI_PADDING_X - 110, bar_y + 12, 30, VEC_ICON_BATTERY, COLOR_SUBTITLE_TXT);
    font_draw_text(g_fb.xres - UI_PADDING_X - 70, bar_y + 14, "98%", FONT_SIZE_STATUS, COLOR_TITLE_TXT);
}

/* Android-Style Bottom Navigation Bar (Back, Home, Recents) */
static void render_nav_bar(void)
{
    int bar_y = g_fb.yres - NAV_BAR_HEIGHT;
    fb_fill_rect(0, bar_y, g_fb.xres, NAV_BAR_HEIGHT, COLOR_NAVBAR_BG);
    fb_fill_rect(0, bar_y, g_fb.xres, 2, COLOR_NAVBAR_BORDER);

    int third = g_fb.xres / 3;

    /* 1. Back Icon */
    int icon_s = 36;
    vector_draw_icon((third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_BACK, COLOR_NAVBAR_ICON);

    /* 2. Home Icon */
    vector_draw_icon(third + (third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_HOME, COLOR_NAVBAR_ICON);

    /* 3. Recents Icon */
    vector_draw_icon((third * 2) + (third / 2) - (icon_s / 2), bar_y + (NAV_BAR_HEIGHT / 2) - (icon_s / 2),
                     icon_s, VEC_ICON_RECENTS, COLOR_NAVBAR_ICON);
}

static void render_main_menu(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14, "RECOVERY SYSTEM", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64, "Dynamic Supervisor // 60 FPS Engine", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    const char *titles[] = {
        "Execute",
        "Mount",
        "Apps",
        "Exit & Panic"
    };

    const char *subtitles[] = {
        "Launch File Picker to execute script/binary",
        "Manage & toggle dynamic partition mounts",
        "Console, Explorer, Sensors & Utilities",
        "Terminate PID 1 and trigger kernel panic"
    };

    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int start_y = topbar_h + 30;

    for (int i = 0; i < 4; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        int is_sel = (i == g_main_selected_idx);
        int is_exit = (i == 3);

        unsigned int bg     = is_sel ? COLOR_CARD_SEL_BG     : (is_exit ? COLOR_CARD_EXIT_BG     : COLOR_CARD_BG);
        unsigned int border = is_sel ? COLOR_CARD_SEL_BORDER : (is_exit ? COLOR_CARD_EXIT_BORDER : COLOR_CARD_BORDER);
        unsigned int text_c = is_sel ? COLOR_CARD_SEL_TXT    : COLOR_CARD_TXT;

        fb_draw_card(UI_PADDING_X, card_y, card_w, BUTTON_HEIGHT, bg, border);

        font_draw_text(UI_PADDING_X + 32, card_y + 24, titles[i], FONT_SIZE_BODY, text_c);
        font_draw_text(UI_PADDING_X + 32, card_y + 70, subtitles[i], FONT_SIZE_SUBTITLE,
                       is_sel ? 0xD8F3DC : COLOR_SUBTITLE_TXT);

        font_draw_text(UI_PADDING_X + card_w - 44, card_y + 44, ">", FONT_SIZE_BODY, text_c);
    }
}

static void render_apps_menu(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14, "APPLICATIONS", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64, "Integrated Diagnostics & Tools", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    const char *titles[] = {
        "File Manager",
        "Terminal Console",
        "Hardware & Sensor Monitor",
        "< Back to Main Menu"
    };

    const char *subtitles[] = {
        "Rootfs file and directory navigator",
        "Interactive command line with soft keyboard",
        "Telemetry, thermals, battery and grip",
        "Return to root dashboard"
    };

    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int start_y = topbar_h + 30;

    for (int i = 0; i < 4; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        int is_sel = (i == g_apps_selected_idx);

        unsigned int bg     = is_sel ? COLOR_CARD_SEL_BG     : COLOR_CARD_BG;
        unsigned int border = is_sel ? COLOR_CARD_SEL_BORDER : COLOR_CARD_BORDER;
        unsigned int text_c = is_sel ? COLOR_CARD_SEL_TXT    : COLOR_CARD_TXT;

        fb_draw_card(UI_PADDING_X, card_y, card_w, BUTTON_HEIGHT, bg, border);
        font_draw_text(UI_PADDING_X + 32, card_y + 24, titles[i], FONT_SIZE_BODY, text_c);
        font_draw_text(UI_PADDING_X + 32, card_y + 70, subtitles[i], FONT_SIZE_SUBTITLE,
                       is_sel ? 0xD8F3DC : COLOR_SUBTITLE_TXT);
    }
}

void ui_render(void)
{
    switch (g_active_view) {
    case VIEW_MAIN:        render_main_menu(); break;
    case VIEW_MOUNT:       mount_screen_render(); break;
    case VIEW_APPS_MENU:   render_apps_menu(); break;
    case VIEW_FILEMANAGER: fm_render(); break;
    case VIEW_TERMINAL:    terminal_render(); break;
    case VIEW_SYSMON:      sysmon_render(); break;
    case VIEW_RECENTS:     recents_render(); break;
    }

    render_status_bar();
    render_nav_bar();

    fb_present();
}

void ui_handle_scroll(float delta_y)
{
    if (g_active_view == VIEW_FILEMANAGER)
        fm_scroll_delta(delta_y);
    else if (g_active_view == VIEW_RECENTS)
        recents_scroll(delta_y);
}

static void execute_main_index(int idx)
{
    switch (idx) {
    case 0:
        fm_init(FM_MODE_PICKER, "/system/bin", on_executable_picked);
        g_active_view = VIEW_FILEMANAGER;
        break;
    case 1:
        mount_screen_init();
        g_active_view = VIEW_MOUNT;
        break;
    case 2:
        g_active_view = VIEW_APPS_MENU;
        break;
    case 3:
        g_exit_requested = 1;
        break;
    }
}

static void execute_apps_index(int idx)
{
    switch (idx) {
    case 0:
        fm_init(FM_MODE_BROWSE, "/", NULL);
        g_active_view = VIEW_FILEMANAGER;
        break;
    case 1:
        g_active_view = VIEW_TERMINAL;
        break;
    case 2:
        g_active_view = VIEW_SYSMON;
        break;
    case 3:
        g_active_view = VIEW_MAIN;
        break;
    }
}

void ui_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    /* 1. Bottom Navigation Bar Interception */
    if (y >= g_fb.yres - NAV_BAR_HEIGHT) {
        trigger_vibration();
        int third = g_fb.xres / 3;
        if (x < third) {
            /* Back Pressed */
            if (g_active_view != VIEW_MAIN)
                g_active_view = VIEW_MAIN;
        } else if (x < third * 2) {
            /* Home Pressed */
            g_active_view = VIEW_MAIN;
        } else {
            /* Recents Pressed */
            recents_init();
            g_active_view = VIEW_RECENTS;
        }
        return;
    }

    /* 2. Route to Active Screen */
    switch (g_active_view) {
    case VIEW_MOUNT:       mount_screen_handle_touch(x, y, is_down); return;
    case VIEW_FILEMANAGER: fm_handle_touch(x, y, is_down); return;
    case VIEW_TERMINAL:    terminal_handle_touch(x, y, is_down); return;
    case VIEW_SYSMON:      sysmon_handle_touch(x, y, is_down); return;
    case VIEW_RECENTS:     recents_handle_touch(x, y, is_down); return;
    default: break;
    }

    trigger_vibration();

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    int start_y = topbar_h + 30;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);

    for (int i = 0; i < 4; i++) {
        int card_y = start_y + i * (BUTTON_HEIGHT + BUTTON_GAP);
        if (x >= UI_PADDING_X && x <= UI_PADDING_X + card_w &&
            y >= card_y && y <= card_y + BUTTON_HEIGHT) {
            if (g_active_view == VIEW_MAIN) {
                g_main_selected_idx = i;
                execute_main_index(i);
            } else if (g_active_view == VIEW_APPS_MENU) {
                g_apps_selected_idx = i;
                execute_apps_index(i);
            }
            break;
        }
    }
}

void ui_on_volume_up(void)
{
    trigger_vibration();
    if (g_active_view == VIEW_MAIN) {
        g_main_selected_idx = (g_main_selected_idx - 1 + 4) % 4;
    } else if (g_active_view == VIEW_APPS_MENU) {
        g_apps_selected_idx = (g_apps_selected_idx - 1 + 4) % 4;
    }
}

void ui_on_volume_down(void)
{
    trigger_vibration();
    if (g_active_view == VIEW_MAIN) {
        g_main_selected_idx = (g_main_selected_idx + 1) % 4;
    } else if (g_active_view == VIEW_APPS_MENU) {
        g_apps_selected_idx = (g_apps_selected_idx + 1) % 4;
    }
}

void ui_on_power_key(void)
{
    trigger_vibration();
    if (g_active_view == VIEW_MAIN) {
        execute_main_index(g_main_selected_idx);
    } else if (g_active_view == VIEW_APPS_MENU) {
        execute_apps_index(g_apps_selected_idx);
    }
}
