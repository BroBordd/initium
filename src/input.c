#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include "input.h"
#include "ui.h"
#include "hardware.h"
#include "mount.h"
#include "gpu.h"
#include "config.h"
#include "log.h"

static volatile int g_flush_requested = 0;

void input_request_flush(void) { g_flush_requested = 1; }

static void drain_fd(int fd)
{
    struct input_event ev;
    if (fd < 0) return;
    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) { }
}

static int try_open_touchscreen(void)
{
    /* 1. Try defined touchscreen path */
    int fd = open(EVENT_PATH_TOUCHSCREEN, O_RDONLY | O_NONBLOCK);
    if (fd >= 0) return fd;

    /* 2. Auto-detect any input device exposing ABS_MT_POSITION_X or ABS_X */
    for (int i = 0; i <= 7; i++) {
        char devpath[64];
        snprintf(devpath, sizeof(devpath), "/dev/input/event%d", i);
        int test_fd = open(devpath, O_RDONLY | O_NONBLOCK);
        if (test_fd >= 0) {
            unsigned char abs_bits[(ABS_MAX + 7) / 8] = {0};
            if (ioctl(test_fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) >= 0) {
                if ((abs_bits[ABS_MT_POSITION_X / 8] & (1 << (ABS_MT_POSITION_X % 8))) ||
                    (abs_bits[ABS_X / 8] & (1 << (ABS_X % 8)))) {
                    dbgf(LOGPFX "auto-detected touchscreen on %s\n", devpath);
                    return test_fd;
                }
            }
            close(test_fd);
        }
    }
    return -1;
}

void input_loop(void)
{
    int fd_gpio  = -1;
    int fd_power = -1;
    int fd_touch = -1;

    struct pollfd pfds[3];
    int num_pfds = 0;
    int idx_gpio = -1, idx_power = -1, idx_touch = -1;

    /* Touch Slop & Gesture Tracking */
    int touch_x = 0, touch_y = 0;
    int start_touch_x = 0, start_touch_y = 0;
    int prev_touch_x = 0, prev_touch_y = 0;
    int touch_down = 0;
    int slop_exceeded = 0;
    float velocity_y = 0.0f;

    struct timespec last_wd_kick;
    clock_gettime(CLOCK_MONOTONIC, &last_wd_kick);

    struct timespec last_scan;
    last_scan = last_wd_kick;

    struct timespec next_frame = last_wd_kick;

    /* Immediate open on startup without waiting 1s */
    handle_firmware_requests();
    setup_input_devnodes();
    fd_gpio = open(EVENT_PATH_GPIO_KEYS, O_RDONLY | O_NONBLOCK);
    fd_power = open(EVENT_PATH_POWER_KEY, O_RDONLY | O_NONBLOCK);
    fd_touch = try_open_touchscreen();

    dbgf(LOGPFX "input loop active (60 FPS + Fast Probing)\n");

    unsigned long g_loop_input_ns_accum = 0;
    unsigned long g_loop_render_ns_accum = 0;
    unsigned long g_loop_samples = 0;
    struct timespec ta, tb, tc;

    while (!ui_is_exit_requested()) {
        struct timespec t_start;
        clock_gettime(CLOCK_MONOTONIC, &t_start);
        ta = t_start;

        /* Re-probe or service pending firmware every second if any descriptor is missing */
        if (t_start.tv_sec - last_scan.tv_sec >= 1) {
            if (fd_touch < 0) {
                handle_firmware_requests();
                setup_input_devnodes();
                fd_touch = try_open_touchscreen();
            }
            if (fd_gpio < 0)  fd_gpio = open(EVENT_PATH_GPIO_KEYS, O_RDONLY | O_NONBLOCK);
            if (fd_power < 0) fd_power = open(EVENT_PATH_POWER_KEY, O_RDONLY | O_NONBLOCK);

            last_scan = t_start;
        }

        if (g_flush_requested) {
            drain_fd(fd_gpio);
            drain_fd(fd_power);
            drain_fd(fd_touch);
            touch_down = 0;
            slop_exceeded = 0;
            velocity_y = 0.0f;
            g_flush_requested = 0;
        }

        num_pfds = 0;
        idx_gpio = idx_power = idx_touch = -1;
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

        long remain_ns = (next_frame.tv_sec - t_start.tv_sec) * 1000000000L +
                          (next_frame.tv_nsec - t_start.tv_nsec);
        int timeout_ms = (int)(remain_ns / 1000000L);
        if (timeout_ms < 0) timeout_ms = 0;
        if (timeout_ms > (int)(FRAME_TIME_NS / 1000000L))
            timeout_ms = (int)(FRAME_TIME_NS / 1000000L);

        poll(pfds, (nfds_t)num_pfds, timeout_ms);

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
                        start_touch_x = touch_x;
                        start_touch_y = touch_y;
                        prev_touch_x = touch_x;
                        prev_touch_y = touch_y;
                        slop_exceeded = 0;
                        velocity_y = 0.0f;
                    } else {
                        touch_down = 0;
                        if (!slop_exceeded) {
                            ui_handle_tap(start_touch_x, start_touch_y);
                        } else {
                            ui_on_page_swipe_end();
                        }
                    }
                }
            }
        }

        if (touch_down) {
            int dx_slop = abs(touch_x - start_touch_x);
            int dy_slop = abs(touch_y - start_touch_y);

            if (!slop_exceeded && (dx_slop > TOUCH_SLOP || dy_slop > TOUCH_SLOP)) {
                slop_exceeded = 1;
            }

            if (slop_exceeded) {
                float dx = (float)(prev_touch_x - touch_x);
                float dy = (float)(prev_touch_y - touch_y);

                if (dx != 0.0f) {
                    ui_handle_drag_x(dx);
                    prev_touch_x = touch_x;
                }
                if (dy != 0.0f) {
                    velocity_y = dy;
                    ui_handle_scroll_y(dy);
                    prev_touch_y = touch_y;
                }
            }
        } else if (velocity_y != 0.0f) {
            ui_handle_scroll_y(velocity_y);
            velocity_y *= 0.88f;
            if (velocity_y > -0.5f && velocity_y < 0.5f)
                velocity_y = 0.0f;
        }

        clock_gettime(CLOCK_MONOTONIC, &tb);

        ui_render();

        clock_gettime(CLOCK_MONOTONIC, &tc);

        g_loop_input_ns_accum  += (unsigned long)((tb.tv_sec - ta.tv_sec) * 1000000000L + (tb.tv_nsec - ta.tv_nsec));
        g_loop_render_ns_accum += (unsigned long)((tc.tv_sec - tb.tv_sec) * 1000000000L + (tc.tv_nsec - tb.tv_nsec));
        g_loop_samples++;

        if (g_loop_samples >= 60) {
            dbgf(LOGPFX "perf: loop total=%luus  input+poll=%luus  ui_render(incl.present)=%luus\n",
                 (g_loop_input_ns_accum + g_loop_render_ns_accum) / g_loop_samples / 1000,
                 g_loop_input_ns_accum / g_loop_samples / 1000,
                 g_loop_render_ns_accum / g_loop_samples / 1000);
            g_loop_input_ns_accum = 0;
            g_loop_render_ns_accum = 0;
            g_loop_samples = 0;
        }

        struct timespec now;
        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - last_wd_kick.tv_sec >= 1) {
            kick_watchdog();
            gpu_kick_keepalive();
            last_wd_kick = now;
        }

        next_frame.tv_nsec += FRAME_TIME_NS;
        while (next_frame.tv_nsec >= 1000000000L) {
            next_frame.tv_nsec -= 1000000000L;
            next_frame.tv_sec++;
        }
        long behind_ns = (now.tv_sec - next_frame.tv_sec) * 1000000000L +
                          (now.tv_nsec - next_frame.tv_nsec);
        if (behind_ns > 0) next_frame = now;
    }

    if (fd_gpio >= 0)  close(fd_gpio);
    if (fd_power >= 0) close(fd_power);
    if (fd_touch >= 0) close(fd_touch);
}
