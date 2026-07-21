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
QEMU-only release line while unsupported capabilities remain fail closed and no
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
| RISK-008 | P1 closed for v0.1; P2 future | Memory protection architecture | PARTIAL / FUTURE | Required for stronger hardening |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | v0.1 userland image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | v0.1 reproducibility |
| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 for v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 for v1 | Desktop applications | OPEN | Blocks v1 usability |
| RISK-015 | P2 hardening | Fault-contained user copy | OPEN | Future hostile/racy mapping hardening |
| RISK-016 | P1 | Process lifecycle | CLOSED | Parent/wait correctness |
| RISK-017 | P1 | Deferred runtime execution | OPEN; MEASUREMENT PHASE LANDED | Blocks formal v0.2 promotion and bounded responsiveness claim |

## Open risks

### RISK-017 — Deferred runtime execution is measured but not bounded

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, GUI responsiveness, input/network
fairness, exception-stack occupancy, formal v0.2 promotion  
**Tracking:** issue #43  
**Current candidate:** PR #44

The physical timer callback performs only tick accounting, `CNTP_CVAL` rearm,
one coalescible periodic-work publication, and scheduler accounting. The generic
IRQ dispatcher sends EOI before the runtime service polls input/devices, drains
GUI events, redraws, and polls the network.

#### Measurement now implemented

The kernel-internal runtime snapshot records:

- accepted request count;
- coalesced request count;
- non-empty and empty consumer invocations;
- passes that leave work requeued;
- last, maximum, and cumulative duration in AArch64 generic-counter ticks;
- passes exceeding the configured observation threshold;
- generic-counter frequency and threshold;
- pending work and last consumed work bits.

The production clock reads `CNTPCT_EL0`; `CNTFRQ_EL0` supplies the conversion
frequency. `timer_init()` sets the initial threshold to one timer interval. At
the current 100 Hz rate, the threshold is approximately 10 ms. This detects a
pass that consumed more than a complete nominal timer interval. It is not the
final acceptable runtime budget.

`tests/run_runtime_service_test.sh` injects a deterministic host counter and
covers coalescing, EOI ordering, requeue preservation/counting, last/max/total
duration, interval overrun counting, snapshot state, null snapshot handling, and
reset semantics. Candidate `Verify ArmoniOS` run `29827738752` completed the
build, size, host, RPi4, stack, and FAT32 smoke matrix successfully. The final
PR-tree result of `CI - Tests` run `29827738742` must be recorded before merge.

#### Why the risk remains open

The service still runs inside the IRQ exception path:

- normal IRQs remain masked by vector entry;
- the 288-byte saved exception frame remains on the EL1 stack;
- EL0 remains paused until the service and process dispatch complete;
- all available input events may be drained in one pass;
- USB/device polling has no operation budget;
- network receive work has no packet/frame budget;
- redraw work has no damage or time budget;
- no per-class high-water or budget-exhaustion counters exist;
- no sustained-load QEMU test proves EL0 progress or explicit loss accounting.

The pending mask is a `volatile uint32_t` using non-atomic read-modify-write.
That remains valid only under the documented single-core, masked-IRQ,
one-consumer model. Telemetry snapshots follow the same non-SMP assumption.

**Failure mode:** sustained input, redraw, USB, or network activity can extend one
exception path and delay EL0 and all normal IRQ handling. Fixed queues can fill
under producer overload without a complete fairness and overflow-accounting
story.

**Remaining exit criteria:**

1. add per-pass input-event, queue high-water, device-work, packet/frame, redraw,
   full-redraw, damage, and overflow metrics;
2. select independent per-class budgets using measured evidence;
3. add a global counter-tick deadline smaller than or justified against one timer
   interval;
4. preserve or republish specific pending bits when a class or global budget is
   exhausted;
5. count queue overflow and budget exhaustion explicitly;
6. add deterministic QEMU stress tests with an EL0 heartbeat under simultaneous
   input/network/redraw load;
7. prove existing focus, usercopy, FAT, framebuffer, USB, and network gates remain
   green;
8. record a dated visible desktop pass;
9. decide whether the bounded bottom half remains permanent or is later promoted
   to a wakeable EL1 service.

Measurement is progress, not closure. RISK-017 continues to block formal v0.2
promotion.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1  
**Affected scope:** persistent storage, Files, Editor, Shell, ext2 integration,
reboot workflow

Useful foundations exist: a fixed mount table, filesystem callbacks,
process-local descriptors, a primary-MBR FAT32 parser, bounded block views, and a
writable root-only FAT32 bridge.

The current platform remains too narrow:

- VFS is capped at 24 nodes, four mounts, eight descriptors per process, and
  64-byte paths;
- no common path resolver exists;
- directory entries are newline-separated strings rather than structured records;
- metadata is essentially file size only;
- no `mkdir`, truncate, structured stat/readdir, or filesystem-info ABI exists;
- FAT32 supports root-directory 8.3 files only;
- no subdirectories, long names, general FAT variants, GPT/extended partitions,
  journaling, crash recovery, or broad compatibility evidence exists;
- no ext2 implementation exists;
- no combined files/settings reboot-persistence gate exists.

**Failure mode:** application work is forced around temporary root-only and
fixed-buffer assumptions, creating rework and preventing the v1 workflow.

**Exit criteria:** complete v0.3-v0.4: block-device metadata and flush/read-only
contract, common path resolution, structured filesystem operations and ABI,
real FAT names/directories/truncate, malformed-image tests, QEMU nested-directory
and persistence gates, and Files/Shell independence from root-only 8.3 names.

### RISK-014 — Desktop applications are not complete daily tools

**Severity:** P1 for v1  
**Affected scope:** Files, Editor, Shell, Control, Monitor, Panel, Clock  
**Tracking:** issue #2, v0.6 useful desktop applications

The seven shipping applications are real EL0 programs and useful demonstrations,
but they do not satisfy the complete v1 product workflow.

Current concrete limits include:

- Files is fixed to `/fat`, shows at most eight entries, and understands root 8.3
  files only;
- Editor uses a 512-byte buffer and renders only the caret line;
- Shell lacks normal copy/move/remove/mkdir/touch/edit/open/df workflows;
- Control persistence is narrow;
- Monitor is informational rather than an operator tool;
- no shared userland heap/container layer or widget toolkit exists;
- no 30-minute stable visible session is recorded;
- no final reboot workflow proves files and settings together.

Issue #2 is intentionally v0.6, not v1.1. It depends on v0.3 common path and
metadata, v0.4 real FAT, and v0.5 shared runtime/widgets.

**Exit criteria:** complete v0.5-v0.8 with shared helpers/widgets, directory-aware
Files, a multi-line scrollable Editor, useful Shell commands, persistent
observable settings, actionable Monitor process controls, reliable panel/window
lifecycle, reboot-persistence automation, and dated visible evidence.

### RISK-015 — User copy is not fault-contained

**Severity:** P2 hardening

Permission-aware validation and kernel-owned syscall payload boundaries are
implemented. The final byte transfer still uses ordinary EL1 loads/stores.
ArmoniOS has no exception table or fixup target that can turn an unexpected
translation fault during copy into a syscall error.

**Exit criteria:** add recoverable `copy_from_user` / `copy_to_user` primitives,
exception/fixup tests, preserve `ERR_INVAL` and `ERR_PERM`, and prove a bad or
racy address cannot enter the fatal EL1 path.

### RISK-007 — Raspberry Pi storage lacks physical evidence

**Severity:** P0 for the hardware track

Implemented scaffolding includes the SDHCI/EMMC2 core, mailbox clock query,
broken-card-detect adaptation, failure telemetry, primary-MBR FAT32 discovery,
bounded partition views, and an opt-in read-only diagnostic image.

Normal RPi4 capabilities remain zero. No physical clock response, card
initialization, sector-zero read, FAT geometry, framebuffer, input, or desktop
boot has been confirmed.

**Exit criteria:** controlled CPU entry and secondary-core parking, repeatable
serial evidence across cold boots, timer/memory validation, card/controller
initialization, sector-zero and FAT reads from disposable media, and only then a
considered read-only capability. Writes require a later recovery-oriented
milestone.

## Future hardening tracked under RISK-008

Kernel W^X is implemented for the v0.1 baseline: text RX, rodata R/NX,
data/BSS/stack RW/NX, MMIO device/NX, and remaining RAM RW/NX.

Each process TTBR0 still duplicates the kernel/RAM identity map and process
switches use broad TLB invalidation. Stronger isolation and scalability require
TTBR1 kernel mappings, user-only TTBR0 roots, ASIDs, scoped invalidation, an
explicit global/non-global policy, and stale-translation tests.

## Closed risks

### RISK-016 — Parent-owned zombie lifecycle

Spawn records a parent PID. A child zombie remains observable until that parent
waits. Foreign waits fail and automatic reclamation is limited to kernel-owned or
orphaned zombies. Evidence: `tests/run_process_parent_wait_test.sh` and the full
verification matrix.

### RISK-012 — Kernel-owned syscall buffers

VFS data/paths, argv, IPC, GUI outputs, and information outputs cross through
bounded kernel-owned temporaries before lower subsystems operate. Destinations
are validated before state-consuming receives dequeue data.

### RISK-001 — Permission-aware user-copy destinations

Output syscalls walk current process page tables and reject missing or read-only
pages before copying any byte. Host and QEMU invalid-output regressions pass.

### RISK-002 — Process-owned file descriptors

Descriptors are local to the owning process, foreign use is rejected, dead
owners are reaped, and process exit closes all owned descriptors.

### RISK-003 — Visible desktop FAT workflow

The visible target attaches FAT32, GPU, and input. Automated wiring markers pass.
Rocco manually verified create/edit/save/rename/reopen/delete on 2026-07-17.
Editor rendered one visible line; persistence still passed.

### RISK-004 — Spawned Editor focus

Normal application windows request focus after creation; panel/dock windows keep
no-focus policy. QEMU markers and manual Files-to-Editor evidence confirm the
current path.

### RISK-005 — Deterministic QEMU gates

Release evidence uses marker-checking scripts for framebuffer, USB, network,
usercopy, focus, and visible FAT wiring rather than timeout-only launches.

### RISK-006 — Raspberry Pi build contract

The board backend defines required functions and explicitly fails unsupported
normal display/input/storage paths. This closes compilation/linkage only.

### RISK-009 — KLI1 mutable-storage contract

Shipping images forbid mutable static `.data` and `.bss`; linker assertions and
synthetic regressions enforce the rule.

### RISK-010 — Scheduling description

EL0 process dispatch is preemptive, EL1 helper threads are cooperative, and the
post-EOI runtime bottom half is a separate third execution mode.

### RISK-011 — Verification infrastructure

The repository has a one-command local baseline and hosted workflows that build,
run host tests, execute deterministic QEMU gates, and retain logs and the kernel
ELF. Evidence must continue to distinguish a validated PR tree from an untested
final merge commit.
