#ifndef KOLIBRIARM_KERNEL_GUI_BACKING_H
#define KOLIBRIARM_KERNEL_GUI_BACKING_H

#include "kernel/gui.h"

fb_t gui_window_backing_fb(const gui_window_t *window);
int gui_window_ensure_backing(gui_window_t *window);
int gui_window_reset_backing(gui_window_t *window);
void gui_window_free_backing(gui_window_t *window);

#endif
