# Codex Handoff

This is the first file an AI coding agent should read before changing ArmoniOS.

ArmoniOS is a freestanding AArch64 operating system. It is not Linux, POSIX, libc-based, or a hosted desktop application.

## Read-first order

1. `docs/DOCUMENTATION_POLICY.md`
2. `docs/CURRENT_STATE.md`
3. `docs/TECHNICAL_RISKS.md`
4. `docs/ROADMAP.md`
5. `docs/ARCHITECTURE.md`
6. `docs/SYSCALLS.md`
7. `docs/MEMORY_MAP.md`
8. `docs/GUI_ABI_NOTES.md`
9. `docs/PORTING.md`
10. relevant source and tests

`docs/TECH_DEBT_REVIEW.md` is historical and must not be treated as proof that no current debt exists.

## Current baseline

- **Product label:** v0.9 QEMU desktop alpha
- **Target:** v1.0 QEMU desktop release candidate
- **Verified primary platform:** QEMU `virt`
- **Raspberry Pi status:** experimental source only, not build- or hardware-verified

The current baseline includes real EL0 processes, syscalls, VFS/FAT32, a graphical compositor, six applications, QEMU virtio paths, and extensive host tests.

Do not upgrade that description into “stable” or “secure.”

## Highest-priority work

The work order is not open-ended. Address these before application polish or new subsystems.

### P0 — RISK-001: permission-aware user copies

Current user-pointer helpers verify only that a range belongs to the process. Input and output helpers apply the same check, so kernel-to-user copies do not prove the destination is writable.

Required outcome:

- effective permissions recorded per process region;
- `copy_from_user`, `copy_to_user`, and c-string helpers;
- output calls reject read-only image memory;
- invalid user destinations cannot halt EL1;
- host and QEMU negative tests.

### P0 — RISK-002: process-owned file descriptors

The VFS now exposes process-local descriptors with central cleanup.

Verified outcome:

- descriptors are owned by or local to a process;
- cross-process access is rejected;
- offsets are not unintentionally shared;
- process exit/fault/kill closes descriptors;
- lifecycle and isolation tests run through `tests/run_vfs_process_fd_test.sh`.

### P1 — Complete visible FAT workflow

- `$(VIRTIO_BLK_IMG)` is attached to `qemu-fb-visible`;
- editor focus from `files` is covered by `tools/qemu_focus_test.sh`;
- rocco manually verified create/edit/save/rename/reopen/delete on 2026-07-17;
- recorded baseline: `8c8400bcddd754d879e6e21b787b8d028a6c6036` working tree via `make qemu-fb-visible`.

### P1 — Deterministic QEMU gates

`qemu-fs-test`, `tools/qemu_focus_test.sh`, `tools/qemu_usercopy_test.sh`, `tools/qemu_marker_test.sh all`, and `tools/qemu_fb_fat_test.sh` check serial markers. The raw `qemu-fb`, `qemu-usb`, and `qemu-net` targets remain launch commands.

Use the marker scripts for release evidence. An external timeout is not a pass condition.

## Hard rules

- Do not assume documentation is correct; reconcile claims with code and tests.
- Do not treat code presence as build or runtime evidence.
- Do not claim a check was run unless the current agent actually ran it or clearly attributes an existing record.
- Keep QEMU stable before hardware or multimedia work.
- Avoid broad rewrites; make the smallest complete change that closes one risk or exit criterion.
- Preserve syscall numbers and user-visible struct layouts.
- Add tests before closing a risk.
- Update `TECHNICAL_RISKS.md` and `CURRENT_STATE.md` when risk or evidence changes.
- Update README last.
- Keep repository code and technical docs in English.

## Architecture facts that agents must preserve

- EL0 processes are preemptive through IRQ trap frames.
- EL1 helper threads are cooperative.
- Each process has a separate TTBR0 root.
- Current process tables also identity-map full RAM for EL1 with W^X (text RX, rodata R+NX, data+bss+stack RW+NX, MMIO device+NX, remaining RAM RW+NX). ALIGN(4096) before `.data` keeps rodata and data on separate pages.
- TTBR1 and ASIDs are not used.
- The PMM manages at most 128 MiB.
- File descriptors are process-local and reclaimed centrally on process exit/fault/kill.
- KLI1 explicitly forbids mutable `.data`/`.bss` in flat application images.
- FAT32 scope is root-only and short-name-only.
- USB scope is basic directly attached boot-protocol HID; no hub claim.
- Networking is a minimal internal DHCP path, not a socket API.

## Verification commands

### Deterministic or established checks

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
bash tools/qemu_usercopy_test.sh
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_fb_fat_test.sh
```

### Current launch targets requiring explicit marker inspection

```sh
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Do not report the second group as passing tests merely because timeout exit code 124 was accepted. Use `bash tools/qemu_marker_test.sh all` or `bash tools/verify.sh` for deterministic marker evidence.

### Visible desktop

```sh
make qemu-fb-visible
```

The current target includes the FAT block image, GPU, keyboard, and mouse. The deterministic wiring half of RISK-003 is covered by `tools/qemu_fb_fat_test.sh`; the documented FAT workflow was manually verified by rocco on 2026-07-17. Editor appears to show one visible text line, which is tracked as polish unless it blocks a concrete workflow.

## Required workflow for an agent task

1. Read the active risk and its exit criteria.
2. Inspect implementation and tests; do not trust comments alone.
3. Reproduce the defect when tools permit.
4. Make the smallest complete change.
5. Add focused tests that fail before the fix and pass after it.
6. Run the smallest relevant checks first.
7. Run broader gates required by the affected subsystem.
8. Update documentation from actual evidence.
9. Report commands not run and why.
10. Leave the risk open if exit criteria are only partially satisfied.

## Handoff format

Every agent handoff must include:

```text
Scope:
Files changed:
Behavior changed:
Risk IDs affected:

Commands run:
- command -> exact result/marker

Manual checks:
- workflow -> result and tester

Not run:
- check -> reason

Remaining risks:
- item
```

## Do not do next

Unless explicitly requested after v1.0 blockers are addressed, do not:

- add audio;
- add a game engine to the kernel;
- add new network protocols;
- start SMP;
- claim Raspberry Pi boot;
- expand FAT32 features;
- rewrite the compositor;
- introduce POSIX/libc compatibility;
- add new syscalls for application polish.

The immediate objective is a trustworthy, repeatable QEMU desktop baseline.
