/* src/terminal.c */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <dirent.h>
#include "terminal.h"
#include "keyboard.h"
#include "framebuffer.h"
#include "font.h"
#include "vector.h"
#include "hardware.h"
#include "config.h"

#define TERM_LINES 16
#define LINE_MAX_LEN 128

static char g_term_history[TERM_LINES][LINE_MAX_LEN];
static int g_term_count = 0;

static void terminal_append_line(const char *line)
{
    if (g_term_count < TERM_LINES) {
        snprintf(g_term_history[g_term_count++], LINE_MAX_LEN, "%s", line);
    } else {
        for (int i = 0; i < TERM_LINES - 1; i++) {
            memcpy(g_term_history[i], g_term_history[i + 1], LINE_MAX_LEN);
        }
        snprintf(g_term_history[TERM_LINES - 1], LINE_MAX_LEN, "%s", line);
    }
}

/* Fallback built-in commands if no shell exists in early boot */
static int handle_builtin(const char *cmd)
{
    if (strcmp(cmd, "clear") == 0) {
        g_term_count = 0;
        return 1;
    }
    if (strcmp(cmd, "help") == 0) {
        terminal_append_line("Built-in CLI: ls, cat, uname, dmesg, clear, reboot");
        return 1;
    }
    if (strcmp(cmd, "uname") == 0) {
        terminal_append_line("Linux aarch64 bare-metal init v15.0");
        return 1;
    }
    if (strncmp(cmd, "ls", 2) == 0) {
        const char *path = cmd[2] == ' ' ? cmd + 3 : "/";
        DIR *d = opendir(path);
        if (!d) {
            terminal_append_line("ls: cannot open directory");
            return 1;
        }
        struct dirent *de;
        char row[LINE_MAX_LEN] = "";
        while ((de = readdir(d))) {
            if (de->d_name[0] == '.') continue;
            strncat(row, de->d_name, sizeof(row) - strlen(row) - 2);
            strcat(row, "  ");
            if (strlen(row) > 60) {
                terminal_append_line(row);
                row[0] = '\0';
            }
        }
        if (strlen(row) > 0) terminal_append_line(row);
        closedir(d);
        return 1;
    }
    return 0;
}

/* Robust process spawner targeting Android's sh and toybox */
void terminal_execute_command(const char *cmd_line)
{
    if (!cmd_line || strlen(cmd_line) == 0) return;

    char echo[160];
    snprintf(echo, sizeof(echo), "# %s", cmd_line);
    terminal_append_line(echo);

    if (handle_builtin(cmd_line)) return;

    int pipefd[2];
    if (pipe(pipefd) < 0) {
        terminal_append_line("Error: pipe creation failed");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        terminal_append_line("Error: fork failed");
        close(pipefd[0]);
        close(pipefd[1]);
        return;
    }

    if (pid == 0) {
        /* Child */
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        /* Set broad PATH including Android system and vendor paths */
        setenv("PATH", "/system/bin:/system/xbin:/vendor/bin:/bin:/sbin", 1);
        setenv("HOME", "/data", 0);
        setenv("TERM", "linux", 1);

        /* Try shells in priority order */
        const char *shells[] = {
            "/system/bin/sh",
            "/bin/sh",
            "/system/bin/toybox",
            "/vendor/bin/sh"
        };

        for (size_t i = 0; i < sizeof(shells)/sizeof(shells[0]); i++) {
            execl(shells[i], "sh", "-c", cmd_line, (char *)NULL);
        }

        /* If command is an absolute path to binary */
        execl(cmd_line, cmd_line, (char *)NULL);

        _exit(127);
    }

    /* Parent */
    close(pipefd[1]);
    FILE *stream = fdopen(pipefd[0], "r");
    if (stream) {
        char buf[LINE_MAX_LEN];
        while (fgets(buf, sizeof(buf), stream)) {
            size_t l = strlen(buf);
            if (l > 0 && buf[l - 1] == '\n') buf[l - 1] = '\0';
            terminal_append_line(buf);
        }
        fclose(stream);
    } else {
        close(pipefd[0]);
    }

    int status;
    waitpid(pid, &status, 0);
}

static void on_kb_submit(const char *text)
{
    terminal_execute_command(text);
}

void terminal_init(void)
{
    g_term_count = 0;
    terminal_append_line("--- RECOVERY TERMINAL CONSOLE v15.0 ---");
    terminal_append_line("Android environment-aware process engine");
    terminal_append_line("Tap [OPEN KEYBOARD] below to write commands");
}

void terminal_render(void)
{
    fb_fill_rect(0, 0, g_fb.xres, g_fb.yres, COLOR_CANVAS);

    int topbar_h = NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 100;
    fb_fill_rect(0, 0, g_fb.xres, topbar_h, COLOR_TOPBAR_BG);
    fb_fill_rect(0, topbar_h, g_fb.xres, 4, COLOR_TOPBAR_ACCENT);

    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 14,
                   "TERMINAL CONSOLE", FONT_SIZE_TITLE, COLOR_TITLE_TXT);
    font_draw_text(UI_PADDING_X, NOTCH_OFFSET_Y + STATUS_BAR_HEIGHT + 64,
                   "PID 1 Interactive Execution Bridge", FONT_SIZE_SUBTITLE, COLOR_SUBTITLE_TXT);

    int start_y = topbar_h + 16;
    int line_h = 34;
    int max_disp = kb_is_visible() ? 7 : TERM_LINES;

    for (int i = 0; i < g_term_count && i < max_disp; i++) {
        font_draw_text(UI_PADDING_X, start_y + i * line_h, g_term_history[i], FONT_SIZE_SMALL, COLOR_TITLE_TXT);
    }

    if (kb_is_visible()) {
        kb_render();
    } else {
        int bar_y = g_fb.yres - NAV_BAR_HEIGHT - 90;
        int btn_w = g_fb.xres - (UI_PADDING_X * 2);
        fb_draw_card(UI_PADDING_X, bar_y, btn_w, 75, COLOR_CARD_SEL_BG, COLOR_CARD_SEL_BORDER);
        vector_draw_icon(UI_PADDING_X + (btn_w/2) - 130, bar_y + 24, 28, VEC_ICON_TERMINAL, COLOR_TITLE_TXT);
        font_draw_text(UI_PADDING_X + (btn_w/2) - 90, bar_y + 26, "OPEN KEYBOARD", FONT_SIZE_BODY, COLOR_TITLE_TXT);
    }
}

void terminal_handle_touch(int x, int y, int is_down)
{
    if (!is_down) return;

    if (kb_is_visible()) {
        if (kb_handle_touch(x, y, is_down))
            return;
    }

    trigger_vibration();

    int bar_y = g_fb.yres - NAV_BAR_HEIGHT - 90;
    int btn_w = g_fb.xres - (UI_PADDING_X * 2);

    if (y >= bar_y && y <= bar_y + 75 &&
        x >= UI_PADDING_X && x <= UI_PADDING_X + btn_w) {
        kb_show("", on_kb_submit);
    }
}
