#include "kernel/gui_cursor.h"

#include "fb/fb.h"
#include "kernel/gui_internal.h"

static int gui_cursor_region_contains(const gui_window_t *window,
                                      uint32_t slot,
                                      int32_t content_x,
                                      int32_t content_y) {
    const int32_t rx = window->cursor_regions[slot].x;
    const int32_t ry = window->cursor_regions[slot].y;
    const int32_t rw = window->cursor_regions[slot].w;
    const int32_t rh = window->cursor_regions[slot].h;
    int64_t x1;
    int64_t y1;

    if (rw <= 0 || rh <= 0) {
        return 0;
    }

    x1 = (int64_t)rx + (int64_t)rw;
    y1 = (int64_t)ry + (int64_t)rh;
    return content_x >= rx && (int64_t)content_x < x1 &&
           content_y >= ry && (int64_t)content_y < y1;
}

void gui_refresh_cursor_shape(gui_desktop_t *desktop) {
    int32_t hit;
    gui_window_t *window;
    int32_t content_x;
    int32_t content_y;

    if (desktop == 0) {
        return;
    }

    desktop->cursor.shape = GUI_CURSOR_ARROW;
    hit = gui_hit_test(desktop, desktop->cursor.x, desktop->cursor.y);
    if (hit == (int32_t)GUI_NO_WINDOW || hit < 0) {
        return;
    }

    window = &desktop->windows[hit];

    content_x = desktop->cursor.x - (int32_t)window->x;
    content_y = desktop->cursor.y - ((int32_t)window->y +
                                     (int32_t)window->title_h);
    for (uint32_t i = 0; i < GUI_MAX_CURSOR_REGIONS; i++) {
        if (window->cursor_regions[i].used == 0U) {
            continue;
        }
        if (gui_cursor_region_contains(window, i, content_x, content_y)) {
            desktop->cursor.shape =
                (uint8_t)window->cursor_regions[i].shape;
            return;
        }
    }

    if (window->title_h > 0U && window->title[0] != '\0' &&
        desktop->cursor.y >= (int32_t)window->y &&
        desktop->cursor.y < (int32_t)(window->y + window->title_h)) {
        desktop->cursor.shape = GUI_CURSOR_HAND;
    }
}

void gui_get_cursor(gui_desktop_t *desktop, int32_t *x, int32_t *y) {
    if (desktop == 0) {
        return;
    }
    if (x != 0) {
        *x = desktop->cursor.x;
    }
    if (y != 0) {
        *y = desktop->cursor.y;
    }
}

int gui_set_cursor_shape(gui_desktop_t *desktop, uint32_t shape) {
    if (desktop == 0 ||
        (shape != GUI_CURSOR_ARROW && shape != GUI_CURSOR_HAND)) {
        return -1;
    }

    desktop->cursor.shape = (uint8_t)shape;
    return 0;
}

int gui_register_cursor_region(gui_desktop_t *desktop, uint32_t window_id,
                               uint32_t slot, int32_t x, int32_t y,
                               uint32_t w, uint32_t h, uint32_t shape) {
    gui_window_t *window;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        slot >= GUI_MAX_CURSOR_REGIONS) {
        return -1;
    }
    window = &desktop->windows[window_id];
    if (window->used == 0U) {
        return -1;
    }

    if (shape == GUI_CURSOR_REGION_DELETE) {
        window->cursor_regions[slot].used = 0U;
        window->cursor_regions[slot].x = 0;
        window->cursor_regions[slot].y = 0;
        window->cursor_regions[slot].w = 0;
        window->cursor_regions[slot].h = 0;
        window->cursor_regions[slot].shape = 0;
        if (window_id == desktop->focused_window_id) {
            gui_refresh_cursor_shape(desktop);
        }
        return 0;
    }

    if (shape != GUI_CURSOR_ARROW && shape != GUI_CURSOR_HAND) {
        return -1;
    }

    if (w == 0U || h == 0U || w > (uint32_t)INT32_MAX ||
        h > (uint32_t)INT32_MAX) {
        return -1;
    }

    window->cursor_regions[slot].x = x;
    window->cursor_regions[slot].y = y;
    window->cursor_regions[slot].w = (int32_t)w;
    window->cursor_regions[slot].h = (int32_t)h;
    window->cursor_regions[slot].shape = shape;
    window->cursor_regions[slot].used = 1U;

    if (window_id == desktop->focused_window_id) {
        gui_refresh_cursor_shape(desktop);
    }
    return 0;
}

void gui_set_cursor(gui_desktop_t *desktop, int32_t x, int32_t y) {
    int32_t prev_x;
    int32_t prev_y;

    if (desktop == 0 || desktop->fb == 0) {
        return;
    }

    if (x < 0) {
        x = 0;
    }
    if (y < 0) {
        y = 0;
    }
    if ((uint32_t)x >= desktop->fb->width) {
        x = (int32_t)desktop->fb->width - 1;
    }
    if ((uint32_t)y >= desktop->fb->height) {
        y = (int32_t)desktop->fb->height - 1;
    }

    prev_x = desktop->cursor.x;
    prev_y = desktop->cursor.y;
    desktop->cursor.prev_x = prev_x;
    desktop->cursor.prev_y = prev_y;
    desktop->cursor.x = x;
    desktop->cursor.y = y;
    gui_refresh_cursor_shape(desktop);

    gui_damage_add(desktop, prev_x, prev_y,
                   (int32_t)GUI_CURSOR_W, (int32_t)GUI_CURSOR_H);
    gui_damage_add(desktop, x, y,
                   (int32_t)GUI_CURSOR_W, (int32_t)GUI_CURSOR_H);
}

void gui_cursor_move(gui_desktop_t *desktop, int32_t dx, int32_t dy) {
    if (desktop == 0) {
        return;
    }
    gui_set_cursor(desktop, desktop->cursor.x + dx, desktop->cursor.y + dy);
}

void gui_drag_start(gui_desktop_t *desktop, uint32_t window_id,
                    int32_t off_x, int32_t off_y) {
    gui_window_t *window;

    if (desktop == 0 || window_id >= GUI_MAX_WINDOWS ||
        desktop->windows[window_id].used == 0) {
        return;
    }

    window = &desktop->windows[window_id];
    if ((window->flags & GUI_WINDOW_NO_DRAG) != 0U) {
        return;
    }

    if (window->title_h > 0U &&
        (off_y < 0 || (uint32_t)off_y >= window->title_h)) {
        return;
    }

    desktop->drag_window_id = window_id;
    desktop->drag_off_x = off_x;
    desktop->drag_off_y = off_y;
}

void gui_drag_update(gui_desktop_t *desktop, int32_t cursor_x,
                     int32_t cursor_y) {
    uint32_t id;
    gui_window_t *window;
    int32_t new_x;
    int32_t new_y;
    int32_t old_x;
    int32_t old_y;
    int32_t old_w;
    int32_t old_h;
    int32_t ux0;
    int32_t uy0;
    int32_t ux1;
    int32_t uy1;

    if (desktop == 0 || desktop->drag_window_id == GUI_NO_WINDOW) {
        return;
    }

    id = desktop->drag_window_id;
    window = &desktop->windows[id];
    if (window->used == 0) {
        desktop->drag_window_id = GUI_NO_WINDOW;
        return;
    }

    new_x = cursor_x - desktop->drag_off_x;
    new_y = cursor_y - desktop->drag_off_y;
    if (new_x < 0) {
        new_x = 0;
    }
    if (new_y < 0) {
        new_y = 0;
    }

    old_x = (int32_t)window->x;
    old_y = (int32_t)window->y;
    old_w = (int32_t)window->w;
    old_h = (int32_t)window->h;
    window->x = (uint32_t)new_x;
    window->y = (uint32_t)new_y;
    gui_refresh_cursor_shape(desktop);

    ux0 = old_x < new_x ? old_x : new_x;
    uy0 = old_y < new_y ? old_y : new_y;
    ux1 = old_x + old_w > new_x + old_w ? old_x + old_w : new_x + old_w;
    uy1 = old_y + old_h > new_y + old_h ? old_y + old_h : new_y + old_h;
    gui_damage_add(desktop, ux0, uy0, ux1 - ux0, uy1 - uy0);
}

void gui_drag_end(gui_desktop_t *desktop) {
    if (desktop == 0) {
        return;
    }
    desktop->drag_window_id = GUI_NO_WINDOW;
}

void gui_cursor_button(gui_desktop_t *desktop, uint32_t button,
                       uint32_t pressed) {
    uint8_t mask;
    int32_t hit;

    if (desktop == 0 || button > 2U) {
        return;
    }

    mask = (uint8_t)(1U << button);
    if (pressed != 0U) {
        desktop->cursor.buttons_mask =
            (uint8_t)(desktop->cursor.buttons_mask | mask);
    } else {
        desktop->cursor.buttons_mask =
            (uint8_t)(desktop->cursor.buttons_mask & (uint8_t)(~mask));
    }

    if (button != 0U || pressed == 0U) {
        return;
    }

    hit = gui_hit_test(desktop, desktop->cursor.x, desktop->cursor.y);
    if (hit != (int32_t)GUI_NO_WINDOW && hit >= 0) {
        (void)gui_focus_window(desktop, (uint32_t)hit);
    } else {
        desktop->focused_window_id = GUI_NO_WINDOW;
    }
}
