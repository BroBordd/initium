#ifndef PROCESSES_H
#define PROCESSES_H

void processes_init(void);
void processes_render(void);
void processes_handle_touch(int x, int y, int is_down);
void processes_scroll(float delta_y);

#endif
