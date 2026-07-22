# Technical Risk Register

This register tracks correctness defects, architectural limits, missing evidence,
and release blockers. Status terms follow `DOCUMENTATION_POLICY.md`.
Operational truth lives in `CURRENT_STATE.md`; sequencing lives in `ROADMAP.md`.

## Severity

- **P0** — can compromise correctness, isolation, data integrity, or the affected
  release's core contract.
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required behavior.
- **P2** — future hardening that should be scheduled deliberately.

Hardware-track severity does not block QEMU while unsupported capabilities remain
fail closed and no physical support claim is made.

## Summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | v0.1 isolation |
| RISK-002 | P0 | VFS descriptors | CLOSED | v0.1 process correctness |
| RISK-003 | P1 | Visible FAT workflow | CLOSED | v0.1 desktop evidence |
| RISK-004 | P1 | Desktop focus | CLOSED | v0.1 usability |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | v0.1 reproducibility |
| RISK-006 | P1 | RPi build contract | CLOSED | Build boundary only |
| RISK-007 | P0 hardware | Raspberry Pi storage | OPEN | Blocks physical support claims |
| RISK-008 | P2 future | Address-space architecture | PARTIAL / FUTURE | Stronger hardening |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | Image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | Reproducibility |
| RISK-012 | P1 v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 v1 | Desktop applications | OPEN | Blocks v1 usability |
| RISK-015 | P2 | Fault-contained user copy | OPEN | Future mapping hardening |
| RISK-016 | P1 | Process lifecycle | CLOSED | Parent/wait correctness |
| RISK-017 | P1 | Deferred runtime execution | OPEN; INPUT + NETWORK BOUNDED | Blocks formal v0.2 |

## Open risks

### RISK-017 — Deferred runtime execution is partially bounded

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, responsiveness, fairness,
exception-stack occupancy, and v0.2 promotion  
**Tracking:** issue #43

Implementation sequence:

- aggregate timing and request telemetry: PR #44 / `b3fd013`;
- input, redraw-submission, and queue telemetry: PR #45 / `a3b9b44`;
- consumed network frames: PR #46 / `f60ab28`;
- USB HID polling operations: PR #47 / `7a6780d`;
- partial damage and full redraws: PR #48 / `f327868`;
- independent, bounded post-EOI network RX: PR #50 / `3797f7e`;
- independent, bounded post-EOI input consumption: PR #52 / `41f3e18`.

The timer callback performs fixed account/rearm/publication/scheduler work. The
runtime backend executes after EOI but before process dispatch and `eret`.

#### Measurements implemented

The internal snapshot records:

- requests, coalescing, non-empty and empty passes, and requeues;
- last, maximum, and cumulative generic-counter duration;
- passes exceeding one timer interval;
- input events produced and consumed;
- maximum input queue depth, lifetime high-water, and overflow;
- USB HID polls reaching xHCI;
- valid virtio-net RX frames consumed;
- successful redraw submissions;
- merged partial-damage rectangles and full-redraw fallbacks;
- input- and network-budget exhaustion;
- counter frequency, threshold, pending bits, and last-consumed bits.

Each indexed class keeps last-pass, maximum-pass, and cumulative counts. Reports
outside the active pass are ignored so cooperative work is excluded from
bottom-half class metrics.

Production timing uses `CNTPCT_EL0`; `CNTFRQ_EL0` provides conversion. At 100 Hz
the current threshold is approximately 10 ms. It remains an observation
threshold, not the accepted final global budget.

#### Input bound implemented

Input readiness is separate from periodic and network readiness. The active input
phase accepts at most 16 successful shared-queue pops per pass.

At the cap, the wrapper queries queue depth without consuming:

1. an empty queue finishes without requeue or exhaustion;
2. remaining events increment `input_budget_exhaustion_count`;
3. `RUNTIME_WORK_INPUT` is republished;
4. process dispatch resumes after the service returns;
5. queue continuation occurs later.

Exactly 16 events with an empty queue do not schedule an empty follow-up. A
seventeenth event is retained. Cooperative console and other outside-service
consumers remain unbudgeted.

#### Network bound implemented

The active network phase accepts at most 16 valid virtio-net RX frames per pass.
At the cap, receive stops, network exhaustion increments, and
`RUNTIME_WORK_NETWORK` is conservatively republished.

The requeue is conservative because retaining a non-consuming RX query crossed a
2 KiB linker-alignment boundary and exceeded the 108000-byte limit. Exactly 16
frames can therefore produce one empty follow-up network pass.

Cooperative network polling outside the active runtime service remains
unbudgeted. The 16-descriptor implementation exposes no trustworthy device-drop
or ring-overflow counter, so consumed-frame counts are not proof of zero loss.

#### Validation

Latest validated input-budget head:
`ba8051cd8edbe6a66a843f80c54c96668d064a91`.

- `Verify ArmoniOS` run `29853659559`: success;
- `CI - Tests` run `29853659491`: success;
- loadable QEMU kernel: 107802 / 108000 bytes;
- remaining margin: 198 bytes;
- merge: `41f3e185ca1f75ed09416313d34279384f3d78a9`.

The runtime regression proves exactly 16 input events without requeue, 17-event
continuation, combined periodic/input single-backend execution, outside-service
input behavior, the previous network budget contracts, reset, EOI ordering, all
telemetry, and static wiring.

#### Why the risk remains open

During the service pass:

- execution remains inside the IRQ exception path;
- the 288-byte frame remains on the EL1 stack;
- nested IRQ helpers preserve the vector's prior mask state;
- EL0 remains paused;
- input queue consumption is bounded, but input producer and USB HID polling are
  not;
- redraw/damage work has no count or time limit;
- no global generic-counter deadline is enforced;
- cooperative input/network work outside the service is unbounded;
- no sustained-load QEMU heartbeat proves EL0 progress;
- only 198 bytes remain under the kernel ceiling.

Pending state and telemetry remain non-atomic single-core structures valid only
under the current one-consumer model.

**Failure mode:** sustained USB/input producer or redraw work can still extend one
exception path and delay EL0 and other normal IRQ handling. Input consumption and
network RX cannot exceed their count budgets, but input overflow may still occur
and device-level network loss remains unobservable.

**Remaining exit criteria:**

1. compact runtime phase state and telemetry while preserving all current tests;
2. split and bound USB/device polling;
3. bound redraw/damage work;
4. enforce a global generic-counter deadline;
5. preserve or republish every exhausted class and count each exhaustion;
6. add QEMU stress with an EL0 heartbeat under combined load;
7. retain all current subsystem gates;
8. record a dated visible desktop pass;
9. decide whether the fully bounded bottom half remains or becomes a wakeable
   service.

RISK-017 still blocks formal v0.2 promotion.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1

Current foundations include a fixed mount table, filesystem callbacks,
process-local descriptors, primary-MBR FAT32 discovery, bounded block views, and
a writable root-only FAT32 bridge.

Limits:

- 24 VFS nodes, four mounts, eight descriptors/process, 64-byte paths;
- no common normalized path resolver;
- no structured directory/metadata ABI;
- no `mkdir`, truncate, structured stat/readdir, or filesystem-info calls;
- root-only 8.3 FAT32;
- no long names, directories, general FAT, ext2, or reboot-persistence gate.

**Exit criteria:** complete v0.3-v0.4 block metadata/flush/read-only contracts,
path resolution, structured filesystem ABI, real FAT names/directories/truncate,
malformed-image tests, and QEMU nested-directory/persistence gates.

### RISK-014 — Desktop applications are incomplete daily tools

**Severity:** P1 for v1  
**Tracking:** issue #2, **v0.6 useful desktop applications**

The seven EL0 apps are real, but Files is root-only, Editor is a 512-byte
caret-line viewport, Shell lacks normal file-management workflows, settings are
narrow, Monitor is informational, and no shared heap/widget layer or final reboot
workflow exists.

Issue #2 is intentionally v0.6, not v1.1. It depends on v0.3 paths/metadata,
v0.4 real FAT, and v0.5 shared runtime/widgets.

**Exit criteria:** complete v0.5-v0.8 with shared helpers/widgets,
directory-aware Files, multi-line Editor, useful Shell commands, persistent
settings, Monitor controls, reliable panel/window lifecycle, reboot persistence,
and dated visible evidence.

### RISK-015 — User copy is not fault-contained

Permission-aware validation and kernel-owned payloads exist. Final transfer uses
ordinary EL1 loads/stores; no exception fixup converts an unexpected translation
fault into a syscall error.

**Exit criteria:** recoverable copy helpers, exception/fixup tests, preserved
`ERR_INVAL`/`ERR_PERM`, and proof that bad or racy addresses cannot enter fatal
EL1.

### RISK-007 — Raspberry Pi storage lacks physical evidence

SDHCI/EMMC2, mailbox clock, telemetry, MBR discovery, partition view, and an
opt-in read-only diagnostic image exist. Normal capabilities remain zero. No
physical card, sector/FAT, framebuffer, input, or desktop evidence exists.

**Exit criteria:** controlled entry/core parking, repeatable cold-boot serial,
memory/timer validation, card initialization, sector/FAT reads on disposable
media, then considered read-only capability. Writes require a separate milestone.

## Future hardening under RISK-008

Kernel W^X exists, but every process TTBR0 duplicates kernel/RAM identity maps
and switches use broad TLB invalidation. Stronger hardening requires TTBR1,
user-only TTBR0 roots, ASIDs, scoped invalidation, global/non-global policy, and
stale-translation tests.

## Closed-risk evidence summary

| Risk | Closure evidence |
|---|---|
| RISK-001 | Page-table checks and host/QEMU invalid-output regressions |
| RISK-002 | Process-local owner checks and exit cleanup tests |
| RISK-003 | FAT+GPU wiring and 2026-07-17 visible workflow |
| RISK-004 | Focus marker and Files-to-Editor evidence |
| RISK-005 | Deterministic subsystem scripts |
| RISK-006 | Complete RPi4 build contract with fail-closed paths |
| RISK-009 | Linker assertions and synthetic KLI1 rejection |
| RISK-010 | Explicit EL0/EL1/bottom-half documentation |
| RISK-011 | One-command local baseline and hosted artifacts |
| RISK-012 | Kernel-owned syscall payload boundaries |
| RISK-016 | Parent-owned zombie/wait regression |
