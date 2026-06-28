#include "kernel/gui_input.h"

#include "kernel/process.h"

static int gui_window_owner_dead(const gui_window_t *window) {
    const process_t *owner;

    if (window == 0 || window->used == 0 ||
        window->owner_pid == GUI_NO_OWNER) {
        return 0;
    }

    owner = process_find(window->owner_pid);
    return owner == 0 || owner->state == PROCESS_ZOMBIE ||
           owner->state == PROCESS_UNUSED;
}

int gui_window_contains(gui_window_t *window, int32_t x, int32_t y) {
    int64_t x1;
    int64_t y1;

    if (window == 0 || window->used == 0) {
        return 0;
    }

    x1 = (int64_t)window->x + (int64_t)window->w;
    y1 = (int64_t)window->y + (int64_t)window->h;
    return x >= (int32_t)window->x &&
           (int64_t)x < x1 &&
           y >= (int32_t)window->y &&
           (int64_t)y < y1;
}

int gui_hit_test(gui_desktop_t *desktop, int32_t x, int32_t y) {
    uint32_t best_z = 0;
    int best = (int)GUI_NO_WINDOW;

    if (desktop == 0) {
        return GUI_NO_WINDOW;
    }

    for (uint32_t i = 0; i < GUI_MAX_WINDOWS; i++) {
        gui_window_t *window = &desktop->windows[i];
        if (gui_window_contains(window, x, y)) {
            if (best == (int)GUI_NO_WINDOW || window->z >= best_z) {
                best = (int)i;
                best_z = window->z;
            }
        }
    }

    return best;
}

int gui_dispatch_input(gui_desktop_t *desktop, const input_event_t *event) {
    if (desktop == 0 || event == 0) {
        return -1;
    }

    switch (event->type) {
    case INPUT_EVENT_KEY_PRESS:
    case INPUT_EVENT_KEY_RELEASE:
        if (desktop->focused_window_id != GUI_NO_WINDOW &&
            desktop->focused_window_id < GUI_MAX_WINDOWS &&
            desktop->windows[desktop->focused_window_id].used != 0) {
            return gui_window_push_event(
                &desktop->windows[desktop->focused_window_id],
                event->type == INPUT_EVENT_KEY_PRESS ? GUI_EVENT_KEY_PRESS
                                                     : GUI_EVENT_KEY_RELEASE,
                (int32_t)event->data.key.key, 0);
        }
        return 0;
    case INPUT_EVENT_MOUSE_MOVE: {
        int32_t hit = gui_hit_test(desktop, desktop->cursor.x,
                                   desktop->cursor.y);
        if (hit == (int32_t)GUI_NO_WINDOW || hit < 0) {
            return 0;
        }
        return gui_window_push_event(&desktop->windows[hit],
                                     GUI_EVENT_MOUSE_MOVE,
                                     desktop->cursor.x, desktop->cursor.y);
    }
    default:
        return -1;
    }
}

int gui_handle_desktop_input(gui_desktop_t *desktop,
                             const input_event_t *event) {
    if (desktop == 0 || event == 0) {
        return -1;
    }

    switch (event->type) {
    case INPUT_EVENT_MOUSE_MOVE:
        gui_cursor_move(desktop, event->data.mouse_move.dx,
                        event->data.mouse_move.dy);
        if (desktop->drag_window_id != GUI_NO_WINDOW) {
            gui_drag_update(desktop, desktop->cursor.x, desktop->cursor.y);
        }
        (void)gui_dispatch_input(desktop, event);
        return 0;
    case INPUT_EVENT_MOUSE_BUTTON:
        gui_cursor_button(desktop, event->data.mouse_button.button,
                          event->data.mouse_button.pressed);
        if (event->data.mouse_button.button == 0U) {
            if (event->data.mouse_button.pressed != 0U) {
                int32_t hit = gui_hit_test(desktop, desktop->cursor.x,
                                           desktop->cursor.y);
                if (hit != (int32_t)GUI_NO_WINDOW && hit >= 0) {
                    gui_window_t *window = &desktop->windows[hit];
                    gui_decoration_hit_t decoration;
                    int has_decoration = gui_hit_test_decoration(
                        window, desktop->cursor.x, desktop->cursor.y,
                        &decoration);

                    if (has_decoration &&
                        decoration.slot == GUI_DECORATION_SLOT_CLOSE) {
                        if (gui_window_owner_dead(window)) {
                            (void)gui_destroy_window(desktop, (uint32_t)hit);
                        } else {
                            (void)gui_window_push_event(window,
                                                        GUI_EVENT_CLOSE, 0, 0);
                        }
                    } else if (has_decoration &&
                               decoration.slot ==
                                   GUI_DECORATION_SLOT_MINIMIZE) {
                        (void)gui_window_minimize(desktop, (uint32_t)hit);
                    } else if (has_decoration &&
                               decoration.slot ==
                                   GUI_DECORATION_SLOT_MAXIMIZE) {
                        (void)gui_window_push_event(window,
                                                    GUI_EVENT_MAXIMIZE, 0, 0);
                    } else {
                        gui_drag_start(desktop, (uint32_t)hit,
                                       desktop->cursor.x -
                                           (int32_t)window->x,
                                       desktop->cursor.y -
                                           (int32_t)window->y);
                        (void)gui_window_push_event(window,
                                                    GUI_EVENT_MOUSE_CLICK,
                                                    desktop->cursor.x,
                                                    desktop->cursor.y);
                    }
                }
            } else {
                gui_drag_end(desktop);
            }
        }
        return 0;
    case INPUT_EVENT_KEY_PRESS:
        (void)gui_focus_window_ensure(desktop);
        if (desktop->focused_window_id != GUI_NO_WINDOW) {
            gui_window_t *window =
                &desktop->windows[desktop->focused_window_id];
            (void)gui_window_push_event(window, GUI_EVENT_KEY_PRESS,
                                        (int32_t)event->data.key.key, 0);
        }
        return 0;
    case INPUT_EVENT_KEY_RELEASE:
        if (desktop->focused_window_id != GUI_NO_WINDOW) {
            gui_window_t *window =
                &desktop->windows[desktop->focused_window_id];
            (void)gui_window_push_event(window, GUI_EVENT_KEY_RELEASE,
                                        (int32_t)event->data.key.key, 0);
        }
        return 0;
    default:
        return -1;
    }
}

static int gui_rect_contains(uint32_t rx, uint32_t ry, uint32_t rw,
                             uint32_t rh, int32_t x, int32_t y) {
    int64_t x1 = (int64_t)rx + (int64_t)rw;
    int64_t y1 = (int64_t)ry + (int64_t)rh;

    return x >= (int32_t)rx &&
           (int64_t)x < x1 &&
           y >= (int32_t)ry &&
           (int64_t)y < y1;
}

int gui_close_box_rect(const gui_window_t *window, uint32_t *out_x,
                       uint32_t *out_y, uint32_t *out_w, uint32_t *out_h) {
    uint32_t bh;
    uint32_t bw;

    if (window == 0 || window->used == 0 || window->title_h == 0U ||
        window->title[0] == '\0' ||
        window->title_h < GUI_CLOSE_BTN_MIN_TITLE_H) {
        return 0;
    }

    bh = window->title_h - 2U * GUI_CLOSE_BTN_PAD;
    if (bh < 4U) {
        return 0;
    }

    bw = bh < GUI_CLOSE_BTN_W ? bh : GUI_CLOSE_BTN_W;
    if (window->w < bw + 2U * GUI_CLOSE_BTN_PAD) {
        return 0;
    }

    *out_x = window->x + window->w - bw - GUI_CLOSE_BTN_PAD;
    *out_y = window->y + GUI_CLOSE_BTN_PAD;
    *out_w = bw;
    *out_h = bh;
    return 1;
}

/*
 * Min / max / close boxes are siblings along the right edge of the
 * title bar. Close sits at the right edge, minimise one button-width
 * to its left, maximise between them.
 */
static int gui_min_max_btn_row(const gui_window_t *window, uint32_t *out_x,
                               uint32_t *out_y, uint32_t *out_w,
                               uint32_t *out_h) {
    uint32_t bw;
    uint32_t bh;

    if (window == 0 || window->used == 0 || window->title_h == 0U ||
        window->title[0] == '\0' ||
        window->title_h < GUI_CLOSE_BTN_MIN_TITLE_H) {
        return 0;
    }
    bh = window->title_h - 2U * GUI_CLOSE_BTN_PAD;
    if (bh < 4U) {
        return 0;
    }
    bw = bh < GUI_CLOSE_BTN_W ? bh : GUI_CLOSE_BTN_W;
    if (window->w < 3U * bw + 4U * GUI_CLOSE_BTN_PAD) {
        return 0;
    }
    *out_w = bw;
    *out_h = bh;
    *out_y = window->y + GUI_CLOSE_BTN_PAD;
    *out_x = window->x + window->w - bw - GUI_CLOSE_BTN_PAD;
    return 1;
}

int gui_minimize_button_rect(const gui_window_t *window, uint32_t *out_x,
                             uint32_t *out_y, uint32_t *out_w,
                             uint32_t *out_h) {
    uint32_t right_x;
    if (!gui_min_max_btn_row(window, &right_x, out_y, out_w, out_h)) {
        return 0;
    }
    *out_x = right_x - 2U * (*out_w) - 2U * GUI_CLOSE_BTN_PAD;
    return 1;
}

int gui_maximize_button_rect(const gui_window_t *window, uint32_t *out_x,
                             uint32_t *out_y, uint32_t *out_w,
                             uint32_t *out_h) {
    uint32_t right_x;
    if (!gui_min_max_btn_row(window, &right_x, out_y, out_w, out_h)) {
        return 0;
    }
    *out_x = right_x - (*out_w) - GUI_CLOSE_BTN_PAD;
    return 1;
}

static int gui_try_decoration_slot(const gui_window_t *window, uint32_t slot,
                                   int32_t x, int32_t y,
                                   gui_decoration_hit_t *out_hit) {
    uint32_t rx = 0;
    uint32_t ry = 0;
    uint32_t rw = 0;
    uint32_t rh = 0;
    int found = 0;

    if (slot == GUI_DECORATION_SLOT_MINIMIZE) {
        found = gui_minimize_button_rect(window, &rx, &ry, &rw, &rh);
    } else if (slot == GUI_DECORATION_SLOT_MAXIMIZE) {
        found = gui_maximize_button_rect(window, &rx, &ry, &rw, &rh);
    } else if (slot == GUI_DECORATION_SLOT_CLOSE) {
        found = gui_close_box_rect(window, &rx, &ry, &rw, &rh);
    }

    if (!found || !gui_rect_contains(rx, ry, rw, rh, x, y)) {
        return 0;
    }

    if (out_hit != 0) {
        out_hit->slot = slot;
        out_hit->x = rx;
        out_hit->y = ry;
        out_hit->w = rw;
        out_hit->h = rh;
    }
    return 1;
}

int gui_hit_test_decoration(const gui_window_t *window, int32_t x, int32_t y,
                            gui_decoration_hit_t *out_hit) {
    if (out_hit != 0) {
        out_hit->slot = GUI_DECORATION_SLOT_NONE;
        out_hit->x = 0;
        out_hit->y = 0;
        out_hit->w = 0;
        out_hit->h = 0;
    }

    return gui_try_decoration_slot(window, GUI_DECORATION_SLOT_MINIMIZE,
                                   x, y, out_hit) ||
           gui_try_decoration_slot(window, GUI_DECORATION_SLOT_MAXIMIZE,
                                   x, y, out_hit) ||
           gui_try_decoration_slot(window, GUI_DECORATION_SLOT_CLOSE,
                                   x, y, out_hit);
}
