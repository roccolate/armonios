#include "fb/fb.h"

#include <stdint.h>

int fb_init(fb_t *fb, uint32_t *pixels, uint32_t width, uint32_t height,
            uint32_t stride_pixels) {
    if (fb == 0 || pixels == 0 || width == 0 || height == 0 ||
        stride_pixels < width) {
        return -1;
    }

    fb->pixels = pixels;
    fb->width = width;
    fb->height = height;
    fb->stride_pixels = stride_pixels;

    return 0;
}

void fb_putpixel(fb_t *fb, uint32_t x, uint32_t y, uint32_t color) {
    if (fb == 0 || fb->pixels == 0 || x >= fb->width || y >= fb->height) {
        return;
    }

    fb->pixels[y * fb->stride_pixels + x] = color;
}

void fb_fillrect(fb_t *fb, uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                 uint32_t color) {
    if (fb == 0 || fb->pixels == 0 || w == 0 || h == 0 ||
        x >= fb->width || y >= fb->height) {
        return;
    }

    if (w > fb->width - x) {
        w = fb->width - x;
    }

    if (h > fb->height - y) {
        h = fb->height - y;
    }

    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            fb->pixels[(y + row) * fb->stride_pixels + x + col] = color;
        }
    }
}

void fb_blit(fb_t *fb, uint32_t dst_x, uint32_t dst_y, const uint32_t *src,
             uint32_t w, uint32_t h) {
    uint32_t src_stride = w;

    if (fb == 0 || fb->pixels == 0 || src == 0 || w == 0 || h == 0 ||
        dst_x >= fb->width || dst_y >= fb->height) {
        return;
    }

    if (w > fb->width - dst_x) {
        w = fb->width - dst_x;
    }

    if (h > fb->height - dst_y) {
        h = fb->height - dst_y;
    }

    for (uint32_t row = 0; row < h; row++) {
        for (uint32_t col = 0; col < w; col++) {
            fb->pixels[(dst_y + row) * fb->stride_pixels + dst_x + col] =
                src[row * src_stride + col];
        }
    }
}
