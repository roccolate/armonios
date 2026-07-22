# Technical Risk Register

This is the active risk register for ArmoniOS. It records correctness defects,
architectural limits, missing evidence, and release blockers.

Status and evidence terminology follows `DOCUMENTATION_POLICY.md`. Operational
truth lives in `CURRENT_STATE.md`; planned sequencing lives in `ROADMAP.md`.

## Severity

- **P0** — can compromise kernel correctness, isolation, data integrity, or the
  core contract of the affected release; blocks that release.
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required product behavior; must be fixed or explicitly
  accepted before the affected release.
- **P2** — hardening or future-facing limitation that should be tracked and
  scheduled deliberately.

Severity is scoped. A P0 for the Raspberry Pi hardware track does not block the
QEMU-only release line when the hardware capability remains fail closed and no
hardware support claim is made.

## Risk summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | v0.1 isolation baseline |
| RISK-002 | P0 | VFS descriptors | CLOSED | v0.1 multi-process baseline |
| RISK-003 | P1 | Visible FAT workflow | CLOSED | v0.1 desktop evidence |
| RISK-004 | P1 | Desktop focus | CLOSED | v0.1 desktop usability |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | v0.1 reproducibility |
| RISK-006 | P1 | Raspberry Pi build contract | CLOSED | Board build boundary only |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | Blocks physical storage/support claims |
| RISK-008 | P1 closed for v0.1; P2 future | Memory protection architecture | PARTIAL / FUTURE | Does not block v0.1; required for stronger hardening |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | v0.1 userland image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Documentation/architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | v0.1 reproducibility |
| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 for v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 for v1 | Desktop applications | OPEN | Blocks v1 product usability |
| RISK-015 | P2 hardening | Fault-contained user copy | OPEN | Hardening; not current v0.1 blocker |
| RISK-016 | P1 | Process lifecycle | CLOSED | v0.1 parent/wait correctness |
| RISK-017 | P1 | Deferred runtime execution | OPEN | Blocks formal v0.2 promotion and bounded responsiveness claim |

## Open risks

### RISK-017 — Deferred runtime execution is not bounded

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, GUI responsiveness, input/network
fairness, exception-stack occupancy, formal v0.2 promotion  
**Current evidence:** `tests/run_runtime_service_test.sh`, QEMU marker matrix,
GitHub Actions runs `29824050151` and `29824050165`

The physical timer callback now performs only:

- tick accounting;
- `CNTP_CVAL` rearm;
- one coalescible periodic-work publication;
- scheduler counter update.

The generic IRQ dispatcher sends EOI before the runtime service polls input and
devices, drains GUI events, redraws, and polls the network. Host coverage proves
coalescing, requeue preservation, and mocked post-EOI ordering.

The remaining risk is broader than the timer callback:

- the service still runs inside the IRQ exception path;
- IRQs remain masked by the vector entry;
- the 288-byte saved exception frame remains on the EL1 stack;
- EL0 remains paused until the service and process dispatch complete;
- all queued input may be drained in one pass;
- redraw work has no measured cost budget;
- network/device polling has no operation budget;
- no maximum duration or overrun counter exists;
- no sustained-load test proves EL0 progress or no event loss.

The pending mask is a `volatile uint32_t` using non-atomic read-modify-write. That
is sufficient only under the documented single-core, masked-IRQ, one-consumer
model. It is not safe evidence for SMP or concurrent publishers.

**Failure mode:** sustained input, redraw, USB, or network activity can lengthen
one exception path and delay EL0 and all normal IRQ handling. Producer overload
can also fill fixed queues without a complete fairness/overflow accounting story.

**Exit criteria:**

1. instrument last, maximum, and cumulative service duration;
2. record input, device, packet, redraw, and pending-work high-water marks;
3. impose independent per-pass budgets plus a global time budget;
4. preserve or republish work bits when a budget is exhausted;
5. count queue overflow and budget exhaustion explicitly;
6. add deterministic QEMU stress tests with an EL0 heartbeat under simultaneous
   input/network/redraw load;
7. prove existing focus, usercopy, FAT, framebuffer, USB, and network gates remain
   green;
8. document whether the bounded bottom half remains permanent or is later
   promoted to a wakeable EL1 service.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1  
**Affected scope:** persistent storage, Files, Editor, Shell, ext2 integration,
reboot workflow  
**Current evidence:** static implementation audit plus host/QEMU/manual narrow FAT
evidence

Useful foundations exist:

- a fixed generic mount table;
- mount callbacks for open/list/unlink/rename;
- process-local descriptors;
- a primary-MBR FAT32 parser;
- bounded block views;
- a writable root-only FAT32 bridge.

The current platform remains too narrow:

- VFS is capped at 24 nodes, four mounts, eight descriptors per process, and
  64-byte paths;
- no common path resolver exists;
- directory entries are newline-separated strings rather than structured records;
- metadata is essentially file size only;
- no `mkdir`, truncate, structured stat/readdir, or filesystem-info ABI exists;
- FAT32 supports root-directory 8.3 files only;
- no subdirectories, long names, GPT/extended partitions, general FAT variants,
  journaling, crash recovery, or broad compatibility evidence exists;
- no ext2 implementation exists;
- no combined files/settings reboot-persistence gate exists.

**Failure mode:** application polish is forced around temporary root-only and
fixed-buffer assumptions, creating rework and preventing the required v1 user
workflow.

**Exit criteria:** land the v0.3-v0.4 roadmap:

1. block-device metadata and read/write/flush/read-only interface;
2. common normalized path resolution and mount boundaries;
3. filesystem driver interface with structured metadata/directory operations;
4. documented ABI additions for mkdir/truncate/stat/readdir/fsinfo;
5. real FAT long-name and directory support;
6. host image and malformed-image tests;
7. QEMU nested-directory and reboot-persistence tests;
8. Files and Shell no longer depend on 8.3 root names.

### RISK-014 — Desktop applications are not complete daily tools

**Severity:** P1 for v1  
**Affected scope:** Files, Editor, Shell, Control/Settings, Monitor, Panel, Clock  
**Current evidence:** application source audit and dated visible workflow

The seven shipping applications are real EL0 programs and useful demonstrations,
but they do not yet satisfy the v1 product workflow.

Current concrete limits include:

- Files is fixed to `/fat`, displays at most eight entries, and understands only
  8.3 root files;
- Editor has a 512-byte buffer and intentionally renders only the caret line;
- Shell has useful history/scrollback and diagnostic commands but lacks normal
  copy/move/remove/mkdir/touch/edit/open/df workflows;
- Control/Settings has narrow observable persistence;
- Monitor is informational rather than a process-management tool;
- no shared userland heap/container layer or widget toolkit exists;
- no 30-minute stable manual session is recorded;
- no final reboot workflow proves files and settings together.

**Failure mode:** ArmoniOS can demonstrate kernel facilities but cannot yet serve
as a coherent small desktop for normal file and configuration tasks.

**Exit criteria:** land the v0.5-v0.8 roadmap:

1. shared `libkarm` runtime helpers and `libkarmdesk` widgets;
2. directory-aware, scrollable Files with metadata and file operations;
3. multi-line, scrollable Editor with safe truncating save and Save As;
4. useful Shell file/process/system commands;
5. at least three persistent observable settings;
6. actionable Monitor process controls;
7. repeated panel/window lifecycle verification;
8. reboot-persistence QEMU gate and dated final visible workflow.

### RISK-015 — User copy is not fault-contained

**Severity:** P2 hardening  
**Affected scope:** syscall exception recovery and future concurrent address-space
changes  
**Current evidence:** static user-copy inspection plus permission/boundary tests

Permission-aware validation and kernel-owned syscall payload boundaries are
implemented. Input and output pages are checked before copying, and state-consuming
outputs validate the whole destination before dequeueing.

The final byte transfer still uses ordinary EL1 loads/stores. ArmoniOS has no
exception table, fixup target, or recoverable copy primitive that can turn an
unexpected translation fault during the transfer into a syscall error.

The current single-core model and lack of concurrent user page-table mutation
reduce the practical race surface, but they do not constitute fault containment.

**Exit criteria:**

- add fault-recoverable `copy_from_user` / `copy_to_user` primitives;
- add targeted lower-level exception/fixup tests;
- preserve `ERR_INVAL` and `ERR_PERM` contracts;
- prove a bad/racy address cannot enter the fatal EL1 path;
- document any restrictions on mappings that can change during a syscall.

### RISK-007 — Raspberry Pi storage lacks physical evidence

**Severity:** P0 for the hardware track  
**Affected scope:** any physical Raspberry Pi boot/storage/support claim  
**Current evidence:** build, controller host tests, diagnostic package, no
physical serial capture

Implemented scaffolding includes:

- SDHCI/EMMC2 controller core;
- firmware mailbox clock query;
- broken-card-detect adaptation;
- failure telemetry;
- primary-MBR FAT32 discovery;
- bounded partition view;
- opt-in read-only diagnostic image.

The normal RPi4 board advertises no storage/display/input capability. No physical
clock response, card initialization, sector-zero read, FAT geometry read,
framebuffer, input, or desktop boot has been confirmed.

**Exit criteria:**

1. controlled CPU entry and secondary-core parking;
2. repeatable serial telemetry across cold boots;
3. timer and memory-map validation;
4. card/controller initialization evidence;
5. sector-zero and FAT geometry reads from disposable media;
6. only then consider exposing read-only storage capability;
7. writes require a later recovery-oriented disposable-media milestone.

## Future hardening tracked under RISK-008

### Kernel address-space architecture

Kernel W^X is implemented for the v0.1 baseline:

- text RX;
- rodata R/NX;
- data, BSS, and stack RW/NX;
- MMIO device/NX;
- remaining RAM RW/NX.

The current process TTBR0 roots still duplicate the kernel/RAM identity map.
Every process switch performs broad TLB invalidation. Stronger isolation and
scalability require:

- shared kernel mappings through TTBR1;
- user-only process TTBR0 roots;
- ASIDs;
- scoped TLB invalidation;
- explicit global/non-global mapping policy;
- tests for process switches and stale translations.

This does not invalidate the v0.1 W^X claim, but it remains future hardening.

## Closed risks

### RISK-016 — Parent-owned zombie lifecycle

Spawn records a parent PID. `sys_wait` accepts only a zombie child of the caller.
Later spawns cannot reclaim that child before its exit status is collected.
Automatic reclamation is limited to kernel-owned or orphaned zombies.

**Evidence:** `tests/run_process_parent_wait_test.sh`, normal build, and complete
verification matrix.

### RISK-012 — Kernel-owned syscall buffers

VFS data and paths, argv, IPC messages, GUI outputs, and information outputs
cross through bounded kernel-owned temporaries before lower subsystems operate.
State-consuming outputs validate the destination before dequeueing.

**Evidence:** host boundary regressions and full verification matrix.

### RISK-001 — Permission-aware user-copy destinations

Output-producing syscalls walk the current process page tables and reject missing
or read-only destination pages before copying any byte.

**Evidence:** host user-copy tests, syscall helper tests, and QEMU invalid-output
regression.

### RISK-002 — Process-owned file descriptors

Descriptors are local to the current process. Kernel handles record owner PID,
foreign use is rejected, dead owners are reaped, and process exit closes all
owned descriptors.

**Evidence:** process-FD isolation and cleanup tests.

### RISK-003 — Visible desktop FAT workflow

The visible target attaches FAT32 with GPU and input. Automated wiring markers
pass. Rocco manually verified create/edit/save/rename/reopen/delete on
2026-07-17.

**Recorded limitation:** Editor rendered one visible line; persistence still
passed. Source inspection confirms the current renderer intentionally draws only
the caret line.

### RISK-004 — Spawned Editor focus

Normal `libkarmdesk` windows request focus after creation. Panel/dock-style
windows retain no-focus policy. QEMU markers and the manual Files-to-Editor pass
confirm the current flow.

### RISK-005 — Deterministic QEMU gates

Release evidence uses marker-checking scripts for framebuffer, USB, network,
usercopy, focus, FAT smoke, and visible-target wiring rather than treating launch
targets as tests.

### RISK-006 — Raspberry Pi build contract

The board backend defines required functions and fails unsupported display/input
paths explicitly. This closes compilation/linkage only, not physical support.

### RISK-009 — KLI1 mutable-storage contract

Shipping images forbid mutable static `.data` and `.bss`; applications use stack
or `SYS_MMAP` storage. Linker assertions and synthetic regression tests enforce
the rule.

### RISK-010 — Scheduling description

EL0 process dispatch is preemptive. EL1 helper threads are cooperative. The
post-EOI runtime bottom half is a third, separate execution mode and is described
in `RUNTIME_SERVICE.md`.

### RISK-011 — Verification infrastructure

The repository has a one-command local baseline and hosted workflows that build,
run host tests, execute deterministic QEMU checks, and retain relevant logs and
the kernel ELF.

The latest validated PR-tree runs are:

- `29824050151` — success;
- `29824050165` — success.

The final merge commit did not receive a separate workflow execution; evidence
must retain that distinction.
