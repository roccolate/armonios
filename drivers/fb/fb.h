#ifndef KOLIBRIARM_DRIVERS_FB_FB_H
#define KOLIBRIARM_DRIVERS_FB_FB_H

#include <stdint.h>

typedef struct {
    uint32_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t stride_pixels;
} fb_t;

int fb_init(fb_t *fb, uint32_t *pixels, uint32_t width, uint32_t height,
            uint32_t stride_pixels);
void fb_putpixel(fb_t *fb, uint32_t x, uint32_t y, uint32_t color);
void fb_fillrect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t color);
void fb_blit(fb_t *fb, uint32_t dst_x, uint32_t dst_y, const uint32_t *src,
             uint32_t w, uint32_t h);

#endif
