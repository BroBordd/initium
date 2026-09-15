#include <stdio.h>
#include "recents.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "ui.h"
#include "hardware.h"
#include "config.h"

void recents_init(void)
{
}

void recents_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "RECENT APPS", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 66,
                   "Installed OS Applications", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    int start_y = topbar_h + 40;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    
    // Placeholder big cards for Recents UI
    fb_draw_card(UI_PADDING_X, start_y, card_w, 200, COLOR_CARD_BG, COLOR_CARD_BORDER);
    vector_draw_icon(UI_PADDING_X + 40, start_y + 60, 80, VEC_ICON_FOLDER, COLOR_ACCENT_BLUE);
    font_draw_text(UI_PADDING_X + 160, start_y + 60, "File Explorer", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X + 160, start_y + 110, "Last Opened", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    start_y += 240;

    fb_draw_card(UI_PADDING_X, start_y, card_w, 200, COLOR_CARD_BG, COLOR_CARD_BORDER);
    vector_draw_icon(UI_PADDING_X + 40, start_y + 60, 80, VEC_ICON_TERMINAL, 0x8B5CF6);
    font_draw_text(UI_PADDING_X + 160, start_y + 60, "Terminal", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X + 160, start_y + 110, "Background", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);
}

void recents_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;
    
    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);
    
    if (x >= UI_PADDING_X && x <= UI_PADDING_X + card_w) {
        if (y >= topbar_h + 40 && y <= topbar_h + 240) {
            trigger_vibration();
            ui_return_to_home();
            // In a real app we would route to File Explorer
        }
    }
}
