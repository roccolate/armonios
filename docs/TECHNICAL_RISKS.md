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
| RISK-017 | P1 | Deferred runtime execution | OPEN; INPUT/NETWORK COUNTS BOUNDED | Blocks formal v0.2 |

## Open risks

### RISK-017 — Deferred runtime execution is partially bounded

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
- bounded virtio-input descriptor draining: PR #56 / `7674b639`.

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
- input/network budget exhaustion;
- counter frequency, threshold, pending bits, and last-consumed bits.

Each indexed class keeps last-pass, maximum-pass, and cumulative counts. Reports
outside the active pass are ignored so cooperative console work is excluded.
Production timing uses `CNTPCT_EL0`; the current approximately 10 ms timer
interval remains an observation threshold, not an enforced global deadline.

#### Bounds implemented

| Work class | Enforced rule | Continuation |
|---|---|---|
| Virtio-input producer | `min(queue_size, 16)` used descriptors/call | Used descriptors remain in ring for the next periodic tick. |
| USB HID producer | Four registered device visits/call | Every supported fixed slot fits in one scan. |
| Shared input consumer | 16 events/active input pass | Requeue only when queue depth remains nonzero. |
| Virtio-net RX | 16 valid frames/active network pass | Conservatively republish network readiness at cap. |

The network requeue is conservative because retaining a non-consuming queue
query crossed a linker-alignment boundary and exceeded the 108000-byte kernel
limit. Exactly 16 frames can therefore produce one empty follow-up pass.

Virtio-input now returns the number of successfully queued events instead of
always zero, repairing input-production telemetry. A deterministic host test
negotiates an eight-entry ring, injects ten descriptors, and proves 8 + 2
continuation with ten events queued.

USB HID polling is clamped independently of the public `count` field. A host test
sets `count` to 255 and proves only the fixed four slots are visited. Each device
poll enters an xHCI path with finite spin limits; the future service-wide deadline
is still required to bound combined elapsed time.

#### Evidence boundary

The 16-descriptor virtio RX implementation exposes no trustworthy device counter
for frames dropped, overwritten, or never delivered to software. Consumed-frame
counts are not proof of zero network loss.

Damage telemetry records merged rectangle count or a full-redraw sentinel after a
successful QEMU submission. It does not measure pixels, damaged area, GPU
completion latency, or CPU work spent on a failed submission.

Cooperative network polling outside the active runtime service remains unbudgeted
and is outside the post-EOI network guarantee.

#### Validation

Latest producer-bound head:
`ee92e8074ed2995a48ce22fb88a901ea02cf031d`.

- `Verify ArmoniOS` run `29859659229`: success;
- `CI - Tests` run `29859659270`: success;
- loadable QEMU kernel: 107706 / 108000 bytes;
- remaining margin: 294 bytes;
- USB merge: `53c1440261267b36e813fb90e6405261ec7bbfad`;
- virtio-input merge: `7674b639b9a53dea4cec42bcccf84e71d7f6d10c`.

The complete matrix proves the producer and consumer count rules while retaining
all previous build, host, stack, FAT32, QEMU, GUI, USB, network, ABI, and RPi4
gates.

#### Why the risk remains open

During the service pass:

- execution remains inside the IRQ exception path;
- the 288-byte frame remains on the EL1 stack;
- nested IRQ helpers preserve the vector's prior mask state;
- EL0 remains paused;
- redraw/damage work has no explicit per-pass or time stop rule;
- no service-wide generic-counter deadline is enforced;
- cooperative network polling outside the service is unbounded;
- no sustained-load QEMU heartbeat proves EL0 progress.

Pending state and telemetry remain non-atomic single-core structures valid only
under the current one-consumer model.

**Failure mode:** sustained redraw work or combined finite device operations can
still extend one exception path and delay EL0. Count-bounded input/network work
cannot loop indefinitely on queue growth, but only a global deadline can bound
their combined elapsed time.

**Remaining exit criteria:**

1. bound redraw/damage work and preserve unfinished repaint state;
2. enforce a global generic-counter deadline;
3. count global-deadline and redraw exhaustion;
4. add QEMU stress with an EL0 heartbeat under combined load;
5. prove pending/native-queue work is not silently lost;
6. retain all current subsystem gates;
7. record a dated visible desktop pass;
8. decide whether the fully bounded bottom half remains or later becomes a
   wakeable service.

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
