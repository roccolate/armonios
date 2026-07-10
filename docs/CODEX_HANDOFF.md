# Codex Handoff

This document is the first file to read before using Codex or another coding agent on ArmoniOS.

ArmoniOS is an assembly-first, bare-metal AArch64 operating system inspired by KolibriOS/MenuetOS. It is not Linux, POSIX, or libc-based.

## Current baseline

The current baseline is **v0.9 QEMU desktop**.

The immediate goal is **v1.0 QEMU desktop release candidate**:

- stable QEMU framebuffer desktop;
- stable panel, shell, editor, files, monitor, and clock apps;
- stable FAT32 root workflow through `files` and `editor`;
- release gates passing repeatedly.

## Current focus: FAT files/editor workflow

Issue #1 tracks the v1.0 FAT workflow.

The code now invalidates dynamic `/fat/<name>` VFS nodes after successful rename/delete, and host coverage exists in `tests/test_vfs_fat32_invalidation.c`.

Codex should next run the local gates and visible workflow:

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
make qemu-fb-visible
```

Manual visible flow:

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

Do not close issue #1 until gates and visible workflow are verified.

## Read-first docs

Read these before coding:

1. `README.md`
2. `docs/CURRENT_STATE.md`
3. `docs/ROADMAP.md`
4. `docs/SYSCALLS.md`
5. `docs/GUI_ABI_NOTES.md`
6. `docs/PORTING.md`
7. `docs/MEMORY_MAP.md`
8. `docs/TECH_DEBT_REVIEW.md`

## Hard rules

- Keep QEMU stable before Raspberry Pi work.
- Do not claim real Raspberry Pi 4 boot until verified on hardware.
- Do not add new syscalls unless a real userland need exists and docs/tests are updated.
- Keep app and kernel stack usage within `make stack-check` limits.
- Avoid broad rewrites.
- Prefer small, testable changes.
- Keep docs synchronized with behavior.
- Be explicit about commands run and commands not run.

## Current release gates

Run these before claiming v1.0 readiness:

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

Manual visual check:

```sh
make qemu-fb-visible
```

## ABI anchors

Current syscall ranges:

- Process: `1..8`
- Memory: `20..21`
- I/O and VFS: `40..48`
- IPC: `60..61`
- GUI/window: `70..86`
- System info: `100..102`

System info syscalls:

- `SYS_TIMEINFO = 100`
- `SYS_MEMINFO = 101`
- `SYS_PROCLIST = 102`

## Preferred work order

1. Reproduce or inspect the issue.
2. Make the smallest correct change.
3. Add or adjust host tests.
4. Run relevant gates.
5. Update docs if behavior changes.
6. Report exactly what was run and what remains.

## Output expectation for future agents

Every agent handoff should say:

- files changed;
- why each change was necessary;
- commands run;
- commands not run and why;
- risks/follow-up.

Never imply validation that did not happen.
