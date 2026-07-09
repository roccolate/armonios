# GUI ABI Notes

`SYSCALLS.md` is the authoritative syscall reference. This file records the GUI
rules that are easy to forget while changing the compositor or userland wrappers.

## Live Range

The live GUI/window range is `70..86`. Do not reuse `60..69`; that range is
used by IPC. System-info calls live at `100..102` and are documented separately
in `SYSCALLS.md`.

Implemented calls:

- `70 sys_window_create`
- `71 sys_window_destroy`
- `72 sys_window_draw_text`
- `73 sys_window_draw_rect`
- `74 sys_window_event`
- `75 sys_window_set_title`
- `76 sys_window_redraw`
- `77 sys_window_focus`
- `78 sys_window_for_pid`
- `79 sys_cursor_set_shape`
- `80 sys_window_flush`
- `81 sys_window_get_bounds`
- `82 sys_window_set_bounds`
- `83 sys_window_minimize`
- `84 sys_window_restore`
- `85 sys_window_state`
- `86 sys_cursor_register_region`

## Ownership

- Owner-only operations: create/destroy ownership, draw, title, event reads,
  get/set bounds, minimize, flush, and cursor-region registration.
- `sys_window_focus` is intentionally cross-process so the panel can raise app
  windows.
- `sys_window_restore` is intentionally cross-process so the panel can make a
  minimized app window visible again from the taskbar.
- `sys_window_state` is intentionally cross-process so the panel can read
  minimized/focused presentation state for windows it does not own.
- `sys_window_for_pid` is intentionally cross-process so the panel can enumerate
  app windows.
- Ownerless windows use `GUI_NO_OWNER` and are skipped by
  `sys_window_for_pid`.

## Event Buffer

`sys_window_event` writes fixed 12-byte records:

```c
typedef struct {
    uint32_t type;
    int32_t data1;
    int32_t data2;
} gui_event_t;
```

Event ids are part of the ABI:

| Type | Name | data1 | data2 |
|------|------|-------|-------|
| 1 | `GUI_EVENT_KEY_PRESS` | key value | 0 |
| 2 | `GUI_EVENT_KEY_RELEASE` | key value | 0 |
| 3 | `GUI_EVENT_MOUSE_CLICK` | absolute x | absolute y |
| 4 | `GUI_EVENT_MOUSE_MOVE` | absolute x | absolute y |
| 5 | `GUI_EVENT_RESIZE` | width | height |
| 6 | `GUI_EVENT_CLOSE` | 0 | 0 |
| 7 | `GUI_EVENT_MINIMIZE` | 0 | 0 |
| 8 | `GUI_EVENT_MAXIMIZE` | 0 | 0 |

`sys_window_event` is a bounded wait and returns `ERR_AGAIN` when no event is
available.

## Drawing And Damage

- Owner drawing goes into a per-window backing buffer.
- `sys_window_flush` pushes a content-local dirty rectangle.
- The compositor tracks framebuffer damage rectangles with a cap and a full
  redraw sentinel.
- Owner drawing coordinates are content-local; title-bar height is applied by
  the kernel.

## Cursor Regions

`sys_cursor_register_region` lets an owner register up to
`GUI_MAX_CURSOR_REGIONS` content-local rectangles. Slots are walked in ascending
order, and the first matching region wins over the default title-bar cursor
shape. Passing `GUI_CURSOR_REGION_DELETE` clears a slot.

## Compatibility Rules

- Append new GUI syscalls; do not renumber existing calls.
- Keep event struct size and event ids stable.
- Keep owner-only checks centralized through syscall helpers.
- Keep cross-process presentation calls explicit and documented; do not expand
  cross-process access casually.
- Update `SYSCALLS.md`, `kernel/syscall_numbers.h`,
  `programs/libkarmdesk/gui.h`, and ABI tests in the same change.
