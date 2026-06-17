#ifndef KOLIBRIARM_KERNEL_FONT_H
#define KOLIBRIARM_KERNEL_FONT_H

#include <stdint.h>

#include "fb/fb.h"

#define FONT_GLYPH_WIDTH 5U
#define FONT_GLYPH_HEIGHT 7U
#define FONT_CHAR_WIDTH 6U
#define FONT_LINE_HEIGHT 8U

void font_draw_char(fb_t *fb, uint32_t x, uint32_t y, char ch,
                    uint32_t color);
void font_draw_text(fb_t *fb, uint32_t x, uint32_t y, const char *text,
                    uint32_t color);

#endif
