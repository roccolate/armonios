#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_RECT_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_RECT_H

#include <stdint.h>

/*
 * Backend-neutral rectangle helpers for userland desktop code.
 *
 * The API follows the small caller-owned geometry model proven in rkc, but is
 * kept ArmoniOS-local so the OS does not gain an external runtime dependency.
 * Coordinates use half-open bounds: [x, x + w) and [y, y + h).
 */

typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
} armdesk_rect_t;

static inline armdesk_rect_t armdesk_rect(int32_t x, int32_t y,
                                          int32_t w, int32_t h) {
    armdesk_rect_t rect = {x, y, w, h};
    return rect;
}

static inline int armdesk_rect_is_empty(armdesk_rect_t rect) {
    return rect.w <= 0 || rect.h <= 0;
}

static inline int64_t armdesk_rect_right(armdesk_rect_t rect) {
    return (int64_t)rect.x + (int64_t)rect.w;
}

static inline int64_t armdesk_rect_bottom(armdesk_rect_t rect) {
    return (int64_t)rect.y + (int64_t)rect.h;
}

static inline int armdesk_rect_contains(armdesk_rect_t rect,
                                        int32_t x, int32_t y) {
    if (armdesk_rect_is_empty(rect)) {
        return 0;
    }
    return (int64_t)x >= rect.x && (int64_t)y >= rect.y &&
           (int64_t)x < armdesk_rect_right(rect) &&
           (int64_t)y < armdesk_rect_bottom(rect);
}

static inline int armdesk_rect_intersects(armdesk_rect_t a,
                                          armdesk_rect_t b) {
    if (armdesk_rect_is_empty(a) || armdesk_rect_is_empty(b)) {
        return 0;
    }
    return (int64_t)a.x < armdesk_rect_right(b) &&
           (int64_t)b.x < armdesk_rect_right(a) &&
           (int64_t)a.y < armdesk_rect_bottom(b) &&
           (int64_t)b.y < armdesk_rect_bottom(a);
}

static inline armdesk_rect_t armdesk_rect_intersection(armdesk_rect_t a,
                                                        armdesk_rect_t b) {
    int64_t left = a.x > b.x ? a.x : b.x;
    int64_t top = a.y > b.y ? a.y : b.y;
    int64_t right_a = armdesk_rect_right(a);
    int64_t right_b = armdesk_rect_right(b);
    int64_t bottom_a = armdesk_rect_bottom(a);
    int64_t bottom_b = armdesk_rect_bottom(b);
    int64_t right = right_a < right_b ? right_a : right_b;
    int64_t bottom = bottom_a < bottom_b ? bottom_a : bottom_b;

    if (right <= left || bottom <= top) {
        return armdesk_rect(0, 0, 0, 0);
    }
    return armdesk_rect((int32_t)left, (int32_t)top,
                        (int32_t)(right - left),
                        (int32_t)(bottom - top));
}

static inline armdesk_rect_t armdesk_rect_clamp_to_bounds(
    armdesk_rect_t rect, int32_t width, int32_t height) {
    return armdesk_rect_intersection(rect,
                                     armdesk_rect(0, 0, width, height));
}

#endif
