/* src/vector.h */
#ifndef VECTOR_H
#define VECTOR_H

typedef enum {
    VEC_ICON_FOLDER,
    VEC_ICON_FILE,
    VEC_ICON_BINARY,
    VEC_ICON_HOME,
    VEC_ICON_BACK,
    VEC_ICON_RECENTS,
    VEC_ICON_TERMINAL,
    VEC_ICON_BATTERY,
    VEC_ICON_3D,
    VEC_ICON_STORAGE,
    VEC_ICON_SENSOR,
    VEC_ICON_POWER
} vector_icon_t;

void vector_init(void);
void vector_draw_icon(int x, int y, int size, vector_icon_t icon, unsigned int color);

#endif
