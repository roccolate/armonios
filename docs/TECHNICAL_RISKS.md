# Technical Risk Register

This is the active risk register for ArmoniOS. It records verified defects, static contradictions, and missing evidence that can affect correctness or release claims.

Status and evidence terminology follows `DOCUMENTATION_POLICY.md`.

## Severity

- **P0** — can compromise kernel correctness, isolation, or the core release contract; blocks the affected release.
- **P1** — materially affects stability, reproducibility, or maintainability; must be fixed or explicitly accepted before release.
- **P2** — limited or future-facing issue; track and schedule deliberately.

## Risk summary

| ID | Severity | Area | Status | Summary |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | Output syscalls validate writable user pages and reject read-only destinations before copying. |
| RISK-002 | P0 | VFS descriptors | CLOSED | File descriptors are process-local, owner-checked, and reclaimed on process exit/fault/kill. |
| RISK-003 | P1 | QEMU desktop FAT workflow | CLOSED | The visible desktop target attaches FAT32 with GPU; automated wiring gate passes and the visible workflow has dated manual evidence. |
| RISK-004 | P1 | Desktop focus | CLOSED | Normal app windows request focus; QEMU focus markers and manual Files-to-Editor evidence exist. |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | Framebuffer, USB, network, usercopy, focus, and visible FAT wiring gates assert serial markers. |
| RISK-006 | P1 | Raspberry Pi build contract | CLOSED | `BOARD=rpi4` builds and links with explicit safe-failure stubs for unsupported display/input. |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | The eMMC implementation is experimental scaffolding and is not hardware-validated. |
| RISK-008 | P1 | Memory protection | CLOSED for v0.1; P2 future hardening remains | Kernel W^X is enforced; TTBR1/ASID/scoped-invalidation work remains future hardening. |
| RISK-009 | P1 | KLI1 application format | CLOSED | Mutable `.data`/`.bss` is forbidden by the linker script and tested. |
| RISK-010 | P2 | Scheduling documentation | CLOSED | EL0 processes are preemptive; EL1 helper threads are cooperative. |
| RISK-011 | P1 | Verification infrastructure | CLOSED | Local and hosted verification gates have evidence for the v0.1 baseline. |

## Open risk

### RISK-007 — Raspberry Pi eMMC driver is not a valid storage reference

**Severity:** P0 for the hardware track
**Affected scope:** Raspberry Pi hardware support
**Evidence:** static driver inspection

The current eMMC implementation mixes register offsets and block-size constants, uses inconsistent buffer indexing, constructs command indexes through conflicting constants, and marks the controller ready without a complete SD-card initialization sequence.

This file must be treated as experimental scaffolding, not a working driver.

**Exit criteria:** rewrite or validate the controller sequence against BCM2711 documentation; add pure command/register tests where possible; confirm sector reads on physical hardware before enabling FAT writes.

## Closed v0.1 risks

Closed risks remain summarized here so the current release claim can be audited without carrying old branch or PR history into the new first commit.

### RISK-001 — User-copy permissions

Output-producing syscalls route user-pointer writes through permission-aware helpers. `sys_user_buf_out()` walks the current process page tables and rejects read-only or missing destination pages before copying any byte.

**Evidence:** host user-copy tests, syscall helper tests, and the QEMU usercopy gate in `tools/verify.sh`.

### RISK-002 — Process-owned file descriptors

VFS file descriptors are local to the owning process. The VFS records owner PID, rejects foreign descriptor use, reclaims dead-owner capacity, and closes descriptors through the central process-exit path.

**Evidence:** process FD standalone tests, process cleanup tests, and the `process-fd-isolation` gate in `tools/verify.sh`.

### RISK-003 — Visible desktop FAT workflow

`make qemu-fb-visible` attaches the generated FAT32 image together with GPU and input devices. `tools/qemu_fb_fat_test.sh` proves FAT, display, and panel markers appear in one boot. The interactive create/edit/save/rename/reopen/delete workflow was manually verified by rocco on 2026-07-17.

**Known polish note:** Editor appeared to show one visible text line, but save/reopen persistence passed.

### RISK-004 — Spawned editor focus

Normal `libkarmdesk` application windows request focus after creation. The kernel still enforces `GUI_WINDOW_NO_FOCUS` for panel/dock-style windows.

**Evidence:** `tools/qemu_focus_test.sh` verifies focus markers; rocco manually verified Files-to-Editor keyboard focus on 2026-07-17.

### RISK-005 — Deterministic QEMU gates

Runtime QEMU launch targets remain available, but release evidence comes from marker-checking scripts. `tools/verify.sh` includes the framebuffer, USB, network, usercopy, focus, and visible FAT wiring gates.

### RISK-006 — Raspberry Pi build contract

The RPi4 backend defines the required board functions and returns explicit safe failures for unsupported display/input paths. This closes the build-contract milestone only; it is not hardware support.

### RISK-008 — Kernel RAM W^X and process-table duplication

Kernel text is RX, rodata is R/NX, data+bss+stack are RW/NX, MMIO is device+NX, and remaining RAM is RW/NX. The remaining future-design limitation is that each process still carries kernel mappings in TTBR0, with global TLB invalidation during switches.

Future hardening target: shared kernel mappings through TTBR1, ASIDs, scoped TLB invalidation, and process TTBR0 roots containing only user mappings.

### RISK-009 — KLI1 mutable storage contract

KLI1 app images explicitly forbid mutable static `.data` and `.bss`. Apps that need mutable state use stack storage or `SYS_MMAP`.

**Evidence:** `programs/apps/image.ld`, `kernel/user_image_format.h`, and `tests/run_kli1_contract_test.sh`.

### RISK-010 — Kernel-thread scheduling is cooperative

Timer IRQ preemption applies to EL0 process trap frames. EL1 helper threads change only through explicit `sched_yield()` / `sched_exit` paths.

### RISK-011 — Verification infrastructure

The v0.1 baseline has local `tools/verify.sh` evidence and a hosted workflow configuration that runs the same baseline and preserves QEMU logs.
