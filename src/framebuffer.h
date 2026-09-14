/* src/framebuffer.h */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <linux/fb.h>

struct fb_context {
    int fbfd;
    unsigned char *fbp;        /* Primary hardware mmapped buffer */
    unsigned char *backbuffer; /* Offscreen 32bpp backbuffer (Zero-blink) */
    long stride;
    int bpp;
    long framesize;
    int xres;
    int yres;
    struct fb_var_screeninfo vinfo;
};

extern struct fb_context g_fb;

int  fb_init(void);
void fb_cleanup(void);
void fb_blank(int blank);
void fb_present(void);
void fb_pan(void);             /* Compatibility alias to fb_present */
void fb_write_pixel(int x, int y, unsigned int rgb);
void fb_blend_pixel(int x, int y, unsigned int rgb, unsigned char alpha);
void fb_fill_rect(int x, int y, int w, int h, unsigned int rgb);
void fb_draw_card(int x, int y, int w, int h, unsigned int bg, unsigned int border);

#endif
