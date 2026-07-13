#ifndef ARMONIOS_PROGRAMS_APPS_PANEL_MODEL_H
#define ARMONIOS_PROGRAMS_APPS_PANEL_MODEL_H

#include <stdint.h>

#define PANEL_SCREEN_W 640
#define PANEL_SCREEN_H 480
#define PANEL_HEIGHT 40
#define PANEL_Y (PANEL_SCREEN_H - PANEL_HEIGHT)

#define PANEL_APP_COUNT 5
#define PANEL_PINNED_COUNT 4
#define PANEL_APP_WINDOW_CAP 4

#define PANEL_ITEM_Y 4
#define PANEL_ITEM_H 32
#define PANEL_APP_X 8
#define PANEL_APP_W 104
#define PANEL_APP_GAP 4
#define PANEL_CLOCK_X 552
#define PANEL_CLOCK_W 80

#define PANEL_TARGET_NONE (-1)
#define PANEL_TARGET_CLOCK PANEL_PINNED_COUNT

#define PANEL_TICKS_PER_SECOND 100ULL

#define PANEL_WINDOW_STATE_MINIMIZED 0x1U
#define PANEL_WINDOW_STATE_FOCUSED 0x2U

typedef enum {
    PANEL_VISUAL_CLOSED = 0,
    PANEL_VISUAL_RUNNING,
    PANEL_VISUAL_FOCUSED,
    PANEL_VISUAL_MINIMIZED,
} panel_visual_state_t;

static inline int panel_app_x(int index) {
    return PANEL_APP_X + index * (PANEL_APP_W + PANEL_APP_GAP);
}

static inline int panel_target_at(int x, int y) {
    if (y < PANEL_ITEM_Y || y >= PANEL_ITEM_Y + PANEL_ITEM_H) {
        return PANEL_TARGET_NONE;
    }

    for (int i = 0; i < PANEL_PINNED_COUNT; i++) {
        int left = panel_app_x(i);
        if (x >= left && x < left + PANEL_APP_W) {
            return i;
        }
    }

    if (x >= PANEL_CLOCK_X && x < PANEL_CLOCK_X + PANEL_CLOCK_W) {
        return PANEL_TARGET_CLOCK;
    }

    return PANEL_TARGET_NONE;
}

static inline panel_visual_state_t panel_visual_state(const uint32_t *states,
                                                       int count) {
    int any_visible = 0;

    if (states == 0 || count <= 0) {
        return PANEL_VISUAL_CLOSED;
    }

    for (int i = 0; i < count; i++) {
        if ((states[i] & PANEL_WINDOW_STATE_FOCUSED) != 0U) {
            return PANEL_VISUAL_FOCUSED;
        }
        if ((states[i] & PANEL_WINDOW_STATE_MINIMIZED) == 0U) {
            any_visible = 1;
        }
    }

    return any_visible != 0 ? PANEL_VISUAL_RUNNING : PANEL_VISUAL_MINIMIZED;
}

static inline panel_visual_state_t panel_app_visual_state(
    const uint32_t *states, int window_count, int process_count) {
    if (window_count > 0) {
        return panel_visual_state(states, window_count);
    }
    return process_count > 0 ? PANEL_VISUAL_RUNNING : PANEL_VISUAL_CLOSED;
}

static inline int panel_pick_window(const uint32_t *states, int count) {
    if (states == 0 || count <= 0) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if ((states[i] & PANEL_WINDOW_STATE_FOCUSED) != 0U) {
            return (i + 1) % count;
        }
    }

    for (int i = 0; i < count; i++) {
        if ((states[i] & PANEL_WINDOW_STATE_MINIMIZED) == 0U) {
            return i;
        }
    }

    return 0;
}

static inline void panel_format_uptime(uint64_t ticks, char out[9]) {
    uint64_t total_seconds = ticks / PANEL_TICKS_PER_SECOND;
    uint64_t hours = (total_seconds / 3600ULL) % 100ULL;
    uint64_t minutes = (total_seconds / 60ULL) % 60ULL;
    uint64_t seconds = total_seconds % 60ULL;

    out[0] = (char)('0' + (hours / 10ULL));
    out[1] = (char)('0' + (hours % 10ULL));
    out[2] = ':';
    out[3] = (char)('0' + (minutes / 10ULL));
    out[4] = (char)('0' + (minutes % 10ULL));
    out[5] = ':';
    out[6] = (char)('0' + (seconds / 10ULL));
    out[7] = (char)('0' + (seconds % 10ULL));
    out[8] = '\0';
}

_Static_assert(PANEL_APP_X + PANEL_PINNED_COUNT * PANEL_APP_W +
                   (PANEL_PINNED_COUNT - 1) * PANEL_APP_GAP <= PANEL_CLOCK_X,
               "panel app buttons overlap clock area");
_Static_assert(PANEL_CLOCK_X + PANEL_CLOCK_W <= PANEL_SCREEN_W,
               "panel clock exceeds screen width");

#endif
