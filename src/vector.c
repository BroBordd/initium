#include <stdio.h>
#include <math.h>
#include "vector.h"
#include "framebuffer.h"
#include "config.h"

void vector_init(void)
{
}

static void draw_filled_circle(int cx, int cy, int r, unsigned int color) {
    for (int y = -r; y <= r; y++) {
        for (int x = -r; x <= r; x++) {
            if (x*x + y*y <= r*r) {
                fb_write_pixel(cx + x, cy + y, color);
            }
        }
    }
}

static void draw_capsule(int x, int y, int w, int h, unsigned int color) {
    int r = (w < h ? w : h) / 2;
    if (w > h) {
        fb_fill_rect(x + r, y, w - 2*r, h, color);
        draw_filled_circle(x + r, y + r, r, color);
        draw_filled_circle(x + w - r, y + r, r, color);
    } else {
        fb_fill_rect(x, y + r, w, h - 2*r, color);
        draw_filled_circle(x + r, y + r, r, color);
        draw_filled_circle(x + r, y + h - r, r, color);
    }
}

void vector_draw_icon(int x, int y, int size, vector_icon_t icon, unsigned int color)
{
    switch (icon) {
    case VEC_ICON_FOLDER: {
        draw_capsule(x, y + (size * 1 / 10), size * 4 / 10, size * 2 / 10, color);
        draw_capsule(x, y + (size * 3 / 10), size, size * 6 / 10, color);
        draw_capsule(x + 4, y + (size * 3 / 10) + 4, size - 8, (size * 6 / 10) - 8, COLOR_CARD_BG);
        break;
    }
    case VEC_ICON_FILE: {
        draw_capsule(x + (size * 2 / 10), y, size * 6 / 10, size, color);
        draw_capsule(x + (size * 2 / 10) + 4, y + 4, (size * 6 / 10) - 8, size - 8, COLOR_CARD_BG);
        for (int row = 0; row < 3; row++) {
            draw_capsule(x + (size * 3 / 10), y + (size * 3 / 10) + (row * size * 2 / 10),
                         size * 4 / 10, 4, color);
        }
        break;
    }
    case VEC_ICON_BINARY: {
        draw_filled_circle(x + size/2, y + size/2, size/2, color);
        draw_filled_circle(x + size/2, y + size/2, size/2 - 4, COLOR_CARD_BG);
        draw_filled_circle(x + size/2, y + size/2, size/4, color);
        break;
    }
    case VEC_ICON_BACK: {
        int cx = x + size/2;
        int cy = y + size/2;
        for (int r = 0; r < size/2; r++) {
            draw_filled_circle(cx - r, cy - r, 2, color);
            draw_filled_circle(cx - r, cy + r, 2, color);
        }
        draw_capsule(cx - size/4, cy - 2, size/2 + size/4, 4, color);
        break;
    }
    case VEC_ICON_HOME: {
        draw_capsule(x + size/4, y + size/2, size/2, size/2, color);
        draw_capsule(x + size/4 + 4, y + size/2 + 4, size/2 - 8, size/2 - 8, COLOR_NAVBAR_BG);
        for (int r = 0; r < size/2; r++) {
            draw_filled_circle(x + size/2 - r, y + size/2 - r, 2, color);
            draw_filled_circle(x + size/2 + r, y + size/2 - r, 2, color);
        }
        break;
    }
    case VEC_ICON_RECENTS: {
        draw_capsule(x + size*2/10, y + size*2/10, size*6/10, size*6/10, color);
        draw_capsule(x + size*2/10 + 4, y + size*2/10 + 4, size*6/10 - 8, size*6/10 - 8, COLOR_NAVBAR_BG);
        break;
    }
    case VEC_ICON_PROCESSES: {
        draw_capsule(x + size*2/10, y + size*2/10, size*6/10, 4, color);
        draw_capsule(x + size*2/10, y + size*5/10, size*6/10, 4, color);
        draw_capsule(x + size*2/10, y + size*8/10, size*6/10, 4, color);
        break;
    }
    case VEC_ICON_TERMINAL: {
        draw_capsule(x + size*1/10, y + size*2/10, size*3/10, 4, color);
        draw_capsule(x + size*1/10, y + size*5/10, size*3/10, 4, color);
        draw_capsule(x + size*4/10, y + size*8/10, size*5/10, 4, color);
        break;
    }
    case VEC_ICON_BATTERY: {
        draw_capsule(x, y + size*1/10, size * 8 / 10, size * 8 / 10, color);
        draw_capsule(x + 4, y + size*1/10 + 4, (size * 8 / 10) - 8, (size * 8 / 10) - 8, COLOR_STATUSBAR_BG);
        draw_capsule(x + (size * 8 / 10), y + (size * 3 / 10), size * 2 / 10, size * 4 / 10, color);
        draw_capsule(x + 6, y + size*1/10 + 6, (size * 5 / 10) - 6, (size * 8 / 10) - 12, COLOR_STATUS_OK);
        break;
    }
    case VEC_ICON_3D: {
        draw_filled_circle(x + size/2, y + size/2, size/2, color);
        draw_filled_circle(x + size/2, y + size/2, size/2 - 4, COLOR_CARD_BG);
        draw_capsule(x + size/2 - 2, y + 4, 4, size - 8, color);
        draw_capsule(x + 4, y + size/2 - 2, size - 8, 4, color);
        break;
    }
    case VEC_ICON_STORAGE: {
        draw_capsule(x, y + (size * 1 / 10), size, size * 2 / 10, color);
        draw_capsule(x, y + (size * 4 / 10), size, size * 2 / 10, color);
        draw_capsule(x, y + (size * 7 / 10), size, size * 2 / 10, color);
        draw_filled_circle(x + (size * 8 / 10), y + (size * 2 / 10), 2, COLOR_STATUS_OK);
        draw_filled_circle(x + (size * 8 / 10), y + (size * 5 / 10), 2, COLOR_STATUS_OK);
        draw_filled_circle(x + (size * 8 / 10), y + (size * 8 / 10), 2, COLOR_STATUS_OK);
        break;
    }
    case VEC_ICON_SENSOR: {
        draw_filled_circle(x + size/2, y + size/2, size/2, color);
        draw_filled_circle(x + size/2, y + size/2, size/2 - 4, COLOR_CARD_BG);
        draw_filled_circle(x + size/2, y + size/2, size/4, color);
        break;
    }
    case VEC_ICON_POWER: {
        draw_capsule(x + size/2 - 2, y, 4, size/2, color);
        draw_filled_circle(x + size/2, y + size/2 + size/6, size/3, color);
        draw_filled_circle(x + size/2, y + size/2 + size/6, size/3 - 4, COLOR_CARD_BG);
        break;
    }
    }
}
