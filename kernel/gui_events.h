#ifndef KOLIBRIARM_KERNEL_GUI_EVENTS_H
#define KOLIBRIARM_KERNEL_GUI_EVENTS_H

#include <stdint.h>

#include "kernel/gui.h"

int gui_window_push_event(gui_window_t *window, uint32_t type,
                          int32_t data1, int32_t data2);
int gui_window_pop_event(gui_window_t *window, gui_event_t *out);

#endif
