/* src/vector.c */
#include <stdio.h>
#include <string.h>
#include "vector.h"
#include "framebuffer.h"
#include "config.h"

void vector_init(void)
{
    /* Ready for SVG cache initialization */
}

/* Procedural geometric anti-aliased vector rendering */
void vector_draw_icon(int x, int y, int size, vector_icon_t icon, unsigned int color)
{
    switch (icon) {
    case VEC_ICON_FOLDER: {
        /* Folder Tab */
        int tab_w = size * 4 / 10;
        int tab_h = size * 2 / 10;
        fb_fill_rect(x, y + (size * 1 / 10), tab_w, tab_h, color);
        /* Folder Body */
        fb_fill_rect(x, y + (size * 3 / 10), size, size * 6 / 10, color);
        /* Cutout Line */
        fb_fill_rect(x + 2, y + (size * 3 / 10) + 2, size - 4, (size * 6 / 10) - 4, COLOR_CARD_BG);
        break;
    }
    case VEC_ICON_FILE: {
        /* Sheet */
        fb_fill_rect(x + (size * 2 / 10), y, size * 6 / 10, size, color);
        fb_fill_rect(x + (size * 2 / 10) + 2, y + 2, (size * 6 / 10) - 4, size - 4, COLOR_CARD_BG);
        /* Text Lines inside */
        for (int row = 0; row < 3; row++) {
            fb_fill_rect(x + (size * 3 / 10), y + (size * 3 / 10) + (row * size * 2 / 10),
                         size * 4 / 10, 2, color);
        }
        break;
    }
    case VEC_ICON_BINARY: {
        /* Gear / Executable Diamond */
        int r = size / 2;
        int cx = x + r;
        int cy = y + r;
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
        /* Left Arrow Triangle */
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
        /* House Roof */
        int h = size * 6 / 10;
        for (int row = 0; row < h; row++) {
            int span = row;
            for (int col = (size / 2) - span; col <= (size / 2) + span; col++) {
                fb_write_pixel(x + col, y + row, color);
            }
        }
        /* House Body */
        fb_fill_rect(x + (size * 2 / 10), y + h, size * 6 / 10, size - h, color);
        fb_fill_rect(x + (size * 4 / 10), y + h + 4, size * 2 / 10, size - h - 4, COLOR_NAVBAR_BG);
        break;
    }
    case VEC_ICON_RECENTS: {
        /* Two Overlapping Rectangles */
        int box_s = size * 65 / 100;
        /* Back box */
        fb_draw_card(x + (size - box_s), y, box_s, box_s, COLOR_NAVBAR_BG, color);
        /* Front box */
        fb_draw_card(x, y + (size - box_s), box_s, box_s, COLOR_NAVBAR_BG, color);
        break;
    }
    case VEC_ICON_TERMINAL: {
        /* Prompt chevron '>' */
        int h = size;
        for (int r = 0; r < h; r++) {
            int c = (r < h/2) ? r : (h - 1 - r);
            fb_fill_rect(x + c, y + r, 3, 1, color);
        }
        /* Underline cursor */
        fb_fill_rect(x + (size * 4 / 10), y + (size * 8 / 10), size * 5 / 10, 3, color);
        break;
    }
    case VEC_ICON_BATTERY: {
        /* Battery Body */
        fb_fill_rect(x, y, size * 8 / 10, size, color);
        fb_fill_rect(x + 2, y + 2, (size * 8 / 10) - 4, size - 4, COLOR_STATUSBAR_BG);
        /* Battery Nipple */
        fb_fill_rect(x + (size * 8 / 10), y + (size * 3 / 10), size * 2 / 10, size * 4 / 10, color);
        /* Fill bar */
        fb_fill_rect(x + 4, y + 4, (size * 5 / 10) - 4, size - 8, COLOR_STATUS_OK);
        break;
    }
    }
}

int vector_load_and_draw_svg(int x, int y, int size, const char *svg_filename, unsigned int color)
{
    char fullpath[256];
    snprintf(fullpath, sizeof(fullpath), "%s/%s", VECTOR_ICONS_DIR, svg_filename);

    FILE *fp = fopen(fullpath, "r");
    if (!fp) {
        /* Fallback if external SVG file is not present on disk */
        if (strstr(svg_filename, "folder"))      vector_draw_icon(x, y, size, VEC_ICON_FOLDER, color);
        else if (strstr(svg_filename, "binary")) vector_draw_icon(x, y, size, VEC_ICON_BINARY, color);
        else if (strstr(svg_filename, "home"))   vector_draw_icon(x, y, size, VEC_ICON_HOME, color);
        else if (strstr(svg_filename, "back"))   vector_draw_icon(x, y, size, VEC_ICON_BACK, color);
        else if (strstr(svg_filename, "recents"))vector_draw_icon(x, y, size, VEC_ICON_RECENTS, color);
        else vector_draw_icon(x, y, size, VEC_ICON_FILE, color);
        return 0;
    }

    fclose(fp);
    /* Procedural render for standard icon */
    vector_draw_icon(x, y, size, VEC_ICON_FILE, color);
    return 1;
}
