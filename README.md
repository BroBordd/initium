# Bare-Metal Android Init & Recovery System

A standalone, statically linked PID 1 replacement for `aarch64` Android devices (specifically tuned for the Samsung Galaxy A14). It boots directly from the Linux kernel before Android userspace initialization, managing hardware watchdogs, power states, display pipelines, and storage mounts.

---

## 1. Project Directory Structure

```text
.
├── Makefile             # Native aarch64 build orchestrator (Zig/Musl)
├── .gitignore           # Ignores build artifacts and migrate.sh
├── README.md            # Technical deployment documentation
└── src/
    ├── config.h         # System paths, color codes, UI dimensions
    ├── stb_truetype.h   # Vector TTF/OTF font rasterizer
    ├── vector.h / .c    # Vector icon rendering engine & SVG loader
    ├── log.h / .c       # /dev/kmsg diagnostic logging
    ├── hardware.h / .c  # Watchdogs, wakelocks, sysfs vibrator, mknod
    ├── framebuffer.h / .c # Double-buffered display driver (Zero-blink)
    ├── font.h / .c      # Dynamic TrueType/OpenType font loader
    ├── keyboard.h / .c  # On-screen touch QWERTY keyboard
    ├── mount.h / .c     # Dynamic block partition scanner & mounter
    ├── filemanager.h / .c # Kinetic-scrolling file browser & binary picker
    ├── terminal.h / .c  # Environment-aware shell & built-in CLI
    ├── recents.h / .c   # /proc process manager & task viewer
    ├── sysmon.h / .c    # Thermal, battery, and sensor monitor
    ├── ui.h / .c        # Window manager, Status Bar, Bottom Navigation
    ├── input.h / .c     # 60 FPS non-blocking event loop & touch gestures
    └── main.c          # Entry point & supervisor orchestration
