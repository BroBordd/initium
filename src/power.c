#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/reboot.h>
#include <linux/reboot.h>
#include "power.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "ui.h"
#include "config.h"
#include "log.h"

void power_init(void)
{
}

/*
 * Real PID1 reboot. As PID 1 we can't rely on the userspace reboot(3)
 * wrapper the way a normal Android app would (that goes through
 * android_reboot()/init's property trigger, which doesn't exist here
 * since we ARE init) -- we call the reboot(2) syscall directly with
 * LINUX_REBOOT_CMD_RESTART2 and a target string, which is the same
 * mechanism Android's real init uses under the hood for targeted
 * reboots (recovery, bootloader, download/odin mode).
 */
static void do_reboot(const char *mode, const char *cmd_target)
{
    ui_show_alert("Rebooting", mode);
    dbgf(LOGPFX "power: reboot requested (%s -> cmd='%s')\n", mode, cmd_target ? cmd_target : "(none)");

    sync();

    if (!cmd_target) {
        /* Plain power off */
        reboot(LINUX_REBOOT_CMD_POWER_OFF);
        return;
    }

    if (cmd_target[0] == '\0') {
        /* Plain warm reboot, no target string */
        reboot(LINUX_REBOOT_CMD_RESTART);
        return;
    }

    /* Targeted reboot (recovery / bootloader / download etc). */
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
            LINUX_REBOOT_CMD_RESTART2, cmd_target);

    /* If RESTART2 with a target isn't supported by this kernel, fall
     * back to a plain restart rather than silently doing nothing. */
    reboot(LINUX_REBOOT_CMD_RESTART);
}

void power_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "POWER MENU", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 66,
                   "Advanced Restart Options", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    int start_y = topbar_h + 40;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);

    const char *labels[] = {"Reboot System", "Recovery", "Bootloader", "Power Off"};
    unsigned int colors[] = {COLOR_TOPBAR_ACCENT, 0x10B981, 0xF59E0B, 0xEF4444};

    for (int i = 0; i < 4; i++) {
        fb_draw_card(UI_PADDING_X, start_y, card_w, BUTTON_HEIGHT, COLOR_CARD_BG, colors[i]);
        vector_draw_icon(UI_PADDING_X + 24, start_y + 36, 48, VEC_ICON_POWER, colors[i]);
        font_draw_text(UI_PADDING_X + 100, start_y + 40, labels[i], FONT_SIZE_BODY, COLOR_TITLE_TXT);
        start_y += BUTTON_HEIGHT + BUTTON_GAP;
    }
}

void power_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    int start_y = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 110 + 40;
    int card_w = g_fb.xres - (UI_PADDING_X * 2);

    for (int i = 0; i < 4; i++) {
        if (x >= UI_PADDING_X && x <= UI_PADDING_X + card_w &&
            y >= start_y && y <= start_y + BUTTON_HEIGHT) {
            trigger_vibration();
            if (i == 0) do_reboot("System", "");
            else if (i == 1) do_reboot("Recovery", "recovery");
            else if (i == 2) do_reboot("Bootloader", "bootloader");
            else if (i == 3) do_reboot("Power Off", NULL);
            break;
        }
        start_y += BUTTON_HEIGHT + BUTTON_GAP;
    }
}
