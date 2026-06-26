#include "kernel/gui_events.h"

int gui_window_push_event(gui_window_t *window, uint32_t type,
                          int32_t data1, int32_t data2) {
    if (window == 0 || window->used == 0) {
        return -1;
    }

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

int gui_window_pop_event(gui_window_t *window, gui_event_t *out) {
    if (window == 0 || window->used == 0 || out == 0) {
        return -1;
    }
    if (window->event_count == 0U) {
        return -1;
    }

    *out = window->events[window->event_head];
    window->event_head = (window->event_head + 1U) % GUI_EVENT_QUEUE_SIZE;
    window->event_count--;
    return 0;
}
