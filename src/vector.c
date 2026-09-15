/* src/vector.c */
#include <stdio.h>
#include <math.h>
#include "vector.h"
#include "framebuffer.h"
#include "config.h"

void vector_init(void)
{
}

void vector_draw_icon(int x, int y, int size, vector_icon_t icon, unsigned int color)
{
    switch (icon) {
    case VEC_ICON_FOLDER: {
        int tab_w = size * 4 / 10;
        int tab_h = size * 2 / 10;
        fb_fill_rect(x, y + (size * 1 / 10), tab_w, tab_h, color);
        fb_fill_rect(x, y + (size * 3 / 10), size, size * 6 / 10, color);
        fb_fill_rect(x + 2, y + (size * 3 / 10) + 2, size - 4, (size * 6 / 10) - 4, COLOR_CARD_BG);
        break;
    }
    case VEC_ICON_FILE: {
        fb_fill_rect(x + (size * 2 / 10), y, size * 6 / 10, size, color);
        fb_fill_rect(x + (size * 2 / 10) + 2, y + 2, (size * 6 / 10) - 4, size - 4, COLOR_CARD_BG);
        for (int row = 0; row < 3; row++) {
            fb_fill_rect(x + (size * 3 / 10), y + (size * 3 / 10) + (row * size * 2 / 10),
                         size * 4 / 10, 2, color);
        }
        break;
    }
    case VEC_ICON_BINARY: {
        int r = size / 2;
        int cx = x + r, cy = y + r;
        for (int dy = -r; dy <= r; dy++) {
            int dx_max = r - (dy < 0 ? -dy : dy);
            for (int dx = -dx_max; dx <= dx_max; dx++) {
                if ((dx*dx + dy*dy) > (r*r/6))
                    fb_write_pixel(cx + dx, cy + dy, color);
            }
        }
        break;
    }
    case VEC_ICON_BACK: {
        int h = size;
        for (int col = 0; col < h; col++) {
            int span = col / 2;
            int start_row = (h / 2) - span;
            int end_row = (h / 2) + span;
            for (int r = start_row; r <= end_row; r++) {
                fb_write_pixel(x + col, y + r, color);
            }
        }
        break;
    }
    case VEC_ICON_HOME: {
        int h = size * 6 / 10;
        for (int row = 0; row < h; row++) {
            int span = row;
            for (int col = (size / 2) - span; col <= (size / 2) + span; col++) {
                fb_write_pixel(x + col, y + row, color);
            }
        }
        fb_fill_rect(x + (size * 2 / 10), y + h, size * 6 / 10, size - h, color);
        fb_fill_rect(x + (size * 4 / 10), y + h + 4, size * 2 / 10, size - h - 4, COLOR_NAVBAR_BG);
        break;
    }
    case VEC_ICON_RECENTS: {
        int box_s = size * 65 / 100;
        fb_draw_card(x + (size - box_s), y, box_s, box_s, COLOR_NAVBAR_BG, color);
        fb_draw_card(x, y + (size - box_s), box_s, box_s, COLOR_NAVBAR_BG, color);
        break;
    }
    case VEC_ICON_TERMINAL: {
        int h = size;
        for (int r = 0; r < h; r++) {
            int c = (r < h/2) ? r : (h - 1 - r);
            fb_fill_rect(x + c, y + r, 3, 1, color);
        }
        fb_fill_rect(x + (size * 4 / 10), y + (size * 8 / 10), size * 5 / 10, 3, color);
        break;
    }
    case VEC_ICON_BATTERY: {
        fb_fill_rect(x, y, size * 8 / 10, size, color);
        fb_fill_rect(x + 2, y + 2, (size * 8 / 10) - 4, size - 4, COLOR_STATUSBAR_BG);
        fb_fill_rect(x + (size * 8 / 10), y + (size * 3 / 10), size * 2 / 10, size * 4 / 10, color);
        fb_fill_rect(x + 4, y + 4, (size * 5 / 10) - 4, size - 8, COLOR_STATUS_OK);
        break;
    }
    case VEC_ICON_3D: {
        int r = size / 2;
        int cx = x + r, cy = y + r;
        for (int i = 0; i < size; i++) {
            fb_write_pixel(x + i, y + (size/3), color);
            fb_write_pixel(x + i, y + (size*2/3), color);
            fb_write_pixel(x + (size/3), y + i, color);
            fb_write_pixel(x + (size*2/3), y + i, color);
        }
        fb_draw_card(cx - (size/4), cy - (size/4), size/2, size/2, color, COLOR_TOPBAR_ACCENT);
        break;
    }
    case VEC_ICON_STORAGE: {
        fb_fill_rect(x, y + (size * 1 / 10), size, size * 2 / 10, color);
        fb_fill_rect(x, y + (size * 4 / 10), size, size * 2 / 10, color);
        fb_fill_rect(x, y + (size * 7 / 10), size, size * 2 / 10, color);
        fb_fill_rect(x + (size * 7 / 10), y + (size * 2 / 10), 4, 2, COLOR_STATUS_OK);
        fb_fill_rect(x + (size * 7 / 10), y + (size * 5 / 10), 4, 2, COLOR_STATUS_OK);
        fb_fill_rect(x + (size * 7 / 10), y + (size * 8 / 10), 4, 2, COLOR_STATUS_OK);
        break;
    }
    case VEC_ICON_SENSOR: {
        int r = size / 2;
        int cx = x + r, cy = y + r;
        for (int a = 0; a < 360; a += 3) {
            float rad = a * 0.01745f;
            int px = cx + (int)(cosf(rad) * (r * 0.9f));
            int py = cy + (int)(sinf(rad) * (r * 0.9f));
            fb_write_pixel(px, py, color);
        }
        fb_fill_rect(cx - 3, cy - 3, 6, 6, COLOR_STATUS_OK);
        break;
    }
    case VEC_ICON_POWER: {
        int r = size / 2;
        int cx = x + r, cy = y + r;
        for (int a = 45; a <= 315; a += 4) {
            float rad = (a - 90) * 0.01745f;
            int px = cx + (int)(cosf(rad) * (r * 0.8f));
            int py = cy + (int)(sinf(rad) * (r * 0.8f));
            fb_fill_rect(px - 1, py - 1, 2, 2, color);
        }
        fb_fill_rect(cx - 1, y, 3, size / 2, color);
        break;
    }
    }
}
