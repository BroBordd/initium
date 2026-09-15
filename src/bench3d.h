/* src/bench3d.h */
#ifndef BENCH3D_H
#define BENCH3D_H

void bench3d_init(void);
void bench3d_render(void);
void bench3d_handle_touch(int x, int y, int is_down);
void bench3d_rotate_drag(float dx, float dy);

#endif
