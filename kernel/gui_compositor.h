#ifndef KOLIBRIARM_KERNEL_GUI_COMPOSITOR_H
#define KOLIBRIARM_KERNEL_GUI_COMPOSITOR_H

#include <stdint.h>

#include "kernel/gui.h"

int gui_init(gui_desktop_t *desktop, fb_t *fb, uint32_t background_color);
int gui_window_draw_text(gui_desktop_t *desktop, uint32_t window_id,
                         int32_t x, int32_t y, const char *text,
                         uint32_t color);
int gui_window_draw_rect(gui_desktop_t *desktop, uint32_t window_id,
                         int32_t x, int32_t y, uint32_t w, uint32_t h,
                         uint32_t color);
int gui_window_clear(gui_desktop_t *desktop, uint32_t window_id,
                     uint32_t color);
/* Damage tracking. gui_damage_add pushes a framebuffer-coords rect onto the
 * desktop's list, merging it with overlapping/adjacent entries and
 * collapsing to a "full" sentinel when the list fills. The full sentinel
 * short-circuits future adds until the next clear. Coords are clipped to
 * the framebuffer; zero-area rects are dropped. */
void gui_damage_add(gui_desktop_t *desktop, int32_t x, int32_t y,
                    int32_t w, int32_t h);
void gui_damage_add_full(gui_desktop_t *desktop);
void gui_damage_clear(gui_desktop_t *desktop);
void gui_draw(gui_desktop_t *desktop);

/* The kernel has exactly one GUI: a single desktop with a fixed pool of
 * windows, a single cursor, and a single dirty flag. These are the
 * entry points the kernel uses from the timer tick and the input/svc
 * paths; tests can also drive them directly. */
void gui_render(fb_t *fb, void *context);
void gui_init_for_framebuffer(fb_t *fb, void *context);
gui_desktop_t *gui_desktop(void);
int gui_handle_input(const input_event_t *event);
int gui_is_dirty(void);
void gui_clear_dirty(void);
void gui_request_redraw(void);

#endif
