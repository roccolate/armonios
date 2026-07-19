// ArmoniOS app: control
//
// Visual registry editor for persisted system preferences. Preferences are
// stored in /fat/CONFIG.INI using a small deterministic INI format.

#include <stddef.h>
#include <stdint.h>

#include "libkarm/syscall.h"
#include "libkarmdesk/gui.h"

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

#define COLOR_BG        0xff18242cU
#define COLOR_BORDER    0xff789090U
#define COLOR_TEXT      0xffdce8e8U
#define COLOR_DIM       0xff9cb0b0U
#define COLOR_SELECT    0xff385068U
#define COLOR_EDIT      0xffe0c080U
#define COLOR_OK        0xffb8e0c0U
#define COLOR_WARN      0xffe0b8a0U

#define INPUT_KEY_UP    0x101
#define INPUT_KEY_DOWN  0x102
#define INPUT_KEY_PGUP  0x105
#define INPUT_KEY_PGDN  0x106
#define INPUT_KEY_HOME  0x107
#define INPUT_KEY_END   0x108

static const char REG_PATH[] = "/fat/CONFIG.INI";

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
    char line_section[VALUE_CAP];
    char line_key[VALUE_CAP];
    char line_value[VALUE_CAP];
    char status[STATUS_CAP];
    char row[LINE_CAP];
    gui_event_t events[EVENT_CAP];
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

static void draw_text(long wid, long x, long y, uint32_t color,
                      const char *text) {
    (void)gui_window_draw_text(wid, x, y, color, text);
}

static void format_row(control_state_t *s, int index) {
    size_t c = 0;
    registry_entry_t *entry = &s->entries[index];
    append_text(s->row, sizeof(s->row), &c, entry->section);
    append_text(s->row, sizeof(s->row), &c, ".");
    append_text(s->row, sizeof(s->row), &c, entry->key);
    append_text(s->row, sizeof(s->row), &c, " = ");
    append_text(s->row, sizeof(s->row), &c, entry->value);
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

static void redraw(control_state_t *s) {
    (void)gui_window_draw_rect(s->wid, 1, 0, WIN_W - 2,
                               WIN_H - TITLE_BAR_H - 2, COLOR_BG);
    draw_text(s->wid, 12, 8, COLOR_TEXT, "CONTROL PANEL REGISTRY");
    draw_text(s->wid, 12, 24, COLOR_DIM,
              "ENTER EDIT  CTRL-S SAVE  R REVERT  D DEFAULTS");
    draw_text(s->wid, 12, 42, s->dirty ? COLOR_WARN : COLOR_OK, s->status);

    for (int row = 0; row < VISIBLE_ROWS; row++) {
        int index = s->scroll + row;
        long y = 66 + row * 20;
        if (index >= ENTRY_CAP) {
            break;
        }
        if (index == s->selected) {
            (void)gui_window_draw_rect(s->wid, 8, y - 3, WIN_W - 16, 15,
                                       COLOR_SELECT);
        }
        format_row(s, index);
        draw_text(s->wid, 16, y, COLOR_TEXT, s->row);
    }

    if (s->editing) {
        draw_text(s->wid, 12, 242, COLOR_EDIT, "VALUE:");
        draw_text(s->wid, 76, 242, COLOR_TEXT, s->edit_value);
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

static int handle_key(control_state_t *s, int key) {
    if (key == 17) {
        (void)gui_window_destroy(s->wid);
        kli_exit(0);
        for (;;) {
            (void)kli_yield();
        }
    }
    if (key == 19) {
        save_registry(s);
        return 1;
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
        load_registry(s);
        return 1;
    }
    if (key == 'd' || key == 'D') {
        reset_defaults(s);
        s->dirty = 1;
        copy_text(s->status, sizeof(s->status), "DEFAULTS READY");
        return 1;
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

    kli_write_cstr(1, "control: starting\n");
    s->wid = gui_window_create(WIN_X, WIN_Y, WIN_W, WIN_H,
                               COLOR_BG, COLOR_BORDER, "control");
    if (s->wid < 0) {
        kli_write_cstr(1, "control: window create failed\n");
        return 1;
    }
    (void)gui_window_set_title(s->wid, "control", TITLE_BAR_H);

    load_registry(s);
    redraw(s);

    for (;;) {
        long n = gui_window_event(s->wid, s->events, EVENT_CAP);
        if (n > 0) {
            int dirty = 0;
            for (long i = 0; i < n; i++) {
                if (s->events[i].type == GUI_EVENT_CLOSE) {
                    (void)gui_window_destroy(s->wid);
                    return 0;
                }
                if (s->events[i].type == GUI_EVENT_KEY_PRESS) {
                    if (handle_key(s, s->events[i].data1)) {
                        dirty = 1;
                    }
                }
            }
            if (dirty) {
                redraw(s);
            }
        } else {
            (void)kli_yield();
        }
    }
}
