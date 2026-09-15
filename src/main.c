#include <unistd.h>
#include "config.h"
#include "log.h"
#include "hardware.h"
#include "gpu.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "ui.h"
#include "input.h"

int main(void)
{
    log_init();
    dbgf(LOGPFX "init v" BUILD_VERSION " starting\n");

    /* Prevent kernel suspend & initialize hardware watchdog */
    init_power_and_signals();
    init_watchdog();
    kick_watchdog();

    /* Create framebuffer and input nodes */
    if (!mknod_fb0())
        die("mknod_fb0 failed");
    setup_input_devnodes();

    /* Initialize Mali GPU kernel interface */
    gpu_init();

    /* Map display framebuffer with double buffering and NEON acceleration */
    if (!fb_init())
        die("fb_init failed");

    /* Initialize vector font and vector icon engines */
    if (!font_init())
        die("font_init failed: no usable TTF/OTF found");
    vector_init();

    /* Initialize navigation, status bar, and apps */
    ui_init();

    /* Run 60 FPS supervisor event loop */
    input_loop();

    /* Clean exit triggering PID 1 panic */
    gpu_cleanup();
    fb_cleanup();

    die("User selected exit");
    return 0;
}
