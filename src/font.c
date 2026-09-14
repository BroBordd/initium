/* font.c */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/stat.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include "font.h"
#include "framebuffer.h"
#include "config.h"
#include "log.h"

static stbtt_fontinfo g_font;
static unsigned char *g_font_buffer = NULL;
static int g_font_loaded = 0;

static int load_font_file(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return 0;

    struct stat st;
    if (fstat(fd, &st) < 0 || st.st_size <= 0) {
        close(fd);
        return 0;
    }

    g_font_buffer = (unsigned char *)malloc(st.st_size);
    if (!g_font_buffer) {
        close(fd);
        return 0;
    }

    ssize_t r = read(fd, g_font_buffer, st.st_size);
    close(fd);

    if (r != st.st_size) {
        free(g_font_buffer);
        g_font_buffer = NULL;
        return 0;
    }

    if (!stbtt_InitFont(&g_font, g_font_buffer, stbtt_GetFontOffsetForIndex(g_font_buffer, 0))) {
        free(g_font_buffer);
        g_font_buffer = NULL;
        return 0;
    }

    dbgf(LOGPFX "loaded font: %s (%lld bytes)\n", path, (long long)st.st_size);
    return 1;
}

int font_init(void)
{
    if (load_font_file(FONT_PATH_PRIMARY))  { g_font_loaded = 1; return 1; }
    if (load_font_file(FONT_PATH_ROBOTO))   { g_font_loaded = 1; return 1; }
    if (load_font_file(FONT_PATH_FALLBACK)) { g_font_loaded = 1; return 1; }

    dbgf(LOGPFX "failed to load any font!\n");
    return 0;
}

void font_draw_text(int px, int py, const char *text, float size_px, unsigned int color)
{
    if (!g_font_loaded || !text) return;

    float scale = stbtt_ScaleForPixelHeight(&g_font, size_px);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&g_font, &ascent, &descent, &lineGap);

    int baseline = py + (int)(ascent * scale);
    float cursor_x = (float)px;

    while (*text) {
        int advanceWidth, leftSideBearing;
        stbtt_GetCodepointHMetrics(&g_font, *text, &advanceWidth, &leftSideBearing);

        int w, h, xoff, yoff;
        unsigned char *bitmap = stbtt_GetCodepointBitmap(
            &g_font, 0, scale, *text, &w, &h, &xoff, &yoff
        );

        if (bitmap) {
            int origin_x = (int)cursor_x + xoff;
            int origin_y = baseline + yoff;

            for (int j = 0; j < h; j++) {
                for (int i = 0; i < w; i++) {
                    unsigned char alpha = bitmap[j * w + i];
                    if (alpha > 0)
                        fb_blend_pixel(origin_x + i, origin_y + j, color, alpha);
                }
            }
            stbtt_FreeBitmap(bitmap, NULL);
        }

        cursor_x += advanceWidth * scale;
        text++;
    }
}
