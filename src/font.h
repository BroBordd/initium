/* font.h */
#ifndef FONT_H
#define FONT_H

int  font_init(void);
void font_draw_text(int px, int py, const char *text, float size_px, unsigned int color);

#endif
