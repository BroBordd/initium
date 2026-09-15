#ifndef FONT_H
#define FONT_H

int  font_init(void);
void font_draw_text(int px, int py, const char *text, float size_px, unsigned int color);
int  font_measure_text(const char *text, float size_px);

#endif
