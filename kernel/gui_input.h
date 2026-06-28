#ifndef ARMONIOS_KERNEL_GUI_INPUT_H
#define ARMONIOS_KERNEL_GUI_INPUT_H

#include <stdint.h>

#include "kernel/gui.h"

#define GUI_DECORATION_SLOT_NONE     0xffffffffU
#define GUI_DECORATION_SLOT_MINIMIZE 0U
#define GUI_DECORATION_SLOT_MAXIMIZE 1U
#define GUI_DECORATION_SLOT_CLOSE    2U

typedef struct {
    uint32_t slot;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} gui_decoration_hit_t;

int gui_close_box_rect(const gui_window_t *window, uint32_t *out_x,
                       uint32_t *out_y, uint32_t *out_w, uint32_t *out_h);
int gui_minimize_button_rect(const gui_window_t *window, uint32_t *out_x,
                             uint32_t *out_y, uint32_t *out_w,
                             uint32_t *out_h);
int gui_maximize_button_rect(const gui_window_t *window, uint32_t *out_x,
                             uint32_t *out_y, uint32_t *out_w,
                             uint32_t *out_h);
int gui_hit_test_decoration(const gui_window_t *window, int32_t x, int32_t y,
                            gui_decoration_hit_t *out_hit);
int gui_window_contains(gui_window_t *window, int32_t x, int32_t y);
int gui_hit_test(gui_desktop_t *desktop, int32_t x, int32_t y);
int gui_dispatch_input(gui_desktop_t *desktop, const input_event_t *event);
int gui_handle_desktop_input(gui_desktop_t *desktop,
                             const input_event_t *event);

#endif
