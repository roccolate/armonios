#include "kernel/gui_compositor.h"

#include <stdint.h>

#include "fb/fb.h"
#include "kernel/gui_internal.h"

void gui_damage_add(gui_desktop_t *desktop, int32_t x, int32_t y,
                    int32_t w, int32_t h) {
    int64_t x1;
    int64_t y1;

    if (desktop == 0 || desktop->fb == 0) {
        return;
    }
    if (desktop->damage_full) {
        return;
    }
    if (w <= 0 || h <= 0) {
        return;
    }
    int32_t fb_w = (int32_t)desktop->fb->width;
    int32_t fb_h = (int32_t)desktop->fb->height;
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x >= fb_w || y >= fb_h) {
        return;
    }
    x1 = (int64_t)x + (int64_t)w;
    y1 = (int64_t)y + (int64_t)h;
    if (x1 > fb_w) {
        w = fb_w - x;
    }
    if (y1 > fb_h) {
        h = fb_h - y;
    }
    if (w <= 0 || h <= 0) {
        return;
    }

    if (x == 0 && y == 0 && w == fb_w && h == fb_h) {
        desktop->damage_full = 1;
        desktop->damage_count = 0;
        gui_mark_dirty();
        return;
    }

    for (uint32_t i = 0; i < desktop->damage_count; ) {
        damage_rect_t *r = &desktop->damage_rects[i];
        int32_t ax1 = x + w;
        int32_t ay1 = y + h;
        int32_t bx1 = r->x + r->w;
        int32_t by1 = r->y + r->h;
        if (x <= bx1 && ax1 >= r->x && y <= by1 && ay1 >= r->y) {
            int32_t nx0 = x < r->x ? x : r->x;
            int32_t ny0 = y < r->y ? y : r->y;
            int32_t nx1 = ax1 > bx1 ? ax1 : bx1;
            int32_t ny1 = ay1 > by1 ? ay1 : by1;
            x = nx0;
            y = ny0;
            w = nx1 - nx0;
            h = ny1 - ny0;
            for (uint32_t k = i; k + 1U < desktop->damage_count; k++) {
                desktop->damage_rects[k] = desktop->damage_rects[k + 1U];
            }
            desktop->damage_count--;
            continue;
        }
        i++;
    }
    if (desktop->damage_count >= GUI_DAMAGE_MAX) {
        desktop->damage_full = 1;
        desktop->damage_count = 0;
        gui_mark_dirty();
        return;
    }
    desktop->damage_rects[desktop->damage_count].x = x;
    desktop->damage_rects[desktop->damage_count].y = y;
    desktop->damage_rects[desktop->damage_count].w = w;
    desktop->damage_rects[desktop->damage_count].h = h;
    desktop->damage_count++;
    gui_mark_dirty();
}

void gui_damage_add_full(gui_desktop_t *desktop) {
    if (desktop == 0) {
        return;
    }
    desktop->damage_full = 1;
    desktop->damage_count = 0;
    gui_mark_dirty();
}

void gui_damage_clear(gui_desktop_t *desktop) {
    if (desktop == 0) {
        return;
    }
    desktop->damage_full = 0;
    desktop->damage_count = 0;
}
