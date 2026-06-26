#ifndef KOLIBRIARM_KERNEL_GUI_POOL_H
#define KOLIBRIARM_KERNEL_GUI_POOL_H

#include <stdint.h>

#include "kernel/gui.h"

int gui_create_window(gui_desktop_t *desktop, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t bg_color,
                      uint32_t border_color, uint32_t *window_id);
int gui_create_window_for_pid(gui_desktop_t *desktop, uint32_t owner_pid,
                              uint32_t x, uint32_t y, uint32_t w, uint32_t h,
                              uint32_t bg_color, uint32_t border_color,
                              const char *title, uint32_t *window_id);
int gui_destroy_window(gui_desktop_t *desktop, uint32_t window_id);
/*
 * Destroy every window whose owner_pid matches `pid`. Called when
 * a process becomes a zombie so the desktop does not accumulate
 * stale windows between spawns.
 */
void gui_destroy_windows_for_pid(gui_desktop_t *desktop, uint32_t pid);
int gui_window_set_title_internal(gui_desktop_t *desktop, uint32_t window_id,
                                  const char *title);
int gui_window_set_title_bar_internal(gui_desktop_t *desktop, uint32_t window_id,
                                      uint32_t title_h);
int gui_window_set_flags_internal(gui_desktop_t *desktop, uint32_t window_id,
                                  uint32_t flags);
int gui_move_window(gui_desktop_t *desktop, uint32_t window_id, uint32_t x,
                    uint32_t y);
int gui_resize_window(gui_desktop_t *desktop, uint32_t window_id, uint32_t x,
                      uint32_t y, uint32_t w, uint32_t h);
int gui_window_get_bounds(const gui_window_t *window, uint32_t *out_x,
                          uint32_t *out_y, uint32_t *out_w, uint32_t *out_h);
int gui_window_minimize(gui_desktop_t *desktop, uint32_t window_id);
int gui_window_restore(gui_desktop_t *desktop, uint32_t window_id);
int gui_focus_window(gui_desktop_t *desktop, uint32_t window_id);
int gui_focus_window_ensure(gui_desktop_t *desktop);
uint32_t gui_window_for_pid(gui_desktop_t *desktop, uint32_t owner_pid,
                            uint32_t index);
const gui_window_t *gui_window_lookup(const gui_desktop_t *desktop,
                                      uint32_t window_id);

#endif
