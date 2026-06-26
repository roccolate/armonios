#ifndef KOLIBRIARM_DRIVERS_FB_FB_H
#define KOLIBRIARM_DRIVERS_FB_FB_H

#include <stdint.h>

typedef struct fb {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
} fb_t;

int fb_init(fb_t *fb, uint32_t *pixels, uint32_t width, uint32_t height,
            uint32_t stride_pixels);
void fb_putpixel(fb_t *fb, uint32_t x, uint32_t y, uint32_t color);
void fb_putpixel_alpha(fb_t *fb, uint32_t x, uint32_t y, uint32_t color);
void fb_fillrect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t color);
void fb_draw_rect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                  uint32_t color);
void fb_draw_line(fb_t *fb, int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                  uint32_t color);
void fb_draw_circle(fb_t *fb, int32_t cx, int32_t cy, int32_t radius,
                    uint32_t color);
void fb_blit(fb_t *fb, uint32_t dst_x, uint32_t dst_y, const uint32_t *src,
             uint32_t w, uint32_t h);

#endif
