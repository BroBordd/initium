/* src/input.c */
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <errno.h>
#include <linux/input.h>
#include "input.h"
#include "ui.h"
#include "hardware.h"
#include "config.h"
#include "log.h"

void input_loop(void)
{
    int fd_gpio  = open(EVENT_PATH_GPIO_KEYS, O_RDONLY | O_NONBLOCK);
    int fd_power = open(EVENT_PATH_POWER_KEY, O_RDONLY | O_NONBLOCK);
    int fd_touch = open(EVENT_PATH_TOUCHSCREEN, O_RDONLY | O_NONBLOCK);

    dbgf(LOGPFX "input loop started (60 FPS)\n");

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

    int touch_x = 0;
    int touch_y = 0;
    int prev_touch_y = 0;
    int touch_down = 0;
    float velocity_y = 0.0f;

    struct timespec last_frame, last_wd_kick;
    clock_gettime(CLOCK_MONOTONIC, &last_frame);
    clock_gettime(CLOCK_MONOTONIC, &last_wd_kick);

    while (!ui_is_exit_requested()) {
        /* 1. Poll with 16ms timeout (~60Hz tick) */
        poll(pfds, (nfds_t)num_pfds, 16);

        struct input_event ev;

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

        if (idx_touch >= 0 && (pfds[idx_touch].revents & POLLIN)) {
            while (read(fd_touch, &ev, sizeof(ev)) == sizeof(ev)) {
                if (ev.type == EV_ABS) {
                    if (ev.code == ABS_MT_POSITION_X)      touch_x = ev.value;
                    else if (ev.code == ABS_MT_POSITION_Y) touch_y = ev.value;
                } else if (ev.type == EV_KEY && ev.code == BTN_TOUCH) {
                    if (ev.value == 1) {
                        touch_down = 1;
                        prev_touch_y = touch_y;
                        velocity_y = 0.0f;
                        ui_handle_touch(touch_x, touch_y, 1);
                    } else {
                        touch_down = 0;
                    }
                }
            }
        }

        /* 2. Kinetic Swipe & Scroll Physics */
        if (touch_down) {
            float dy = (float)(prev_touch_y - touch_y);
            if (dy != 0.0f) {
                velocity_y = dy;
                ui_handle_scroll(dy);
                prev_touch_y = touch_y;
            }
        } else if (velocity_y != 0.0f) {
            /* Inertial Deceleration */
            ui_handle_scroll(velocity_y);
            velocity_y *= 0.88f; /* Damping friction */
            if (velocity_y > -0.5f && velocity_y < 0.5f)
                velocity_y = 0.0f;
        }

        /* 3. 60 FPS Render Tick */
        ui_render();

        /* 4. Watchdog kick once every second */
        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - last_wd_kick.tv_sec >= 1) {
            kick_watchdog();
            last_wd_kick = now;
        }
    }

    if (fd_gpio >= 0)  close(fd_gpio);
    if (fd_power >= 0) close(fd_power);
    if (fd_touch >= 0) close(fd_touch);
}
