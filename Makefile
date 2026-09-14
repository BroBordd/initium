# Prevent Termux Bionic headers/libraries from leaking into the Musl toolchain
unexport CPATH
unexport C_INCLUDE_PATH
unexport CPLUS_INCLUDE_PATH
unexport LIBRARY_PATH

CC        := zig cc -target aarch64-linux-musl
CFLAGS    := -O2 -Wall -Wextra -DNDEBUG -Isrc
LDFLAGS   := -static -s
LDLIBS    := -lm

SRC_DIR   := src
BUILD_DIR := build
TARGET    := init

SRCS      := $(wildcard $(SRC_DIR)/*.c)
OBJS      := $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRCS))
DEPS      := $(OBJS:.o=.d)

.PHONY: all clean

all: $(TARGET)
	@echo "  STRIP   $(TARGET)"
	@strip --strip-all $(TARGET) 2>/dev/null || true
	@echo "=== Verifying PID 1 Binary ==="
	@file $(TARGET)
	@echo -n "PT_INTERP: "
	@if readelf -l $(TARGET) 2>/dev/null | grep -qi interp; then \
		echo "FAILED (dynamic interpreter detected!)"; exit 1; \
	else \
		echo "NONE (Fully static Musl aarch64 binary)"; \
	fi
	@echo ">>> Build successful: $(TARGET) is ready for /system/bin/init <<<"

$(TARGET): $(OBJS)
	@echo "  LD      $@"
	@$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) -MMD -MP -c $< -o $@

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

-include $(DEPS)

clean:
	@echo "  CLEAN"
	@rm -rf $(BUILD_DIR) $(TARGET)
