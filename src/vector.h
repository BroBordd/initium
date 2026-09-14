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
    VEC_ICON_BATTERY
} vector_icon_t;

void vector_init(void);
void vector_draw_icon(int x, int y, int size, vector_icon_t icon, unsigned int color);
int  vector_load_and_draw_svg(int x, int y, int size, const char *svg_filename, unsigned int color);

#endif
