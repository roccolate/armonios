// ArmoniOS app: panel (C version, on libkarm + libkarmdesk)
//
// Compact taskbar for the QEMU desktop. Four pinned application buttons share
// one row with an integrated uptime clock. A pinned button is both a launcher
// and a grouped task button:
//
//   - no window: launch the application;
//   - minimized window: restore it;
//   - visible window: focus it;
//   - multiple windows: repeated clicks cycle through the group.
//
// Running, focused, minimized, hover, and multi-window states are expressed by
// geometry rather than by a new color theme. The panel continues to rely on the
// kernel's built-in "panel" policy (DOCK, NO_FOCUS, NO_DRAG, SKIP_TASKBAR).

#include <stddef.h>
#include <stdint.h>

#include "libkarm/syscall.h"
#include "libkarmdesk/gui.h"
#include "panel_model.h"

#define REFRESH_PERIOD 20
#define LAUNCH_SETTLE_YIELDS 20
#define EVENT_CAP 8
#define PROCLIST_MAX 16
#define NAME_CAP 16
#define LABEL_CAP 12
#define PATH_CAP 32

/* Existing neutral palette. This change intentionally does not define a new
 * visual theme; state is conveyed through indicator shape and thickness. */
#define COLOR_BG 0xff202428U
#define COLOR_BORDER 0xff808080U
#define COLOR_EDGE 0xff506070U
#define COLOR_ITEM_BG 0xff3a4658U
#define COLOR_ITEM_HOVER 0xff5870a0U
#define COLOR_TEXT 0xffe0e8f0U
#define COLOR_MARK 0xffc0d0e0U

#define GUI_CURSOR_HAND 1U

typedef struct {
    uint32_t pid;
    uint32_t state;
    char name[NAME_CAP];
} proc_entry_t;

typedef struct {
    char process_name[NAME_CAP];
    char label[LABEL_CAP];
    char path[PATH_CAP];
    uint32_t pids[PANEL_APP_WINDOW_CAP];
    uint32_t window_ids[PANEL_APP_WINDOW_CAP];
    uint32_t window_states[PANEL_APP_WINDOW_CAP];
    int window_count;
} panel_app_t;

typedef struct {
    long wid;
    uint32_t panel_pid;
    int hover_target;
    int yield_counter;
    panel_app_t apps[PANEL_APP_COUNT];
    proc_entry_t procs[PROCLIST_MAX];
    uint64_t time_info[3];
    char clock_text[9];
    gui_event_t events[EVENT_CAP];
} panel_state_t;

static void copy_text(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;

    if (dst_size == 0) {
        return;
    }
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int text_equal(const char *left, const char *right) {
    size_t i = 0;

    while (left[i] != '\0' || right[i] != '\0') {
        if (left[i] != right[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

static int text_width(const char *text, int cap) {
    int count = 0;

    while (count < cap && text[count] != '\0') {
        count++;
    }
    return count * 8;
}

static int abs_to_panel_y(int32_t absolute_y) {
    return (int)absolute_y - PANEL_Y;
}

static void init_app(panel_app_t *app, const char *process_name,
                     const char *label, const char *path) {
    copy_text(app->process_name, sizeof(app->process_name), process_name);
    copy_text(app->label, sizeof(app->label), label);
    copy_text(app->path, sizeof(app->path), path);
    app->window_count = 0;
}

static void init_apps(panel_state_t *panel) {
    /* Keep literals in .rodata and copy them into mmap-backed state. Avoid a
     * static pointer table because the flat KLI1 image does not map .data. */
    init_app(&panel->apps[0], "shell", "Shell", "/armonios/shell");
    init_app(&panel->apps[1], "editor", "Editor", "/armonios/editor");
    init_app(&panel->apps[2], "files", "Files", "/armonios/files");
    init_app(&panel->apps[3], "monitor", "Monitor", "/armonios/monitor");
    init_app(&panel->apps[4], "clock", "Clock", "/armonios/clock");
}

static void clear_app_windows(panel_app_t *app) {
    app->window_count = 0;
    for (int i = 0; i < PANEL_APP_WINDOW_CAP; i++) {
        app->pids[i] = 0;
        app->window_ids[i] = 0;
        app->window_states[i] = 0;
    }
}

static void refresh_app_model(panel_state_t *panel) {
    for (int i = 0; i < PANEL_APP_COUNT; i++) {
        clear_app_windows(&panel->apps[i]);
    }

    long count = kli_proclist(panel->procs, PROCLIST_MAX);
    if (count <= 0) {
        return;
    }

    for (long proc_index = 0; proc_index < count; proc_index++) {
        uint32_t pid = panel->procs[proc_index].pid;
        if (pid == 0 || pid == panel->panel_pid) {
            continue;
        }

        for (int app_index = 0; app_index < PANEL_APP_COUNT; app_index++) {
            panel_app_t *app = &panel->apps[app_index];
            if (!text_equal(panel->procs[proc_index].name,
                            app->process_name) ||
                app->window_count >= PANEL_APP_WINDOW_CAP) {
                continue;
            }

            long window_id = gui_window_for_pid((long)pid, 0);
            if (window_id < 0) {
                continue;
            }

            uint32_t state = 0;
            if (gui_window_state(window_id, &state) < 0) {
                state = 0;
            }

            int slot = app->window_count++;
            app->pids[slot] = pid;
            app->window_ids[slot] = (uint32_t)window_id;
            app->window_states[slot] = state;
            break;
        }
    }
}

static void refresh_clock(panel_state_t *panel) {
    if (kli_timeinfo(panel->time_info) < 0) {
        copy_text(panel->clock_text, sizeof(panel->clock_text), "--:--:--");
        return;
    }
    panel_format_uptime(panel->time_info[0], panel->clock_text);
}

static void draw_instance_marks(panel_state_t *panel, const panel_app_t *app,
                                int x, int y, int width) {
    int count = app->window_count;
    if (count > PANEL_APP_WINDOW_CAP) {
        count = PANEL_APP_WINDOW_CAP;
    }

    for (int i = 0; i < count; i++) {
        int mark_x = x + width - 8 - i * 6;
        (void)gui_window_draw_rect(panel->wid, mark_x, y + 5, 3, 3,
                                   COLOR_MARK);
    }
}

static void draw_state_indicator(panel_state_t *panel,
                                 panel_visual_state_t state,
                                 int x, int y, int width, int height) {
    int center = x + width / 2;
    int baseline = y + height - 4;

    if (state == PANEL_VISUAL_RUNNING) {
        (void)gui_window_draw_rect(panel->wid, center - 12, baseline,
                                   24, 2, COLOR_MARK);
    } else if (state == PANEL_VISUAL_FOCUSED) {
        (void)gui_window_draw_rect(panel->wid, center - 28, baseline - 1,
                                   56, 3, COLOR_MARK);
        (void)gui_window_draw_rect(panel->wid, x + 2, y + 2,
                                   width - 4, 1, COLOR_MARK);
    } else if (state == PANEL_VISUAL_MINIMIZED) {
        (void)gui_window_draw_rect(panel->wid, center - 13, baseline,
                                   10, 2, COLOR_MARK);
        (void)gui_window_draw_rect(panel->wid, center + 3, baseline,
                                   10, 2, COLOR_MARK);
    }
}

static void draw_app_button(panel_state_t *panel, int app_index) {
    panel_app_t *app = &panel->apps[app_index];
    int x = panel_app_x(app_index);
    uint32_t background = panel->hover_target == app_index
                              ? COLOR_ITEM_HOVER
                              : COLOR_ITEM_BG;
    int label_width = text_width(app->label, LABEL_CAP);
    int label_x = x + (PANEL_APP_W - label_width) / 2;
    panel_visual_state_t state = panel_visual_state(
        app->window_states, app->window_count);

    (void)gui_window_draw_rect(panel->wid, x, PANEL_ITEM_Y,
                               PANEL_APP_W, PANEL_ITEM_H, background);
    (void)gui_window_draw_text(panel->wid, label_x, PANEL_ITEM_Y + 10,
                               COLOR_TEXT, app->label);
    draw_instance_marks(panel, app, x, PANEL_ITEM_Y, PANEL_APP_W);
    draw_state_indicator(panel, state, x, PANEL_ITEM_Y,
                         PANEL_APP_W, PANEL_ITEM_H);
}

static void draw_clock(panel_state_t *panel) {
    panel_app_t *clock_app = &panel->apps[PANEL_TARGET_CLOCK];
    uint32_t background = panel->hover_target == PANEL_TARGET_CLOCK
                              ? COLOR_ITEM_HOVER
                              : COLOR_ITEM_BG;
    panel_visual_state_t state = panel_visual_state(
        clock_app->window_states, clock_app->window_count);

    (void)gui_window_draw_rect(panel->wid, PANEL_CLOCK_X, PANEL_ITEM_Y,
                               PANEL_CLOCK_W, PANEL_ITEM_H, background);
    (void)gui_window_draw_text(panel->wid, PANEL_CLOCK_X + 8,
                               PANEL_ITEM_Y + 10, COLOR_TEXT,
                               panel->clock_text);
    draw_instance_marks(panel, clock_app, PANEL_CLOCK_X, PANEL_ITEM_Y,
                        PANEL_CLOCK_W);
    draw_state_indicator(panel, state, PANEL_CLOCK_X, PANEL_ITEM_Y,
                         PANEL_CLOCK_W, PANEL_ITEM_H);
}

static void draw_panel(panel_state_t *panel) {
    (void)gui_window_draw_rect(panel->wid, 0, 0,
                               PANEL_SCREEN_W, PANEL_HEIGHT, COLOR_BG);
    (void)gui_window_draw_rect(panel->wid, 0, 0,
                               PANEL_SCREEN_W, 1, COLOR_EDGE);

    for (int i = 0; i < PANEL_PINNED_COUNT; i++) {
        draw_app_button(panel, i);
    }

    (void)gui_window_draw_rect(panel->wid, PANEL_CLOCK_X - 8, 8,
                               1, PANEL_HEIGHT - 16, COLOR_EDGE);
    draw_clock(panel);
    (void)gui_window_flush(panel->wid, 0, 0,
                           PANEL_SCREEN_W, PANEL_HEIGHT);
}

static void redraw_target(panel_state_t *panel, int target) {
    if (target >= 0 && target < PANEL_PINNED_COUNT) {
        int x = panel_app_x(target);
        draw_app_button(panel, target);
        (void)gui_window_flush(panel->wid, x, PANEL_ITEM_Y,
                               PANEL_APP_W, PANEL_ITEM_H);
    } else if (target == PANEL_TARGET_CLOCK) {
        draw_clock(panel);
        (void)gui_window_flush(panel->wid, PANEL_CLOCK_X, PANEL_ITEM_Y,
                               PANEL_CLOCK_W, PANEL_ITEM_H);
    }
}

static void refresh_panel(panel_state_t *panel) {
    refresh_app_model(panel);
    refresh_clock(panel);
    draw_panel(panel);
}

static void activate_app(panel_state_t *panel, int app_index) {
    if (app_index < 0 || app_index >= PANEL_APP_COUNT) {
        return;
    }

    panel_app_t *app = &panel->apps[app_index];
    int target = panel_pick_window(app->window_states, app->window_count);
    if (target >= 0) {
        long window_id = (long)app->window_ids[target];
        kli_write_cstr(1, "panel: activate ");
        kli_write_cstr(1, app->process_name);
        kli_write_cstr(1, "\n");
        if ((app->window_states[target] &
             PANEL_WINDOW_STATE_MINIMIZED) != 0U) {
            (void)gui_window_restore(window_id);
        } else {
            (void)gui_window_focus(window_id);
        }
        refresh_panel(panel);
        return;
    }

    kli_write_cstr(1, "panel: launch ");
    kli_write_cstr(1, app->process_name);
    kli_write_cstr(1, "\n");
    if (kli_spawn(app->path, 0) < 0) {
        kli_write_cstr(1, "panel: launch failed\n");
        return;
    }

    for (int i = 0; i < LAUNCH_SETTLE_YIELDS; i++) {
        (void)kli_yield();
        refresh_app_model(panel);
        if (app->window_count > 0) {
            break;
        }
    }
    refresh_panel(panel);
}

static void on_click(panel_state_t *panel, int32_t absolute_x,
                     int32_t absolute_y) {
    int target = panel_target_at((int)absolute_x,
                                 abs_to_panel_y(absolute_y));
    if (target >= 0 && target < PANEL_PINNED_COUNT) {
        activate_app(panel, target);
    } else if (target == PANEL_TARGET_CLOCK) {
        activate_app(panel, PANEL_TARGET_CLOCK);
    }
}

static void on_move(panel_state_t *panel, int32_t absolute_x,
                    int32_t absolute_y) {
    int target = panel_target_at((int)absolute_x,
                                 abs_to_panel_y(absolute_y));
    if (target == panel->hover_target) {
        return;
    }

    int old_target = panel->hover_target;
    panel->hover_target = target;
    redraw_target(panel, old_target);
    redraw_target(panel, target);
}

static void register_cursor_regions(panel_state_t *panel) {
    for (int i = 0; i < PANEL_PINNED_COUNT; i++) {
        (void)gui_cursor_register_region(panel->wid, (long)i,
                                         (long)panel_app_x(i),
                                         PANEL_ITEM_Y, PANEL_APP_W,
                                         PANEL_ITEM_H, GUI_CURSOR_HAND);
    }
    (void)gui_cursor_register_region(panel->wid, PANEL_TARGET_CLOCK,
                                     PANEL_CLOCK_X, PANEL_ITEM_Y,
                                     PANEL_CLOCK_W, PANEL_ITEM_H,
                                     GUI_CURSOR_HAND);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    long state_address = kli_mmap(0, sizeof(panel_state_t), 0);
    if (state_address < 0) {
        kli_write_cstr(1, "panel: state mmap failed\n");
        return 1;
    }

    panel_state_t *panel = (panel_state_t *)(uintptr_t)state_address;
    panel->wid = 0;
    panel->panel_pid = 0;
    panel->hover_target = PANEL_TARGET_NONE;
    panel->yield_counter = 0;
    copy_text(panel->clock_text, sizeof(panel->clock_text), "--:--:--");
    init_apps(panel);

    kli_write_cstr(1, "panel: starting\n");
    panel->wid = gui_window_create(0, PANEL_Y, PANEL_SCREEN_W, PANEL_HEIGHT,
                                   COLOR_BG, COLOR_BORDER, "panel");
    if (panel->wid < 0) {
        kli_write_cstr(1, "panel: window create failed\n");
        for (;;) {
            (void)kli_yield();
        }
    }

    register_cursor_regions(panel);
    long pid = kli_getpid();
    panel->panel_pid = pid > 0 ? (uint32_t)pid : 0;
    refresh_panel(panel);
    kli_write_cstr(1, "panel: ready\n");

#ifdef PANEL_FORCE_FAULT
    kli_write_cstr(1, "panel: forced fault\n");
    *((volatile uint64_t *)(uintptr_t)0) = 0x50414e454c464c54ULL;
#endif

#ifdef PANEL_AUTO_TEST
    kli_write_cstr(1, "panel: auto-test launch every app\n");
    for (int app_index = 0; app_index < PANEL_APP_COUNT; app_index++) {
        kli_write_cstr(1, "panel: auto-test click ");
        kli_write_cstr(1, panel->apps[app_index].process_name);
        kli_write_cstr(1, "\n");
        activate_app(panel, app_index);
        (void)kli_yield();
    }
#endif

    for (;;) {
        long event_count = gui_window_event(panel->wid, panel->events,
                                            EVENT_CAP);
        if (event_count > 0) {
            for (long i = 0; i < event_count; i++) {
                if (panel->events[i].type == GUI_EVENT_MOUSE_CLICK) {
                    on_click(panel, panel->events[i].data1,
                             panel->events[i].data2);
                } else if (panel->events[i].type == GUI_EVENT_MOUSE_MOVE) {
                    on_move(panel, panel->events[i].data1,
                            panel->events[i].data2);
                }
            }
        }

        panel->yield_counter++;
        if (panel->yield_counter >= REFRESH_PERIOD) {
            panel->yield_counter = 0;
            refresh_panel(panel);
        }
        (void)kli_yield();
    }
}
