// ArmoniOS app: clock (C version, on libkarm + libarmdesk)
//
// Creates a window that displays the current uptime as HH:MM:SS, reads
// timer ticks via SYS_TIMEINFO, and redraws once per second using a
// yield-based delay. 'q' closes the app; a click on the kernel-drawn
// close box fires GUI_EVENT_CLOSE and exits cleanly. Runtime state lives in
// one libkarm arena backed by a single anonymous mapping.

#include <stdint.h>

#include "libarmdesk/gui.h"
#include "libkarm/arena.h"
#include "libkarm/syscall.h"

#define WIN_W            200
#define WIN_H             80
#define TITLE_BAR_H       16
#define EVENT_CAP          4
#define YIELDS_PER_SEC   200
#define CLOCK_ARENA_SIZE 4096U

typedef struct {
    long wid;
    int wait;
    arm_timeinfo_t info;
    char text[9];
    gui_event_t events[EVENT_CAP];
} clock_state_t;

static void format_hhmmss(uint64_t ticks, char *out) {
    // 100 Hz timer: seconds = ticks / 100.
    uint64_t total_seconds = ticks / 100ULL;
    uint64_t hh = total_seconds / 3600ULL;
    uint64_t mm = (total_seconds / 60ULL) % 60ULL;
    uint64_t ss = total_seconds % 60ULL;
    out[0] = (char)('0' + (int)(hh / 10U));
    out[1] = (char)('0' + (int)(hh % 10U));
    out[2] = ':';
    out[3] = (char)('0' + (int)(mm / 10U));
    out[4] = (char)('0' + (int)(mm % 10U));
    out[5] = ':';
    out[6] = (char)('0' + (int)(ss / 10U));
    out[7] = (char)('0' + (int)(ss % 10U));
    out[8] = '\0';
}

static void redraw(clock_state_t *state) {
    if (kli_timeinfo_v1(&state->info) < 0) {
        return;
    }
    format_hhmmss(state->info.timer_ticks, state->text);

    (void)gui_window_draw_rect(state->wid, 20, 30, WIN_W - 40, 28,
                               0xff202830ULL);
    (void)gui_window_draw_text(state->wid, 40, 34, 0xffe0e8f0ULL,
                               state->text);
    (void)gui_window_flush(state->wid, 0, 0, WIN_W, WIN_H - TITLE_BAR_H);
}

static int close_clock(kli_arena_t *arena, clock_state_t *state) {
    (void)gui_window_destroy(state->wid);
    (void)kli_arena_destroy(arena);
    return 0;
}

int main(int argc, char **argv) {
    kli_arena_t arena;
    clock_state_t *state;

    (void)argc;
    (void)argv;

    kli_write_cstr(ARM_FD_STDOUT, "clock: starting\n");

    if (kli_arena_init(&arena, CLOCK_ARENA_SIZE) < 0) {
        kli_write_cstr(ARM_FD_STDOUT, "clock: arena mmap failed\n");
        return 1;
    }

    state = (clock_state_t *)kli_arena_alloc_zero(&arena, sizeof(*state));
    if (state == 0) {
        kli_write_cstr(ARM_FD_STDOUT, "clock: arena allocation failed\n");
        (void)kli_arena_destroy(&arena);
        return 1;
    }

    state->wid = gui_window_create(440, 64, WIN_W, WIN_H,
                                   0xff202830LL, 0xff808080LL, "clock");
    if (state->wid < 0) {
        kli_write_cstr(ARM_FD_STDOUT, "clock: window create failed\n");
        (void)kli_arena_destroy(&arena);
        return 1;
    }
    (void)gui_window_set_title(state->wid, "clock", TITLE_BAR_H);

    state->wait = YIELDS_PER_SEC;
    redraw(state);

    for (;;) {
        // Drain pending events every yield so close / q are responsive.
        long n = gui_window_event(state->wid, state->events, EVENT_CAP);
        if (n > 0) {
            for (long i = 0; i < n; i++) {
                if (state->events[i].type == GUI_EVENT_CLOSE) {
                    return close_clock(&arena, state);
                }
                if (state->events[i].type == GUI_EVENT_KEY_PRESS &&
                    (state->events[i].data1 == 'q' ||
                     state->events[i].data1 == 'Q')) {
                    return close_clock(&arena, state);
                }
            }
        }

        state->wait--;
        if (state->wait <= 0) {
            redraw(state);
            state->wait = YIELDS_PER_SEC;
        }

        (void)kli_yield();
    }
}
