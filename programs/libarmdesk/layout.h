#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_LAYOUT_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_LAYOUT_H

#include <stdint.h>

#include "rect.h"

typedef enum {
    ARMDESK_LAYOUT_ROW = 0,
    ARMDESK_LAYOUT_COLUMN = 1,
} armdesk_layout_axis_t;

typedef struct {
    armdesk_rect_t remaining;
    int32_t gap;
    armdesk_layout_axis_t axis;
} armdesk_linear_layout_t;

static inline armdesk_linear_layout_t armdesk_linear_layout(
    armdesk_rect_t bounds, armdesk_layout_axis_t axis, int32_t gap) {
    armdesk_linear_layout_t layout;

    layout.remaining = bounds;
    layout.gap = gap > 0 ? gap : 0;
    layout.axis = axis;
    return layout;
}

static inline armdesk_linear_layout_t armdesk_row_layout(
    armdesk_rect_t bounds, int32_t gap) {
    return armdesk_linear_layout(bounds, ARMDESK_LAYOUT_ROW, gap);
}

static inline armdesk_linear_layout_t armdesk_column_layout(
    armdesk_rect_t bounds, int32_t gap) {
    return armdesk_linear_layout(bounds, ARMDESK_LAYOUT_COLUMN, gap);
}

/*
 * Reserve one item from the leading edge of a row or column.
 *
 * The operation is transactional: invalid or oversized requests leave the
 * layout unchanged. A gap is consumed only when space remains after the item.
 */
static inline int armdesk_linear_layout_take(armdesk_linear_layout_t *layout,
                                              int32_t extent,
                                              armdesk_rect_t *out) {
    armdesk_rect_t item;
    int32_t available;
    int32_t spacing;

    if (layout == 0 || out == 0 || extent <= 0 ||
        armdesk_rect_is_empty(layout->remaining) ||
        (layout->axis != ARMDESK_LAYOUT_ROW &&
         layout->axis != ARMDESK_LAYOUT_COLUMN)) {
        return -1;
    }

    available = layout->axis == ARMDESK_LAYOUT_ROW
                    ? layout->remaining.w
                    : layout->remaining.h;
    if (extent > available) {
        return -1;
    }

    item = layout->remaining;
    if (layout->axis == ARMDESK_LAYOUT_ROW) {
        item.w = extent;
    } else {
        item.h = extent;
    }

    spacing = available > extent ? layout->gap : 0;
    if (spacing > available - extent) {
        spacing = available - extent;
    }

    if (layout->axis == ARMDESK_LAYOUT_ROW) {
        layout->remaining.x += extent + spacing;
        layout->remaining.w -= extent + spacing;
    } else {
        layout->remaining.y += extent + spacing;
        layout->remaining.h -= extent + spacing;
    }

    *out = item;
    return 0;
}

#endif