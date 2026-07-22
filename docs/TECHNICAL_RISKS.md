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
| RISK-017 | P1 | Deferred runtime execution | OPEN; AUTOMATED EVIDENCE COMPLETE, RELEASE DISPOSITION OPEN | Blocks formal v0.2 |
| RISK-018 | P1 | Intermittent VMM fault | OPEN; SINGLE OBSERVATION | Blocks unqualified reproducibility claim |

## Open risks

### RISK-017 — Deferred runtime execution needs final release disposition

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, responsiveness, fairness,
exception-stack occupancy, and v0.2 promotion  
**Tracking:** issue #43

Implementation sequence:

- aggregate telemetry: PR #44 / `b3fd013`;
- input/redraw/queue telemetry: PR #45 / `a3b9b44`;
- consumed network frames: PR #46 / `f60ab28`;
- USB HID poll telemetry: PR #47 / `7a6780d`;
- damage/full-redraw telemetry: PR #48 / `f327868`;
- bounded post-EOI network RX: PR #50 / `3797f7e`;
- bounded shared input consumption: PR #52 / `41f3e185`;
- runtime-state compaction: PR #54 / `39dea455`;
- bounded USB HID device visits: PR #55 / `53c14402`;
- bounded virtio-input descriptor draining: PR #56 / `7674b639`;
- bounded partial compositor damage: PR #58 / `fe4f2a62`;
- service-wide generic-counter deadline: PR #60 / merge `1e51e818`;
- QEMU EL0 heartbeat and forced-expiry stress: PR #61 / merge `172496d8`;
- strict network-phase routing and natural RX saturation: PR #62.

The timer callback performs fixed account/rearm/publication/scheduler work. The
runtime backend executes after EOI but before process dispatch and `eret`.

#### Measurements implemented

The internal snapshot records:

- requests, coalescing, non-empty and empty passes, and requeues;
- last, maximum, and cumulative generic-counter duration;
- global deadline exhaustion through `over_budget_count`;
- input events produced and consumed;
- maximum input queue depth, lifetime high-water, and overflow;
- USB HID polls reaching xHCI;
- valid virtio-net RX frames consumed;
- successful redraw submissions;
- partial-damage rectangles, full redraws, and redraw exhaustion;
- input/network count-budget exhaustion;
- counter frequency, configured deadline budget, pending bits, and last work.

Each indexed class keeps last-pass, maximum-pass, and cumulative counts. Reports
outside the active pass are ignored. Production timing uses `CNTPCT_EL0` and a
budget of one nominal timer interval, approximately 10 ms at 100 Hz.

#### Bounds implemented

| Work class | Enforced rule | Continuation |
|---|---|---|
| Whole service | Deadline at `start + CNTFRQ_EL0 / timer_hz`, checked at safe boundaries | Original work snapshot is conservatively republished on expiry. |
| Virtio-input producer | `min(queue_size, 16)` used descriptors/call | Used descriptors remain in the ring. |
| USB HID producer | Four registered device visits/call | Every supported fixed slot fits in one scan. |
| Shared input consumer | 16 events/active input pass | Requeue when queue depth remains nonzero. |
| Partial compositor damage | Eight rectangles/successful redraw | Remaining ordered damage stays dirty. |
| Virtio-net RX | 16 valid frames/active NETWORK pass | Conservatively republish network readiness at cap. |

At a deadline checkpoint, the runtime service records one exhaustion, marks the
pass expired, ORs the original `last_work` snapshot into `pending_work`, skips
later optional work, and returns toward process dispatch. Native continuation
remains in input queues, virtio rings, and the compositor damage list.

Network polling and descriptor receive now consume nothing outside the active
NETWORK phase. The legacy console-thread poll therefore cannot bypass the count
or time budgets.

#### Deadline boundary

The deadline is cooperative and cannot asynchronously interrupt an operation
already executing. A full redraw remains one explicit operation. It may cross the
nominal deadline before the next checkpoint, after which the pass is counted as
exhausted and readiness is republished.

#### Deterministic validation

The host deadline regression proves:

- periodic expiry can prevent redraw and the later network phase;
- the network receive loop stops at a checkpoint and retains unconsumed frames;
- an operation completing after the deadline is detected, counted, and
  conservatively republished;
- network polling and receive outside the NETWORK phase consume no descriptors.

#### Forced-expiry heartbeat evidence

PR #61 builds a separate test-only image. Its validated run recorded 509 EL0
heartbeat markers, 311 deadline republications, input/redraw/network/DHCP
activity, zero observable input overflow, and zero panic markers.

This demonstrates repeated EL0 progress while the production republish path is
exercised hundreds of times. Forced expiry is instrumentation rather than natural
latency evidence.

#### Natural RX saturation evidence

PR #62 builds a second test-only image without shortening the production budget.
QEMU completes DHCP, injects sustained UDP through `hostfwd`, injects USB keyboard
input, and keeps panel redraw work active.

Validated head:
`eac4ff990baddbf83406567b4a20e58bcae6600d`.

- `Verify ArmoniOS` run `29896102906` (#290): success;
- `CI - Tests` run `29896102904` (#430): success;
- production loadable kernel: 107918 / 108000 bytes;
- production margin: 82 bytes;
- EL0 yields: 38,912;
- input events consumed: 16;
- redraw submissions: 738;
- virtio-net frames consumed: 29,234;
- maximum frames/pass: 16;
- network-cap exhaustions: 1,827;
- runtime requeues: 1,827;
- natural deadline overruns: 0;
- maximum pass duration: 385,763 / 625,000 ticks, or 61.7%;
- observable input overflow: 0;
- panic markers: 0.

This satisfies the missing software-visible RX backlog, natural-duration, and EL0
progress evidence for the current QEMU contract.

#### Evidence boundary

The 16-descriptor virtio RX implementation exposes no trustworthy counter for
frames dropped, overwritten, or never delivered to software. The run proves
continuation for frames that reached software, not loss-free delivery of every
host-submitted packet.

One already-started full redraw or driver operation can still cross the deadline
before the next checkpoint. The measured saturation run remained below budget,
but this does not constitute asynchronous preemption or an indefinite fairness
proof.

#### Why the risk remains open

The automated runtime exit evidence is complete, but formal v0.2 promotion still
requires explicit decisions:

1. accept the absence of device-level RX-drop telemetry for v0.2 or schedule a
   separate driver-counter milestone;
2. accept the one-operation full-redraw boundary or add finer checkpoints;
3. retain all current subsystem and stress gates;
4. record a dated visible desktop pass after the final runtime boundary;
5. resolve or explicitly disposition RISK-018 / issue #63;
6. close or accept the residuals and tag v0.2.

The bounded bottom half may later become a wakeable EL1 service, but that change
is no longer required to demonstrate the current cooperative contract.

### RISK-018 — Intermittent EL1 VMM data abort during FAT32 smoke

**Severity:** P1 reproducibility/correctness  
**Tracking:** issue #63  
**First observed:** PR #62 validation, `Verify ArmoniOS` run `29894678482`, first
attempt

One existing `qemu-fs-test` boot reached the EL0 panel path and then panicked:

```text
ESR 0x96000004
ELR 0x40089cac
FAR 0x00003fdfd5033000
```

The archived ELF resolves the ELR to the `table[index]` load in `next_table()` in
`kernel/mm/vmm.c`. The same source commit passed the failed-job rerun, and later
Verify #287, Verify #290, and CI #430 also passed. The observation is therefore
intermittent and is not attributed to PR #62 without reproduction evidence.

Possible classes include stale page-table state, premature table reclamation,
allocator corruption, unrelated memory overwrite, or a timing-sensitive test
path. It must not be classified as infrastructure-only merely because reruns
passed.

**Exit criteria:**

1. add a repeated FAT32 boot/soak gate;
2. capture process, page-table root, virtual address, level/index, and allocator
   context around VMM failures;
3. identify and fix the root cause, or establish a bounded external cause with
   strong repeat evidence;
4. retain production size and ABI contracts;
5. update release evidence before v0.2 promotion.

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
