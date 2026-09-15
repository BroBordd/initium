#ifndef CONFIG_H
#define CONFIG_H

#define BUILD_VERSION           "17.0-AOSP"
#define LOGPFX                  "[INIT_v" BUILD_VERSION "] "

/* Display Geometry */
#define FB_DEV_NODE             "/dev/graphics/fb0"
#define FB_SYSFS_DEV_FILE       "/sys/class/graphics/fb0/dev"
#define BLANK_PATH              "/sys/class/graphics/fb0/blank"

#define NOTCH_OFFSET_Y          140
#define UI_PADDING_X            36

#define STATUS_BAR_HEIGHT       54
#define NAV_BAR_HEIGHT          96

/* Touch Slop: pixels of movement before converting a tap into a scroll */
#define TOUCH_SLOP              24

/* Target Refresh Rate: 60 FPS */
#define FRAME_TIME_NS           16666666L

/* ARM Mali GPU Kernel Node */
#define MALI_DEV_NODE           "/dev/mali0"
#define MALI_SYSFS_DEV          "/sys/class/misc/mali0/dev"

/* Typography Sizes */
#define FONT_PATH_PRIMARY       "/system/fonts/font.ttf"
#define FONT_PATH_ROBOTO        "/system/fonts/Roboto-Regular.ttf"
#define FONT_PATH_FALLBACK      "/system/etc/font.ttf"

#define FONT_SIZE_TITLE         38.0f
#define FONT_SIZE_SUBTITLE      22.0f
#define FONT_SIZE_BODY          28.0f
#define FONT_SIZE_SMALL         20.0f
#define FONT_SIZE_STATUS        20.0f
#define FONT_SIZE_KB            28.0f
#define FONT_SIZE_APP_LABEL     22.0f

/* AOSP Homescreen Grid */
#define APPS_PER_PAGE           8
#define GRID_COLS               3
#define GRID_ICON_SIZE          84
#define BUTTON_HEIGHT           120
#define BUTTON_GAP              16

/* Vector Assets */
#define VECTOR_ICONS_DIR        "/system/res/icons"

/* Hardware Watchdog & Power */
#define EVENT_PATH_GPIO_KEYS    "/dev/input/event0"
#define EVENT_PATH_POWER_KEY    "/dev/input/event1"
#define EVENT_PATH_TOUCHSCREEN  "/dev/input/event2"

#define WATCHDOG_DEV_NODE       "/dev/watchdog"
#define WATCHDOG_SYS_DEV        "/sys/class/watchdog/watchdog0/dev"
#define WAKELOCK_SYSFS          "/sys/power/wake_lock"
#define AUTOSLEEP_SYSFS         "/sys/power/autosleep"
#define WAKELOCK_TAG            "recovery_aosp_keepalive"

#define VIBRATOR_ENABLE_PATH    "/sys/class/timed_output/vibrator/enable"
#define VIBRATOR_PULSE_LEN      "100\n"

/* Colors */
#define COLOR_CANVAS            0x090D16
#define COLOR_STATUSBAR_BG      0x0E1422
#define COLOR_TOPBAR_BG         0x131B2E
#define COLOR_TOPBAR_ACCENT     0x2563EB
#define COLOR_TITLE_TXT         0xFFFFFF
#define COLOR_SUBTITLE_TXT      0x94A3B8

#define COLOR_CARD_BG           0x172033
#define COLOR_CARD_BORDER       0x2B3954
#define COLOR_CARD_TXT          0xF8FAFC

#define COLOR_CARD_SEL_BG       0x1D4ED8
#define COLOR_CARD_SEL_BORDER   0x60A5FA
#define COLOR_CARD_SEL_TXT      0xFFFFFF

#define COLOR_CARD_EXIT_BG      0x381820
#define COLOR_CARD_EXIT_BORDER  0xEF4444

#define COLOR_NAVBAR_BG         0x0E1422
#define COLOR_FOOTER_BG         0x0E1422
#define COLOR_NAVBAR_BORDER     0x1E293B
#define COLOR_NAVBAR_ICON       0x60A5FA
#define COLOR_NAVBAR_ICON_DIM   0x475569

#define COLOR_ACCENT_BLUE       0x3B82F6
#define COLOR_CHECK_ON          0x38BDF8
#define COLOR_CHECK_OFF         0x475569
#define COLOR_STATUS_OK         0x34D399
#define COLOR_HEARTBEAT_OK      COLOR_STATUS_OK
#define COLOR_HINT_TXT          0x64748B

/* Virtual Keyboard */
#define COLOR_KB_BG             0x0A0F1D
#define COLOR_KB_KEY_BG         0x1E293B
#define COLOR_KB_KEY_BORDER     0x334155
#define COLOR_KB_KEY_TXT        0xF8FAFC
#define COLOR_KB_SPEC_BG        0x2563EB

#endif
