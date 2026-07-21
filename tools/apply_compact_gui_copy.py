#!/usr/bin/env python3

from pathlib import Path

path = Path("kernel/syscall_gui.c")
text = path.read_text()


def replace_function(source: str, name: str, replacement: str) -> str:
    start = source.find(f"int64_t {name}(")
    if start < 0:
        raise SystemExit(f"missing {name}")
    brace = source.find("{", start)
    depth = 0
    end = brace
    while end < len(source):
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
            if depth == 0:
                end += 1
                break
        end += 1
    return source[:start] + replacement + source[end:]

text = replace_function(text, "sys_window_get_bounds", '''int64_t sys_window_get_bounds(process_t *process, uint64_t window_id,
                              uint64_t out_ptr) {
    gui_desktop_t *desktop;
    gui_window_t *window;
    uint32_t out[4];
    int64_t status;

    status = sys_user_buf_out(process, out_ptr, sizeof(out));
    if (status != 0) {
        return status;
    }
    status = sys_owner_window(process, window_id, &desktop, &window);
    if (status != 0) {
        return status;
    }
    out[0] = window->x;
    out[1] = window->y;
    out[2] = window->w;
    out[3] = window->h;
    sys_copy_to_user_validated(out_ptr, out, sizeof(out));
    return 0;
}''')

text = replace_function(text, "sys_window_state", '''int64_t sys_window_state(process_t *process, uint64_t window_id,
                         uint64_t out_ptr) {
    gui_desktop_t *desktop = gui_desktop();
    gui_window_t *window;
    uint32_t state = 0;
    int64_t status;

    status = sys_user_buf_out(process, out_ptr, sizeof(state));
    if (status != 0) {
        return status;
    }
    if (process == 0 || window_id >= GUI_MAX_WINDOWS) {
        return ERR_INVAL;
    }
    if (desktop == 0) {
        return ERR_AGAIN;
    }
    window = &desktop->windows[window_id];
    if (window->used == 0) {
        return ERR_NOENT;
    }
    if (window->minimized != 0U) {
        state |= 0x1U;
    }
    if (desktop->focused_window_id == (uint32_t)window_id &&
        (window->flags & GUI_WINDOW_NO_FOCUS) == 0U) {
        state |= 0x2U;
    }
    sys_copy_to_user_validated(out_ptr, &state, sizeof(state));
    return 0;
}''')

text = replace_function(text, "sys_window_event", '''int64_t sys_window_event(process_t *process, uint64_t window_id,
                         uint64_t buf_ptr, uint64_t buf_count) {
    gui_window_t *window;
    uint64_t n = 0;
    int64_t status;

    if (process == 0 || window_id >= GUI_MAX_WINDOWS ||
        buf_count == 0 || buf_count > 64) {
        return ERR_INVAL;
    }
    status = sys_user_buf_out(process, buf_ptr,
                              buf_count * 3U * sizeof(uint32_t));
    if (status != 0) {
        return status;
    }
    status = sys_owner_window_badf(process, window_id, 0, &window);
    if (status != 0) {
        return status;
    }
    if (window->event_count == 0) {
        return ERR_AGAIN;
    }

    while (n < buf_count && window->event_count > 0) {
        gui_event_t ev;
        uint32_t out[3];

        if (gui_window_pop_event(window, &ev) != 0) {
            break;
        }
        out[0] = ev.type;
        out[1] = (uint32_t)ev.data1;
        out[2] = (uint32_t)ev.data2;
        sys_copy_to_user_validated(buf_ptr + n * sizeof(out),
                                   out, sizeof(out));
        n++;
    }
    return (int64_t)n;
}''')

path.write_text(text)
Path(__file__).unlink()
