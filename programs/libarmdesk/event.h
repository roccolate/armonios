#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_EVENT_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_EVENT_H

#include <stdint.h>

#include "rect.h"

typedef struct {
    int32_t x;
    int32_t y;
} armdesk_point_t;

/*
 * Convert an absolute desktop pointer coordinate into window content space.
 *
 * Window bounds include the title bar. The returned point is relative to the
 * top-left of the client/content area. Outside points and invalid title heights
 * fail without modifying the output.
 */
static inline int armdesk_pointer_to_content(armdesk_rect_t window_bounds,
                                              int32_t title_height,
                                              int32_t absolute_x,
                                              int32_t absolute_y,
                                              armdesk_point_t *out) {
    armdesk_rect_t content;
    int64_t content_y;
    int64_t local_x;
    int64_t local_y;

    if (out == 0 || armdesk_rect_is_empty(window_bounds) ||
        title_height < 0 || title_height >= window_bounds.h) {
        return -1;
    }

    content_y = (int64_t)window_bounds.y + title_height;
    if (content_y < INT32_MIN || content_y > INT32_MAX) {
        return -1;
    }

    content = armdesk_rect(window_bounds.x, (int32_t)content_y,
                           window_bounds.w,
                           window_bounds.h - title_height);
    if (!armdesk_rect_contains(content, absolute_x, absolute_y)) {
        return -1;
    }

    local_x = (int64_t)absolute_x - content.x;
    local_y = (int64_t)absolute_y - content.y;
    if (local_x < INT32_MIN || local_x > INT32_MAX ||
        local_y < INT32_MIN || local_y > INT32_MAX) {
        return -1;
    }

    out->x = (int32_t)local_x;
    out->y = (int32_t)local_y;
    return 0;
}

#endif