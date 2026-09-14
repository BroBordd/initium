/* src/mount.h */
#ifndef MOUNT_H
#define MOUNT_H

void mount_screen_init(void);
void mount_screen_render(void);
void mount_screen_handle_touch(int x, int y, int is_down);
void mount_screen_scroll(int direction);

#endif
