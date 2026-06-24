// KolibriARM app: monitor (C version)
//
// Windowed process monitor. It owns a desktop window, redraws memory
// and tick counters plus a short process list once every REDRAW_WAIT
// yields, and exits on 'q', 'Q', or title-bar close.
//
// This is the second app migrated to programs/libkarm. Non-window
// syscalls (write, yield, meminfo, timeinfo, proclist, exit) go
// through libkarm's typed wrappers in <libkarm/syscall.h>. Number
// formatting uses kli_utoa from <libkarm/string.h>. Window syscalls
// (create, destroy, draw_text, draw_rect, event, flush, set_title)
// still go through libkarm's raw __syscallN trampolines —
// programs/libkarmdesk will wrap them in a typed gui.h once every
// app is on libkarm.

#include <stddef.h>
#include <stdint.h>

#include "libkarm/syscall.h"
#include "libkarm/string.h"
#include "libkarm/errno.h"
#include "kernel/syscall_numbers.h"

#define WIN_X          56
#define WIN_Y         120
#define WIN_W         340
#define WIN_H         220
#define TITLE_BAR_H    12
#define EVENT_CAP       8
#define PROC_CAP        6
#define REDRAW_WAIT    20

#define COLOR_BG       0xff182024U
#define COLOR_BORDER   0xff809080U
#define COLOR_TEXT     0xffd0e0d0U

// gui_event_t must match kernel/gui.h: type(u32) data1(i32) data2(i32).
typedef struct {
    uint32_t type;
    int32_t  data1;
    int32_t  data2;
} gui_event_t;

#define GUI_EVENT_KEY_PRESS  1U
#define GUI_EVENT_CLOSE      6U

// sys_proclist entry: pid(u32) state(u32) name[16] -> 24 bytes.
typedef struct {
    uint32_t pid;
    uint32_t state;
    char     name[16];
} proc_entry_t;

// Window/compositor syscalls via libkarm's raw trampolines.
extern long __syscall1(long n, long a0);
extern long __syscall3(long n, long a0, long a1, long a2);
extern long __syscall6(long n, long a0, long a1, long a2,
                       long a3, long a4, long a5);

static long win_create(long x, long y, long w, long h,
                       long bg, long border) {
    // Title goes via sys_window_set_title (75) right after; the
    // documented x6 title_ptr is ignored by syscall_dispatch today.
    return __syscall6(SYS_WINDOW_CREATE, x, y, w, h, bg, border);
}

static long win_destroy(long wid) {
    return __syscall1(SYS_WINDOW_DESTROY, wid);
}

static long win_set_title(long wid, const char *title, long h) {
    return __syscall3(SYS_WINDOW_SET_TITLE, wid, (long)(uintptr_t)title, h);
}

static long win_draw_rect(long wid, long x, long y, long w, long h,
                          long color) {
    return __syscall6(SYS_WINDOW_DRAW_RECT, wid, x, y, w, h, color);
}

static long win_draw_text(long wid, long x, long y, long color,
                          const char *str) {
    return __syscall6(SYS_WINDOW_DRAW_TEXT, wid, x, y, color,
                      (long)(uintptr_t)str, 0);
}

static long win_flush(long wid, long x, long y, long w, long h) {
    return __syscall6(SYS_WINDOW_FLUSH, wid, x, y, w, h, 0);
}

static long win_event(long wid, gui_event_t *buf, long cap) {
    return __syscall3(SYS_WINDOW_EVENT, wid, (long)(uintptr_t)buf, cap);
}

static void write_cstr(long fd, const char *s) {
    while (*s) {
        (void)kli_write((int)fd, s, 1);
        s++;
    }
}

static void draw_text(long wid, long x, long y, const char *s) {
    (void)win_draw_text(wid, x, y, COLOR_TEXT, s);
}

static void redraw(long wid, uint64_t *info, char *numbuf,
                   proc_entry_t *procs) {
    // Clear content area.
    (void)win_draw_rect(wid, 1, 0, WIN_W - 2, WIN_H - TITLE_BAR_H - 2,
                        COLOR_BG);

    draw_text(wid, 12, 8, "SYSTEM MONITOR");

    if (kli_meminfo(info) >= 0) {
        draw_text(wid, 12, 28, "FREE PG");
        kli_utoa(info[1], numbuf, 24);
        draw_text(wid, 96, 28, numbuf);
    }

    if (kli_timeinfo(info) >= 0) {
        draw_text(wid, 12, 44, "TICKS");
        kli_utoa(info[0], numbuf, 24);
        draw_text(wid, 96, 44, numbuf);
    }

    draw_text(wid, 12, 68, "PID   ST    NAME");

    long n = kli_proclist(procs, PROC_CAP);
    if (n < 0) {
        n = 0;
    }
    long y = 84;
    for (long i = 0; i < n && i < PROC_CAP; i++) {
        kli_utoa((uint64_t)procs[i].pid, numbuf, 24);
        draw_text(wid, 12, y, numbuf);
        kli_utoa((uint64_t)procs[i].state, numbuf, 24);
        draw_text(wid, 56, y, numbuf);
        // name is a 16-byte fixed field; draw it as a cstring by
        // forcing a NUL terminator if the kernel didn't.
        char name_buf[17];
        for (int j = 0; j < 16; j++) {
            char c = procs[i].name[j];
            name_buf[j] = (c == '\0') ? '\0' : c;
        }
        name_buf[16] = '\0';
        draw_text(wid, 108, y, name_buf);
        y += 16;
    }

    // Flush the content area (below the kernel-drawn title bar).
    (void)win_flush(wid, 0, 0, WIN_W, WIN_H - TITLE_BAR_H);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    write_cstr(1, "monitor: starting\n");

    long wid = win_create(WIN_X, WIN_Y, WIN_W, WIN_H, COLOR_BG, COLOR_BORDER);
    if (wid < 0) {
        write_cstr(1, "monitor: window create failed\n");
        return 1;
    }
    (void)win_set_title(wid, "monitor", TITLE_BAR_H);

    uint64_t info[3];
    char     numbuf[24];
    proc_entry_t procs[PROC_CAP];
    gui_event_t events[EVENT_CAP];

    long wait = REDRAW_WAIT;
    redraw(wid, info, numbuf, procs);

    for (;;) {
        long n = win_event(wid, events, EVENT_CAP);
        if (n > 0) {
            for (long i = 0; i < n; i++) {
                if (events[i].type == GUI_EVENT_CLOSE) {
                    (void)win_destroy(wid);
                    return 0;
                }
                if (events[i].type == GUI_EVENT_KEY_PRESS &&
                    (events[i].data1 == 'q' || events[i].data1 == 'Q')) {
                    (void)win_destroy(wid);
                    return 0;
                }
            }
        }

        wait--;
        if (wait <= 0) {
            redraw(wid, info, numbuf, procs);
            wait = REDRAW_WAIT;
        }

        (void)kli_yield();
    }
}
