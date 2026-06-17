#ifndef KOLIBRIARM_KERNEL_GUI_H
#define KOLIBRIARM_KERNEL_GUI_H

#include <stdint.h>

#include "fb/fb.h"

#define GUI_MAX_WINDOWS 4U
#define GUI_NO_WINDOW 0xffffffffU

typedef struct {
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
    uint32_t bg_color;
    uint32_t border_color;
    uint32_t key_count;
    char last_key;
    uint8_t used;
} gui_window_t;

typedef struct {
    fb_t *fb;
    uint32_t background_color;
    uint32_t focused_window_id;
    gui_window_t windows[GUI_MAX_WINDOWS];
} gui_desktop_t;

int gui_init(gui_desktop_t *desktop, fb_t *fb, uint32_t background_color);
int gui_create_window(gui_desktop_t *desktop, uint32_t x, uint32_t y,
                      uint32_t w, uint32_t h, uint32_t bg_color,
                      uint32_t border_color, uint32_t *window_id);
int gui_move_window(gui_desktop_t *desktop, uint32_t window_id, uint32_t x,
                    uint32_t y);
int gui_focus_window(gui_desktop_t *desktop, uint32_t window_id);
int gui_send_key(gui_desktop_t *desktop, char key);
void gui_draw(gui_desktop_t *desktop);
void gui_draw_demo(fb_t *fb, void *context);
int gui_demo_send_key(char key);

#endif
