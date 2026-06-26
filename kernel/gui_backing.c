#include "kernel/gui_backing.h"

#include "fb/fb.h"
#include "kernel/mm/kheap.h"

fb_t gui_window_backing_fb(const gui_window_t *window) {
    fb_t fb;
    uint32_t content_h = window->h > window->title_h
                             ? window->h - window->title_h
                             : 0U;

    fb.pixels = window->backing;
    fb.width = window->w;
    fb.height = content_h;
    fb.stride_pixels = window->w;
    return fb;
}

static int gui_window_backing_size(const gui_window_t *window,
                                   uint32_t *out_bytes) {
    uint32_t content_h;

    if (window == 0 || window->w == 0U || window->h == 0U ||
        out_bytes == 0) {
        return -1;
    }

    content_h = window->h > window->title_h ? window->h - window->title_h : 0U;
    if (content_h == 0U) {
        return -1;
    }

    *out_bytes = window->w * content_h * sizeof(uint32_t);
    return 0;
}

static int gui_window_alloc_backing(gui_window_t *window,
                                    uint32_t needed_bytes) {
    uint32_t *new_backing;
    fb_t fb;

    new_backing = (uint32_t *)kmalloc((unsigned long)needed_bytes);
    if (new_backing == 0) {
        return -1;
    }

    fb = gui_window_backing_fb(window);
    fb.pixels = new_backing;
    fb_fillrect(&fb, 0, 0, fb.width, fb.height, window->bg_color);

    if (window->backing != 0) {
        kfree(window->backing);
    }
    window->backing = new_backing;
    window->backing_capacity = needed_bytes;
    return 0;
}

int gui_window_ensure_backing(gui_window_t *window) {
    uint32_t needed_bytes;

    if (gui_window_backing_size(window, &needed_bytes) != 0) {
        return -1;
    }

    if (window->backing != 0 && window->backing_capacity >= needed_bytes) {
        return 0;
    }

    return gui_window_alloc_backing(window, needed_bytes);
}

int gui_window_reset_backing(gui_window_t *window) {
    uint32_t needed_bytes;
    fb_t fb;

    if (gui_window_backing_size(window, &needed_bytes) != 0) {
        return -1;
    }

    if (window->backing != 0 && window->backing_capacity >= needed_bytes) {
        fb = gui_window_backing_fb(window);
        fb_fillrect(&fb, 0, 0, fb.width, fb.height, window->bg_color);
        return 0;
    }

    return gui_window_alloc_backing(window, needed_bytes);
}

void gui_window_free_backing(gui_window_t *window) {
    if (window == 0 || window->backing == 0) {
        return;
    }
    kfree(window->backing);
    window->backing = 0;
    window->backing_capacity = 0;
    window->owner_drawn = 0;
}
