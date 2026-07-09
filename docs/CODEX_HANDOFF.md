# Codex Handoff

This is the short operational guide for using Codex or another coding agent on
ArmoniOS. It is intentionally stricter than the README so automated changes do
not drift away from the current release plan.

## Current Baseline

- Current target: **v0.9 QEMU desktop baseline**.
- Next target: **v1.0 stable, repeatable QEMU desktop release**.
- Primary board: QEMU `virt` / AArch64 / Cortex-A72.
- Planned hardware board: Raspberry Pi 4 / 5, but not before the v1.0 QEMU
  desktop is stable.
- Kernel size limit: `KERNEL_SIZE_LIMIT`, currently 100000 bytes.
- Last documented size: see `docs/CURRENT_STATE.md` and `docs/ROADMAP.md`.

## Read First

Before changing code, read these files in this order:

1. `README.md` — public project overview and user-visible claims.
2. `docs/CURRENT_STATE.md` — live system snapshot.
3. `docs/ROADMAP.md` — current version targets and release gates.
4. `docs/ARCHITECTURE.md` — subsystem architecture.
5. `docs/SYSCALLS.md` — syscall ABI reference.
6. `docs/GUI_ABI_NOTES.md` — GUI/window ABI invariants.
7. `docs/PORTING.md` — board-layer rules.
8. `docs/CONTRIBUTING.md` — coding and PR rules.

Treat `docs/TECH_DEBT_REVIEW.md` as closed history, not an active backlog.

## Hard Rules

- Do not add POSIX, libc, hosted runtime, or broad compatibility-layer work.
- Do not claim Raspberry Pi hardware support until a real board reaches a
  documented serial/boot milestone.
- Do not renumber syscalls.
- Do not change GUI event struct size, event ids, or syscall argument order
  without updating docs, wrappers, and ABI tests in the same commit.
- Do not introduce large subsystems during v1.0 stabilization.
- Do not start kernel-side engine/audio/multimedia work unless a userland demo
  proves the missing capability.
- Keep board-specific constants behind `drivers/boards/<board>/` and
  `drivers/board.h`.
- Keep app persistent state out of the fixed 4 KB EL0 stack; prefer anonymous
  user mappings for larger state.
- Keep kernel size under the configured limit.

## Required Release Gates

For kernel, driver, syscall, boot, ABI, Makefile, and userland changes that
affect shipped app images, run:

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

For visible desktop behavior, also run:

```sh
make qemu-fb-visible
```

Manual visible-pass checklist lives in `docs/ROADMAP.md`.

## Documentation Sync Rules

Update documentation in the same change when any of these move:

| Change | Docs/files to update |
| --- | --- |
| New syscall or ABI change | `kernel/syscall_numbers.h`, `docs/SYSCALLS.md`, wrappers, ABI tests |
| GUI/window syscall or event change | `docs/SYSCALLS.md`, `docs/GUI_ABI_NOTES.md`, `programs/libkarmdesk/gui.h`, window ABI tests |
| Board interface change | `drivers/board.h`, `docs/PORTING.md`, `docs/ARCHITECTURE.md` if architecture changes |
| Build target or release gate change | `README.md`, `docs/ROADMAP.md`, `docs/CONTRIBUTING.md` |
| Current app/user-visible desktop behavior | `README.md`, `docs/CURRENT_STATE.md`, `docs/ROADMAP.md` |
| Memory/linker layout change | `docs/MEMORY_MAP.md`, relevant linker script note |
| Engine/multimedia direction | `docs/ENGINE_MULTIMEDIA.md`, `docs/ROADMAP.md` |

## Current ABI Anchors

- Process syscalls: `1..8`.
- Memory syscalls: `20..21`.
- VFS/I/O syscalls: `40..48`.
- IPC syscalls: `60..61`.
- GUI/window syscalls: `70..86`.
- System-info syscalls: `100..102`.
- Userland process/memory/I/O/IPC/system-info wrappers live in
  `programs/libkarm`.
- Desktop/window wrappers live in `programs/libkarmdesk`.

## Preferred Work Order

1. Fix regressions found by gates.
2. Keep v1.0 docs and tests synchronized.
3. Finish desktop-core usability checks for panel/shell/editor/files.
4. Only then start minimal userland engine helpers.
5. Only after v1.0 stability, resume Raspberry Pi 4 bring-up.

## Output Expectations For Coding Agents

Every automated change should end with:

- what files changed;
- why each change was necessary;
- which commands were run;
- which commands could not be run and why;
- any remaining risk or follow-up.

If no build/test commands were run, say so explicitly. Do not imply validation
that did not happen.
