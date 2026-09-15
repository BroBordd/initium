/* src/framebuffer.c */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <arm_neon.h>
#include "framebuffer.h"
#include "config.h"
#include "log.h"

struct fb_context g_fb = {0};

void fb_blank(int blank)
{
    int fd = open(BLANK_PATH, O_WRONLY);
    if (fd >= 0) {
        write(fd, blank ? "1" : "0", 1);
        close(fd);
    }
    if (g_fb.fbfd >= 0)
        ioctl(g_fb.fbfd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
}

int fb_init(void)
{
    g_fb.fbfd = open(FB_DEV_NODE, O_RDWR);
    fb_blank(1);
    fb_blank(0);

    if (g_fb.fbfd < 0) {
        g_fb.fbfd = open(FB_DEV_NODE, O_RDWR);
        if (g_fb.fbfd < 0) return 0;
    }

    struct fb_fix_screeninfo finfo;
    memset(&g_fb.vinfo, 0, sizeof(g_fb.vinfo));
    memset(&finfo, 0, sizeof(finfo));

    if (ioctl(g_fb.fbfd, FBIOGET_VSCREENINFO, &g_fb.vinfo) < 0 ||
        ioctl(g_fb.fbfd, FBIOGET_FSCREENINFO, &finfo) < 0)
        return 0;

    g_fb.bpp = g_fb.vinfo.bits_per_pixel / 8;
    g_fb.xres = (int)g_fb.vinfo.xres;
    g_fb.yres = (int)g_fb.vinfo.yres;
    g_fb.stride = (long)finfo.line_length;
    g_fb.framesize = g_fb.stride * g_fb.vinfo.yres_virtual;

    if (g_fb.framesize <= 0 || g_fb.bpp <= 0) return 0;

    g_fb.fbp = mmap(NULL, g_fb.framesize, PROT_READ | PROT_WRITE,
                    MAP_SHARED, g_fb.fbfd, 0);
    if (g_fb.fbp == MAP_FAILED) return 0;

    g_fb.backbuffer = (unsigned char *)malloc(g_fb.framesize);
    if (!g_fb.backbuffer) {
        munmap(g_fb.fbp, g_fb.framesize);
        return 0;
    }
    memset(g_fb.backbuffer, 0, g_fb.framesize);

    g_fb.dirty_min_x = 0;
    g_fb.dirty_min_y = 0;
    g_fb.dirty_max_x = g_fb.xres;
    g_fb.dirty_max_y = g_fb.yres;
    g_fb.has_dirty = 1;
    g_fb.clip_enabled = 0;

    return 1;
}

void fb_cleanup(void)
{
    if (g_fb.backbuffer) {
        free(g_fb.backbuffer);
        g_fb.backbuffer = NULL;
    }
    if (g_fb.fbp && g_fb.fbp != MAP_FAILED) {
        munmap(g_fb.fbp, g_fb.framesize);
        g_fb.fbp = NULL;
    }
    if (g_fb.fbfd >= 0) {
        close(g_fb.fbfd);
        g_fb.fbfd = -1;
    }
}

void fb_set_clip(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_fb.xres) w = g_fb.xres - x;
    if (y + h > g_fb.yres) h = g_fb.yres - y;

    g_fb.clip_x1 = x;
    g_fb.clip_y1 = y;
    g_fb.clip_x2 = x + w;
    g_fb.clip_y2 = y + h;
    g_fb.clip_enabled = 1;
}

void fb_clear_clip(void)
{
    g_fb.clip_enabled = 0;
}

void fb_mark_dirty(int x, int y, int w, int h)
{
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > g_fb.xres) w = g_fb.xres - x;
    if (y + h > g_fb.yres) h = g_fb.yres - y;
    if (w <= 0 || h <= 0) return;

    if (!g_fb.has_dirty) {
        g_fb.dirty_min_x = x;
        g_fb.dirty_min_y = y;
        g_fb.dirty_max_x = x + w;
        g_fb.dirty_max_y = y + h;
        g_fb.has_dirty = 1;
    } else {
        if (x < g_fb.dirty_min_x) g_fb.dirty_min_x = x;
        if (y < g_fb.dirty_min_y) g_fb.dirty_min_y = y;
        if (x + w > g_fb.dirty_max_x) g_fb.dirty_max_x = x + w;
        if (y + h > g_fb.dirty_max_y) g_fb.dirty_max_y = y + h;
    }
}

static inline void neon_memcpy_row(void *dst, const void *src, size_t bytes)
{
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    while (bytes >= 64) {
        uint8x16x4_t val = vld1q_u8_x4(s);
        vst1q_u8_x4(d, val);
        d += 64; s += 64; bytes -= 64;
    }
    while (bytes >= 16) {
        uint8x16_t val = vld1q_u8(s);
        vst1q_u8(d, val);
        d += 16; s += 16; bytes -= 16;
    }
    while (bytes > 0) {
        *d++ = *s++;
        bytes--;
    }
}

void fb_present(void)
{
    if (!g_fb.fbp || !g_fb.backbuffer || !g_fb.has_dirty) return;

    int min_y = g_fb.dirty_min_y;
    int max_y = g_fb.dirty_max_y;
    int min_x = g_fb.dirty_min_x;
    int width = g_fb.dirty_max_x - min_x;
    size_t row_bytes = (size_t)width * g_fb.bpp;

    for (int y = min_y; y < max_y; y++) {
        long offset = (long)y * g_fb.stride + (long)min_x * g_fb.bpp;
        neon_memcpy_row(g_fb.fbp + offset, g_fb.backbuffer + offset, row_bytes);
    }

    g_fb.has_dirty = 0;
    g_fb.vinfo.xoffset = 0;
    g_fb.vinfo.yoffset = 0;
    ioctl(g_fb.fbfd, FBIOPAN_DISPLAY, &g_fb.vinfo);
}

void fb_pan(void)
{
    fb_present();
}

void fb_write_pixel(int x, int y, unsigned int rgb)
{
    if (g_fb.clip_enabled) {
        if (x < g_fb.clip_x1 || x >= g_fb.clip_x2 || y < g_fb.clip_y1 || y >= g_fb.clip_y2)
            return;
    }
    if (x < 0 || y < 0 || x >= g_fb.xres || y >= g_fb.yres) return;

    long off = (long)y * g_fb.stride + (long)x * g_fb.bpp;
    if (off < 0 || off + g_fb.bpp > g_fb.framesize) return;

    unsigned char *p = g_fb.backbuffer + off;
    p[0] = rgb & 0xFF;
    p[1] = (rgb >> 8) & 0xFF;
    p[2] = (rgb >> 16) & 0xFF;
    if (g_fb.bpp > 3) p[3] = 0xFF;

    fb_mark_dirty(x, y, 1, 1);
}

void fb_blend_pixel(int x, int y, unsigned int rgb, unsigned char alpha)
{
    if (g_fb.clip_enabled) {
        if (x < g_fb.clip_x1 || x >= g_fb.clip_x2 || y < g_fb.clip_y1 || y >= g_fb.clip_y2)
            return;
    }
    if (alpha == 0 || x < 0 || y < 0 || x >= g_fb.xres || y >= g_fb.yres) return;

    long off = (long)y * g_fb.stride + (long)x * g_fb.bpp;
    if (off < 0 || off + g_fb.bpp > g_fb.framesize) return;

    unsigned char *p = g_fb.backbuffer + off;
    if (alpha == 255) {
        p[0] = rgb & 0xFF;
        p[1] = (rgb >> 8) & 0xFF;
        p[2] = (rgb >> 16) & 0xFF;
        if (g_fb.bpp > 3) p[3] = 0xFF;
        fb_mark_dirty(x, y, 1, 1);
        return;
    }

    unsigned char inv = 255 - alpha;
    p[0] = (unsigned char)(((rgb & 0xFF) * alpha + p[0] * inv) / 255);
    p[1] = (unsigned char)((((rgb >> 8) & 0xFF) * alpha + p[1] * inv) / 255);
    p[2] = (unsigned char)((((rgb >> 16) & 0xFF) * alpha + p[2] * inv) / 255);
    if (g_fb.bpp > 3) p[3] = 0xFF;

    fb_mark_dirty(x, y, 1, 1);
}

void fb_fill_rect(int x, int y, int w, int h, unsigned int rgb)
{
    int start_x = x, end_x = x + w;
    int start_y = y, end_y = y + h;

    if (g_fb.clip_enabled) {
        if (start_x < g_fb.clip_x1) start_x = g_fb.clip_x1;
        if (end_x > g_fb.clip_x2)   end_x = g_fb.clip_x2;
        if (start_y < g_fb.clip_y1) start_y = g_fb.clip_y1;
        if (end_y > g_fb.clip_y2)   end_y = g_fb.clip_y2;
    }

    if (start_x >= end_x || start_y >= end_y) return;

    for (int cy = start_y; cy < end_y; cy++) {
        for (int cx = start_x; cx < end_x; cx++) {
            long off = (long)cy * g_fb.stride + (long)cx * g_fb.bpp;
            if (off >= 0 && off + g_fb.bpp <= g_fb.framesize) {
                unsigned char *p = g_fb.backbuffer + off;
                p[0] = rgb & 0xFF;
                p[1] = (rgb >> 8) & 0xFF;
                p[2] = (rgb >> 16) & 0xFF;
                if (g_fb.bpp > 3) p[3] = 0xFF;
            }
        }
    }
    fb_mark_dirty(start_x, start_y, end_x - start_x, end_y - start_y);
}

void fb_draw_card(int x, int y, int w, int h, unsigned int bg, unsigned int border)
{
    fb_fill_rect(x + 4, y, w - 8, h, bg);
    fb_fill_rect(x, y + 4, w, h - 8, bg);

    fb_fill_rect(x + 4, y, w - 8, 2, border);
    fb_fill_rect(x + 4, y + h - 2, w - 8, 2, border);
    fb_fill_rect(x, y + 4, 2, h - 8, border);
    fb_fill_rect(x + w - 2, y + 4, 2, h - 8, border);
}
