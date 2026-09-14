/* src/ui.h */
#ifndef UI_H
#define UI_H

typedef enum {
    VIEW_MAIN,
    VIEW_MOUNT,
    VIEW_APPS_MENU,
    VIEW_FILEMANAGER,
    VIEW_TERMINAL,
    VIEW_SYSMON,
    VIEW_RECENTS
} current_view_t;

void ui_init(void);
void ui_render(void);
void ui_handle_touch(int x, int y, int is_down);
void ui_handle_scroll(float delta_y);
void ui_on_volume_up(void);
void ui_on_volume_down(void);
void ui_on_power_key(void);
void ui_return_to_main(void);
int  ui_is_exit_requested(void);
current_view_t ui_get_current_view(void);

#endif
