# GUI ABI notes

`SYSCALLS.md` is the authoritative syscall table. This document records GUI
ownership, event, drawing, and compatibility rules that are easy to break while
changing the compositor or desktop wrappers.

Public values and layouts live in `include/armonios/abi/gui.h`. The canonical
userland wrapper layer is `programs/libarmdesk/`; see `LIBARMDESK.md`.

## Implemented range

The current GUI/window syscall range is `70..86`:

| Number | Public operation |
|---:|---|
| 70 | create window |
| 71 | destroy window |
| 72 | draw text |
| 73 | draw rectangle |
| 74 | receive event |
| 75 | set title |
| 76 | request redraw |
| 77 | focus window |
| 78 | find window for PID |
| 79 | set cursor shape |
| 80 | flush a damage rectangle |
| 81 | get bounds |
| 82 | set bounds |
| 83 | minimize |
| 84 | restore |
| 85 | get state |
| 86 | register cursor region |

New GUI calls must be appended. Existing numbers, arguments, flags, and layouts
must not be renumbered or reused.

Prefer userland composition in `libarmdesk` when an operation can be expressed
through the existing ABI. Add a syscall only when the kernel must own the action
or enforce authority that userland cannot provide safely.

## Window ownership

Window creation assigns the calling process as owner.

Owner-only operations include:

- destroy;
- draw text and rectangles;
- set title;
- receive events;
- get and set bounds;
- minimize;
- flush damage;
- register cursor regions.

Ownerless kernel windows use `GUI_NO_OWNER` and are omitted from ordinary
process-window discovery.

Cross-process presentation operations are deliberately narrow so the trusted
panel can manage application windows:

- focus;
- restore;
- inspect state;
- discover a process-owned window.

Do not expand cross-process mutation for convenience. A new exception requires a
desktop responsibility, authority analysis, public ABI decision, and tests.
Before untrusted external applications are supported, this trusted-desktop model
needs an explicit capability or authority layer.

## Focus policy

Current behavior:

- the first focusable window receives focus when none is active;
- mouse clicks can raise and focus a window;
- the focus syscall can raise and focus a selected window;
- normal `libarmdesk` application wrappers request focus after successful window
  creation;
- windows marked `GUI_WINDOW_NO_FOCUS`, such as docks or taskbars, cannot take
  normal focus.

Deterministic focus behavior is exercised by the QEMU focus gate. Visible layout
and interaction remain separate manual evidence and must be recorded against the
exact tested tree.

## Events

`gui_event_t` is a frozen 12-byte public record:

```c
typedef struct {
    uint32_t type;
    int32_t  data1;
    int32_t  data2;
} gui_event_t;
```

Current event values:

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

The receive call performs a bounded wait and returns `AGAIN` when no event becomes
available. It validates the complete writable destination before consuming the
queued event.

Final user copies are permission-checked but not fault-contained against a late
translation fault after validation.

## Drawing and backing storage

- application drawing targets a kernel-owned per-window backing buffer;
- owner coordinates are content-local;
- title-bar height and decoration remain kernel policy;
- damage flush converts a content-local rectangle into framebuffer coordinates;
- compositor damage is clipped and merged;
- excessive damage may collapse to one full-redraw sentinel;
- partial repaint clips every draw to the active damage rectangle;
- the cursor is drawn after windows for full and partial repaint;
- backing storage is freed with the window;
- resize allocates replacement storage before releasing the old buffer.

A failed renderer submission consumes no pending damage. Runtime-service redraw
budgets and continuation are defined in `RUNTIME_SERVICE.md`.

## Window state

The public state result is a 32-bit bitmap:

```text
bit 0  minimized
bit 1  focused
```

Minimized windows are skipped by composition. Restore clears minimized state,
raises the window, and emits `GUI_EVENT_MAXIMIZE`.

## Cursor regions

A window owner can configure a fixed number of content-local cursor regions.

Rules:

- slots are checked in ascending order;
- the first matching region wins;
- title-bar cursor behavior remains kernel-owned;
- `GUI_CURSOR_REGION_DELETE` clears a slot;
- all regions disappear when the window is destroyed.

## libarmdesk boundary

The canonical desktop wrapper is `programs/libarmdesk/gui.h`.
`programs/libkarmdesk/gui.h` is a temporary compatibility include only.

Current `main` has geometry and theme foundations but no promoted generic widget
toolkit. Buttons, text fields, layouts, lists, dialogs, and notifications remain
future work until merged with tests and real consumers.

## Change checklist

A GUI ABI or compositor-boundary change must update together:

- `include/armonios/abi/gui.h` when public values or layouts change;
- public syscall numbers and dispatch when a call changes;
- ownership and authority checks;
- kernel GUI implementation;
- `libarmdesk` wrappers and compatibility paths;
- affected applications;
- ABI, focus, event, damage, and ownership tests;
- `SYSCALLS.md`, `LIBARMDESK.md`, and this document;
- current state, risks, or roadmap when capability changes;
- visible evidence when layout or interaction is claimed.
