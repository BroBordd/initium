#include <stdio.h>
#include <string.h>
#include "sysmon.h"
#include "framebuffer.h"
#include "font.h"
#include "gpu.h"
#include "hardware.h"
#include "config.h"

static unsigned long long g_prev_idle[8] = {0};
static unsigned long long g_prev_total[8] = {0};
static int g_cpu_usage[8] = {0};
static int g_num_cpus = 0;

static void update_cpu_usage(void) {
    FILE *fp = fopen("/proc/stat", "r");
    if (!fp) return;
    char line[256];
    int c = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "cpu", 3) == 0 && line[3] >= '0' && line[3] <= '9') {
            unsigned long long user, nice, system, idle, iowait, irq, softirq;
            if (sscanf(line, "%*s %llu %llu %llu %llu %llu %llu %llu", 
                   &user, &nice, &system, &idle, &iowait, &irq, &softirq) >= 4) {
                unsigned long long total = user + nice + system + idle + iowait + irq + softirq;
                unsigned long long totald = total - g_prev_total[c];
                unsigned long long idled = idle - g_prev_idle[c];
                if (totald > 0) {
                    g_cpu_usage[c] = (int)((100 * (totald - idled)) / totald);
                }
                g_prev_total[c] = total;
                g_prev_idle[c] = idle;
                c++;
                if (c >= 8) break;
            }
        }
    }
    g_num_cpus = c > 0 ? c : 4; 
    fclose(fp);
}

void sysmon_render(void)
{
    update_cpu_usage();

    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "TASK MANAGER", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64,
                   "System Performance & Telemetry", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    fb_set_clip(0, topbar_h, g_fb.xres, g_fb.yres - topbar_h - NAV_BAR_HEIGHT);

    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    int start_y = topbar_h + 20;

    // CPU Graph
    fb_draw_card(UI_PADDING_X, start_y, card_w, 200, COLOR_CARD_BG, COLOR_CARD_BORDER);
    font_draw_text(UI_PADDING_X + 24, start_y + 20, "CPU Utilization per Core", FONT_SIZE_BODY, COLOR_TITLE_TXT);

    int graph_w = card_w - 48;
    int bar_w = (graph_w / g_num_cpus) - 10;
    for (int i = 0; i < g_num_cpus; i++) {
        int h = (g_cpu_usage[i] * 120) / 100;
        int bx = UI_PADDING_X + 24 + i * (bar_w + 10);
        int by = start_y + 180;
        fb_fill_rect(bx, by - 120, bar_w, 120, 0x1E293B);
        fb_fill_rect(bx, by - h, bar_w, h, COLOR_TOPBAR_ACCENT);
        char lbl[16];
        snprintf(lbl, sizeof(lbl), "%d%%", g_cpu_usage[i]);
        font_draw_text(bx + 4, by - h - 6, lbl, 16.0f, COLOR_TITLE_TXT);
    }

    start_y += 200 + 14;

    // Memory Info
    fb_draw_card(UI_PADDING_X, start_y, card_w, 120, COLOR_CARD_BG, COLOR_CARD_BORDER);
    font_draw_text(UI_PADDING_X + 24, start_y + 20, "Memory Usage", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    
    FILE *fp = fopen("/proc/meminfo", "r");
    if (fp) {
        char line[128];
        long mem_total = 0, mem_free = 0, mem_avail = 0;
        while(fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "MemTotal:", 9) == 0) sscanf(line, "%*s %ld", &mem_total);
            if (strncmp(line, "MemFree:", 8) == 0) sscanf(line, "%*s %ld", &mem_free);
            if (strncmp(line, "MemAvailable:", 13) == 0) sscanf(line, "%*s %ld", &mem_avail);
        }
        fclose(fp);
        long used = mem_total - mem_avail;
        int pct = mem_total > 0 ? (used * 100) / mem_total : 0;

        fb_fill_rect(UI_PADDING_X + 24, start_y + 70, graph_w, 20, 0x1E293B);
        fb_fill_rect(UI_PADDING_X + 24, start_y + 70, (graph_w * pct) / 100, 20, 0xF59E0B);

        char mem_str[64];
        snprintf(mem_str, sizeof(mem_str), "%ld MB / %ld MB Used", used / 1024, mem_total / 1024);
        font_draw_text(UI_PADDING_X + 24, start_y + 110, mem_str, FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);
    }

    start_y += 120 + 14;
    
    // Battery Info
    char capacity[32] = "N/A", temp[32] = "N/A";
    FILE *bf = fopen("/sys/class/power_supply/battery/capacity", "r");
    if (bf) { fgets(capacity, sizeof(capacity), bf); fclose(bf); }
    FILE *tf = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    if (tf) { fgets(temp, sizeof(temp), tf); fclose(tf); }
    int temp_c = 0;
    sscanf(temp, "%d", &temp_c);

    fb_draw_card(UI_PADDING_X, start_y, card_w, 100, COLOR_CARD_BG, COLOR_CARD_BORDER);
    char bat_str[128];
    snprintf(bat_str, sizeof(bat_str), "Battery: %s%% | Thermal: %.1f C", 
             capacity, temp_c > 1000 ? (float)temp_c/1000.0f : (float)temp_c);
    font_draw_text(UI_PADDING_X + 24, start_y + 36, bat_str, FONT_SIZE_BODY, COLOR_STATUS_OK);

    fb_clear_clip();
}

void sysmon_handle_touch(int x, int y, int is_down)
{
    (void)x; (void)y; (void)is_down;
}
