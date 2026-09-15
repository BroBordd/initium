#ifndef UI_H
#define UI_H

typedef enum {
    VIEW_HOMESCREEN,
    VIEW_MOUNT,
    VIEW_FILEMANAGER,
    VIEW_TERMINAL,
    VIEW_SYSMON,
    VIEW_PROCESSES,
    VIEW_RECENTS,
    VIEW_BENCH3D,
    VIEW_POWER
} current_view_t;

void ui_init(void);
void ui_render(void);
void ui_handle_tap(int x, int y);
void ui_handle_scroll_y(float delta_y);
void ui_handle_drag_x(float delta_x);
void ui_on_page_swipe_end(void);
void ui_on_volume_up(void);
void ui_on_volume_down(void);
void ui_on_power_key(void);
void ui_return_to_home(void);
int  ui_is_exit_requested(void);
current_view_t ui_get_current_view(void);

void ui_show_toast(const char *msg);
void ui_show_alert(const char *title, const char *msg);

#endif
