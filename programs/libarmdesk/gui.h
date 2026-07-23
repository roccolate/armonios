// programs/libarmdesk/gui.h
//
// Typed wrappers for the ArmoniOS window/compositor syscalls (70..86).
// GUI ABI layouts and constants come from the public shared header; syscall
// dispatch remains implemented by libkarm.
//
// Return value: raw `long` from the kernel; >= 0 on success, negative error
// code from <libkarm/errno.h> on failure.

#ifndef ARMONIOS_PROGRAMS_LIBARMDESK_GUI_H
#define ARMONIOS_PROGRAMS_LIBARMDESK_GUI_H

#include <stddef.h>
#include <stdint.h>

#include "include/armonios/abi/gui.h"
#include "libkarm/errno.h"
#include "libkarm/syscall.h"
#include "kernel/syscall_numbers.h"

/*
 * Userland-created windows are presentation windows and should receive
 * keyboard focus immediately. Kernel-owned/no-focus windows keep their kernel
 * policy: SYS_WINDOW_FOCUS rejects them and the create result is still
 * returned unchanged.
 */
static inline long gui_window_create(long x, long y, long w, long h,
                                     long bg, long border,
                                     const char *title) {
    long window_id = __syscall7(SYS_WINDOW_CREATE, x, y, w, h, bg, border,
                                (long)(uintptr_t)title);
    if (window_id >= 0) {
        (void)__syscall1(SYS_WINDOW_FOCUS, window_id);
    }
    return window_id;
}

static inline long gui_window_destroy(long window_id) {
    return __syscall1(SYS_WINDOW_DESTROY, window_id);
}

static inline long gui_window_draw_text(long window_id, long x, long y,
                                        long color, const char *str) {
    return __syscall5(SYS_WINDOW_DRAW_TEXT, window_id, x, y, color,
                      (long)(uintptr_t)str);
}

static inline long gui_window_draw_rect(long window_id, long x, long y,
                                        long w, long h, long color) {
    return __syscall6(SYS_WINDOW_DRAW_RECT, window_id, x, y, w, h, color);
}

static inline long gui_window_event(long window_id, gui_event_t *buf,
                                    size_t max_events) {
    return __syscall3(SYS_WINDOW_EVENT, window_id,
                      (long)(uintptr_t)buf, (long)max_events);
}

static inline long gui_window_set_title(long window_id, const char *title,
                                        long title_h) {
    return __syscall3(SYS_WINDOW_SET_TITLE, window_id,
                      (long)(uintptr_t)title, title_h);
}

static inline long gui_window_redraw(long window_id) {
    return __syscall1(SYS_WINDOW_REDRAW, window_id);
}

static inline long gui_window_focus(long window_id) {
    return __syscall1(SYS_WINDOW_FOCUS, window_id);
}

static inline long gui_window_for_pid(long owner_pid, size_t index) {
    return __syscall2(SYS_WINDOW_FOR_PID, owner_pid, (long)index);
}

static inline long gui_cursor_set_shape(long shape) {
    return __syscall1(SYS_CURSOR_SET_SHAPE, shape);
}

static inline long gui_window_flush(long window_id, long x, long y,
                                    long w, long h) {
    return __syscall5(SYS_WINDOW_FLUSH, window_id, x, y, w, h);
}

static inline long gui_window_get_bounds(long window_id, void *out_ptr) {
    return __syscall2(SYS_WINDOW_GET_BOUNDS, window_id,
                      (long)(uintptr_t)out_ptr);
}

static inline long gui_window_set_bounds(long window_id, long x, long y,
                                         long w, long h) {
    return __syscall5(SYS_WINDOW_SET_BOUNDS, window_id, x, y, w, h);
}

static inline long gui_window_minimize(long window_id) {
    return __syscall1(SYS_WINDOW_MINIMIZE, window_id);
}

static inline long gui_window_restore(long window_id) {
    return __syscall1(SYS_WINDOW_RESTORE, window_id);
}

static inline long gui_window_state(long window_id, uint32_t *out_ptr) {
    return __syscall2(SYS_WINDOW_STATE, window_id, (long)(uintptr_t)out_ptr);
}

static inline long gui_cursor_register_region(long window_id, long slot,
                                              long x, long y, long w,
                                              long h, long shape) {
    return __syscall7(SYS_CURSOR_REGISTER_REGION, window_id, slot, x, y,
                      w, h, shape);
}

#endif
