#ifndef ARMONIOS_KERNEL_GUI_CURSOR_H
#define ARMONIOS_KERNEL_GUI_CURSOR_H

#include <stdint.h>

#include "kernel/gui.h"

void gui_get_cursor(gui_desktop_t *desktop, int32_t *x, int32_t *y);
int gui_set_cursor_shape(gui_desktop_t *desktop, uint32_t shape);
/* Install or clear a per-window cursor-shape region. The region is
 * addressed by `slot` (0..GUI_MAX_CURSOR_REGIONS-1) and replaces any
 * existing entry at that slot. Coords are content-local: the kernel
 * adds the window's (x, y + title_h) when checking containment.
 * Passing shape == GUI_CURSOR_REGION_DELETE clears the slot without
 * installing a new region. Returns 0 on success, -1 if the window
 * does not exist or slot is out of range. */
int gui_register_cursor_region(gui_desktop_t *desktop, uint32_t window_id,
                               uint32_t slot, int32_t x, int32_t y,
                               uint32_t w, uint32_t h, uint32_t shape);
void gui_set_cursor(gui_desktop_t *desktop, int32_t x, int32_t y);
void gui_cursor_button(gui_desktop_t *desktop, uint32_t button,
                       uint32_t pressed);
void gui_cursor_move(gui_desktop_t *desktop, int32_t dx, int32_t dy);
void gui_drag_start(gui_desktop_t *desktop, uint32_t window_id,
                    int32_t off_x, int32_t off_y);
void gui_drag_update(gui_desktop_t *desktop, int32_t cursor_x,
                     int32_t cursor_y);
void gui_drag_end(gui_desktop_t *desktop);
static inline int gui_drag_active(const gui_desktop_t *desktop) {
    return (desktop != 0 && desktop->drag_window_id != GUI_NO_WINDOW) ? 1 : 0;
}

#endif
