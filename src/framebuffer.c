/* src/framebuffer.c */
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
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

    /* Allocate offscreen backbuffer for zero-flicker double buffering */
    g_fb.backbuffer = (unsigned char *)malloc(g_fb.framesize);
    if (!g_fb.backbuffer) {
        munmap(g_fb.fbp, g_fb.framesize);
        return 0;
    }
    memset(g_fb.backbuffer, 0, g_fb.framesize);

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

/* Present offscreen backbuffer to screen without any tearing or blink */
void fb_present(void)
{
    if (!g_fb.fbp || !g_fb.backbuffer) return;

    memcpy(g_fb.fbp, g_fb.backbuffer, g_fb.framesize);
    msync(g_fb.fbp, g_fb.framesize, MS_SYNC);

    g_fb.vinfo.xoffset = 0;
    g_fb.vinfo.yoffset = 0;
    ioctl(g_fb.fbfd, FBIOPAN_DISPLAY, &g_fb.vinfo);
}

void fb_write_pixel(int x, int y, unsigned int rgb)
{
    if (x < 0 || y < 0 || x >= g_fb.xres || y >= g_fb.yres) return;

    long off = (long)y * g_fb.stride + (long)x * g_fb.bpp;
    if (off < 0 || off + g_fb.bpp > g_fb.framesize) return;

    unsigned char *p = g_fb.backbuffer + off;
    p[0] = rgb & 0xFF;         /* Blue  */
    p[1] = (rgb >> 8) & 0xFF;  /* Green */
    p[2] = (rgb >> 16) & 0xFF; /* Red   */
    if (g_fb.bpp > 3) p[3] = 0xFF;
}

void fb_blend_pixel(int x, int y, unsigned int rgb, unsigned char alpha)
{
    if (alpha == 0 || x < 0 || y < 0 || x >= g_fb.xres || y >= g_fb.yres) return;

    long off = (long)y * g_fb.stride + (long)x * g_fb.bpp;
    if (off < 0 || off + g_fb.bpp > g_fb.framesize) return;

    unsigned char *p = g_fb.backbuffer + off;
    if (alpha == 255) {
        p[0] = rgb & 0xFF;
        p[1] = (rgb >> 8) & 0xFF;
        p[2] = (rgb >> 16) & 0xFF;
        if (g_fb.bpp > 3) p[3] = 0xFF;
        return;
    }

    unsigned char inv = 255 - alpha;
    p[0] = (unsigned char)(((rgb & 0xFF) * alpha + p[0] * inv) / 255);
    p[1] = (unsigned char)((((rgb >> 8) & 0xFF) * alpha + p[1] * inv) / 255);
    p[2] = (unsigned char)((((rgb >> 16) & 0xFF) * alpha + p[2] * inv) / 255);
    if (g_fb.bpp > 3) p[3] = 0xFF;
}

void fb_fill_rect(int x, int y, int w, int h, unsigned int rgb)
{
    for (int cy = y; cy < y + h; cy++)
        for (int cx = x; cx < x + w; cx++)
            fb_write_pixel(cx, cy, rgb);
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

void fb_pan(void)
{
    fb_present();
}
