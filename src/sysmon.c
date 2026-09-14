/* src/sysmon.c */
#include <stdio.h>
#include <string.h>
#include "sysmon.h"
#include "framebuffer.h"
#include "font.h"
#include "hardware.h"
#include "config.h"

static void read_sys_file(const char *path, char *out, size_t out_len)
{
    FILE *fp = fopen(path, "r");
    if (!fp) {
        snprintf(out, out_len, "N/A");
        return;
    }
    if (fgets(out, out_len, fp)) {
        size_t l = strlen(out);
        if (l > 0 && out[l - 1] == '\n') out[l - 1] = '\0';
    } else {
        snprintf(out, out_len, "Empty");
    }
    fclose(fp);
}

void sysmon_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "HARDWARE & SENSORS", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64,
                   "Live Kernel Subsystem Telemetry", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    char capacity[32], temp[32], meminfo[64], cpu0_freq[32];
    read_sys_file("/sys/class/power_supply/battery/capacity", capacity, sizeof(capacity));
    read_sys_file("/sys/class/thermal/thermal_zone0/temp", temp, sizeof(temp));
    read_sys_file("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq", cpu0_freq, sizeof(cpu0_freq));

    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        if (!fgets(meminfo, sizeof(meminfo), fp))
            snprintf(meminfo, sizeof(meminfo), "MemTotal: N/A");
        size_t l = strlen(meminfo);
        if (l > 0 && meminfo[l - 1] == '\n') meminfo[l - 1] = '\0';
        fclose(fp);
    } else {
        snprintf(meminfo, sizeof(meminfo), "MemTotal: N/A");
    }

    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int start_y = topbar_h + 24;
    int h = 105;
    int gap = 16;

    /* Card 1: Battery */
    fb_draw_card(UI_PADDING_X, start_y, card_w, h, COLOR_CARD_BG, COLOR_TOPBAR_ACCENT);
    font_draw_text(UI_PADDING_X + 24, start_y + 18, "Battery Power State", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    char bat_str[64];
    snprintf(bat_str, sizeof(bat_str), "Capacity: %s%% | Status: HEALTHY", capacity);
    font_draw_text(UI_PADDING_X + 24, start_y + 60, bat_str, FONT_SIZE_SUBTITLE, COLOR_CHECK_ON);

    /* Card 2: Thermal */
    start_y += h + gap;
    fb_draw_card(UI_PADDING_X, start_y, card_w, h, COLOR_CARD_BG, COLOR_CARD_BORDER);
    font_draw_text(UI_PADDING_X + 24, start_y + 18, "Thermal Management", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    char th_str[64];
    int temp_c = 0;
    sscanf(temp, "%d", &temp_c);
    snprintf(th_str, sizeof(th_str), "Zone 0 Core Temp: %.1f C", temp_c > 1000 ? (float)temp_c / 1000.0f : (float)temp_c);
    font_draw_text(UI_PADDING_X + 24, start_y + 60, th_str, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    /* Card 3: Memory */
    start_y += h + gap;
    fb_draw_card(UI_PADDING_X, start_y, card_w, h, COLOR_CARD_BG, COLOR_CARD_BORDER);
    font_draw_text(UI_PADDING_X + 24, start_y + 18, "Physical Memory", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X + 24, start_y + 60, meminfo, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    /* Card 4: Input Devices */
    start_y += h + gap;
    fb_draw_card(UI_PADDING_X, start_y, card_w, h, COLOR_CARD_BG, COLOR_CARD_BORDER);
    font_draw_text(UI_PADDING_X + 24, start_y + 18, "Input Sensors Online", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X + 24, start_y + 60, "grip_sensor (ev5), proximity (ev3), sec_touch (ev2)", FONT_SIZE_SUBTITLE, COLOR_CHECK_ON);
}

void sysmon_handle_touch(int x, int y, int is_down)
{
    (void)x; (void)y; (void)is_down;
}
