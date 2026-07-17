#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/gui.h"
#include "kernel/syscall_helpers.h"

int64_t sys_window_create(process_t *process, uint64_t x, uint64_t y,
                          uint64_t w, uint64_t h, uint64_t bg,
                          uint64_t border, uint64_t title_ptr) {
    gui_desktop_t *desktop;
    char title[GUI_TITLE_LEN];
    uint32_t window_id = GUI_NO_WINDOW;

    if (process == 0 || x > UINT32_MAX || y > UINT32_MAX ||
        w > UINT32_MAX || h > UINT32_MAX || w < 8 || h < 8 ||
        bg > UINT32_MAX || border > UINT32_MAX) {
        return ERR_INVAL;
    }

    for (uint32_t i = 0; i < GUI_TITLE_LEN; i++) {
        title[i] = '\0';
    }
    if (title_ptr != 0) {
        if (sys_user_copy_cstr(process, title_ptr, title, GUI_TITLE_LEN) != 0) {
            return ERR_INVAL;
        }
    }

    desktop = gui_desktop();
    if (desktop == 0) {
        return ERR_AGAIN;
    }

    if (gui_create_window_for_pid(desktop, process->pid,
                                  (uint32_t)x, (uint32_t)y, (uint32_t)w,
                                  (uint32_t)h, (uint32_t)bg,
                                  (uint32_t)border, title,
                                  &window_id) != 0) {
        return ERR_AGAIN;
    }
    gui_request_redraw();
    return (int64_t)window_id;
}

int64_t sys_window_destroy(process_t *process, uint64_t window_id) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    int64_t status;

    status = sys_owner_window(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (gui_destroy_window(desktop, (uint32_t)window_id) != 0) {
        return ERR_BADF;
    }
    gui_request_redraw();
    return 0;
}

int64_t sys_window_draw_text(process_t *process, uint64_t window_id,
                             uint64_t x, uint64_t y, uint64_t color,
                             uint64_t str_ptr) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    char text[128];
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        x > INT32_MAX || y > INT32_MAX || color > UINT32_MAX) {
        return ERR_INVAL;
    }
    status = sys_owner_window_badf(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (sys_user_copy_cstr(process, str_ptr, text, sizeof(text)) != 0) {
        return ERR_INVAL;
    }
    if (gui_window_draw_text(desktop, (uint32_t)window_id,
                             (int32_t)x, (int32_t)y, text,
                             (uint32_t)color) != 0) {
        return ERR_BADF;
    }
    return 0;
}

int64_t sys_window_draw_rect(process_t *process, uint64_t window_id,
                             uint64_t x, uint64_t y, uint64_t w,
                             uint64_t h, uint64_t color) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        x > INT32_MAX || y > INT32_MAX ||
        w > UINT32_MAX || h > UINT32_MAX || color > UINT32_MAX) {
        return ERR_INVAL;
    }
    status = sys_owner_window_badf(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (gui_window_draw_rect(desktop, (uint32_t)window_id,
                             (int32_t)x, (int32_t)y, (uint32_t)w,
                             (uint32_t)h, (uint32_t)color) != 0) {
        return ERR_BADF;
    }
    return 0;
}

int64_t sys_window_set_title(process_t *process, uint64_t window_id,
                             uint64_t title_ptr, uint64_t title_h) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    char title[GUI_TITLE_LEN];
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        title_h > UINT32_MAX) {
        return ERR_INVAL;
    }
    status = sys_owner_window_badf(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (sys_user_copy_cstr(process, title_ptr, title, GUI_TITLE_LEN) != 0) {
        return ERR_INVAL;
    }
    if (title_h > 0U && (uint32_t)title_h >= window->h) {
        return ERR_INVAL;
    }
    if (gui_window_set_title_internal(desktop, (uint32_t)window_id, title) != 0) {
        return ERR_BADF;
    }
    gui_request_redraw();
    if (title_h > 0U &&
        gui_window_set_title_bar_internal(desktop, (uint32_t)window_id,
                                          (uint32_t)title_h) != 0) {
        return ERR_INVAL;
    }
    return 0;
}

int64_t sys_window_redraw(process_t *process, uint64_t window_id) {
    gui_window_t *window;
    int64_t status;

    status = sys_owner_window_badf(process, window_id, 0, &window);
    if (status != 0) {
        return status;
    }
    gui_request_redraw();
    return 0;
}

int64_t sys_window_focus(process_t *process, uint64_t window_id) {
    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return ERR_NOENT;
    }
    if (gui_focus_window(desktop, (uint32_t)window_id) != 0) {
        return ERR_INVAL;
    }
    gui_request_redraw();
    return 0;
}

int64_t sys_window_for_pid(process_t *process, uint64_t owner_pid,
                           uint64_t index) {
    gui_desktop_t *desktop = gui_desktop();
    uint32_t window_id;

    if (process == 0 || owner_pid > UINT32_MAX || index >= GUI_MAX_WINDOWS) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window_id = gui_window_for_pid(desktop, (uint32_t)owner_pid,
                                   (uint32_t)index);
    if (window_id == GUI_NO_WINDOW) {
        return ERR_NOENT;
    }
    return (int64_t)window_id;
}

int64_t sys_cursor_set_shape(process_t *process, uint64_t shape) {
    gui_desktop_t *desktop = gui_desktop();

    if (process == 0 || shape > UINT32_MAX) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    if (gui_set_cursor_shape(desktop, (uint32_t)shape) != 0) {
        return ERR_INVAL;
    }
    gui_request_redraw();
    return 0;
}

int64_t sys_cursor_register_region(process_t *process, uint64_t win,
                                   uint64_t slot, uint64_t x, uint64_t y,
                                   uint64_t w, uint64_t h, uint64_t shape) {
    gui_desktop_t *desktop = gui_desktop();
    const gui_window_t *window;

    if (process == 0 || win > UINT32_MAX || slot > UINT32_MAX ||
        x > INT32_MAX || y > INT32_MAX || w > INT32_MAX ||
        h > INT32_MAX || shape > UINT32_MAX) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window = gui_window_lookup(desktop, (uint32_t)win);
    if (window == 0 || window->used == 0U) {
        return ERR_NOENT;
    }
    if (window->owner_pid != GUI_NO_OWNER &&
        window->owner_pid != (uint32_t)process->pid) {
        return ERR_PERM;
    }
    if (gui_register_cursor_region(desktop, (uint32_t)win,
                                   (uint32_t)slot, (int32_t)x, (int32_t)y,
                                   (uint32_t)w, (uint32_t)h,
                                   (uint32_t)shape) != 0) {
        return ERR_INVAL;
    }
    gui_request_redraw();
    return 0;
}

int64_t sys_window_flush(process_t *process, uint64_t window_id,
                         uint64_t x, uint64_t y, uint64_t w, uint64_t h) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    uint32_t content_h;
    int64_t x0;
    int64_t y0;
    int64_t x1;
    int64_t y1;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        x > INT32_MAX || y > INT32_MAX ||
        w > INT32_MAX || h > INT32_MAX) {
        return ERR_INVAL;
    }
    status = sys_owner_window_badf(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (w == 0 || h == 0) {
        return 0;
    }
    content_h = window->h > window->title_h ? window->h - window->title_h : 0U;
    x0 = (int64_t)(int32_t)x;
    y0 = (int64_t)(int32_t)y;
    x1 = x0 + (int64_t)(int32_t)w;
    y1 = y0 + (int64_t)(int32_t)h;
    if (x0 < 0) {
        x0 = 0;
    }
    if (y0 < 0) {
        y0 = 0;
    }
    if (x1 > (int64_t)window->w) {
        x1 = (int64_t)window->w;
    }
    if (y1 > (int64_t)content_h) {
        y1 = (int64_t)content_h;
    }
    if (x1 <= x0 || y1 <= y0) {
        return 0;
    }
    gui_damage_add(desktop,
                   (int32_t)window->x + (int32_t)x0,
                   (int32_t)window->y + (int32_t)window->title_h + (int32_t)y0,
                   (int32_t)(x1 - x0), (int32_t)(y1 - y0));
    return 0;
}

int64_t sys_window_get_bounds(process_t *process, uint64_t window_id,
                              uint64_t out_ptr) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    uint32_t *out = (uint32_t *)(uintptr_t)out_ptr;
    int64_t status;

    status = sys_user_buf_out(process, out_ptr, sizeof(uint32_t) * 4U);
    if (status != 0) {
        return status;
    }
    status = sys_owner_window(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    out[0] = window->x;
    out[1] = window->y;
    out[2] = window->w;
    out[3] = window->h;
    return 0;
}

int64_t sys_window_set_bounds(process_t *process, uint64_t window_id,
                              uint64_t x, uint64_t y, uint64_t w,
                              uint64_t h) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        x > INT32_MAX || y > INT32_MAX ||
        w > UINT32_MAX || h > UINT32_MAX) {
        return ERR_INVAL;
    }
    status = sys_owner_window(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (gui_resize_window(desktop, (uint32_t)window_id, (uint32_t)x,
                          (uint32_t)y, (uint32_t)w, (uint32_t)h) != 0) {
        return ERR_INVAL;
    }
    return 0;
}

int64_t sys_window_minimize(process_t *process, uint64_t window_id) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    int64_t status;

    status = sys_owner_window(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    if (gui_window_minimize(desktop, (uint32_t)window_id) != 0) {
        return ERR_INVAL;
    }
    return 0;
}

int64_t sys_window_restore(process_t *process, uint64_t window_id) {
    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return ERR_NOENT;
    }
    if (gui_window_restore(desktop, (uint32_t)window_id) != 0) {
        return ERR_INVAL;
    }
    return 0;
}

int64_t sys_window_state(process_t *process, uint64_t window_id,
                         uint64_t out_ptr) {
    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;
    uint32_t *out = (uint32_t *)(uintptr_t)out_ptr;
    uint32_t state = 0;
    int64_t status;

    status = sys_user_buf_out(process, out_ptr, sizeof(uint32_t));
    if (status != 0) {
        return status;
    }
    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return ERR_NOENT;
    }
    if (window->minimized != 0U) {
        state |= 0x1U;
    }
    if (desktop->focused_window_id == (uint32_t)window_id &&
        (window->flags & GUI_WINDOW_NO_FOCUS) == 0U) {
        state |= 0x2U;
    }
    *out = state;
    return 0;
}

int64_t sys_window_event(process_t *process, uint64_t window_id,
                         uint64_t buf_ptr, uint64_t buf_count) {
    gui_window_t *window;
    uint32_t *out = (uint32_t *)(uintptr_t)buf_ptr;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        buf_count == 0 || buf_count > 64) {
        return ERR_INVAL;
    }
    status = sys_user_buf_out(process, buf_ptr,
                              buf_count * 3U * sizeof(uint32_t));
    if (status != 0) {
        return status;
    }
    status = sys_owner_window_badf(process, window_id, 0, &window);
    if (status != 0) {
        return status;
    }

    if (window->event_count == 0) {
        return ERR_AGAIN;
    }

    uint64_t n = 0;
    while (n < buf_count && window->event_count > 0) {
        gui_event_t ev;
        if (gui_window_pop_event(window, &ev) != 0) {
            break;
        }
        out[n * 3U + 0U] = ev.type;
        out[n * 3U + 1U] = (uint32_t)ev.data1;
        out[n * 3U + 2U] = (uint32_t)ev.data2;
        n++;
    }
    return (int64_t)n;
}
