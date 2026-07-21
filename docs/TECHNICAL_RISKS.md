# Technical Risk Register

This is the active risk register for ArmoniOS. It records correctness defects,
architectural limits, missing evidence, and release blockers.

Status terminology follows `DOCUMENTATION_POLICY.md`. Operational truth lives in
`CURRENT_STATE.md`; future sequencing lives in `ROADMAP.md`.

## Severity

- **P0** — can compromise correctness, isolation, data integrity, or the core
  contract of the affected release.
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required product behavior.
- **P2** — hardening or future-facing limitation that should be scheduled
  deliberately.

Severity is scoped. A hardware-track P0 does not block the QEMU-only line while
that capability remains fail closed and no hardware claim is made.

## Risk summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | v0.1 isolation |
| RISK-002 | P0 | VFS descriptors | CLOSED | v0.1 multi-process correctness |
| RISK-003 | P1 | Visible FAT workflow | CLOSED | v0.1 desktop evidence |
| RISK-004 | P1 | Desktop focus | CLOSED | v0.1 usability |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | v0.1 reproducibility |
| RISK-006 | P1 | Raspberry Pi build contract | CLOSED | Build boundary only |
| RISK-007 | P0 for hardware track | Raspberry Pi storage | OPEN | Blocks physical support claims |
| RISK-008 | P2 future | Address-space architecture | PARTIAL / FUTURE | Stronger hardening |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | Userland image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | Reproducibility |
| RISK-012 | P1 for v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 for v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 for v1 | Desktop applications | OPEN | Blocks v1 usability |
| RISK-015 | P2 | Fault-contained user copy | OPEN | Future hostile/racy mapping hardening |
| RISK-016 | P1 | Process lifecycle | CLOSED | Parent/wait correctness |
| RISK-017 | P1 | Deferred runtime execution | OPEN; MEASUREMENT LANDED | Blocks formal v0.2 promotion |

## Open risks

### RISK-017 — Deferred runtime execution is measured but not bounded

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, GUI responsiveness, input/network
fairness, exception-stack occupancy, and formal v0.2 promotion  
**Tracking:** issue #43  
**Implementation:** PR #44

The timer callback itself performs fixed work: tick accounting, `CNTP_CVAL`
rearm, one coalescible periodic-work publication, and scheduler accounting. The
runtime backend executes after EOI but before process dispatch and `eret`.

#### Measurement now implemented

The kernel-internal snapshot records:

- accepted request count;
- coalesced request count;
- non-empty and empty consumer invocations;
- passes that leave work requeued;
- last, maximum, and cumulative duration in generic-counter ticks;
- passes exceeding the configured observation threshold;
- counter frequency, threshold, pending bits, and last consumed bits.

Production timing reads `CNTPCT_EL0`; `CNTFRQ_EL0` supplies the conversion
frequency. `timer_init()` sets the initial threshold to one timer interval. At
100 Hz this is approximately 10 ms. This is an observation threshold, not the
final accepted runtime budget.

The host regression injects a deterministic counter and covers EOI order,
coalescing, requeue preservation/counting, last/max/total duration, interval
overruns, snapshot state, null snapshot handling, and reset semantics.

**Validated candidate tree:** `95616b865ba021da6ff733bb54213a2db404ba9d`  
**Hosted evidence:**

- `Verify ArmoniOS` run `29828181038`: success;
- `CI - Tests` run `29828181039`: success.

Those runs cover the code, tests, README, AGENTS, runtime contract, status, and
risk documentation. Later evidence-only metadata commits do not change kernel
behavior. The final merge commit is not independently tested unless a workflow
runs against that exact commit.

#### Why the risk remains open

During the service pass:

- execution remains inside the IRQ exception path;
- normal IRQs remain masked;
- the 288-byte saved exception frame stays on the EL1 stack;
- EL0 remains paused;
- all available input events may be drained in one pass;
- USB/device work has no operation budget;
- network work has no frame/packet budget;
- redraw has no damage or time budget;
- no per-class high-water, overflow, or budget-exhaustion counters exist;
- no sustained-load QEMU test proves EL0 progress or explicit loss accounting.

The pending mask uses a non-atomic `volatile uint32_t` read-modify-write. Pending
publication and telemetry remain valid only under the documented single-core,
IRQ-masked, one-consumer assumptions.

**Failure mode:** sustained input, redraw, USB, or network activity can extend one
exception path and delay EL0 and all normal IRQ handling. Fixed queues can fill
without a complete fairness and overflow-accounting story.

**Remaining exit criteria:**

1. add per-pass input-event, queue high-water, device-work, packet/frame, redraw,
   full-redraw, damage, and overflow metrics;
2. select independent per-class budgets using measured evidence;
3. add a global counter-tick deadline smaller than or justified against one timer
   interval;
4. preserve or republish specific pending bits when a class or global budget is
   exhausted;
5. count queue overflow and budget exhaustion explicitly;
6. add deterministic QEMU stress with an EL0 heartbeat under simultaneous
   input/network/redraw load;
7. prove all existing focus, usercopy, FAT, framebuffer, USB, and network gates
   remain green;
8. record a dated visible desktop pass;
9. decide whether the bounded bottom half remains permanent or later becomes a
   wakeable EL1 service.

Measurement is progress, not closure. RISK-017 still blocks formal v0.2
promotion.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1

Current foundations include a fixed mount table, filesystem callbacks,
process-local descriptors, primary-MBR FAT32 discovery, bounded block views, and
a writable root-only FAT32 bridge.

The platform remains too narrow:

- 24 VFS nodes, four mounts, eight descriptors/process, 64-byte paths;
- no common normalized path resolver;
- no structured directory/metadata ABI;
- no `mkdir`, truncate, structured stat/readdir, or filesystem-info calls;
- root-directory 8.3 FAT32 only;
- no long names, directories, general FAT variants, ext2, or combined reboot
  persistence gate.

**Exit criteria:** complete v0.3-v0.4 with block-device metadata/flush/read-only
contracts, path resolution, structured filesystem operations and ABI, real FAT
names/directories/truncate, malformed-image tests, and QEMU nested-directory and
persistence gates.

### RISK-014 — Desktop applications are not complete daily tools

**Severity:** P1 for v1  
**Tracking:** issue #2, **v0.6 useful desktop applications**

The seven EL0 applications are real and useful demonstrations, but:

- Files is tied to root-only `/fat` and an eight-entry view;
- Editor uses a 512-byte buffer and renders only the caret line;
- Shell lacks normal copy/move/remove/mkdir/touch/edit/open/df workflows;
- Control persistence is narrow;
- Monitor is informational rather than an operator tool;
- no shared heap/container layer or widget toolkit exists;
- no final files/settings reboot workflow or 30-minute stable session exists.

Issue #2 is intentionally v0.6, not v1.1. It depends on v0.3 common paths and
metadata, v0.4 real FAT, and v0.5 shared runtime/widgets.

**Exit criteria:** complete v0.5-v0.8 with shared helpers/widgets,
directory-aware Files, multi-line Editor, useful Shell commands, persistent
observable settings, Monitor process controls, reliable panel/window lifecycle,
reboot-persistence automation, and dated visible evidence.

### RISK-015 — User copy is not fault-contained

Permission-aware validation and kernel-owned syscall payloads are implemented.
The final transfer still uses ordinary EL1 loads/stores; no exception table or
fixup converts an unexpected translation fault into a syscall error.

**Exit criteria:** recoverable `copy_from_user` / `copy_to_user`, exception/fixup
tests, preserved `ERR_INVAL`/`ERR_PERM`, and proof that a bad or racy address
cannot enter the fatal EL1 path.

### RISK-007 — Raspberry Pi storage lacks physical evidence

The SDHCI/EMMC2 core, mailbox clock query, failure telemetry, MBR discovery,
bounded partition view, and opt-in read-only diagnostic image exist. Normal RPi4
capabilities remain zero. No physical clock response, card initialization,
sector-zero/FAT read, framebuffer, input, or desktop boot has been confirmed.

**Exit criteria:** controlled CPU entry and core parking, repeatable cold-boot
serial evidence, memory/timer validation, card initialization, sector/FAT reads
on disposable media, and only then considered read-only capability. Writes need
a separate recovery-oriented milestone.

## Future hardening under RISK-008

Kernel W^X is implemented, but every process TTBR0 still duplicates the
kernel/RAM identity map and process switches use broad TLB invalidation. Stronger
hardening requires TTBR1 kernel mappings, user-only TTBR0 roots, ASIDs, scoped
invalidation, explicit global/non-global policy, and stale-translation tests.

## Closed-risk evidence summary

| Risk | Closure evidence |
|---|---|
| RISK-001 | Page-table permission checks plus host and QEMU invalid-output regressions |
| RISK-002 | Process-local owner checks and exit cleanup tests |
| RISK-003 | FAT+GPU wiring gate and Rocco's 2026-07-17 visible workflow |
| RISK-004 | Focus marker gate and manual Files-to-Editor focus evidence |
| RISK-005 | Deterministic framebuffer, USB, network, usercopy, focus, and FAT scripts |
| RISK-006 | Complete RPi4 board symbol contract with fail-closed unsupported paths |
| RISK-009 | Linker assertions and synthetic KLI1 mutable-storage rejection |
| RISK-010 | Explicit preemptive EL0 / cooperative EL1 / bottom-half documentation |
| RISK-011 | One-command local baseline and hosted workflows with logs/artifacts |
| RISK-012 | Kernel-owned VFS/argv/IPC/GUI/info payload boundaries |
| RISK-016 | Parent-owned zombie/wait regression and orphan reclamation |
