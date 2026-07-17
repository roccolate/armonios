# Panel design and behavior

## Status

Modern panel phase 1 is isolated in draft PR #21 on branch `agent/modern-panel-phase1`.

Phase 2 is implemented on dependent branch `agent/modern-panel-phase2`. The 2026-07-17 working-tree baseline passed build, size, host, stack, QEMU marker, and visible desktop FAT workflow verification; any branch-specific PR promotion still needs its own recorded run.

The implementation remains userland-only. It does not add a syscall, renumber the ABI, or broaden an owner-only window permission.

## Taskbar behavior

The panel is a compact 40-pixel dock without a visible title bar. The kernel classifies the window named `panel` as:

- `GUI_WINDOW_DOCK`;
- `GUI_WINDOW_NO_FOCUS`;
- `GUI_WINDOW_NO_DRAG`;
- `GUI_WINDOW_SKIP_TASKBAR`.

The dock is one row:

```text
[ Apps ] [ > ] [ E ] [ F ] [ M ]                            [ 00:00:00 ]
```

The first four applications are pinned compact launchers. Their labels live in the Apps menu; the dock uses compact ASCII glyph icons drawn with the existing text primitive. The clock area is both a session-time display and the task control for Clock.

Each task target follows these rules:

1. no matching process or window: launch the application;
2. matching process still opening its first window: wait instead of spawning a duplicate;
3. minimized window: restore it;
4. visible non-focused window: focus it;
5. multiple matching windows: repeated clicks cycle through the group;
6. one already-focused window: keep it focused.

The panel groups at most four windows per application. Extra windows remain valid desktop windows but are not represented by additional instance marks.

Clicking an already-focused app does **not** minimize it yet. `SYS_WINDOW_MINIMIZE` is owner-only, and the panel must not receive broad ownership over another process's window merely for desktop polish. A future change needs a narrowly scoped presentation permission with tests and documentation.

## Application menu

The `Apps` control opens a real second window owned by the panel process. It is not a painted placeholder.

The menu:

- lists Shell, Editor, Files, Monitor, Control Panel, and Clock;
- launches or activates the selected application;
- supports mouse hover and click;
- supports Up, Down, Enter, and Escape;
- closes when it loses focus;
- restores the previously focused application when dismissed with Escape or by toggling the Apps button;
- displays a small running/focused marker beside each application.

The popup uses a one-pixel kernel title area. This preserves normal keyboard focus while ensuring clicks in the menu content do not start a window drag. The one-pixel area is not intended as visible decoration.

## State language

The panel deliberately retains the existing neutral palette. State is communicated through shape:

- closed: no bottom marker;
- running: short solid bottom marker;
- focused: wide bottom marker plus an inner top line;
- minimized: split bottom marker;
- multiple instances: small marks in the upper-right corner;
- hover: hand cursor region only on the main dock; the dock does not repaint on mouse move;
- open application menu: marker under the Apps control.

No color or theme decision should be inferred from this implementation.

## Architecture

Pure behavior lives in `programs/apps/panel_model.h`:

- taskbar and menu geometry;
- hit testing;
- aggregate visual state;
- activation decisions;
- grouped-window target selection;
- uptime formatting.

The KLI1 panel remains one translation unit:

- `programs/apps/panel.c` provides the small panel-specific window-creation adapter;
- `programs/apps/panel_runtime.inc` owns taskbar/menu runtime integration;
- the `.inc` file is included exactly once by `panel.c` and is tracked by the Makefile dependency flags.

Runtime integration uses the existing interfaces:

- `SYS_PROCLIST` polling;
- `SYS_WINDOW_FOR_PID` lookup;
- `SYS_WINDOW_STATE` inspection;
- launch, focus, and restore behavior;
- process-owned popup creation and destruction;
- drawing and event handling.

The focused host runner is:

```sh
bash tests/run_panel_model_test.sh
```

It is also part of:

```sh
bash tools/verify.sh
```

## Deferred work

The following still require separate review:

- panel-scoped click-active-to-minimize permission;
- global shortcuts such as Super and Alt-Tab;
- compositor-to-panel lifecycle events replacing polling;
- pin reordering or persistent settings;
- richer icon artwork and color theme work;
- tooltips and context menus.

Do not add placeholder controls for deferred behavior. Every visible control in the shipping panel must perform a real action.

## Required visible validation

Run:

```sh
make qemu-fb-visible
```

Verify:

1. panel occupies one compact row and does not receive keyboard focus;
2. Apps opens a popup above the taskbar;
3. popup content cannot be dragged;
4. Up/Down change selection, Enter activates, and Escape closes;
5. clicking outside the popup closes it without leaving a stale window;
6. each pinned button launches its application once;
7. clicking a running application focuses it;
8. clicking a minimized application restores it;
9. grouped application instances cycle on repeated clicks;
10. state and instance markers update;
11. uptime advances and clicking it launches or activates Clock;
12. no app window is hidden behind an incorrectly sized dock;
13. Files → Editor → FAT32 workflow still works;
14. no user fault, scheduler stall, stale focus, or compositor blank frame occurs.
