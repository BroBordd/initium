#include <unistd.h>
#include "config.h"
#include "log.h"
#include "hardware.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "ui.h"
#include "input.h"

int main(void)
{
    log_init();
    dbgf(LOGPFX "init v" BUILD_VERSION " starting\n");

    /* Prevent kernel sleep and initialize watchdog */
    init_power_and_signals();
    init_watchdog();
    kick_watchdog();

    /* Create missing device nodes */
    if (!mknod_fb0())
        die("mknod_fb0 failed");
    setup_input_devnodes();

    /* Map Display Framebuffer with Double Buffering */
    if (!fb_init())
        die("fb_init failed");

    /* Initialize TrueType/OpenType Vector Font Engine */
    if (!font_init())
        die("font_init failed: no usable font found");

    /* Initialize Vector Graphics Subsystem */
    vector_init();

    /* Initialize UI State Engine */
    ui_init();

    /* Start 60 FPS Supervisor Loop */
    input_loop();

    /* Clean exit to trigger PID 1 panic */
    fb_cleanup();

    die("User selected exit");
    return 0;
}
