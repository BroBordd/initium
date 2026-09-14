/* src/terminal.h */
#ifndef TERMINAL_H
#define TERMINAL_H

void terminal_init(void);
void terminal_render(void);
void terminal_handle_touch(int x, int y, int is_down);
void terminal_execute_command(const char *cmd_line);

#endif
