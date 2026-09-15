/* src/input.c */
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <linux/input.h>
#include "input.h"
#include "ui.h"
#include "hardware.h"
#include "gpu.h"
#include "config.h"
#include "log.h"

void input_loop(void)
{
    int fd_gpio  = open(EVENT_PATH_GPIO_KEYS, O_RDONLY | O_NONBLOCK);
    int fd_power = open(EVENT_PATH_POWER_KEY, O_RDONLY | O_NONBLOCK);
    int fd_touch = open(EVENT_PATH_TOUCHSCREEN, O_RDONLY | O_NONBLOCK);

    dbgf(LOGPFX "input loop active (60 FPS + Touch Slop)\n");

    struct pollfd pfds[3];
    int num_pfds = 0;
    int idx_gpio = -1, idx_power = -1, idx_touch = -1;

    if (fd_gpio >= 0) {
        idx_gpio = num_pfds;
        pfds[num_pfds].fd = fd_gpio;
        pfds[num_pfds].events = POLLIN;
        num_pfds++;
    }
    if (fd_power >= 0) {
        idx_power = num_pfds;
        pfds[num_pfds].fd = fd_power;
        pfds[num_pfds].events = POLLIN;
        num_pfds++;
    }
    if (fd_touch >= 0) {
        idx_touch = num_pfds;
        pfds[num_pfds].fd = fd_touch;
        pfds[num_pfds].events = POLLIN;
        num_pfds++;
    }

    /* Touch Slop & Gesture Tracking */
    int touch_x = 0, touch_y = 0;
    int start_touch_x = 0, start_touch_y = 0;
    int prev_touch_x = 0, prev_touch_y = 0;
    int touch_down = 0;
    int slop_exceeded = 0;
    float velocity_y = 0.0f;

    struct timespec last_wd_kick;
    clock_gettime(CLOCK_MONOTONIC, &last_wd_kick);

    while (!ui_is_exit_requested()) {
        /* Poll at 16ms cadence for 60Hz loop */
        poll(pfds, (nfds_t)num_pfds, 16);

        struct input_event ev;

        /* Hardware Keys */
        if (idx_gpio >= 0 && (pfds[idx_gpio].revents & POLLIN)) {
            while (read(fd_gpio, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY && ev.value == 1) {
                    if (ev.code == KEY_VOLUMEUP)        ui_on_volume_up();
                    else if (ev.code == KEY_VOLUMEDOWN) ui_on_volume_down();
                }
            }
        }

        if (idx_power >= 0 && (pfds[idx_power].revents & POLLIN)) {
            while (read(fd_power, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_KEY && ev.code == KEY_POWER && ev.value == 1)
                    ui_on_power_key();
            }
        }

        /* Touchscreen Multi-Touch Protocol Type B */
        if (idx_touch >= 0 && (pfds[idx_touch].revents & POLLIN)) {
            while (read(fd_touch, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_MT_POSITION_X)      touch_x = ev.value;
                    else if (ev.code == ABS_MT_POSITION_Y) touch_y = ev.value;
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    if (ev.value == 1) {
                        /* Finger Down: Do NOT trigger action yet */
                        touch_down = 1;
                        start_touch_x = touch_x;
                        start_touch_y = touch_y;
                        prev_touch_x = touch_x;
                        prev_touch_y = touch_y;
                        slop_exceeded = 0;
                        velocity_y = 0.0f;
                    } else {
                        /* Finger Up: Disambiguate Tap vs Drag */
                        touch_down = 0;
                        if (!slop_exceeded) {
                            /* User did not scroll; trigger clean click */
                            ui_handle_tap(start_touch_x, start_touch_y);
                        } else {
                            /* Finish page slide snapping */
                            ui_on_page_swipe_end();
                        }
                    }
                }
            }
        }

        /* Continuous Drag & Kinetic Physics */
        if (touch_down) {
            int dx_slop = abs(touch_x - start_touch_x);
            int dy_slop = abs(touch_y - start_touch_y);

            if (!slop_exceeded && (dx_slop > TOUCH_SLOP || dy_slop > TOUCH_SLOP)) {
                slop_exceeded = 1;
            }

            if (slop_exceeded) {
                float dx = (float)(prev_touch_x - touch_x);
                float dy = (float)(prev_touch_y - touch_y);

                /* Horizontal page drag for homescreen / 3D rotation */
                if (dx != 0.0f) {
                    ui_handle_drag_x(dx);
                    prev_touch_x = touch_x;
                }

                /* Vertical drag for lists and files */
                if (dy != 0.0f) {
                    velocity_y = dy;
                    ui_handle_scroll_y(dy);
                    prev_touch_y = touch_y;
                }
            }
        } else if (velocity_y != 0.0f) {
            /* Kinetic deceleration friction */
            ui_handle_scroll_y(velocity_y);
            velocity_y *= 0.88f;
            if (velocity_y > -0.5f && velocity_y < 0.5f)
                velocity_y = 0.0f;
        }

        /* 60 FPS Render Tick */
        ui_render();

        /* Watchdog & GPU keepalive */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - last_wd_kick.tv_sec >= 1) {
            kick_watchdog();
            gpu_kick_keepalive();
            last_wd_kick = now;
        }
    }

    if (fd_gpio >= 0)  close(fd_gpio);
    if (fd_power >= 0) close(fd_power);
    if (fd_touch >= 0) close(fd_touch);
}
