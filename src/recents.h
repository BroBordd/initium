/* src/recents.h */
#ifndef RECENTS_H
#define RECENTS_H

void recents_init(void);
void recents_render(void);
void recents_handle_touch(int x, int y, int is_down);
void recents_scroll(float delta_y);

#endif
