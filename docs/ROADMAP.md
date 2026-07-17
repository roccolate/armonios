# Roadmap

This roadmap is ordered by correctness and evidence. New features do not take priority over unresolved release blockers.

Status claims must follow `DOCUMENTATION_POLICY.md`. Active blockers and their exit criteria live in `TECHNICAL_RISKS.md`.

## Current milestone: v1.0 QEMU desktop release candidate

**Current state:** v0.9 QEMU desktop alpha  
**Goal:** a repeatable QEMU desktop whose core process, file, GUI, and test contracts can be trusted.

A v1.0 release candidate is not reached merely because the desktop appears. All P0 risks affecting QEMU v1.0 must be closed, mandatory runtime gates must assert success, and the visible FAT workflow must have a dated manual result.

## Phase 0 — Documentation and baseline recovery

- [x] Establish a documentation evidence policy.
- [x] Create an active technical risk register.
- [x] Replace broad status claims with an audited evidence matrix.
- [x] Mark Raspberry Pi work as experimental rather than supported.
- [ ] Keep issue and PR verification records synchronized with `CURRENT_STATE.md`.

## Phase 1 — v1.0 correctness blockers

### User-copy permissions — RISK-001

- [x] Store effective read/write/execute permissions with each registered user region.
- [x] Split pointer handling into `copy_from_user`, `copy_to_user`, and c-string helpers.
- [x] Reject kernel-to-user writes to read-only image pages.
- [x] Add host tests for readable, writable, boundary-crossing, and read-only destinations.
- [x] Add a QEMU test application that attempts an invalid output buffer and proves the kernel remains responsive.

Closed on `4494c55`; see `TECHNICAL_RISKS.md` for the recorded evidence and `tools/verify.sh` for the regression gate.

### Process-owned file descriptors — RISK-002

- [x] Define per-process descriptor ownership.
- [x] Make descriptor numbers local to the caller.
- [x] Close descriptors on exit, fault, kill, and explicit wait/reclaim.
- [x] Add tests proving one process cannot use or close another process's descriptor.
- [x] Add exhaustion/reuse tests across repeated process lifecycles.

Closed on `4494c55`; see `TECHNICAL_RISKS.md` for the recorded evidence and the `process-fd-isolation` gate in `tools/verify.sh`.

These two items block v1.0 regardless of desktop polish because they affect kernel correctness and isolation.

## Phase 2 — Visible desktop workflow

### Complete QEMU desktop target — RISK-003

- [x] Make `qemu-fb-visible` depend on `$(VIRTIO_BLK_IMG)`.
- [x] Attach the image through `virtio-blk-device`.
- [x] Confirm `files` sees `/fat` in the visible desktop via the deterministic `tools/qemu_fb_fat_test.sh` gate.
- [ ] A named human tester records the create/edit/save/rename/reopen/delete workflow on `make qemu-fb-visible` against a current commit.

### Correct spawned-window focus — RISK-004

- [ ] Define the focus policy for a newly created normal application window.
- [ ] Ensure an editor spawned from `files` receives keyboard focus.
- [ ] Add host coverage for the policy where practical.

### Manual FAT workflow

After the two fixes above, record the date, commit, environment, and tester for this workflow:

1. Start the complete visible desktop target.
2. Open `files` from the panel.
3. List `/fat`.
4. Create a new valid 8.3 root file.
5. Open it in `editor`.
6. Type content.
7. Save with Ctrl-S.
8. Close the editor.
9. Return to `files`.
10. Rename the file.
11. Reopen the renamed file and confirm content persists.
12. Delete the file.
13. Refresh `/fat` and confirm the old and renamed paths are gone.
14. Confirm no user fault, scheduler stall, compositor blank frame, stale path, or descriptor exhaustion.

## Phase 3 — Deterministic release gates

### Existing deterministic gates

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
```

### Gates that must be made deterministic — RISK-005

Current launch commands:

```sh
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

Required changes:

- [x] each target captures guest serial output;
- [x] each subsystem prints an explicit completion marker;
- [x] the target exits non-zero if the marker is absent;
- [x] timeout exit is not treated as success by itself;
- [ ] CI runs all non-visual mandatory gates (blocked on RISK-011).

Suggested markers:

```text
TEST framebuffer: PASS
TEST usb-hid: PASS
TEST dhcp: PASS
```

## Phase 4 — KLI1 and memory contract decisions

### KLI1 mutable storage — RISK-009

Choose one documented v1.0 contract:

- [ ] forbid `.data` and `.bss` and make application linking fail when they are emitted; or
- [ ] extend KLI1 with explicit file-size/memory-size or data/BSS metadata and test loading.

### Memory hardening — RISK-008

This is at least a v1.x hardening goal and may be promoted to v1.0 if implementation work touches the same boundary:

- [ ] move shared kernel mappings to TTBR1;
- [ ] map kernel text RX, rodata R, mutable kernel data RW/NX, and MMIO device/NX;
- [ ] introduce ASIDs;
- [ ] replace global TLB invalidation on every process switch with scoped invalidation;
- [ ] stop cloning the full RAM identity map into every process TTBR0.

## v1.0 release-candidate exit criteria

All items below are mandatory:

- [x] RISK-001 closed with host and QEMU evidence.
- [x] RISK-002 closed with isolation and cleanup tests.
- [ ] RISK-003 closed (wiring verified; interactive workflow still pending).
- [ ] RISK-004 closed (pending named human tester).
- [x] RISK-005 closed for every mandatory runtime target.
- [ ] KLI1 `.data`/`.bss` policy explicitly defined and enforced.
- [ ] Full host suite passes.
- [ ] Kernel size gate passes.
- [ ] Stack gate passes.
- [ ] All deterministic QEMU gates pass.
- [ ] Visible FAT workflow passes and is recorded.
- [ ] `README.md`, `CURRENT_STATE.md`, `ARCHITECTURE.md`, `MEMORY_MAP.md`, `SYSCALLS.md`, and this roadmap agree.
- [ ] Every remaining P1 risk is closed or has a written acceptance rationale.

## v1.1 desktop application polish

Start only after the v1.0 correctness and verification gates are satisfied.

Scope should remain mostly userland:

- `programs/apps/panel.c`
- `programs/apps/shell.c`
- `programs/apps/editor.c`
- `programs/apps/files.c`
- `programs/apps/monitor.c`
- `programs/apps/clock.c`
- `programs/libkarm/`
- `programs/libkarmdesk/`

Candidate work:

- clearer focused/minimized state in panel taskbar slots;
- prevent duplicate launches where the product behavior requires one instance;
- grouped shell help and better error messages;
- clearer editor modified/saved/open-failed status;
- improved files create/rename/delete feedback;
- selection preservation after refresh;
- small reusable userland helpers only where they reduce duplication or image size.

Do not use v1.1 polish as a reason to add new kernel syscalls without a demonstrated userland need.

## v1.5 Raspberry Pi 4 bring-up

The current `rpi4` files are experimental scaffolding, not a working port.

### Build-contract milestone — RISK-006

- [ ] `make BOARD=rpi4` compiles and links in CI.
- [ ] every required board function exists;
- [ ] unsupported capabilities return explicit safe failures;
- [ ] generic kernel code no longer assumes virtio capability functions exist on every board.

### Serial milestone

- [ ] handle the firmware's initial exception level and controlled EL2-to-EL1 transition;
- [ ] park secondary cores;
- [ ] document exact Pi model, firmware, boot files, and configuration;
- [ ] reach a stable early serial marker on physical hardware.

### Timer and memory milestone

- [ ] validate DTB memory discovery;
- [ ] reserve firmware/device regions correctly;
- [ ] validate interrupts and timer ticks on hardware;
- [ ] remove the 128 MiB QEMU-only PMM ceiling or document a temporary board limit.

### Storage milestone — RISK-007

- [ ] replace or thoroughly correct the experimental eMMC/SD implementation;
- [ ] validate controller initialization and card negotiation;
- [ ] prove read-only sector access first;
- [ ] prove FAT mount next;
- [ ] enable writes only after repeatable read and corruption tests.

### Display and input milestone

- [ ] acquire a framebuffer through the Raspberry Pi firmware mailbox or another documented path;
- [ ] validate pixel format, pitch, and cache behavior;
- [ ] initialize the BCM2711 PCIe host bridge before attempting VL805 xHCI;
- [ ] add USB HID only after serial, timer, memory, and display are stable.

Do not claim Raspberry Pi support before the hardware evidence rules in `DOCUMENTATION_POLICY.md` are met.

## v2.0 engine and multimedia runtime

Begin only after the desktop core and application ABI are stable.

Start in userland:

- fixed-step loop helper;
- sprite, tile, and blit helpers over the existing window API;
- keyboard and mouse state helpers;
- one small demo application;
- performance measurement before new kernel ABI.

Not in scope until a concrete need is proven:

- audio kernel ABI;
- GPU acceleration;
- SMP;
- full POSIX;
- browser-class networking;
- dynamic packages or a complex loader.
