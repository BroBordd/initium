/* src/filemanager.h */
#ifndef FILEMANAGER_H
#define FILEMANAGER_H

typedef enum {
    FM_MODE_BROWSE,
    FM_MODE_PICKER
} fm_mode_t;

typedef void (*fm_pick_callback_t)(const char *executable_path);

void fm_init(fm_mode_t mode, const char *start_path, fm_pick_callback_t on_pick);
void fm_render(void);
void fm_handle_touch(int x, int y, int is_down);
void fm_scroll_delta(float delta_y);

#endif
