#ifndef KOLIBRIARM_SYSCALL_DIAG_H
#define KOLIBRIARM_SYSCALL_DIAG_H

#include <stdint.h>

#include "kernel/gui.h"
#include "kernel/print.h"
#include "uart/pl011.h"

static inline void syscall_diag_u64(const char *name, uint64_t value) {
    uart_puts(name);
    uart_puts("=");
    print_dec64(value);
    uart_puts(" ");
}

static inline int syscall_diag_gui_window_draw_rect(gui_desktop_t *desktop,
                                                    uint32_t window_id,
                                                    int32_t x, int32_t y,
                                                    uint32_t w, uint32_t h,
                                                    uint32_t color) {
    int ret;

    uart_puts("sysdiag: draw_rect ");
    syscall_diag_u64("win", window_id);
    syscall_diag_u64("x", (uint64_t)(uint32_t)x);
    syscall_diag_u64("y", (uint64_t)(uint32_t)y);
    syscall_diag_u64("w", w);
    syscall_diag_u64("h", h);
    syscall_diag_u64("color", color);
    uart_puts("\n");

    ret = gui_window_draw_rect(desktop, window_id, x, y, w, h, color);

    uart_puts("sysdiag: draw_rect_ret ");
    print_dec64((uint64_t)(int64_t)ret);
    uart_puts("\n");

    return ret;
}

static inline int syscall_diag_gui_window_draw_text(gui_desktop_t *desktop,
                                                    uint32_t window_id,
                                                    int32_t x, int32_t y,
                                                    const char *text,
                                                    uint32_t color) {
    int ret;

    uart_puts("sysdiag: draw_text ");
    syscall_diag_u64("win", window_id);
    syscall_diag_u64("x", (uint64_t)(uint32_t)x);
    syscall_diag_u64("y", (uint64_t)(uint32_t)y);
    syscall_diag_u64("color", color);
    uart_puts("text=");
    uart_puts(text != 0 ? text : "<null>");
    uart_puts("\n");

    ret = gui_window_draw_text(desktop, window_id, x, y, text, color);

    uart_puts("sysdiag: draw_text_ret ");
    print_dec64((uint64_t)(int64_t)ret);
    uart_puts("\n");

    return ret;
}

static inline void syscall_diag_gui_request_redraw(void) {
    uart_puts("sysdiag: request_redraw\n");
    gui_request_redraw();
}

#define gui_window_draw_rect syscall_diag_gui_window_draw_rect
#define gui_window_draw_text syscall_diag_gui_window_draw_text
#define gui_request_redraw syscall_diag_gui_request_redraw

#endif
