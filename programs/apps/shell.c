// KolibriARM app: shell (C version)
//
// Minimal windowed command shell. It reads key events from its GUI
// window, keeps a tiny fixed line history, and supports a small
// command set: help, ls, mem, ps, ticks, run <name> [args...], kill
// last, pwd, exit. The `run` command splits its tail on whitespace
// and passes the resulting tokens to the spawned app through
// SYS_SPAWN_ARGV so each arg lands in the new process's argv[] on
// its initial stack.
//
// This is the fourth app migrated to programs/libkarm. Process
// spawn/kill, I/O readdir, and system info calls all go through
// libkarm's typed wrappers in <libkarm/syscall.h>; number formatting
// uses kli_utoa from <libkarm/string.h>. Window syscalls still go
// through libkarm's raw __syscallN trampolines — programs/libkarmdesk
// is not built yet.

#include <stddef.h>
#include <stdint.h>

#include "libkarm/syscall.h"
#include "libkarm/string.h"
#include "libkarm/errno.h"
#include "kernel/syscall_numbers.h"

#define WIN_X           72
#define WIN_Y           52
#define WIN_W          480
#define WIN_H          280
#define TITLE_BAR_H     12
#define LINE_CAP        64
#define DISPLAY_LINES    8
#define DISPLAY_COLS    48
#define EVENT_CAP        8
#define PROC_CAP         4
#define ARGV_MAX         8     // matches kernel USER_DEMO_ARGV_MAX_STRINGS
#define HISTORY_DEPTH    8

#define COLOR_BG         0xff141820U
#define COLOR_BORDER     0xff708870U
#define COLOR_TEXT       0xffd8e8d8U

// gui_event_t must match kernel/gui.h: type(u32) data1(i32) data2(i32).
typedef struct {
    uint32_t type;
    int32_t  data1;
    int32_t  data2;
} gui_event_t;

#define GUI_EVENT_KEY_PRESS  1U
#define GUI_EVENT_CLOSE      6U

// Arrow keys come in as synthetic codes from drivers/input/input.c.
// See drivers/input/input.h.
#define INPUT_KEY_UP    0x101
#define INPUT_KEY_DOWN  0x102

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

// Shell state — all on the C stack/globals inside main. Static
// globals would work too but keeping everything in main() makes the
// state obvious at a glance.
typedef struct {
    char       line[LINE_CAP];   // current input
    size_t     line_len;
    char       display[DISPLAY_LINES][DISPLAY_COLS];
    char       history[HISTORY_DEPTH][LINE_CAP];
    int        history_count;
    int        history_cursor;   // == history_count when at "live"
    int        last_spawned_pid;
    long       wid;
    uint64_t   info[3];
    char       numbuf[24];
    proc_entry_t procs[PROC_CAP];
    char       argv_strs[ARGV_MAX][LINE_CAP];
    uint64_t   argv_ptrs[ARGV_MAX];   // kernel sees these as raw u64 addresses
} shell_state_t;

static void display_clear(shell_state_t *s) {
    for (int i = 0; i < DISPLAY_LINES; i++) {
        for (int j = 0; j < DISPLAY_COLS; j++) {
            s->display[i][j] = '\0';
        }
    }
}

// Shift every display row up by one and copy `line` into the last
// row, truncating with a NUL terminator. `line` may be any length;
// only the first DISPLAY_COLS - 1 bytes are kept.
static void push_line(shell_state_t *s, const char *line) {
    for (int i = 0; i < DISPLAY_LINES - 1; i++) {
        for (int j = 0; j < DISPLAY_COLS; j++) {
            s->display[i][j] = s->display[i + 1][j];
        }
    }
    int n = 0;
    while (n < DISPLAY_COLS - 1 && line[n] != '\0') {
        s->display[DISPLAY_LINES - 1][n] = line[n];
        n++;
    }
    while (n < DISPLAY_COLS) {
        s->display[DISPLAY_LINES - 1][n] = '\0';
        n++;
    }
}

static void draw_text(long wid, long x, long y, const char *s) {
    (void)win_draw_text(wid, x, y, COLOR_TEXT, s);
}

static void redraw(shell_state_t *s) {
    // Clear content area (everything below the kernel-drawn title bar).
    (void)win_draw_rect(s->wid, 1, 0, WIN_W - 2,
                        WIN_H - TITLE_BAR_H - 2, COLOR_BG);

    draw_text(s->wid, 12, 8, "USER SHELL");
    for (int i = 0; i < DISPLAY_LINES; i++) {
        draw_text(s->wid, 12, 28 + i * 16, s->display[i]);
    }
    draw_text(s->wid, 12, 168, "U");
    draw_text(s->wid, 32, 168, s->line);
    draw_text(s->wid, 12, 196, "ENTER RUNS COMMAND");

    (void)win_flush(s->wid, 0, 0, WIN_W, WIN_H - TITLE_BAR_H);
}

// History ring: cursor == count means "live" (current line), 0..count
// are the stored entries. Push non-empty input; Up walks back, Down
// walks forward.
static void history_push(shell_state_t *s) {
    if (s->line_len == 0) {
        return;
    }
    if (s->history_count < HISTORY_DEPTH) {
        for (size_t i = 0; i < s->line_len; i++) {
            s->history[s->history_count][i] = s->line[i];
        }
        s->history[s->history_count][s->line_len] = '\0';
        s->history_count++;
    } else {
        // Drop the oldest, shift up.
        for (int i = 0; i < HISTORY_DEPTH - 1; i++) {
            for (size_t j = 0; j < LINE_CAP; j++) {
                s->history[i][j] = s->history[i + 1][j];
            }
        }
        for (size_t i = 0; i < s->line_len; i++) {
            s->history[HISTORY_DEPTH - 1][i] = s->line[i];
        }
        s->history[HISTORY_DEPTH - 1][s->line_len] = '\0';
    }
    s->history_cursor = s->history_count;
}

static void history_load(shell_state_t *s, int idx) {
    for (size_t i = 0; i < LINE_CAP; i++) {
        s->line[i] = '\0';
    }
    size_t n = 0;
    while (s->history[idx][n] != '\0' && n + 1 < LINE_CAP) {
        s->line[n] = s->history[idx][n];
        n++;
    }
    s->line[n] = '\0';
    s->line_len = n;
}

// Command handlers --------------------------------------------------------

static int starts_with(const char *s, const char *prefix) {
    while (*prefix != '\0') {
        if (*s != *prefix) {
            return 0;
        }
        s++;
        prefix++;
    }
    return 1;
}

// shell_cmd_run: parse "run X Y Z" and spawn X with argv = ["X","Y","Z"].
// The first whitespace-delimited token is the app name; remaining tokens
// are passed as argv to the spawned process via SYS_SPAWN_ARGV.
static void cmd_run(shell_state_t *s, const char *input) {
    // Skip "run ".
    while (*input == ' ') {
        input++;
    }
    if (*input == '\0') {
        push_line(s, "RUN FAILED");
        return;
    }
    int argc = 0;
    // Tokenise into argv_strs (kernel can copy from there). argv_ptrs
    // carries the pointer array expected by sys_spawn_argv.
    const char *p = input;
    while (*p != '\0' && argc < ARGV_MAX) {
        while (*p == ' ') {
            p++;
        }
        if (*p == '\0' || *p == ' ') {
            break;
        }
        const char *start = p;
        while (*p != '\0' && *p != ' ') {
            p++;
        }
        size_t len = (size_t)(p - start);
        if (len >= LINE_CAP) {
            len = LINE_CAP - 1;
        }
        for (size_t i = 0; i < len; i++) {
            s->argv_strs[argc][i] = start[i];
        }
        s->argv_strs[argc][len] = '\0';
        s->argv_ptrs[argc] = (uint64_t)(uintptr_t)s->argv_strs[argc];
        argc++;
    }
    if (argc == 0) {
        push_line(s, "RUN FAILED");
        return;
    }
    // Build "/kolibri/<name>".
    char path[32];
    const char *prefix = "/kolibri/";
    size_t pi = 0;
    while (prefix[pi] != '\0') {
        path[pi] = prefix[pi];
        pi++;
    }
    for (size_t i = 0; s->argv_strs[0][i] != '\0' && pi + 1 < sizeof(path); i++) {
        path[pi++] = s->argv_strs[0][i];
    }
    path[pi] = '\0';

    long pid = kli_spawn_argv(path, 0, (const long *)s->argv_ptrs, (long)argc);
    if (pid < 0) {
        push_line(s, "RUN FAILED");
        return;
    }
    s->last_spawned_pid = (int)pid;
    push_line(s, "SPAWNED");
}

static void dispatch(shell_state_t *s, const char *line) {
    if (strcmp(line, "help") == 0) {
        push_line(s, "HELP LS MEM PS TICKS RUN NAME KILL LAST EXIT");
    } else if (strcmp(line, "ls") == 0) {
        char buf[DISPLAY_COLS];
        long n = kli_readdir("/", buf, (long)sizeof(buf));
        if (n < 0) {
            push_line(s, "LS FAILED");
        } else {
            if (n >= DISPLAY_COLS) n = DISPLAY_COLS - 1;
            buf[n] = '\0';
            push_line(s, buf);
        }
    } else if (strcmp(line, "mem") == 0) {
        if (kli_meminfo(s->info) < 0) {
            push_line(s, "MEM FAILED");
        } else {
            push_line(s, "FREE PAGES");
            kli_utoa(s->info[1], s->numbuf, sizeof(s->numbuf));
            push_line(s, s->numbuf);
        }
    } else if (strcmp(line, "ps") == 0) {
        long n = kli_proclist(s->procs, PROC_CAP);
        if (n < 0) {
            push_line(s, "PS FAILED");
            return;
        }
        push_line(s, "PROCESSES");
        for (long i = 0; i < n && i < PROC_CAP; i++) {
            // proc.name is a 16-byte fixed field; null-terminate
            // before pushing so push_line sees a cstring.
            char name_buf[17];
            for (int j = 0; j < 16; j++) {
                char c = s->procs[i].name[j];
                name_buf[j] = (c == '\0') ? '\0' : c;
            }
            name_buf[16] = '\0';
            push_line(s, name_buf);
        }
    } else if (strcmp(line, "ticks") == 0) {
        if (kli_timeinfo(s->info) < 0) {
            push_line(s, "TICKS FAILED");
        } else {
            push_line(s, "TIMER TICKS");
            kli_utoa(s->info[0], s->numbuf, sizeof(s->numbuf));
            push_line(s, s->numbuf);
        }
    } else if (starts_with(line, "run ")) {
        cmd_run(s, line + 4);
    } else if (strcmp(line, "kill last") == 0) {
        if (s->last_spawned_pid == 0) {
            push_line(s, "KILL FAILED");
        } else {
            (void)kli_kill((long)s->last_spawned_pid);
            s->last_spawned_pid = 0;
            push_line(s, "KILLED");
        }
    } else if (strcmp(line, "pwd") == 0) {
        push_line(s, "ROOT");
    } else if (strcmp(line, "exit") == 0) {
        (void)win_destroy(s->wid);
        // kli_exit via crt0 returns the main return value.
        // Use kli_exit explicitly so the loop is bypassed.
        kli_exit(0);
        for (;;) {
            (void)kli_yield();
        }
    } else {
        push_line(s, "UNKNOWN COMMAND");
    }
}

static void handle_key(shell_state_t *s, int key) {
    if (key == 17) {                              // Ctrl-Q
        (void)win_destroy(s->wid);
        kli_exit(0);
        for (;;) {
            (void)kli_yield();
        }
    }
    if (key == INPUT_KEY_UP) {
        if (s->history_cursor > 0) {
            s->history_cursor--;
            history_load(s, s->history_cursor);
        }
        return;
    }
    if (key == INPUT_KEY_DOWN) {
        if (s->history_cursor + 1 < s->history_count) {
            s->history_cursor++;
            history_load(s, s->history_cursor);
        } else if (s->history_cursor + 1 == s->history_count) {
            s->history_cursor = s->history_count;
            for (size_t i = 0; i < LINE_CAP; i++) {
                s->line[i] = '\0';
            }
            s->line_len = 0;
        }
        return;
    }
    if (key == 8 || key == 127) {                 // backspace
        if (s->line_len > 0) {
            s->line_len--;
            s->line[s->line_len] = '\0';
        }
        return;
    }
    if (key == 13 || key == 10) {                 // enter
        // NUL-terminate for strcmp / dispatch.
        s->line[s->line_len] = '\0';
        char submitted[LINE_CAP];
        for (size_t i = 0; i <= s->line_len; i++) {
            submitted[i] = s->line[i];
        }
        history_push(s);
        dispatch(s, submitted);
        // Clear the input line for the next command.
        for (size_t i = 0; i < s->line_len; i++) {
            s->line[i] = '\0';
        }
        s->line_len = 0;
        return;
    }
    if (key >= 32 && key <= 126) {
        if (s->line_len + 1 < LINE_CAP) {
            s->line[s->line_len++] = (char)key;
            s->line[s->line_len] = '\0';
        }
    }
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    static shell_state_t s;
    s.line_len = 0;
    s.history_count = 0;
    s.history_cursor = 0;
    s.last_spawned_pid = 0;
    for (size_t i = 0; i < LINE_CAP; i++) {
        s.line[i] = '\0';
        for (int h = 0; h < HISTORY_DEPTH; h++) {
            s.history[h][i] = '\0';
        }
    }
    display_clear(&s);

    write_cstr(1, "shell: starting\n");

    s.wid = win_create(WIN_X, WIN_Y, WIN_W, WIN_H, COLOR_BG, COLOR_BORDER);
    if (s.wid < 0) {
        write_cstr(1, "shell: window create failed\n");
        return 1;
    }
    (void)win_set_title(s.wid, "shell", TITLE_BAR_H);

    push_line(&s, "SHELL READY");
    redraw(&s);

    gui_event_t events[EVENT_CAP];
    for (;;) {
        long n = win_event(s.wid, events, EVENT_CAP);
        if (n > 0) {
            int dirty = 0;
            for (long i = 0; i < n; i++) {
                if (events[i].type == GUI_EVENT_CLOSE) {
                    (void)win_destroy(s.wid);
                    return 0;
                }
                if (events[i].type == GUI_EVENT_KEY_PRESS) {
                    handle_key(&s, events[i].data1);
                    dirty = 1;
                }
            }
            if (dirty) {
                redraw(&s);
            }
        } else {
            (void)kli_yield();
        }
    }
}
