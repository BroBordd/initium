/* src/framebuffer.h */
#ifndef FRAMEBUFFER_H
#define FRAMEBUFFER_H

#include <linux/fb.h>

struct fb_context {
    int fbfd;
    unsigned char *fbp;
    unsigned char *backbuffer;
    long stride;
    int bpp;
    long framesize;
    int xres;
    int yres;
    struct fb_var_screeninfo vinfo;

    /* Dirty region tracking */
    int dirty_min_x;
    int dirty_min_y;
    int dirty_max_x;
    int dirty_max_y;
    int has_dirty;

    /* Viewport Scissor Clipping */
    int clip_enabled;
    int clip_x1;
    int clip_y1;
    int clip_x2;
    int clip_y2;
};

extern struct fb_context g_fb;

int  fb_init(void);
void fb_cleanup(void);
void fb_blank(int blank);
void fb_present(void);
void fb_pan(void);
void fb_set_clip(int x, int y, int w, int h);
void fb_clear_clip(void);
void fb_mark_dirty(int x, int y, int w, int h);
void fb_write_pixel(int x, int y, unsigned int rgb);
void fb_blend_pixel(int x, int y, unsigned int rgb, unsigned char alpha);
void fb_fill_rect(int x, int y, int w, int h, unsigned int rgb);
void fb_draw_card(int x, int y, int w, int h, unsigned int bg, unsigned int border);

#endif
