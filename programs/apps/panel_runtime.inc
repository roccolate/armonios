// ArmoniOS panel runtime implementation. Included once by panel.c.

#include <stddef.h>
#include <stdint.h>

#include "include/armonios/app_catalog.h"
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

#define COLOR_BG 0xff202428U
#define COLOR_BORDER 0xff808080U
#define COLOR_EDGE 0xff506070U
#define COLOR_ITEM_BG 0xff3a4658U
#define COLOR_TEXT 0xffe0e8f0U
#define COLOR_MARK 0xffc0d0e0U
#define COLOR_MENU_BG 0xff202428U
#define COLOR_MENU_HEADER 0xff182028U
#define COLOR_MENU_SELECT 0xff3a4658U
#define COLOR_MENU_HOVER 0xff506070U

#define GUI_CURSOR_HAND 1U
#define INPUT_KEY_UP 0x101
#define INPUT_KEY_DOWN 0x102
#define PROCESS_STATE_ZOMBIE 4U

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
    int process_count;
} panel_app_t;

#define PANEL_APP_INDEX_ENUM(id, label, path, pinned, is_clock) \
    PANEL_APP_INDEX_##id,
enum {
    ARMONIOS_APP_CATALOG(PANEL_APP_INDEX_ENUM)
    PANEL_APP_INDEX_COUNT
};
#undef PANEL_APP_INDEX_ENUM

#define PANEL_COUNT_APP(id, label, path, pinned, is_clock) + 1
#define PANEL_COUNT_PINNED(id, label, path, pinned, is_clock) + pinned
#define PANEL_COUNT_CLOCK(id, label, path, pinned, is_clock) + is_clock
#define PANEL_CLOCK_INDEX_TERM(id, label, path, pinned, is_clock) \
    + ((is_clock) ? PANEL_APP_INDEX_##id : 0)
enum {
    PANEL_CATALOG_APP_COUNT = 0 ARMONIOS_APP_CATALOG(PANEL_COUNT_APP),
    PANEL_CATALOG_PINNED_COUNT = 0 ARMONIOS_APP_CATALOG(PANEL_COUNT_PINNED),
    PANEL_CATALOG_CLOCK_COUNT = 0 ARMONIOS_APP_CATALOG(PANEL_COUNT_CLOCK),
    PANEL_CLOCK_APP_INDEX = 0 ARMONIOS_APP_CATALOG(PANEL_CLOCK_INDEX_TERM),
};
#undef PANEL_COUNT_APP
#undef PANEL_COUNT_PINNED
#undef PANEL_COUNT_CLOCK
#undef PANEL_CLOCK_INDEX_TERM

_Static_assert(PANEL_CATALOG_APP_COUNT == PANEL_APP_COUNT,
               "panel model app count must match app catalog");
_Static_assert(PANEL_CATALOG_PINNED_COUNT == PANEL_PINNED_COUNT,
               "panel pinned count must match app catalog");
_Static_assert(PANEL_CATALOG_CLOCK_COUNT == 1,
               "panel catalog must mark exactly one clock app");
_Static_assert(PANEL_CLOCK_APP_INDEX < PANEL_APP_COUNT,
               "panel clock app index must be valid");

typedef struct {
    long wid;
    long menu_wid;
    uint32_t panel_pid;
    uint32_t menu_return_window;
    int menu_return_valid;
    int menu_hover;
    int menu_selected;
    int yield_counter;
    uint32_t app_signatures[PANEL_APP_COUNT];
    panel_app_t apps[PANEL_APP_COUNT];
    proc_entry_t procs[PROCLIST_MAX];
    uint64_t time_info[3];
    char clock_text[9];
    gui_event_t events[EVENT_CAP];
    gui_event_t menu_events[EVENT_CAP];
} panel_state_t;

static void refresh_panel(panel_state_t *panel);
static void refresh_panel_full(panel_state_t *panel);
static void refresh_panel_tick(panel_state_t *panel);
static void activate_app(panel_state_t *panel, int app_index);

static void copy_text(char *dst, size_t dst_size, const char *src) {
    size_t i = 0;
    if (dst_size == 0) return;
    while (i + 1 < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static int text_equal(const char *left, const char *right) {
    size_t i = 0;
    while (left[i] != '\0' || right[i] != '\0') {
        if (left[i] != right[i]) return 0;
        i++;
    }
    return 1;
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
    app->process_count = 0;
}

static void init_apps(panel_state_t *panel) {
#define PANEL_INIT_APP(id, label, path, pinned, is_clock) \
    init_app(&panel->apps[PANEL_APP_INDEX_##id], #id, label, path);
    ARMONIOS_APP_CATALOG(PANEL_INIT_APP)
#undef PANEL_INIT_APP
}

static int panel_pinned_app_index(int pinned_slot) {
    int seen = 0;

    if (pinned_slot < 0 || pinned_slot >= PANEL_PINNED_COUNT) {
        return -1;
    }

#define PANEL_PINNED_MATCH(id, label, path, pinned, is_clock) \
    if (pinned) { \
        if (seen == pinned_slot) { \
            return PANEL_APP_INDEX_##id; \
        } \
        seen++; \
    }
    ARMONIOS_APP_CATALOG(PANEL_PINNED_MATCH)
#undef PANEL_PINNED_MATCH
    return -1;
}

static void clear_app_windows(panel_app_t *app) {
    app->window_count = 0;
    app->process_count = 0;
    for (int i = 0; i < PANEL_APP_WINDOW_CAP; i++) {
        app->pids[i] = 0;
        app->window_ids[i] = 0;
        app->window_states[i] = 0;
    }
}

static void refresh_app_model(panel_state_t *panel) {
    for (int i = 0; i < PANEL_APP_COUNT; i++) clear_app_windows(&panel->apps[i]);
    long count = kli_proclist(panel->procs, PROCLIST_MAX);
    if (count <= 0) return;

    for (long proc_index = 0; proc_index < count; proc_index++) {
        uint32_t pid = panel->procs[proc_index].pid;
        if (pid == 0 || pid == panel->panel_pid) continue;
        if (panel->procs[proc_index].state == PROCESS_STATE_ZOMBIE) continue;
        for (int app_index = 0; app_index < PANEL_APP_COUNT; app_index++) {
            panel_app_t *app = &panel->apps[app_index];
            if (!text_equal(panel->procs[proc_index].name, app->process_name)) continue;
            app->process_count++;
            if (app->window_count >= PANEL_APP_WINDOW_CAP) break;
            long window_id = gui_window_for_pid((long)pid, 0);
            if (window_id < 0) break;
            uint32_t state = 0;
            if (gui_window_state(window_id, &state) < 0) state = 0;
            int slot = app->window_count++;
            app->pids[slot] = pid;
            app->window_ids[slot] = (uint32_t)window_id;
            app->window_states[slot] = state;
            break;
        }
    }
}

static uint32_t panel_app_signature(const panel_app_t *app) {
    uint32_t sig = ((uint32_t)app->process_count << 16) |
                   ((uint32_t)app->window_count & 0xffffU);
    for (int i = 0; i < PANEL_APP_WINDOW_CAP; i++) {
        sig = (sig * 33U) ^ app->window_ids[i];
        sig = (sig * 33U) ^ app->window_states[i];
    }
    return sig;
}

static uint32_t refresh_app_model_changed(panel_state_t *panel) {
    uint32_t changed = 0;
    refresh_app_model(panel);
    for (int i = 0; i < PANEL_APP_COUNT; i++) {
        uint32_t sig = panel_app_signature(&panel->apps[i]);
        if (sig != panel->app_signatures[i]) {
            panel->app_signatures[i] = sig;
            changed |= 1U << (uint32_t)i;
        }
    }
    return changed;
}

static int refresh_clock(panel_state_t *panel) {
    char next[9];
    if (kli_timeinfo(panel->time_info) < 0) {
        copy_text(next, sizeof(next), "--:--:--");
    } else {
        panel_format_uptime(panel->time_info[0], next);
    }
    if (text_equal(panel->clock_text, next)) {
        return 0;
    }
    copy_text(panel->clock_text, sizeof(panel->clock_text), next);
    return 1;
}

static void draw_instance_marks(panel_state_t *panel, const panel_app_t *app,
                                int x, int y, int width) {
    int count = app->window_count > PANEL_APP_WINDOW_CAP
                    ? PANEL_APP_WINDOW_CAP : app->window_count;
    for (int i = 0; i < count; i++) {
        int mark_x = x + width - 8 - i * 6;
        (void)gui_window_draw_rect(panel->wid, mark_x, y + 5, 3, 3, COLOR_MARK);
    }
}

static void draw_state_indicator(panel_state_t *panel,
                                 panel_visual_state_t state,
                                 int x, int y, int width, int height) {
    int center = x + width / 2;
    int baseline = y + height - 4;
    int inset = width > 16 ? 6 : 2;
    int run_w = width > 12 ? width - 12 : width;
    int focus_w = width > 12 ? width - 12 : width;
    int mini_w = width > 24 ? 8 : 4;
    if (run_w > 24) run_w = 24;
    if (state == PANEL_VISUAL_RUNNING) {
        (void)gui_window_draw_rect(panel->wid, center - run_w / 2,
                                   baseline, run_w, 2, COLOR_MARK);
    } else if (state == PANEL_VISUAL_FOCUSED) {
        (void)gui_window_draw_rect(panel->wid, center - focus_w / 2,
                                   baseline - 1, focus_w, 3, COLOR_MARK);
        (void)gui_window_draw_rect(panel->wid, x + inset, y + 2,
                                   width - 2 * inset, 1, COLOR_MARK);
    } else if (state == PANEL_VISUAL_MINIMIZED) {
        (void)gui_window_draw_rect(panel->wid, center - mini_w - 2,
                                   baseline, mini_w, 2, COLOR_MARK);
        (void)gui_window_draw_rect(panel->wid, center + 2,
                                   baseline, mini_w, 2, COLOR_MARK);
    }
}

static void draw_menu_button(panel_state_t *panel) {
    (void)gui_window_draw_rect(panel->wid, PANEL_MENU_BUTTON_X, PANEL_ITEM_Y,
                               PANEL_MENU_BUTTON_W, PANEL_ITEM_H, COLOR_ITEM_BG);
    (void)gui_window_draw_text(panel->wid, PANEL_MENU_BUTTON_X + 12,
                               PANEL_ITEM_Y + 10, COLOR_TEXT, "Apps");
    if (panel->menu_wid >= 0) {
        (void)gui_window_draw_rect(panel->wid, PANEL_MENU_BUTTON_X + 8,
                                   PANEL_ITEM_Y + PANEL_ITEM_H - 4,
                                   PANEL_MENU_BUTTON_W - 16, 3, COLOR_MARK);
    }
}

static const char *app_icon(int app_index) {
    if (app_index == 0) return ">";
    if (app_index == 1) return "E";
    if (app_index == 2) return "F";
    if (app_index == 3) return "M";
    return "?";
}

static void draw_app_icon(panel_state_t *panel, int app_index, int x) {
    (void)gui_window_draw_text(panel->wid, x + 14, PANEL_ITEM_Y + 10,
                               COLOR_TEXT, app_icon(app_index));
}

static void draw_app_button(panel_state_t *panel, int pinned_slot) {
    int app_index = panel_pinned_app_index(pinned_slot);
    if (app_index < 0) return;
    panel_app_t *app = &panel->apps[app_index];
    int x = panel_app_x(pinned_slot);
    panel_visual_state_t state = panel_app_visual_state(
        app->window_states, app->window_count, app->process_count);
    (void)gui_window_draw_rect(panel->wid, x, PANEL_ITEM_Y,
                               PANEL_APP_W, PANEL_ITEM_H, COLOR_ITEM_BG);
    draw_app_icon(panel, app_index, x);
    draw_instance_marks(panel, app, x, PANEL_ITEM_Y, PANEL_APP_W);
    draw_state_indicator(panel, state, x, PANEL_ITEM_Y, PANEL_APP_W, PANEL_ITEM_H);
}

static void draw_clock(panel_state_t *panel) {
    panel_app_t *clock_app = &panel->apps[PANEL_CLOCK_APP_INDEX];
    panel_visual_state_t state = panel_app_visual_state(
        clock_app->window_states, clock_app->window_count,
        clock_app->process_count);
    (void)gui_window_draw_rect(panel->wid, PANEL_CLOCK_X, PANEL_ITEM_Y,
                               PANEL_CLOCK_W, PANEL_ITEM_H, COLOR_ITEM_BG);
    (void)gui_window_draw_text(panel->wid, PANEL_CLOCK_X + 8,
                               PANEL_ITEM_Y + 10, COLOR_TEXT, panel->clock_text);
    draw_instance_marks(panel, clock_app, PANEL_CLOCK_X, PANEL_ITEM_Y, PANEL_CLOCK_W);
    draw_state_indicator(panel, state, PANEL_CLOCK_X, PANEL_ITEM_Y,
                         PANEL_CLOCK_W, PANEL_ITEM_H);
}

static void draw_panel(panel_state_t *panel) {
    (void)gui_window_draw_rect(panel->wid, 0, 0, PANEL_SCREEN_W, PANEL_HEIGHT, COLOR_BG);
    (void)gui_window_draw_rect(panel->wid, 0, 0, PANEL_SCREEN_W, 1, COLOR_EDGE);
    draw_menu_button(panel);
    for (int i = 0; i < PANEL_PINNED_COUNT; i++) draw_app_button(panel, i);
    (void)gui_window_draw_rect(panel->wid, PANEL_CLOCK_X - 8, 8,
                               1, PANEL_HEIGHT - 16, COLOR_EDGE);
    draw_clock(panel);
    (void)gui_window_flush(panel->wid, 0, 0, PANEL_SCREEN_W, PANEL_HEIGHT);
}

static void redraw_target(panel_state_t *panel, int target) {
    if (target == PANEL_TARGET_MENU) {
        draw_menu_button(panel);
        (void)gui_window_flush(panel->wid, PANEL_MENU_BUTTON_X, PANEL_ITEM_Y,
                               PANEL_MENU_BUTTON_W, PANEL_ITEM_H);
    } else if (target >= 0 && target < PANEL_PINNED_COUNT) {
        int x = panel_app_x(target);
        draw_app_button(panel, target);
        (void)gui_window_flush(panel->wid, x, PANEL_ITEM_Y, PANEL_APP_W, PANEL_ITEM_H);
    } else if (target == PANEL_TARGET_CLOCK) {
        draw_clock(panel);
        (void)gui_window_flush(panel->wid, PANEL_CLOCK_X, PANEL_ITEM_Y,
                               PANEL_CLOCK_W, PANEL_ITEM_H);
    }
}

static uint32_t focused_app_window(const panel_state_t *panel, int *valid) {
    for (int app_index = 0; app_index < PANEL_APP_COUNT; app_index++) {
        const panel_app_t *app = &panel->apps[app_index];
        for (int i = 0; i < app->window_count; i++) {
            if ((app->window_states[i] & PANEL_WINDOW_STATE_FOCUSED) != 0U) {
                *valid = 1;
                return app->window_ids[i];
            }
        }
    }
    *valid = 0;
    return 0;
}

static void draw_menu_item(panel_state_t *panel, int app_index) {
    int y = panel_menu_item_y(app_index);
    uint32_t background = COLOR_MENU_BG;
    if (panel->menu_hover == app_index) background = COLOR_MENU_HOVER;
    else if (panel->menu_selected == app_index) background = COLOR_MENU_SELECT;
    (void)gui_window_draw_rect(panel->menu_wid, PANEL_MENU_ITEM_X, y,
                               PANEL_MENU_ITEM_W, PANEL_MENU_ITEM_H, background);
    (void)gui_window_draw_text(panel->menu_wid, PANEL_MENU_ITEM_X + 12,
                               y + 7, COLOR_TEXT, panel->apps[app_index].label);
    panel_visual_state_t state = panel_app_visual_state(
        panel->apps[app_index].window_states,
        panel->apps[app_index].window_count,
        panel->apps[app_index].process_count);
    if (state != PANEL_VISUAL_CLOSED) {
        int marker_w = state == PANEL_VISUAL_FOCUSED ? 4 : 2;
        (void)gui_window_draw_rect(panel->menu_wid,
                                   PANEL_MENU_ITEM_X + 2, y + 4,
                                   marker_w, PANEL_MENU_ITEM_H - 8, COLOR_MARK);
    }
}

static void draw_menu(panel_state_t *panel) {
    if (panel->menu_wid < 0) return;
    (void)gui_window_draw_rect(panel->menu_wid, 0, 0,
                               PANEL_MENU_W, PANEL_MENU_H, COLOR_MENU_BG);
    (void)gui_window_draw_rect(panel->menu_wid, 0, 0,
                               PANEL_MENU_W, 28, COLOR_MENU_HEADER);
    (void)gui_window_draw_text(panel->menu_wid, 12, 10,
                               COLOR_TEXT, "APPLICATIONS");
    for (int i = 0; i < PANEL_APP_COUNT; i++) draw_menu_item(panel, i);
    (void)gui_window_flush(panel->menu_wid, 0, 0, PANEL_MENU_W, PANEL_MENU_H);
}

static void close_menu(panel_state_t *panel, int restore_previous) {
    if (panel->menu_wid < 0) return;
    long old_menu = panel->menu_wid;
    panel->menu_wid = -1;
    (void)gui_window_destroy(old_menu);
    if (restore_previous != 0 && panel->menu_return_valid != 0) {
        (void)gui_window_focus((long)panel->menu_return_window);
    }
    panel->menu_return_valid = 0;
    panel->menu_hover = -1;
    redraw_target(panel, PANEL_TARGET_MENU);
}

static void open_menu(panel_state_t *panel) {
    if (panel->menu_wid >= 0) return;
    refresh_app_model(panel);
    panel->menu_return_window = focused_app_window(panel, &panel->menu_return_valid);
    panel->menu_hover = -1;
    panel->menu_selected = 0;
    panel->menu_wid = gui_window_create(
        PANEL_MENU_X, PANEL_MENU_Y, PANEL_MENU_W, PANEL_MENU_H,
        COLOR_MENU_BG, COLOR_BORDER, "panel-menu");
    if (panel->menu_wid < 0) {
        panel->menu_return_valid = 0;
        return;
    }
    for (int i = 0; i < PANEL_APP_COUNT; i++) {
        (void)gui_cursor_register_region(panel->menu_wid, i,
                                         PANEL_MENU_ITEM_X,
                                         panel_menu_item_y(i),
                                         PANEL_MENU_ITEM_W,
                                         PANEL_MENU_ITEM_H,
                                         GUI_CURSOR_HAND);
    }
    draw_menu(panel);
    redraw_target(panel, PANEL_TARGET_MENU);
}

static void toggle_menu(panel_state_t *panel) {
    if (panel->menu_wid >= 0) close_menu(panel, 1);
    else open_menu(panel);
}

static void refresh_panel_full(panel_state_t *panel) {
    (void)refresh_app_model_changed(panel);
    refresh_clock(panel);
    draw_panel(panel);
    if (panel->menu_wid >= 0) draw_menu(panel);
}

static void refresh_panel(panel_state_t *panel) {
    (void)refresh_app_model_changed(panel);
    refresh_clock(panel);

    redraw_target(panel, PANEL_TARGET_MENU);
    for (int i = 0; i < PANEL_PINNED_COUNT; i++) redraw_target(panel, i);
    redraw_target(panel, PANEL_TARGET_CLOCK);
    if (panel->menu_wid >= 0) draw_menu(panel);
}

static void refresh_panel_tick(panel_state_t *panel) {
    uint32_t changed = refresh_app_model_changed(panel);
    for (int i = 0; i < PANEL_PINNED_COUNT; i++) {
        if ((changed & (1U << (uint32_t)i)) != 0U) {
            redraw_target(panel, i);
        }
    }
    if (refresh_clock(panel) != 0) {
        redraw_target(panel, PANEL_TARGET_CLOCK);
    } else if ((changed & (1U << (uint32_t)PANEL_CLOCK_APP_INDEX)) != 0U) {
        redraw_target(panel, PANEL_TARGET_CLOCK);
    }
    if (changed != 0U && panel->menu_wid >= 0) {
        draw_menu(panel);
    }
}

static void activate_app(panel_state_t *panel, int app_index) {
    if (app_index < 0 || app_index >= PANEL_APP_COUNT) return;
    (void)refresh_app_model_changed(panel);
    panel_app_t *app = &panel->apps[app_index];
    panel_activation_action_t action = panel_activation_action(
        app->window_states, app->window_count, app->process_count);
    int target = panel_pick_window(app->window_states, app->window_count);

    if (action == PANEL_ACTION_MINIMIZE && app->window_count == 1) {
        (void)gui_window_minimize((long)app->window_ids[0]);
        refresh_panel(panel);
        return;
    }
    if ((action == PANEL_ACTION_FOCUS || action == PANEL_ACTION_RESTORE) && target >= 0) {
        long window_id = (long)app->window_ids[target];
        if (action == PANEL_ACTION_RESTORE) (void)gui_window_restore(window_id);
        else (void)gui_window_focus(window_id);
        refresh_panel(panel);
        return;
    }
    if (action == PANEL_ACTION_WAIT) {
        kli_write_cstr(1, "panel: launch pending ");
        kli_write_cstr(1, app->process_name);
        kli_write_cstr(1, "\n");
        return;
    }
    if (action != PANEL_ACTION_LAUNCH) return;

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
        if (app->window_count > 0) break;
    }
    refresh_panel(panel);
}

static void on_click(panel_state_t *panel, int32_t absolute_x,
                     int32_t absolute_y) {
    int target = panel_target_at((int)absolute_x, abs_to_panel_y(absolute_y));
    if (target == PANEL_TARGET_MENU) {
        toggle_menu(panel);
        return;
    }
    if (panel->menu_wid >= 0) close_menu(panel, 0);
    if (target >= 0 && target < PANEL_PINNED_COUNT) {
        activate_app(panel, panel_pinned_app_index(target));
    } else if (target == PANEL_TARGET_CLOCK) {
        activate_app(panel, PANEL_CLOCK_APP_INDEX);
    }
}

static void on_move(panel_state_t *panel, int32_t absolute_x,
                    int32_t absolute_y) {
    (void)panel;
    (void)absolute_x;
    (void)absolute_y;
}

static void menu_choose(panel_state_t *panel, int app_index) {
    close_menu(panel, 0);
    activate_app(panel, app_index);
}

static void on_menu_click(panel_state_t *panel, int32_t absolute_x,
                          int32_t absolute_y) {
    int item = panel_menu_item_at((int)absolute_x - PANEL_MENU_X,
                                  (int)absolute_y - PANEL_MENU_Y);
    if (item >= 0) menu_choose(panel, item);
}

static void on_menu_move(panel_state_t *panel, int32_t absolute_x,
                         int32_t absolute_y) {
    int item = panel_menu_item_at((int)absolute_x - PANEL_MENU_X,
                                  (int)absolute_y - PANEL_MENU_Y);
    if (item == panel->menu_hover) return;
    panel->menu_hover = item;
    if (item >= 0) panel->menu_selected = item;
    draw_menu(panel);
}

static void on_menu_key(panel_state_t *panel, int key) {
    if (key == 27) {
        close_menu(panel, 1);
    } else if (key == INPUT_KEY_UP) {
        panel->menu_selected =
            (panel->menu_selected + PANEL_APP_COUNT - 1) % PANEL_APP_COUNT;
        panel->menu_hover = -1;
        draw_menu(panel);
    } else if (key == INPUT_KEY_DOWN) {
        panel->menu_selected = (panel->menu_selected + 1) % PANEL_APP_COUNT;
        panel->menu_hover = -1;
        draw_menu(panel);
    } else if (key == 13 || key == 10) {
        menu_choose(panel, panel->menu_selected);
    }
}

static void poll_menu(panel_state_t *panel) {
    if (panel->menu_wid < 0) return;
    long count = gui_window_event(panel->menu_wid, panel->menu_events, EVENT_CAP);
    if (count > 0) {
        for (long i = 0; i < count && panel->menu_wid >= 0; i++) {
            if (panel->menu_events[i].type == GUI_EVENT_MOUSE_CLICK) {
                on_menu_click(panel, panel->menu_events[i].data1,
                              panel->menu_events[i].data2);
            } else if (panel->menu_events[i].type == GUI_EVENT_MOUSE_MOVE) {
                on_menu_move(panel, panel->menu_events[i].data1,
                             panel->menu_events[i].data2);
            } else if (panel->menu_events[i].type == GUI_EVENT_KEY_PRESS) {
                on_menu_key(panel, panel->menu_events[i].data1);
            }
        }
    }
    if (panel->menu_wid >= 0) {
        uint32_t state = 0;
        if (gui_window_state(panel->menu_wid, &state) < 0 ||
            (state & GUI_WINDOW_STATE_FOCUSED) == 0U) {
            close_menu(panel, 0);
        }
    }
}

static void register_cursor_regions(panel_state_t *panel) {
    (void)gui_cursor_register_region(panel->wid, 0,
                                     PANEL_MENU_BUTTON_X, PANEL_ITEM_Y,
                                     PANEL_MENU_BUTTON_W, PANEL_ITEM_H,
                                     GUI_CURSOR_HAND);
    for (int i = 0; i < PANEL_PINNED_COUNT; i++) {
        (void)gui_cursor_register_region(panel->wid, i + 1,
                                         (long)panel_app_x(i),
                                         PANEL_ITEM_Y, PANEL_APP_W,
                                         PANEL_ITEM_H, GUI_CURSOR_HAND);
    }
    (void)gui_cursor_register_region(panel->wid, PANEL_PINNED_COUNT + 1,
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
    panel->menu_wid = -1;
    panel->panel_pid = 0;
    panel->menu_return_window = 0;
    panel->menu_return_valid = 0;
    panel->menu_hover = -1;
    panel->menu_selected = 0;
    panel->yield_counter = 0;
    for (int i = 0; i < PANEL_APP_COUNT; i++) {
        panel->app_signatures[i] = 0;
    }
    copy_text(panel->clock_text, sizeof(panel->clock_text), "--:--:--");
    init_apps(panel);

    kli_write_cstr(1, "panel: starting\n");
    panel->wid = gui_window_create(0, PANEL_Y, PANEL_SCREEN_W, PANEL_HEIGHT,
                                   COLOR_BG, COLOR_BORDER, "panel");
    if (panel->wid < 0) {
        kli_write_cstr(1, "panel: window create failed (no GUI available)\n");
        kli_write_cstr(1, "panel: idling in headless mode\n");
        for (;;) (void)kli_yield();
    }
    register_cursor_regions(panel);
    long pid = kli_getpid();
    panel->panel_pid = pid > 0 ? (uint32_t)pid : 0;
    refresh_panel_full(panel);
    kli_write_cstr(1, "panel: ready\n");

#ifdef PANEL_FORCE_FAULT
    kli_write_cstr(1, "panel: forced fault\n");
    *((volatile uint64_t *)(uintptr_t)0) = 0x50414e454c464c54ULL;
#endif

#ifdef PANEL_AUTO_TEST
    kli_write_cstr(1, "panel: auto-test launch every app\n");
    for (int app_index = 0; app_index < PANEL_APP_COUNT; app_index++) {
        activate_app(panel, app_index);
        (void)kli_yield();
    }
#endif

    for (;;) {
        long event_count = gui_window_event(panel->wid, panel->events, EVENT_CAP);
        if (event_count > 0) {
            for (long i = 0; i < event_count; i++) {
                if (panel->events[i].type == GUI_EVENT_MOUSE_CLICK) {
                    on_click(panel, panel->events[i].data1, panel->events[i].data2);
                } else if (panel->events[i].type == GUI_EVENT_MOUSE_MOVE) {
                    on_move(panel, panel->events[i].data1, panel->events[i].data2);
                }
            }
        }
        poll_menu(panel);
        panel->yield_counter++;
        if (panel->yield_counter >= REFRESH_PERIOD) {
            panel->yield_counter = 0;
            refresh_panel_tick(panel);
        }
        (void)kli_yield();
    }
}
