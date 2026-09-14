/* src/keyboard.h */
#ifndef KEYBOARD_H
#define KEYBOARD_H

typedef void (*kb_callback_t)(const char *submitted_text);

void kb_init(void);
void kb_show(const char *initial_text, kb_callback_t on_submit);
void kb_hide(void);
int  kb_is_visible(void);
int  kb_get_height(void);
void kb_render(void);
int  kb_handle_touch(int x, int y, int is_down);
const char *kb_get_buffer(void);

#endif
