# Technical Risk Register

This is the active risk register for ArmoniOS. It records verified defects, static contradictions, and missing evidence that can affect correctness or release claims.

Status and evidence terminology follows `DOCUMENTATION_POLICY.md`.

## Severity

- **P0** — can compromise kernel correctness, isolation, or the core release contract; blocks the affected release.
- **P1** — materially affects stability, reproducibility, or maintainability; must be fixed or explicitly accepted before release.
- **P2** — limited or future-facing issue; track and schedule deliberately.

## Open risks

| ID | Severity | Area | Status | Summary |
|---|---:|---|---|---|
| RISK-001 | P0 | Syscall boundary | OPEN | Output buffers are range-checked but not checked for write permission. |
| RISK-002 | P0 | VFS/process isolation | OPEN | VFS descriptors are global rather than owned by a process. |
| RISK-003 | P1 | QEMU desktop target | OPEN | `qemu-fb-visible` does not attach the FAT32 virtio block image. |
| RISK-004 | P1 | Desktop focus | OPEN | A spawned editor window does not request focus after creation. |
| RISK-005 | P1 | Runtime verification | OPEN | Several QEMU release gates launch a VM but do not assert success markers. |
| RISK-006 | P1 | Raspberry Pi 4 build | OPEN | The RPi4 board backend does not satisfy the complete board interface. |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | The current eMMC implementation contains contradictory register and command handling and is not hardware-validated. |
| RISK-008 | P1 | Memory protection | OPEN | Process page tables identity-map the full RAM range as kernel RWX. |
| RISK-009 | P1 | KLI1 application format | OPEN | Mutable `.data` and `.bss` behavior is not an explicit, tested image-format contract. |
| RISK-010 | P2 | Scheduling documentation | OPEN | EL0 processes are preemptive, but EL1 kernel threads are cooperative. |

## RISK-001 — Output buffer permissions are not enforced

**Severity:** P0  
**Affected release:** v1.0 QEMU desktop  
**Evidence:** static code inspection

`sys_user_buf_in()` and `sys_user_buf_out()` currently use the same region-membership check. Process regions record ownership metadata but do not expose read/write/execute permission metadata to the syscall helper layer.

The loader maps application images read/execute and stacks read/write. A syscall that writes to user memory can therefore accept an address inside a registered but non-writable image region. A kernel write through that address can fault in EL1 and enter the fatal exception path.

**Required direction:**

- record effective user permissions for every process region;
- provide separate `copy_from_user`, `copy_to_user`, and c-string helpers;
- require write permission for kernel-to-user copies;
- add negative tests for output syscalls targeting read-only image memory;
- define recoverable behavior for a bad user destination without halting the kernel.

**Exit criteria:** automated host coverage plus a QEMU user program proving the kernel rejects a read-only destination and remains responsive.

## RISK-002 — File descriptors are global

**Severity:** P0  
**Affected release:** v1.0 QEMU desktop  
**Evidence:** static code inspection

`kernel/vfs.c` owns one fixed `g_open_files` table with eight slots for the entire kernel. Syscall-visible descriptors do not encode or validate an owner PID.

Consequences include cross-process close/read/write operations, shared offsets, descriptor leaks after process exit, and system-wide exhaustion by one process.

**Required direction:**

- move descriptor ownership into `process_t` or add owner PID and per-process lookup;
- make descriptor numbers local to the caller;
- close every descriptor during process cleanup;
- test cross-process isolation and process-exit cleanup.

**Exit criteria:** host tests prove process A cannot use or close process B's descriptor, and descriptors are reclaimed after exit/fault/kill.

## RISK-003 — Visible desktop target has no FAT disk

**Severity:** P1  
**Affected release:** v1.0 QEMU desktop  
**Evidence:** Makefile inspection and manual QEMU observation recorded in issue #1

The documented visible FAT workflow uses `make qemu-fb-visible`, but the target currently attaches GPU and input devices without the generated virtio block image. The `files` application therefore reports FAT as unavailable.

**Exit criteria:** the visible target attaches `$(VIRTIO_BLK_IMG)` and the full create/edit/save/rename/reopen/delete workflow passes.

## RISK-004 — Spawned editor lacks initial focus

**Severity:** P1  
**Affected release:** v1.0 QEMU desktop  
**Evidence:** application and GUI code inspection plus issue #1 manual observation

The editor creates and titles a window but does not focus it. Window creation only selects a window automatically when no other application window is focused, so keyboard input can remain with `files` after spawning the editor.

**Exit criteria:** define and test the window-manager focus policy, then verify the visible `files` to `editor` workflow.

## RISK-005 — QEMU gates are not all deterministic tests

**Severity:** P1  
**Affected release:** v1.0 QEMU desktop  
**Evidence:** Makefile inspection

`qemu-fs-test` captures serial output and asserts markers. `qemu-fb`, `qemu-usb`, and `qemu-net` currently launch QEMU and rely on an external timeout without checking a subsystem-specific success marker.

A VM that hangs after partial initialization may therefore look equivalent to a successful timed smoke run.

**Exit criteria:** every mandatory runtime gate captures output and asserts explicit final markers. The command must exit non-zero when the marker is absent.

## RISK-006 — Raspberry Pi board contract is incomplete

**Severity:** P1  
**Affected release:** v1.5 hardware bring-up  
**Evidence:** static board-interface inspection

The generic board contract requires virtio-input functions that generic kernel code calls unconditionally. The current RPi4 backend does not provide the complete set of functions.

The repository therefore must not claim that `BOARD=rpi4` is build-verified until a clean compile and link is recorded.

**Exit criteria:** CI builds and links `make BOARD=rpi4`; unsupported capabilities return explicit safe failures; no generic kernel symbol remains unresolved.

## RISK-007 — Raspberry Pi eMMC driver is not a valid storage reference

**Severity:** P0 for the hardware track  
**Affected release:** v1.5 hardware bring-up  
**Evidence:** static driver inspection

The current implementation mixes register offsets and block-size constants, uses inconsistent buffer indexing, constructs command indexes through conflicting constants, and marks the controller ready without a complete SD-card initialization sequence.

This file must be treated as experimental scaffolding, not a working driver.

**Exit criteria:** rewrite or validate the controller sequence against BCM2711 documentation; add pure command/register tests where possible; confirm sector reads on physical hardware before enabling FAT writes.

## RISK-008 — Full RAM is kernel RWX in each process table

**Severity:** P1  
**Affected release:** post-v1.0 hardening unless a release decision promotes it  
**Evidence:** static VMM and loader inspection

The bootstrap table and every process page table identity-map the detected RAM range as read/write/execute for EL1. EL0 access is restricted, but kernel W^X is absent and each process duplicates the full kernel mapping in TTBR0.

This increases page-table cost, forces global TLB invalidation during switches, and leaves kernel data executable.

**Target direction:** shared kernel mappings through TTBR1, section-specific permissions, ASIDs, and scoped TLB invalidation.

**Exit criteria:** architecture tests and QEMU verification show RX text, R rodata, RW/NX data/heap/stacks, device/NX MMIO, and isolated TTBR0 user mappings.

## RISK-009 — KLI1 mutable storage contract is undefined

**Severity:** P1  
**Affected release:** v1.0 or v1.1, depending on whether v1.0 formally forbids mutable static storage  
**Evidence:** application linker-script inspection

The KLI1 image script explicitly places header, text, rodata, and end marker. It does not define a tested load contract for initialized `.data` or zero-initialized `.bss`.

Applications currently avoid this through restricted coding patterns and anonymous mappings, but the toolchain does not clearly reject unsupported mutable static storage.

**Exit criteria:** either:

- formally forbid `.data`/`.bss` and make the link fail when they are emitted; or
- extend KLI1 with file-size/memory-size or explicit data/BSS metadata and test loader behavior.

## RISK-010 — Kernel-thread scheduling is cooperative

**Severity:** P2  
**Affected release:** documentation accuracy now; implementation only when needed  
**Evidence:** scheduler inspection

Timer IRQ preemption applies to EL0 process trap frames. EL1 helper threads change only through explicit yield/exit paths. Documentation must describe the system as preemptive for EL0 and cooperative for kernel threads.

**Exit criteria:** documentation remains accurate, or a separately designed EL1 preemption mechanism is implemented and tested.

## Closed risks

No risk may be moved here without the evidence and exit criteria required by `DOCUMENTATION_POLICY.md`.
