#include <math.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include "bench3d.h"
#include "framebuffer.h"
#include "font.h"
#include "gpu.h"
#include "config.h"

typedef struct { float x, y, z; } vec3_t;
typedef struct { int a, b, c; unsigned int color; } triangle_t;
typedef struct {
    int a, b, c;
    unsigned int color;
    float z;
    int normal;
} sort_tri_t;

/* 3D Cube Model Vertices */
static const vec3_t CUBE_VERTS[8] = {
    {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
    { 1.0f,  1.0f, -1.0f}, {-1.0f,  1.0f, -1.0f},
    {-1.0f, -1.0f,  1.0f}, { 1.0f, -1.0f,  1.0f},
    { 1.0f,  1.0f,  1.0f}, {-1.0f,  1.0f,  1.0f}
};

/* 12 Triangles composing the 6 cube faces */
static const triangle_t CUBE_TRIS[12] = {
    {0, 2, 1, 0x3B82F6}, {0, 3, 2, 0x3B82F6}, /* Front (Blue) */
    {1, 6, 5, 0x10B981}, {1, 2, 6, 0x10B981}, /* Right (Emerald) */
    {5, 7, 4, 0xEF4444}, {5, 6, 7, 0xEF4444}, /* Back (Red) */
    {4, 3, 0, 0xF59E0B}, {4, 7, 3, 0xF59E0B}, /* Left (Amber) */
    {3, 6, 2, 0x8B5CF6}, {3, 7, 6, 0x8B5CF6}, /* Top (Purple) */
    {4, 1, 5, 0x06B6D4}, {4, 0, 1, 0x06B6D4}  /* Bottom (Cyan) */
};

static float g_rot_x = 0.4f;
static float g_rot_y = 0.6f;
static float g_rot_speed_x = 0.015f;
static float g_rot_speed_y = 0.025f;

static unsigned long g_frame_count = 0;
static float g_fps = 60.0f;
static struct timespec g_last_time;

void bench3d_init(void)
{
    clock_gettime(CLOCK_MONOTONIC, &g_last_time);
    g_frame_count = 0;
}

void bench3d_rotate_drag(float dx, float dy)
{
    g_rot_y += dx * 0.008f;
    g_rot_x += dy * 0.008f;
}

static void draw_triangle_2d(int x0, int y0, int x1, int y1, int x2, int y2, unsigned int color)
{
    int min_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2);
    int max_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    int min_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    int max_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);

    if (min_x < 0) min_x = 0;
    if (min_y < 0) min_y = 0;
    if (max_x >= g_fb.xres) max_x = g_fb.xres - 1;
    if (max_y >= g_fb.yres) max_y = g_fb.yres - 1;
    if (min_x > max_x || min_y > max_y) return;

    int dy12 = y1 - y2;
    int dx21 = x2 - x1;
    int dy20 = y2 - y0;
    int dx02 = x0 - x2;

    int denom = dy12 * dx02 + dx21 * dy20;
    if (denom == 0) return;

    int w0_row = dy12 * (min_x - x2) + dx21 * (min_y - y2);
    int w1_row = dy20 * (min_x - x2) + dx02 * (min_y - y2);

    for (int y = min_y; y <= max_y; y++) {
        int w0 = w0_row;
        int w1 = w1_row;
        for (int x = min_x; x <= max_x; x++) {
            int w2 = denom - w0 - w1;
            if ((denom > 0 && w0 >= 0 && w1 >= 0 && w2 >= 0) ||
                (denom < 0 && w0 <= 0 && w1 <= 0 && w2 <= 0)) {
                fb_write_pixel(x, y, color);
            }
            w0 += dy12;
            w1 += dy20;
        }
        w0_row += dx21;
        w1_row += dx02;
    }
}

static int compare_tri_z(const void *a, const void *b)
{
    const sort_tri_t *ta = (const sort_tri_t *)a;
    const sort_tri_t *tb = (const sort_tri_t *)b;
    if (ta->z < tb->z) return 1;
    if (ta->z > tb->z) return -1;
    return 0;
}

void bench3d_render(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    g_frame_count++;

    double elapsed = (now.tv_sec - g_last_time.tv_sec) +
                     (now.tv_nsec - g_last_time.tv_nsec) / 1000000000.0;
    if (elapsed >= 0.5) {
        g_fps = (float)(g_frame_count / elapsed);
        g_frame_count = 0;
        g_last_time = now;
    }

    g_rot_x += g_rot_speed_x;
    g_rot_y += g_rot_speed_y;

    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "3D HARDWARE BENCHMARK", FONT_SIZE_TITLE, COLOR_TITLE_TXT);

    char perf_str[128];
    snprintf(perf_str, sizeof(perf_str), "Software Engine // %.1f FPS (%.1f ms)",
             g_fps, g_fps > 0 ? 1000.0f / g_fps : 0.0f);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64,
                   perf_str, FONT_SIZE_SUBTITLE, COLOR_STATUS_OK);

    int center_x = g_fb.xres / 2;
    int center_y = topbar_h + (g_fb.yres - topbar_h - NAV_BAR_HEIGHT) / 2;
    float scale = 380.0f;
    float distance = 3.5f;

    float cx = cosf(g_rot_x), sx = sinf(g_rot_x);
    float cy = cosf(g_rot_y), sy = sinf(g_rot_y);

    vec3_t rot_verts[8];
    int proj_x[8], proj_y[8];

    for (int i = 0; i < 8; i++) {
        vec3_t v = CUBE_VERTS[i];
        float x1 = v.x * cy + v.z * sy;
        float z1 = -v.x * sy + v.z * cy;
        float y2 = v.y * cx - z1 * sx;
        float z2 = v.y * sx + z1 * cx;

        rot_verts[i] = (vec3_t){x1, y2, z2 + distance};
        proj_x[i] = center_x + (int)((x1 / (z2 + distance)) * scale);
        proj_y[i] = center_y + (int)((y2 / (z2 + distance)) * scale);
    }

    fb_set_clip(0, topbar_h, g_fb.xres, g_fb.yres - topbar_h - NAV_BAR_HEIGHT);

    sort_tri_t sorted[12];
    int valid_tris = 0;

    for (int i = 0; i < 12; i++) {
        triangle_t t = CUBE_TRIS[i];
        int x0 = proj_x[t.a], y0 = proj_y[t.a];
        int x1 = proj_x[t.b], y1 = proj_y[t.b];
        int x2 = proj_x[t.c], y2 = proj_y[t.c];

        int normal = (x1 - x0) * (y2 - y0) - (y1 - y0) * (x2 - x0);
        if (normal > 0) {
            sorted[valid_tris].a = t.a;
            sorted[valid_tris].b = t.b;
            sorted[valid_tris].c = t.c;
            sorted[valid_tris].color = t.color;
            sorted[valid_tris].normal = normal;
            sorted[valid_tris].z = (rot_verts[t.a].z + rot_verts[t.b].z + rot_verts[t.c].z) / 3.0f;
            valid_tris++;
        }
    }

    qsort(sorted, valid_tris, sizeof(sort_tri_t), compare_tri_z);

    for (int i = 0; i < valid_tris; i++) {
        sort_tri_t t = sorted[i];
        float normal_scale = (float)t.normal / 18000.0f;
        if (normal_scale > 1.0f) normal_scale = 1.0f;
        if (normal_scale < 0.35f) normal_scale = 0.35f;

        unsigned char r = (unsigned char)(((t.color >> 16) & 0xFF) * normal_scale);
        unsigned char g = (unsigned char)(((t.color >> 8)  & 0xFF) * normal_scale);
        unsigned char b = (unsigned char)((t.color & 0xFF)        * normal_scale);
        unsigned int shaded_color = (r << 16) | (g << 8) | b;

        draw_triangle_2d(proj_x[t.a], proj_y[t.a],
                         proj_x[t.b], proj_y[t.b],
                         proj_x[t.c], proj_y[t.c], shaded_color);
    }

    fb_clear_clip();

    char info_buf[96];
    snprintf(info_buf, sizeof(info_buf), "Triangles: 12 | Shading: Diffuse Flat | Drag to Rotate");
    font_draw_text(UI_PADDING_X, g_fb.yres - NAV_BAR_HEIGHT - 40, info_buf, FONT_SIZE_SMALL, COLOR_SUBTITLE_TXT);
}

void bench3d_handle_touch(int x, int y, int is_down)
{
    (void)x; (void)y; (void)is_down;
}
