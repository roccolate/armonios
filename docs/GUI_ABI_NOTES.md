# GUI ABI Notes

`SYSCALLS.md` is the authoritative syscall reference. This file records GUI rules that are easy to break while changing the compositor or userland wrappers.

Current GUI risks:

- GUI output buffers are permission-checked but not fault-contained;
- `RISK-004` — the focus syscall path is automated and the visible files-to-editor workflow was manually verified by rocco on 2026-07-17.

## Live range

The implemented GUI/window range is `70..86`. IPC uses `60..61`; system information uses `100..102`.

| # | Call |
|---:|---|
| 70 | `sys_window_create` |
| 71 | `sys_window_destroy` |
| 72 | `sys_window_draw_text` |
| 73 | `sys_window_draw_rect` |
| 74 | `sys_window_event` |
| 75 | `sys_window_set_title` |
| 76 | `sys_window_redraw` |
| 77 | `sys_window_focus` |
| 78 | `sys_window_for_pid` |
| 79 | `sys_cursor_set_shape` |
| 80 | `sys_window_flush` |
| 81 | `sys_window_get_bounds` |
| 82 | `sys_window_set_bounds` |
| 83 | `sys_window_minimize` |
| 84 | `sys_window_restore` |
| 85 | `sys_window_state` |
| 86 | `sys_cursor_register_region` |

Append new calls. Never renumber or reuse existing entries.

## Ownership

Owner-only operations include:

- destroy;
- draw text/rect;
- set title;
- read events;
- get/set bounds;
- minimize;
- flush damage;
- register cursor regions.

Window creation assigns the caller as owner.

Cross-process presentation operations are intentionally limited to:

- `sys_window_focus` — panel can raise an app window;
- `sys_window_restore` — panel can restore a minimized app;
- `sys_window_state` — panel can read minimized/focused state;
- `sys_window_for_pid` — panel can enumerate app windows.

Ownerless windows use `GUI_NO_OWNER` and are skipped by process-window enumeration.

Do not expand cross-process mutation casually. A new exception requires a documented desktop responsibility and ABI tests.

## Focus policy

Current behavior:

- the first focusable window receives focus when no window is focused;
- mouse clicks can focus and raise a window;
- `sys_window_focus` can focus and raise a selected window;
- libkarmdesk application wrappers request focus after creating a normal window.

The stable policy is wrapper-driven focus for normal app windows plus kernel-side rejection for `GUI_WINDOW_NO_FOCUS` docks and taskbars. `tools/qemu_focus_test.sh` verifies the syscall path with serial markers; visible human confirmation remains separate evidence.

## Event buffer

`sys_window_event` writes fixed 12-byte records:

```c
typedef struct {
    uint32_t type;
    int32_t  data1;
    int32_t  data2;
} gui_event_t;
```

Event IDs are ABI:

| Type | Name | `data1` | `data2` |
|---:|---|---|---|
| 1 | `GUI_EVENT_KEY_PRESS` | key value | 0 |
| 2 | `GUI_EVENT_KEY_RELEASE` | key value | 0 |
| 3 | `GUI_EVENT_MOUSE_CLICK` | absolute x | absolute y |
| 4 | `GUI_EVENT_MOUSE_MOVE` | absolute x | absolute y |
| 5 | `GUI_EVENT_RESIZE` | width | height |
| 6 | `GUI_EVENT_CLOSE` | 0 | 0 |
| 7 | `GUI_EVENT_MINIMIZE` | 0 | 0 |
| 8 | `GUI_EVENT_MAXIMIZE` | 0 | 0 |

The call waits for a bounded number of scheduler turns and returns `ERR_AGAIN` when no event is available.

The destination is validated as a registered process range and writable EL0 pages. The same rule applies to bounds and state output buffers. Copies are not fault-contained against unexpected faults after validation.

## Drawing and backing buffers

- owner drawing lands in a per-window kernel backing buffer;
- title-bar height is added by the kernel, so owner coordinates remain content-local;
- `sys_window_flush` adds a content-local damage rectangle;
- damage rectangles are framebuffer-coordinate rectangles after syscall conversion;
- the compositor clips/merges damage and can collapse to a full-redraw sentinel;
- partial repaint clips every repaint to the active damage rectangle;
- the cursor is drawn last, after windows, on both full and partial repaint;
- backing storage is freed when the window is destroyed;
- resize must allocate replacement storage successfully before discarding the old buffer.

## Window state

`sys_window_state` writes a 32-bit bitmap:

```text
bit 0  minimized
bit 1  focused
```

Minimized windows are skipped by composition. Restore clears minimized state, raises the window, and emits `GUI_EVENT_MAXIMIZE`.

## Cursor regions

A window owner can configure up to `GUI_MAX_CURSOR_REGIONS` content-local rectangles.

- slots are checked in ascending order;
- first matching region wins;
- title-bar cursor behavior remains kernel-owned;
- `GUI_CURSOR_REGION_DELETE` clears a slot;
- regions are removed when the window is destroyed.

## Compatibility checklist

A GUI ABI change must update:

- `kernel/syscall_numbers.h`;
- syscall dispatch and owner checks;
- `kernel/gui*.h` public constants/structures;
- `programs/libkarmdesk` wrappers;
- shipping applications;
- `tests/test_window_abi.c` and relevant GUI tests;
- `SYSCALLS.md` and this file;
- `CURRENT_STATE.md` if evidence or user-visible behavior changes.

Do not describe a GUI workflow as verified without recording the exact visible steps, commit, environment, and tester.
