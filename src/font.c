#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
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

/*
 * Glyph bitmap cache.
 *
 * font_draw_text() used to call stbtt_GetCodepointBitmap() for every
 * character, every single frame -- a full spline tessellation + scanline
 * rasterization from scratch, 60 times a second. With any meaningful
 * amount of on-screen text (file lists, status bar, labels) this alone
 * was the dominant cost per frame and the main reason FPS tanked while
 * scrolling. Only a handful of discrete pixel sizes are used in the UI
 * (see FONT_SIZE_* in config.h) and text is effectively ASCII, so we
 * rasterize each (codepoint, size) pair exactly once and reuse the
 * cached alpha bitmap on every subsequent draw. font_measure_text()
 * reuses the same cached advance widths instead of re-querying stb.
 */
#define GLYPH_CACHE_SLOTS   512
#define GLYPH_SIZE_BUCKETS  8      /* distinct pixel sizes we expect to see */

struct glyph_entry {
    int used;
    int codepoint;
    float size_px;
    int w, h, xoff, yoff;
    int advance;                  /* fixed-point: advanceWidth * scale * 256 */
    unsigned char *bitmap;        /* w * h alpha coverage, owned by cache */
};

static struct glyph_entry g_glyph_cache[GLYPH_CACHE_SLOTS];

/* Distinct size buckets seen so far, so different FONT_SIZE_* constants
 * don't collide/alias into the same cache slot due to float rounding. */
static float g_size_buckets[GLYPH_SIZE_BUCKETS];
static int g_num_size_buckets = 0;

static int bucket_for_size(float size_px)
{
    for (int i = 0; i < g_num_size_buckets; i++) {
        if (g_size_buckets[i] == size_px)
            return i;
    }
    if (g_num_size_buckets < GLYPH_SIZE_BUCKETS) {
        g_size_buckets[g_num_size_buckets] = size_px;
        return g_num_size_buckets++;
    }
    /* Fallback: too many distinct sizes requested, just alias to bucket 0.
     * Shouldn't happen given the fixed FONT_SIZE_* set in config.h. */
    return 0;
}

static struct glyph_entry *glyph_cache_lookup(int codepoint, float size_px)
{
    int bucket = bucket_for_size(size_px);
    unsigned int key = ((unsigned int)codepoint * 31u + (unsigned int)bucket * 2654435761u);
    unsigned int slot = key % GLYPH_CACHE_SLOTS;

    /* Linear probe a small run; codepoints are ASCII-range so collisions
     * are rare and cheap to resolve. */
    for (int probe = 0; probe < 8; probe++) {
        struct glyph_entry *e = &g_glyph_cache[(slot + (unsigned int)probe) % GLYPH_CACHE_SLOTS];

        if (e->used && e->codepoint == codepoint && e->size_px == size_px)
            return e;

        if (!e->used) {
            /* Rasterize once and populate this slot. */
            float scale = stbtt_ScaleForPixelHeight(&g_font, size_px);
            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&g_font, codepoint, &advanceWidth, &leftSideBearing);

            int w = 0, h = 0, xoff = 0, yoff = 0;
            unsigned char *bmp = stbtt_GetCodepointBitmap(
                &g_font, 0, scale, codepoint, &w, &h, &xoff, &yoff
            );

            e->used = 1;
            e->codepoint = codepoint;
            e->size_px = size_px;
            e->w = w;
            e->h = h;
            e->xoff = xoff;
            e->yoff = yoff;
            e->advance = (int)(advanceWidth * scale * 256.0f);
            e->bitmap = bmp; /* may be NULL for glyphs with no ink (e.g. space) */

            return e;
        }
    }

    /* Cache pressure fallback (shouldn't happen in practice): caller
     * rasterizes directly without caching rather than corrupting an
     * existing entry. */
    return NULL;
}

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
    memset(g_glyph_cache, 0, sizeof(g_glyph_cache));
    g_num_size_buckets = 0;

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
    int cursor_x_fp = px * 256; /* fixed-point, 1/256 px, to avoid float drift across long strings */

    while (*text) {
        int codepoint = (unsigned char)*text;
        struct glyph_entry *e = glyph_cache_lookup(codepoint, size_px);

        if (e) {
            if (e->bitmap) {
                int origin_x = (cursor_x_fp >> 8) + e->xoff;
                int origin_y = baseline + e->yoff;

                for (int j = 0; j < e->h; j++) {
                    const unsigned char *row = e->bitmap + (size_t)j * e->w;
                    for (int i = 0; i < e->w; i++) {
                        unsigned char alpha = row[i];
                        if (alpha > 0)
                            fb_blend_pixel(origin_x + i, origin_y + j, color, alpha);
                    }
                }
            }
            cursor_x_fp += e->advance;
        } else {
            /* Cache exhausted (shouldn't happen) -- rasterize uncached
             * as a safe fallback so text never silently disappears. */
            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&g_font, codepoint, &advanceWidth, &leftSideBearing);

            int w, h, xoff, yoff;
            unsigned char *bitmap = stbtt_GetCodepointBitmap(
                &g_font, 0, scale, codepoint, &w, &h, &xoff, &yoff
            );
            if (bitmap) {
                int origin_x = (cursor_x_fp >> 8) + xoff;
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
            cursor_x_fp += (int)(advanceWidth * scale * 256.0f);
        }

        text++;
    }
}

int font_measure_text(const char *text, float size_px)
{
    if (!g_font_loaded || !text) return 0;

    int width_fp = 0; /* fixed-point, 1/256 px */

    while (*text) {
        int codepoint = (unsigned char)*text;
        struct glyph_entry *e = glyph_cache_lookup(codepoint, size_px);

        if (e) {
            width_fp += e->advance;
        } else {
            /* Cache exhausted fallback: query stb directly, uncached. */
            float scale = stbtt_ScaleForPixelHeight(&g_font, size_px);
            int advanceWidth, leftSideBearing;
            stbtt_GetCodepointHMetrics(&g_font, codepoint, &advanceWidth, &leftSideBearing);
            width_fp += (int)(advanceWidth * scale * 256.0f);
        }

        text++;
    }

    return width_fp >> 8;
}
