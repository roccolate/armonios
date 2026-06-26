#include "kernel/syscall_helpers.h"

#include <stdint.h>

static int64_t owner_window_lookup(process_t *process, uint64_t window_id,
                                   int64_t missing_error,
                                   gui_desktop_t **out_desktop,
                                   gui_window_t **out_window) {
    gui_desktop_t *desktop;
    gui_window_t *window;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS || out_window == 0) {
        return ERR_INVAL;
    }

    desktop = gui_desktop();
    if (desktop == 0) {
        return ERR_AGAIN;
    }

    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return missing_error;
    }
    if (window->owner_pid != process->pid) {
        return ERR_BADF;
    }

    if (out_desktop != 0) {
        *out_desktop = desktop;
    }
    *out_window = window;
    return 0;
}

int64_t sys_owner_window(process_t *process, uint64_t window_id,
                         gui_desktop_t **out_desktop,
                         gui_window_t **out_window) {
    return owner_window_lookup(process, window_id, ERR_NOENT, out_desktop,
                               out_window);
}

int64_t sys_owner_window_badf(process_t *process, uint64_t window_id,
                              gui_desktop_t **out_desktop,
                              gui_window_t **out_window) {
    return owner_window_lookup(process, window_id, ERR_BADF, out_desktop,
                               out_window);
}

static int64_t user_buf_range(const process_t *process, uint64_t ptr,
                              uint64_t len) {
    if (process == 0 || (ptr == 0 && len != 0)) {
        return ERR_INVAL;
    }
    if (!process_user_range_contains(process, ptr, len)) {
        return ERR_INVAL;
    }
    return 0;
}

int64_t sys_user_buf_in(const process_t *process, uint64_t ptr, uint64_t len) {
    return user_buf_range(process, ptr, len);
}

int64_t sys_user_buf_out(const process_t *process, uint64_t ptr, uint64_t len) {
    return user_buf_range(process, ptr, len);
}

int64_t sys_user_copy_cstr(const process_t *process, uint64_t ptr,
                           char *out, uint64_t capacity) {
    const char *src = (const char *)(uintptr_t)ptr;

    if (process == 0 || ptr == 0 || out == 0 || capacity == 0) {
        return ERR_INVAL;
    }

    for (uint64_t i = 0; i < capacity; i++) {
        if (sys_user_buf_in(process, ptr + i, 1) != 0) {
            return ERR_INVAL;
        }

        out[i] = src[i];
        if (out[i] == '\0') {
            return 0;
        }
    }

    return ERR_INVAL;
}
