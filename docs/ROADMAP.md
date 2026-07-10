# Roadmap

This roadmap is intentionally conservative. ArmoniOS should become stable in QEMU before chasing hardware or multimedia scope.

## Current milestone: v1.0 QEMU desktop release candidate

Goal: turn the current v0.9 desktop baseline into a repeatable, documented QEMU desktop release.

### Required gates

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Manual visible pass:

```sh
make qemu-fb-visible
```

### v1.0 desktop-core checklist

- [x] QEMU framebuffer desktop boots.
- [x] Panel starts and launches apps.
- [x] Shell/editor/files/monitor/clock are registered as bootfs apps.
- [x] FAT32 root create/read/write/rename/delete/list exists.
- [x] Dynamic `/fat/<name>` VFS nodes are invalidated after successful rename/delete.
- [x] Host coverage exists for VFS static unmount and FAT32 dynamic node invalidation.
- [ ] Full release gates pass after the FAT invalidation change.
- [ ] Visible `files` -> `editor` -> FAT32 workflow is manually confirmed in QEMU.
- [ ] `README.md`, `CURRENT_STATE.md`, `SYSCALLS.md`, and this roadmap match the verified behavior.

### Manual FAT workflow to confirm before v1.0

In `make qemu-fb-visible`:

1. Open `files` from the panel.
2. List `/fat`.
3. Create a new 8.3 file.
4. Open it in `editor`.
5. Type content.
6. Save with Ctrl-S.
7. Close editor.
8. Return to `files`.
9. Rename the file.
10. Reopen the renamed file and confirm content remains.
11. Delete the file.
12. Refresh/list `/fat` and confirm the deleted name is gone.
13. Confirm no user fault, scheduler stall, compositor blank frame, or stale editor path.

## v1.1 desktop app polish

Start only after v1.0 is verified or the remaining v1.0 items are explicitly marked non-blocking.

Scope should remain mostly userland:

- `programs/apps/panel.c`
- `programs/apps/shell.c`
- `programs/apps/editor.c`
- `programs/apps/files.c`
- `programs/apps/monitor.c`
- `programs/apps/clock.c`
- `programs/libkarm/`
- `programs/libkarmdesk/`

Rules:

- no broad kernel rewrite;
- no new syscall unless a userland need is proven and documented;
- keep app stack usage within the current stack-check limit;
- prefer small helpers in `libkarm` / `libkarmdesk` only when they reduce duplication or app image size;
- keep every UI behavior testable in `make qemu-fb-visible`.

Candidate polish tasks:

- clearer focused/minimized state in panel taskbar slots;
- grouped shell help output;
- better shell error messages;
- clearer editor modified/saved/open-failed status;
- clearer files create/rename/delete feedback;
- select newly created/renamed file after refresh;
- keep monitor and clock useful as small system-info demos.

## v1.5 Raspberry Pi 4 bring-up

Only after QEMU v1.0 is stable.

Goals:

- keep board code behind `drivers/board.h`;
- preserve QEMU as the primary regression target;
- bring up serial first;
- bring up framebuffer only after basic boot is stable;
- do not claim Raspberry Pi 4 support until real hardware boot is confirmed.

## v2.0 engine / multimedia runtime

Only after the desktop core is stable.

Start in userland first:

- fixed-step game loop helper;
- sprite/tile helpers using existing framebuffer/window primitives;
- input helpers over existing keyboard/mouse events;
- tiny demo app;
- no audio/kernel multimedia ABI until a concrete userland need is proven.

Not in scope until proven:

- SMP;
- full POSIX;
- browser/network stack apps;
- GPU acceleration;
- complex package system.
