/* src/keyboard.c */
#include <string.h>
#include <stdio.h>
#include "keyboard.h"
#include "framebuffer.h"
#include "font.h"
#include "hardware.h"
#include "config.h"

#define MAX_BUFFER 256

static int g_kb_visible = 0;
static int g_shift = 0;
static char g_buffer[MAX_BUFFER];
static kb_callback_t g_on_submit = NULL;

static const char *ROW0[] = {"1","2","3","4","5","6","7","8","9","0","-","="};
static const char *ROW1[] = {"q","w","e","r","t","y","u","i","o","p","/","."};
static const char *ROW2[] = {"a","s","d","f","g","h","j","k","l","_","@"};
static const char *ROW3[] = {"SHIFT","z","x","c","v","b","n","m","BKSP"};

void kb_init(void)
{
    g_kb_visible = 0;
    g_shift = 0;
    memset(g_buffer, 0, sizeof(g_buffer));
    g_on_submit = NULL;
}

void kb_show(const char *initial_text, kb_callback_t on_submit)
{
    g_kb_visible = 1;
    g_shift = 0;
    memset(g_buffer, 0, sizeof(g_buffer));
    if (initial_text)
        snprintf(g_buffer, sizeof(g_buffer), "%s", initial_text);
    g_on_submit = on_submit;
}

void kb_hide(void)
{
    g_kb_visible = 0;
}

int kb_is_visible(void)
{
    return g_kb_visible;
}

int kb_get_height(void)
{
    return 480;
}

const char *kb_get_buffer(void)
{
    return g_buffer;
}

void kb_render(void)
{
    if (!g_kb_visible) return;

    int kb_h = kb_get_height();
    int start_y = g_fb.yres - kb_h;

    fb_fill_rect(0, start_y, g_fb.xres, kb_h, COLOR_KB_BG);
    fb_fill_rect(0, start_y, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    fb_fill_rect(20, start_y + 12, g_fb.xres - 40, 56, COLOR_TOPBAR_BG);
    fb_draw_card(20, start_y + 12, g_fb.xres - 40, 56, COLOR_TOPBAR_BG, COLOR_TOPBAR_ACCENT);

    char preview[300];
    snprintf(preview, sizeof(preview), "> %s_", g_buffer);
    font_draw_text(36, start_y + 24, preview, FONT_SIZE_BODY, COLOR_TITLE_TXT);

    int key_area_y = start_y + 80;
    int row_h = 72;
    int gap = 8;

    /* Row 0 */
    int cols = 12;
    int kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
    for (int i = 0; i < cols; i++) {
        int kx = 20 + i * (kw + gap);
        fb_draw_card(kx, key_area_y, kw, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
        font_draw_text(kx + (kw / 2) - 8, key_area_y + 20, ROW0[i], FONT_SIZE_KB, COLOR_KB_KEY_TXT);
    }

    /* Row 1 */
    key_area_y += row_h + gap;
    cols = 12;
    kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
    for (int i = 0; i < cols; i++) {
        int kx = 20 + i * (kw + gap);
        char letter[2] = { ROW1[i][0], '\0' };
        if (g_shift && letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 32;

        fb_draw_card(kx, key_area_y, kw, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
        font_draw_text(kx + (kw / 2) - 8, key_area_y + 20, letter, FONT_SIZE_KB, COLOR_KB_KEY_TXT);
    }

    /* Row 2 */
    key_area_y += row_h + gap;
    cols = 11;
    kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
    for (int i = 0; i < cols; i++) {
        int kx = 20 + i * (kw + gap);
        char letter[2] = { ROW2[i][0], '\0' };
        if (g_shift && letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 32;

        fb_draw_card(kx, key_area_y, kw, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
        font_draw_text(kx + (kw / 2) - 8, key_area_y + 20, letter, FONT_SIZE_KB, COLOR_KB_KEY_TXT);
    }

    /* Row 3 */
    key_area_y += row_h + gap;
    int shift_w = 120;
    int bksp_w = 140;
    int rem_w = g_fb.xres - 40 - shift_w - bksp_w - (8 * gap);
    int norm_w = rem_w / 7;

    int cur_x = 20;
    fb_draw_card(cur_x, key_area_y, shift_w, row_h,
                 g_shift ? COLOR_KB_SPEC_BG : COLOR_KB_KEY_BG,
                 g_shift ? COLOR_CARD_SEL_BORDER : COLOR_KB_KEY_BORDER);
    font_draw_text(cur_x + 18, key_area_y + 20, "SHIFT", FONT_SIZE_SMALL, COLOR_TITLE_TXT);
    cur_x += shift_w + gap;

    for (int i = 1; i <= 7; i++) {
        char letter[2] = { ROW3[i][0], '\0' };
        if (g_shift && letter[0] >= 'a' && letter[0] <= 'z') letter[0] -= 32;

        fb_draw_card(cur_x, key_area_y, norm_w, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
        font_draw_text(cur_x + (norm_w / 2) - 8, key_area_y + 20, letter, FONT_SIZE_KB, COLOR_KB_KEY_TXT);
        cur_x += norm_w + gap;
    }

    fb_draw_card(cur_x, key_area_y, bksp_w, row_h, COLOR_CARD_EXIT_BG, COLOR_CARD_EXIT_BORDER);
    font_draw_text(cur_x + 24, key_area_y + 20, "BKSP", FONT_SIZE_SMALL, COLOR_TITLE_TXT);

    /* Row 4 */
    key_area_y += row_h + gap;
    int hide_w = 140;
    int enter_w = 180;
    int space_w = g_fb.xres - 40 - hide_w - enter_w - (2 * gap);

    cur_x = 20;
    fb_draw_card(cur_x, key_area_y, hide_w, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
    font_draw_text(cur_x + 28, key_area_y + 20, "HIDE", FONT_SIZE_SMALL, COLOR_TITLE_TXT);
    cur_x += hide_w + gap;

    fb_draw_card(cur_x, key_area_y, space_w, row_h, COLOR_KB_KEY_BG, COLOR_KB_KEY_BORDER);
    font_draw_text(cur_x + (space_w / 2) - 34, key_area_y + 20, "SPACE", FONT_SIZE_SMALL, COLOR_SUBTITLE_TXT);
    cur_x += space_w + gap;

    fb_draw_card(cur_x, key_area_y, enter_w, row_h, COLOR_KB_SPEC_BG, COLOR_CARD_SEL_BORDER);
    font_draw_text(cur_x + 36, key_area_y + 20, "ENTER", FONT_SIZE_SMALL, COLOR_TITLE_TXT);
}

int kb_handle_touch(int x, int y, int is_down)
{
    if (!g_kb_visible || !is_down) return 0;

    int kb_h = kb_get_height();
    int start_y = g_fb.yres - kb_h;
    if (y < start_y) return 0;

    trigger_vibration();

    int key_area_y = start_y + 80;
    int row_h = 72;
    int gap = 8;

    /* Row 0 */
    if (y >= key_area_y && y < key_area_y + row_h) {
        int cols = 12;
        int kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
        int idx = (x - 20) / (kw + gap);
        if (idx >= 0 && idx < cols) {
            size_t len = strlen(g_buffer);
            if (len + 1 < sizeof(g_buffer)) {
                g_buffer[len] = ROW0[idx][0];
                g_buffer[len + 1] = '\0';
            }
        }
        return 1;
    }

    /* Row 1 */
    key_area_y += row_h + gap;
    if (y >= key_area_y && y < key_area_y + row_h) {
        int cols = 12;
        int kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
        int idx = (x - 20) / (kw + gap);
        if (idx >= 0 && idx < cols) {
            char c = ROW1[idx][0];
            if (g_shift && c >= 'a' && c <= 'z') c -= 32;
            size_t len = strlen(g_buffer);
            if (len + 1 < sizeof(g_buffer)) {
                g_buffer[len] = c;
                g_buffer[len + 1] = '\0';
            }
        }
        return 1;
    }

    /* Row 2 */
    key_area_y += row_h + gap;
    if (y >= key_area_y && y < key_area_y + row_h) {
        int cols = 11;
        int kw = (g_fb.xres - 40 - (cols - 1) * gap) / cols;
        int idx = (x - 20) / (kw + gap);
        if (idx >= 0 && idx < cols) {
            char c = ROW2[idx][0];
            if (g_shift && c >= 'a' && c <= 'z') c -= 32;
            size_t len = strlen(g_buffer);
            if (len + 1 < sizeof(g_buffer)) {
                g_buffer[len] = c;
                g_buffer[len + 1] = '\0';
            }
        }
        return 1;
    }

    /* Row 3 */
    key_area_y += row_h + gap;
    if (y >= key_area_y && y < key_area_y + row_h) {
        int shift_w = 120;
        int bksp_w = 140;
        int rem_w = g_fb.xres - 40 - shift_w - bksp_w - (8 * gap);
        int norm_w = rem_w / 7;

        if (x >= 20 && x < 20 + shift_w) {
            g_shift = !g_shift;
            return 1;
        }

        int cur_x = 20 + shift_w + gap;
        for (int i = 1; i <= 7; i++) {
            if (x >= cur_x && x < cur_x + norm_w) {
                char c = ROW3[i][0];
                if (g_shift && c >= 'a' && c <= 'z') c -= 32;
                size_t len = strlen(g_buffer);
                if (len + 1 < sizeof(g_buffer)) {
                    g_buffer[len] = c;
                    g_buffer[len + 1] = '\0';
                }
                return 1;
            }
            cur_x += norm_w + gap;
        }

        if (x >= cur_x && x < cur_x + bksp_w) {
            size_t len = strlen(g_buffer);
            if (len > 0) g_buffer[len - 1] = '\0';
            return 1;
        }
        return 1;
    }

    /* Row 4 */
    key_area_y += row_h + gap;
    if (y >= key_area_y && y < key_area_y + row_h) {
        int hide_w = 140;
        int enter_w = 180;
        int space_w = g_fb.xres - 40 - hide_w - enter_w - (2 * gap);

        if (x >= 20 && x < 20 + hide_w) {
            kb_hide();
            return 1;
        }

        int space_x = 20 + hide_w + gap;
        if (x >= space_x && x < space_x + space_w) {
            size_t len = strlen(g_buffer);
            if (len + 1 < sizeof(g_buffer)) {
                g_buffer[len] = ' ';
                g_buffer[len + 1] = '\0';
            }
            return 1;
        }

        int enter_x = space_x + space_w + gap;
        if (x >= enter_x && x < enter_x + enter_w) {
            if (g_on_submit)
                g_on_submit(g_buffer);
            kb_hide();
            return 1;
        }
        return 1;
    }

    return 1;
}
