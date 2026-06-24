#include <stdint.h>

#include "kernel/gui.h"

void gui_drag_start(gui_desktop_t *desktop, uint32_t window_id,
                    int32_t off_x, int32_t off_y) {
    gui_window_t *window;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return;
    }

    window = &desktop->windows[window_id];

    /* Dock/panel-style windows have no kernel title bar and should not
     * behave like draggable application windows. Titled windows drag only
     * from their title bar, matching normal desktop window-manager policy. */
    if (window->title_h == 0U || off_y < 0 ||
        (uint32_t)off_y >= window->title_h) {
        return;
    }

    desktop->drag_window_id = window_id;
    desktop->drag_off_x = off_x;
    desktop->drag_off_y = off_y;
}

int gui_window_push_event(gui_window_t *window, uint32_t type,
                          int32_t data1, int32_t data2) {
    if (window == 0 || window->used == 0) {
        return -1;
    }

    /* Mouse motion can arrive much faster than an EL0 panel/app polls its
     * window event queue. Coalesce consecutive motion events so high-rate
     * movement cannot starve clicks. */
    if (type == GUI_EVENT_MOUSE_MOVE && window->event_count > 0U) {
        uint32_t prev = window->event_tail == 0U
                            ? GUI_EVENT_QUEUE_SIZE - 1U
                            : window->event_tail - 1U;
        if (window->events[prev].type == GUI_EVENT_MOUSE_MOVE) {
            window->events[prev].data1 = data1;
            window->events[prev].data2 = data2;
            return 0;
        }
    }

    if (window->event_count >= GUI_EVENT_QUEUE_SIZE) {
        if (type == GUI_EVENT_MOUSE_MOVE) {
            return 0;
        }

        /* Preserve button/key events by evicting the oldest queued item when
         * the queue is full. In practice the evicted item is almost always a
         * stale mouse-move; losing that is better than dropping the click. */
        window->event_head = (window->event_head + 1U) % GUI_EVENT_QUEUE_SIZE;
        window->event_count--;
    }

    window->events[window->event_tail].type = type;
    window->events[window->event_tail].data1 = data1;
    window->events[window->event_tail].data2 = data2;
    window->event_tail = (window->event_tail + 1U) % GUI_EVENT_QUEUE_SIZE;
    window->event_count++;
    return 0;
}
