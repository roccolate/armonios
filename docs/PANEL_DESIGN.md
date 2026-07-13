# Panel design and behavior

## Status

The modern panel phase 1 is implemented on branch `agent/modern-panel-phase1` and remains **UNVERIFIED** until the branch passes the complete build, size, host, stack, QEMU framebuffer, and visible desktop gates.

This work is userland-only. It does not change the syscall ABI or the kernel's built-in panel policy.

## Phase 1 behavior

The panel is a compact 40-pixel dock without a title bar. The kernel still classifies the window named `panel` as:

- `GUI_WINDOW_DOCK`;
- `GUI_WINDOW_NO_FOCUS`;
- `GUI_WINDOW_NO_DRAG`;
- `GUI_WINDOW_SKIP_TASKBAR`.

The previous two-row launcher/task layout is replaced by one taskbar row:

```text
[ Shell ] [ Editor ] [ Files ] [ Monitor ]                 [ 00:00:00 ]
```

The first four applications are pinned. The clock area is both an uptime display and the task button for the Clock application.

Each task target follows the same rules:

1. no matching window: launch the application;
2. minimized window: restore it;
3. visible window: focus it;
4. multiple matching windows: repeated clicks cycle through the group.

The panel groups at most four windows per application. Extra windows remain valid desktop windows but are not represented by additional instance marks.

## State language

Phase 1 deliberately retains the existing neutral palette. State is communicated through shape:

- closed: no bottom marker;
- running: short solid bottom marker;
- focused: wide bottom marker plus an inner top line;
- minimized: split bottom marker;
- multiple instances: small marks in the upper-right corner;
- hover: existing hover background and hand cursor region.

No color/theme decision should be inferred from this implementation.

## Architecture

Pure panel behavior lives in `programs/apps/panel_model.h`:

- geometry and hit testing;
- aggregate visual state;
- grouped-window target selection;
- uptime formatting.

`programs/apps/panel.c` owns runtime integration:

- `SYS_PROCLIST` polling;
- `SYS_WINDOW_FOR_PID` lookup;
- `SYS_WINDOW_STATE` inspection;
- launch, focus, and restore behavior;
- drawing and event handling.

The focused host runner is:

```sh
bash tests/run_panel_model_test.sh
```

It is also part of:

```sh
bash tools/verify.sh
```

## Deferred phase 2

The following features require separate review and are not part of phase 1:

- application menu or popup window;
- click-active-to-minimize across process ownership;
- global shortcuts such as Super and Alt-Tab;
- compositor-to-panel lifecycle events replacing polling;
- pin reordering or persistent settings;
- icon and color theme work.

Do not add placeholder controls for deferred behavior. Every visible control in the shipping panel must perform a real action.

## Required visible validation

Run:

```sh
make qemu-fb-visible
```

Verify:

1. panel occupies one compact row and does not receive keyboard focus;
2. each pinned button launches its application once;
3. clicking a running application focuses it;
4. clicking a minimized application restores it;
5. grouped application instances cycle on repeated clicks;
6. focused, running, minimized, and multiple-instance marks update;
7. the integrated uptime clock advances;
8. clicking the clock launches or activates Clock;
9. no app window is hidden behind an incorrectly sized dock;
10. Files → Editor → FAT32 workflow still works.
