// ArmoniOS app: control
//
// Visual registry editor for persisted system preferences. Preferences are
// stored in /fat/CONFIG.INI using a small deterministic INI format.

#include <stddef.h>
#include <stdint.h>

#include "libarmdesk/event.h"
#include "libarmdesk/gui.h"
#include "libarmdesk/layout.h"
#include "libarmdesk/render.h"
#include "libarmdesk/theme.h"
#include "libarmdesk/widget.h"
#include "libkarm/syscall.h"

#define O_RDONLY        0
#define O_RDWR          2
#define O_CREAT      0x40

#define WIN_X          88
#define WIN_Y          64
#define WIN_W         464
#define WIN_H         288
#define TITLE_BAR_H    16
#define EVENT_CAP       8
#define ENTRY_CAP      11
#define VALUE_CAP      32
#define LINE_CAP       80
#define FILE_CAP      768
#define STATUS_CAP     48
#define VISIBLE_ROWS    8
#define ACTION_COUNT    3

#define INPUT_KEY_UP    0x101
#define INPUT_KEY_DOWN  0x102
#define INPUT_KEY_PGUP  0x105
#define INPUT_KEY_PGDN  0x106
#define INPUT_KEY_HOME  0x107
#define INPUT_KEY_END   0x108

enum {
    ACTION_SAVE = 0,
    ACTION_RELOAD,
    ACTION_DEFAULTS,
};

static const char REG_PATH[] = "/fat/CONFIG.INI";
static const armdesk_theme_t CONTROL_THEME = ARMDESK_THEME_DARK_INIT;

typedef struct {
    const char *section;
    const char *key;
    const char *fallback;
    char value[VALUE_CAP];
} registry_entry_t;

typedef struct {
    long wid;
    int selected;
    int scroll;
    int editing;
    int dirty;
    registry_entry_t entries[ENTRY_CAP];
    char edit_value[VALUE_CAP];
    int edit_len;
    char file_buf[FILE_CAP];
    char line_key[VALUE_CAP];
    char line_value[VALUE_CAP];
    char status[STATUS_CAP];
    char row[LINE_CAP];
    gui_event_t events[EVENT_CAP];
    uint32_t window_bounds[4];
    armdesk_button_t actions[ACTION_COUNT];
} control_state_t;

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

static int text_equal(const char *a, const char *b) {
    size_t i = 0;

    while (a[i] != '\0' || b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return 1;
}

static size_t text_len(const char *s) {
    size_t n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

static void append_text(char *out, size_t out_size, size_t *cursor,
                        const char *text) {
    while (*cursor + 1 < out_size && *text != '\0') {
        out[*cursor] = *text;
        (*cursor)++;
        text++;
    }
    out[*cursor] = '\0';
}

static void entry_init(registry_entry_t *entry, const char *section,
                       const char *key, const char *fallback) {
    entry->section = section;
    entry->key = key;
    entry->fallback = fallback;
    copy_text(entry->value, sizeof(entry->value), fallback);
}

static void reset_defaults(control_state_t *s) {
    entry_init(&s->entries[0], "system", "hostname", "armonios");
    entry_init(&s->entries[1], "system", "boot_mode", "desktop");
    entry_init(&s->entries[2], "display", "panel_clock", "on");
    entry_init(&s->entries[3], "display", "window_borders", "classic");
    entry_init(&s->entries[4], "input", "key_repeat", "on");
    entry_init(&s->entries[5], "input", "repeat_delay", "400");
    entry_init(&s->entries[6], "input", "mouse_speed", "3");
    entry_init(&s->entries[7], "files", "default_path", "/fat");
    entry_init(&s->entries[8], "files", "confirm_delete", "on");
    entry_init(&s->entries[9], "editor", "default_path", "/tmp/note");
    entry_init(&s->entries[10], "diagnostics", "log_level", "normal");
}

static registry_entry_t *find_entry(control_state_t *s, const char *section,
                                    const char *key) {
    for (int i = 0; i < ENTRY_CAP; i++) {
        if (text_equal(s->entries[i].section, section) &&
            text_equal(s->entries[i].key, key)) {
            return &s->entries[i];
        }
    }
    return 0;
}

static int is_space(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

static void trim_right(char *s) {
    size_t n = text_len(s);

    while (n > 0 && is_space(s[n - 1])) {
        n--;
        s[n] = '\0';
    }
}

static const char *skip_space(const char *s) {
    while (*s == ' ' || *s == '\t') {
        s++;
    }
    return s;
}

static void parse_ini_line(control_state_t *s, char *section,
                           const char *line) {
    const char *p = skip_space(line);

    if (*p == '\0' || *p == '#') {
        return;
    }
    if (*p == '[') {
        size_t i = 0;

        p++;
        while (i + 1 < VALUE_CAP && *p != '\0' && *p != ']') {
            section[i++] = *p++;
        }
        section[i] = '\0';
        trim_right(section);
        return;
    }

    size_t k = 0;
    while (k + 1 < VALUE_CAP && p[k] != '\0' && p[k] != '=') {
        s->line_key[k] = p[k];
        k++;
    }
    s->line_key[k] = '\0';
    trim_right(s->line_key);
    if (p[k] != '=') {
        return;
    }

    p = skip_space(&p[k + 1]);
    copy_text(s->line_value, sizeof(s->line_value), p);
    trim_right(s->line_value);

    registry_entry_t *entry = find_entry(s, section, s->line_key);
    if (entry != 0) {
        copy_text(entry->value, sizeof(entry->value), s->line_value);
    }
}

static void parse_ini(control_state_t *s, long bytes) {
    char section[VALUE_CAP];
    size_t line_len = 0;

    section[0] = '\0';
    for (long i = 0; i <= bytes && i < FILE_CAP; i++) {
        char c = i == bytes ? '\n' : s->file_buf[i];

        if (c == '\n') {
            s->line_value[line_len] = '\0';
            parse_ini_line(s, section, s->line_value);
            line_len = 0;
            continue;
        }
        if (line_len + 1 < VALUE_CAP) {
            s->line_value[line_len++] = c;
        }
    }
}

static void load_registry(control_state_t *s) {
    reset_defaults(s);

    long fd = kli_open(REG_PATH, O_RDONLY);
    if (fd < 0) {
        s->dirty = 0;
        copy_text(s->status, sizeof(s->status), "DEFAULTS (NO CONFIG)");
        return;
    }

    long n = kli_read((int)fd, s->file_buf, sizeof(s->file_buf) - 1);
    (void)kli_close((int)fd);
    if (n < 0) {
        copy_text(s->status, sizeof(s->status), "LOAD FAILED");
        return;
    }

    s->file_buf[n] = '\0';
    parse_ini(s, n);
    s->dirty = 0;
    copy_text(s->status, sizeof(s->status), "LOADED /fat/CONFIG.INI");
}

static void serialize_registry(control_state_t *s, char *out,
                               size_t out_size, size_t *out_len) {
    const char *section = "";
    size_t cursor = 0;

    out[0] = '\0';
    for (int i = 0; i < ENTRY_CAP; i++) {
        registry_entry_t *entry = &s->entries[i];

        if (!text_equal(section, entry->section)) {
            if (cursor > 0) {
                append_text(out, out_size, &cursor, "\n");
            }
            append_text(out, out_size, &cursor, "[");
            append_text(out, out_size, &cursor, entry->section);
            append_text(out, out_size, &cursor, "]\n");
            section = entry->section;
        }
        append_text(out, out_size, &cursor, entry->key);
        append_text(out, out_size, &cursor, "=");
        append_text(out, out_size, &cursor, entry->value);
        append_text(out, out_size, &cursor, "\n");
    }
    *out_len = cursor;
}

static void save_registry(control_state_t *s) {
    size_t len = 0;
    char truncate_byte = '\0';

    serialize_registry(s, s->file_buf, sizeof(s->file_buf), &len);
    long fd = kli_open(REG_PATH, O_RDWR | O_CREAT);
    if (fd < 0) {
        copy_text(s->status, sizeof(s->status), "SAVE FAILED");
        return;
    }

    (void)kli_seek((int)fd, 0, 0);
    long written = kli_write((int)fd, s->file_buf, len);
    if (written == (long)len) {
        (void)kli_write((int)fd, &truncate_byte, 0);
    }
    (void)kli_close((int)fd);

    if (written == (long)len) {
        s->dirty = 0;
        copy_text(s->status, sizeof(s->status), "SAVED /fat/CONFIG.INI");
    } else {
        copy_text(s->status, sizeof(s->status), "SAVE FAILED");
    }
}

static void format_row(control_state_t *s, int index) {
    size_t cursor = 0;
    registry_entry_t *entry = &s->entries[index];

    append_text(s->row, sizeof(s->row), &cursor, entry->section);
    append_text(s->row, sizeof(s->row), &cursor, ".");
    append_text(s->row, sizeof(s->row), &cursor, entry->key);
    append_text(s->row, sizeof(s->row), &cursor, " = ");
    append_text(s->row, sizeof(s->row), &cursor, entry->value);
}

static void clamp_scroll(control_state_t *s) {
    if (s->selected < 0) {
        s->selected = 0;
    }
    if (s->selected >= ENTRY_CAP) {
        s->selected = ENTRY_CAP - 1;
    }
    if (s->scroll > s->selected) {
        s->scroll = s->selected;
    }
    if (s->selected >= s->scroll + VISIBLE_ROWS) {
        s->scroll = s->selected - VISIBLE_ROWS + 1;
    }
    if (s->scroll < 0) {
        s->scroll = 0;
    }
}

static void layout_actions(control_state_t *s) {
    armdesk_linear_layout_t layout =
        armdesk_row_layout(armdesk_rect(12, 244, 276, 20), 8);
    armdesk_rect_t bounds;

    (void)armdesk_linear_layout_take(&layout, 72, &bounds);
    s->actions[ACTION_SAVE] = armdesk_button(bounds, "SAVE");
    (void)armdesk_linear_layout_take(&layout, 72, &bounds);
    s->actions[ACTION_RELOAD] = armdesk_button(bounds, "RELOAD");
    (void)armdesk_linear_layout_take(&layout, 116, &bounds);
    s->actions[ACTION_DEFAULTS] = armdesk_button(bounds, "DEFAULTS");
}

static void register_action_cursors(control_state_t *s) {
    for (int i = 0; i < ACTION_COUNT; i++) {
        armdesk_rect_t bounds = s->actions[i].bounds;

        (void)gui_cursor_register_region(s->wid, i, bounds.x, bounds.y,
                                         bounds.w, bounds.h,
                                         GUI_CURSOR_HAND);
    }
}

static void render_label(long wid, int32_t x, int32_t y,
                         armdesk_theme_token_t token, const char *text) {
    armdesk_label_t label = armdesk_label(
        armdesk_rect(x, y, WIN_W - x - 8, 12), text, token);

    (void)armdesk_render_label(wid, &label, &CONTROL_THEME);
}

static void redraw(control_state_t *s) {
    armdesk_rect_t content =
        armdesk_rect(1, 0, WIN_W - 2, WIN_H - TITLE_BAR_H - 2);

    (void)armdesk_render_fill(
        s->wid, content,
        armdesk_theme_color(&CONTROL_THEME, ARMDESK_THEME_WINDOW_BG));
    render_label(s->wid, 12, 8, ARMDESK_THEME_WINDOW_FG,
                 "CONTROL PANEL REGISTRY");
    render_label(s->wid, 12, 24, ARMDESK_THEME_DISABLED_FG,
                 "ENTER EDIT  CTRL-S SAVE  R RELOAD  D DEFAULTS");
    render_label(s->wid, 12, 42,
                 s->dirty ? ARMDESK_THEME_WARNING_FG
                          : ARMDESK_THEME_STATUS_FG,
                 s->status);

    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int index = s->scroll + row;
        int32_t y = 66 + row * 20;
        int selected;

        if (index >= ENTRY_CAP) {
            break;
        }
        selected = index == s->selected;
        if (selected) {
            (void)armdesk_render_fill(
                s->wid, armdesk_rect(8, y - 3, WIN_W - 16, 15),
                armdesk_theme_color(&CONTROL_THEME,
                                    ARMDESK_THEME_SELECTION_BG));
        }
        format_row(s, index);
        render_label(s->wid, 16, y,
                     selected ? ARMDESK_THEME_SELECTION_FG
                              : ARMDESK_THEME_WINDOW_FG,
                     s->row);
    }

    if (s->editing) {
        render_label(s->wid, 12, 222, ARMDESK_THEME_MENU_HOTKEY_FG,
                     "VALUE:");
        render_label(s->wid, 76, 222, ARMDESK_THEME_WINDOW_FG,
                     s->edit_value);
    }

    for (int i = 0; i < ACTION_COUNT; i++) {
        (void)armdesk_render_button(s->wid, &s->actions[i], &CONTROL_THEME);
    }

    (void)gui_window_flush(s->wid, 0, 0, WIN_W, WIN_H - TITLE_BAR_H);
}

static void begin_edit(control_state_t *s) {
    registry_entry_t *entry = &s->entries[s->selected];

    copy_text(s->edit_value, sizeof(s->edit_value), entry->value);
    s->edit_len = (int)text_len(s->edit_value);
    s->editing = 1;
    copy_text(s->status, sizeof(s->status), "EDITING VALUE");
}

static void commit_edit(control_state_t *s) {
    registry_entry_t *entry = &s->entries[s->selected];

    copy_text(entry->value, sizeof(entry->value), s->edit_value);
    s->editing = 0;
    s->dirty = 1;
    copy_text(s->status, sizeof(s->status), "MODIFIED");
}

static int handle_edit_key(control_state_t *s, int key) {
    if (key == 27) {
        s->editing = 0;
        copy_text(s->status, sizeof(s->status), "CANCELLED");
        return 1;
    }
    if (key == 13 || key == 10) {
        commit_edit(s);
        return 1;
    }
    if (key == 8 || key == 127) {
        if (s->edit_len > 0) {
            s->edit_len--;
            s->edit_value[s->edit_len] = '\0';
        }
        return 1;
    }
    if (key >= 32 && key <= 126 && s->edit_len + 1 < VALUE_CAP) {
        s->edit_value[s->edit_len++] = (char)key;
        s->edit_value[s->edit_len] = '\0';
        return 1;
    }
    return 0;
}

static int activate_action(control_state_t *s, int action) {
    if (action == ACTION_SAVE) {
        save_registry(s);
        return 1;
    }
    if (action == ACTION_RELOAD) {
        load_registry(s);
        return 1;
    }
    if (action == ACTION_DEFAULTS) {
        reset_defaults(s);
        s->dirty = 1;
        copy_text(s->status, sizeof(s->status), "DEFAULTS READY");
        return 1;
    }
    return 0;
}

static int handle_key(control_state_t *s, int key) {
    if (key == 17) {
        (void)gui_window_destroy(s->wid);
        kli_exit(0);
        for (;;) {
            (void)kli_yield();
        }
    }
    if (key == 19) {
        return activate_action(s, ACTION_SAVE);
    }
    if (s->editing) {
        return handle_edit_key(s, key);
    }
    if (key == INPUT_KEY_UP) {
        s->selected--;
        clamp_scroll(s);
        return 1;
    }
    if (key == INPUT_KEY_DOWN) {
        s->selected++;
        clamp_scroll(s);
        return 1;
    }
    if (key == INPUT_KEY_HOME) {
        s->selected = 0;
        clamp_scroll(s);
        return 1;
    }
    if (key == INPUT_KEY_END) {
        s->selected = ENTRY_CAP - 1;
        clamp_scroll(s);
        return 1;
    }
    if (key == INPUT_KEY_PGUP) {
        s->selected -= VISIBLE_ROWS;
        clamp_scroll(s);
        return 1;
    }
    if (key == INPUT_KEY_PGDN) {
        s->selected += VISIBLE_ROWS;
        clamp_scroll(s);
        return 1;
    }
    if (key == 13 || key == 10) {
        begin_edit(s);
        return 1;
    }
    if (key == 'r' || key == 'R') {
        return activate_action(s, ACTION_RELOAD);
    }
    if (key == 'd' || key == 'D') {
        return activate_action(s, ACTION_DEFAULTS);
    }
    return 0;
}

static int pointer_to_content(control_state_t *s, int32_t absolute_x,
                              int32_t absolute_y, armdesk_point_t *point) {
    armdesk_rect_t window;

    if (gui_window_get_bounds(s->wid, s->window_bounds) < 0 ||
        s->window_bounds[0] > INT32_MAX ||
        s->window_bounds[1] > INT32_MAX ||
        s->window_bounds[2] > INT32_MAX ||
        s->window_bounds[3] > INT32_MAX) {
        return -1;
    }

    window = armdesk_rect((int32_t)s->window_bounds[0],
                          (int32_t)s->window_bounds[1],
                          (int32_t)s->window_bounds[2],
                          (int32_t)s->window_bounds[3]);
    return armdesk_pointer_to_content(window, TITLE_BAR_H,
                                      absolute_x, absolute_y, point);
}

static int update_hover(control_state_t *s, int32_t absolute_x,
                        int32_t absolute_y) {
    armdesk_point_t point;
    int inside = pointer_to_content(s, absolute_x, absolute_y, &point) == 0;
    int changed = 0;

    for (int i = 0; i < ACTION_COUNT; i++) {
        uint8_t hovered = inside &&
                                  armdesk_button_hit_test(&s->actions[i],
                                                          point.x, point.y)
                              ? 1U
                              : 0U;
        if (s->actions[i].hovered != hovered) {
            s->actions[i].hovered = hovered;
            changed = 1;
        }
    }
    return changed;
}

static int handle_click(control_state_t *s, int32_t absolute_x,
                        int32_t absolute_y) {
    armdesk_point_t point;

    if (pointer_to_content(s, absolute_x, absolute_y, &point) != 0) {
        return 0;
    }

    for (int i = 0; i < ACTION_COUNT; i++) {
        if (armdesk_button_hit_test(&s->actions[i], point.x, point.y)) {
            return activate_action(s, i);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    long state_addr = kli_mmap(0, sizeof(control_state_t), 0);
    if (state_addr < 0) {
        kli_write_cstr(1, "control: state mmap failed\n");
        return 1;
    }

    control_state_t *s = (control_state_t *)(uintptr_t)state_addr;
    s->wid = 0;
    s->selected = 0;
    s->scroll = 0;
    s->editing = 0;
    s->dirty = 0;
    s->edit_value[0] = '\0';
    s->edit_len = 0;
    copy_text(s->status, sizeof(s->status), "STARTING");
    reset_defaults(s);
    layout_actions(s);

    kli_write_cstr(1, "control: starting\n");
    s->wid = gui_window_create(
        WIN_X, WIN_Y, WIN_W, WIN_H,
        armdesk_theme_color(&CONTROL_THEME, ARMDESK_THEME_WINDOW_BG),
        armdesk_theme_color(&CONTROL_THEME, ARMDESK_THEME_WINDOW_BORDER),
        "control");
    if (s->wid < 0) {
        kli_write_cstr(1, "control: window create failed\n");
        return 1;
    }

    (void)gui_window_set_title(s->wid, "control", TITLE_BAR_H);
    register_action_cursors(s);
    load_registry(s);
    redraw(s);

    for (;;) {
        long n = gui_window_event(s->wid, s->events, EVENT_CAP);

        if (n > 0) {
            int needs_redraw = 0;

            for (long i = 0; i < n; i++) {
                gui_event_t *event = &s->events[i];

                if (event->type == GUI_EVENT_CLOSE) {
                    (void)gui_window_destroy(s->wid);
                    return 0;
                }
                if (event->type == GUI_EVENT_KEY_PRESS) {
                    needs_redraw |= handle_key(s, event->data1);
                } else if (event->type == GUI_EVENT_MOUSE_MOVE) {
                    needs_redraw |= update_hover(s, event->data1,
                                                 event->data2);
                } else if (event->type == GUI_EVENT_MOUSE_CLICK) {
                    needs_redraw |= handle_click(s, event->data1,
                                                 event->data2);
                }
            }
            if (needs_redraw) {
                redraw(s);
            }
        } else {
            (void)kli_yield();
        }
    }
}
