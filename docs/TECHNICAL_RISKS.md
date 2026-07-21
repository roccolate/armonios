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
| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | VFS, argv, IPC, GUI, and information syscall payloads cross through bounded kernel-owned temporaries. |
| RISK-015 | P2 hardening | Fault-contained copy | OPEN | User-copy transfers remain ordinary EL1 loads/stores without exception recovery. |
| RISK-013 | P1 for v1 | Storage/VFS | OPEN | Current VFS/FAT path is too narrow for the v1 filesystem target. |
| RISK-014 | P1 for v1 | Desktop apps | OPEN | Current apps are useful demos, not complete daily-use tools. |

## Open risks

### RISK-007 — Raspberry Pi storage lacks physical evidence

**Severity:** P0 for the hardware track
**Affected scope:** Raspberry Pi hardware support
**Evidence:** controller host tests, build-verified diagnostic image, no physical serial run

The SDHCI controller core, firmware clock query, broken-card-detect adapter, failure telemetry, primary FAT32 MBR discovery, and bounded partition view are implemented. The opt-in image is read-only and the normal RPi4 board still advertises no capabilities.

The remaining blocker is physical evidence, not a claim that the old scaffold is production-ready. No clock response, card initialization, sector read, or FAT geometry has been confirmed on a Raspberry Pi 4.

**Exit criteria:** boot the diagnostic image on real hardware; record repeatable serial telemetry across cold boots; confirm sector-zero and FAT geometry reads; only then consider exposing `BOARD_CAP_STORAGE`. Writes remain a later disposable-media milestone.

### RISK-015 — User-copy is not fault-contained

**Severity:** P2 hardening
**Affected scope:** syscall exception recovery and hostile/racy address-space changes
**Evidence:** static user-copy inspection

Permission-aware validation and kernel-owned buffer boundaries are implemented. The final byte transfers still use ordinary EL1 loads/stores. ArmoniOS currently has no exception-table or recovery mechanism that can turn an unexpected translation fault during copy into a syscall error.

This is distinct from buffer ownership: lower subsystems no longer operate on caller pointers, but the boundary copy itself is not hardened against a mapping changing unexpectedly after validation.

**Exit criteria:** add fault-recoverable copyin/copyout primitives, targeted exception-path tests, and preserve the current `ERR_INVAL`/`ERR_PERM` contracts without crashing EL1.

### RISK-012 — Kernel-owned syscall buffers

VFS buffers and paths, argv, IPC messages, GUI output, and system-information output are copied through bounded kernel-owned temporaries before lower layers use them. Invalid or read-only destinations are rejected before state-consuming receives dequeue data.

**Evidence:** host user-copy boundary regressions plus the complete `tools/verify.sh` matrix recorded with the closing change.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1
**Affected scope:** VFS, FAT, desktop persistence, Shell/Files behavior
**Evidence:** static architecture inspection and current documented filesystem scope

The current VFS now has a small generic mount table and filesystem callbacks, and the storage layer has a reusable primary-MBR FAT32 parser plus bounded block views. FAT32 is still limited to root-directory 8.3 names and does not provide long names, subdirectories, GPT/extended partitions, a common path resolver, structured directory entries, or ext2.

This is acceptable for v0.1, but it cannot satisfy the v1 requirement that a
user browse persistent storage, create folders, edit files in directories, and
verify persistence after reboot.

**Exit criteria:** the v0.3-v0.4 storage roadmap lands with richer block-device metadata, common path resolution, structured filesystem operations, real FAT long-name/directory support, host image tests, QEMU persistence tests, and updated syscall documentation.

### RISK-014 — Desktop applications are not complete daily tools

**Severity:** P1 for v1
**Affected scope:** Files, Editor, Shell, Settings/Control, Monitor, Panel
**Evidence:** static application inspection plus current visible workflow evidence

The current applications are useful as desktop and syscall demos, but v1 needs
normal workflows. Files is tied to the narrow `/fat` surface, Editor has a
single-visible-line polish limitation in the recorded manual workflow, Shell
lacks basic file-management commands, Settings persistence is narrow, and
Monitor is informational rather than an operator tool.

**Exit criteria:** the v0.5-v0.8 application roadmap lands with shared runtime
helpers and widgets; Files, Editor, Shell, Settings, Monitor, Panel, and Clock
support the v1 manual workflow; persistence survives a QEMU reboot; visible
manual evidence is recorded.

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
