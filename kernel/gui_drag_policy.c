#include <stdint.h>

#include "kernel/gui.h"

static int str_eq(const char *a, const char *b) {
    uint32_t i = 0;

    if (a == 0 || b == 0) {
        return 0;
    }

    for (;;) {
        if (a[i] != b[i]) {
            return 0;
        }
        if (a[i] == '\0') {
            return 1;
        }
        i++;
    }
}

static uint32_t effective_window_flags(const gui_window_t *window) {
    uint32_t flags;

    if (window == 0 || window->used == 0) {
        return 0;
    }

    flags = window->flags;

    /* Bootstrap policy for the built-in taskbar. The structural field now
     * exists, but userland does not yet have a SET_FLAGS syscall. Until that
     * ABI lands, the panel title gives the window manager a stable way to
     * classify the desktop dock without relying on geometry. */
    if (str_eq(window->title, "panel")) {
        flags |= GUI_WINDOW_DOCK | GUI_WINDOW_NO_FOCUS |
                 GUI_WINDOW_NO_DRAG | GUI_WINDOW_SKIP_TASKBAR;
    }

    return flags;
}

void gui_drag_start(gui_desktop_t *desktop, uint32_t window_id,
                    int32_t off_x, int32_t off_y) {
    gui_window_t *window;
    uint32_t flags;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return;
    }

    window = &desktop->windows[window_id];
    flags = effective_window_flags(window);

    if ((flags & GUI_WINDOW_NO_DRAG) != 0U) {
        return;
    }

    /* Titled windows drag only from their title bar. Untitled normal windows
     * keep the legacy behavior and can drag from their content area. */
    if (window->title_h > 0U &&
        (off_y < 0 || (uint32_t)off_y >= window->title_h)) {
        return;
    }

    desktop->drag_window_id = window_id;
    desktop->drag_off_x = off_x;
    desktop->drag_off_y = off_y;
}

int gui_focus_window(gui_desktop_t *desktop, uint32_t window_id) {
    uint32_t prev;
    gui_window_t *window;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return -1;
    }

    window = &desktop->windows[window_id];
    if ((effective_window_flags(window) & GUI_WINDOW_NO_FOCUS) != 0U) {
        /* Dock/taskbar windows still receive mouse events via gui_handle_input,
         * but they do not become the active app and do not raise above normal
         * windows just because the user clicked inside them. */
        gui_damage_add(desktop, (int32_t)window->x, (int32_t)window->y,
                       (int32_t)window->w, (int32_t)window->h);
        return 0;
    }

    prev = desktop->focused_window_id;
    desktop->focused_window_id = window_id;
    window->z = desktop->next_z++;

    if (prev != GUI_NO_WINDOW && prev < GUI_MAX_WINDOWS &&
        desktop->windows[prev].used != 0) {
        gui_window_t *old = &desktop->windows[prev];
        gui_damage_add(desktop, (int32_t)old->x, (int32_t)old->y,
                       (int32_t)old->w, (int32_t)old->h);
    }

    gui_damage_add(desktop, (int32_t)window->x, (int32_t)window->y,
                   (int32_t)window->w, (int32_t)window->h);
    return 0;
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
