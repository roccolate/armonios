# Technical Risk Register

This register tracks correctness defects, architectural limits, missing evidence,
and release blockers.

- Operational truth: `CURRENT_STATE.md`
- Milestone order: `ROADMAP.md`
- Evidence rules: `DOCUMENTATION_POLICY.md`
- Work procedures: `DEVELOPMENT_GUIDE.md`

A risk remains open until its exit criteria are met or a named release explicitly
accepts the residual with rationale. Green reruns and clean soaks are evidence,
not automatic root-cause closure.

## Severity and status

- **P0** — can compromise correctness, isolation, data integrity, or the affected
  release's core contract.
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required behavior.
- **P2** — deliberate future hardening that does not currently block the named
  baseline.

| Status | Meaning |
|---|---|
| `OPEN` | Exit criteria are incomplete. |
| `OPEN; EVIDENCE IN PROGRESS` | Investigation exists but has not changed the promoted claim. |
| `ACCEPTED FOR <release>` | Residual is explicitly bounded and accepted for one release; it is not closed. |
| `PARTIAL / FUTURE` | Some foundation exists; stronger design is deferred. |
| `CLOSED` | Exit criteria and exact evidence are recorded. |

Hardware-track severity does not block QEMU while unsupported capabilities fail
closed and no physical support claim is made.

## Summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | v0.1 isolation |
| RISK-002 | P0 | VFS descriptors | CLOSED | v0.1 process correctness |
| RISK-003 | P1 | Visible FAT workflow | CLOSED | v0.1 desktop evidence |
| RISK-004 | P1 | Desktop focus | CLOSED | v0.1 usability |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | v0.1 reproducibility |
| RISK-006 | P1 | RPi build contract | CLOSED | Build boundary only |
| RISK-007 | P0 hardware | Raspberry Pi runtime/storage | OPEN | Blocks physical support claims |
| RISK-008 | P2 future | Address-space architecture | PARTIAL / FUTURE | Stronger isolation/scaling |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | Image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | Reproducibility |
| RISK-012 | P1 v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 v1 | Desktop applications | OPEN | Blocks v1 usability |
| RISK-015 | P2 | Fault-contained user copy | OPEN | Future syscall hardening |
| RISK-016 | P1 | Process lifecycle | CLOSED | Parent/wait correctness |
| RISK-017 | P1 v0.2 | Deferred runtime execution | OPEN; AUTOMATED CONTRACT COMPLETE | Blocks formal v0.2 pending residual disposition and visible evidence |
| RISK-018 | P1 v0.2 | Intermittent EL1 VMM fault | OPEN; EVIDENCE IN PROGRESS | Blocks an unqualified reproducibility claim |

## RISK-017 — Deferred runtime execution needs release disposition

**Severity:** P1  
**Tracking:** issue #43  
**Scope:** interrupt-to-EL0 latency, responsiveness, fairness, exception-stack
occupancy, v0.2 promotion

### Implementation history

- telemetry: PRs #44-#48;
- bounded network RX: PR #50;
- bounded shared input: PR #52;
- runtime-state compaction: PR #54;
- bounded USB HID visits: PR #55;
- bounded virtio-input draining: PR #56;
- bounded partial damage: PR #58;
- service-wide deadline: PR #60 / original merge `1e51e818`;
- forced-expiry heartbeat stress: PR #61 / original merge `172496d8`;
- strict NETWORK routing and natural RX saturation: PR #62.

PR #62 provenance:

- original GitHub merge metadata:
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`;
- current-main runtime replay:
  `d5c104a0badc3a2d553516159b2b745737dd252f`.

The current audited main tree is recorded in `CURRENT_STATE.md`.

### Current contract

The timer callback performs only fixed accounting, rearm, readiness publication,
and scheduler-counter work. The runtime service runs after EOI and before process
dispatch and `eret`.

| Work class | Rule | Continuation |
|---|---|---|
| Whole service | Deadline at `start + CNTFRQ_EL0 / timer_hz` at safe checkpoints | Republish original work snapshot on expiry. |
| Virtio-input | <= negotiated ring and <=16 descriptors/call | Later descriptors remain in ring. |
| USB HID | Four fixed device visits/call | All supported direct slots fit. |
| Shared input | 16 events/active pass | Requeue INPUT when events remain. |
| Partial redraw | Eight ordered rectangles/successful submission | Preserve remainder; failure consumes none. |
| Virtio-net RX | 16 valid frames/NETWORK pass | Conservatively requeue NETWORK at cap. |

Network polling and descriptor receive consume nothing outside NETWORK phase.
Deadline expiry is counted once, republishes original readiness, skips later
optional work, and returns toward process dispatch.

The deadline is cooperative. It cannot interrupt a full redraw or driver call
already in progress.

### Telemetry

The internal snapshot records requests, coalescing, empty/non-empty passes,
requeues, duration, deadline exhaustion, input production/consumption, queue
pressure/overflow, USB polls, consumed network frames, redraw/damage/full-redraw
counts, count-budget exhaustion, configured timing, pending work, and last work.

The layout is kernel-internal and is not a stable Monitor/syscall ABI.

### Automated evidence

Forced-expiry stress recorded 509 EL0 heartbeats, 311 deadline republications,
input/redraw/network/DHCP activity, zero observable input overflow, and zero panic.
The expiry is deliberate instrumentation, not natural latency evidence.

Natural RX stress preserved the production deadline and recorded:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum duration:                  385,763 / 625,000 ticks (61.7%)
observable input overflow:               0
panic markers:                           0
```

Evidence identity:

- implementation/evidence head:
  `eac4ff990baddbf83406567b4a20e58bcae6600d`;
- final PR #62 head:
  `04f65776d1bbe07545113652342c32f2448bfc7b`;
- original final PR validation:
  - `Verify ArmoniOS` `29896952424` (#295): success;
  - `CI - Tests` `29896952435` (#435): success;
- production kernel 107918 / 108000 bytes; margin 82 bytes.

Current-main final-tree workflows are recorded separately in `CURRENT_STATE.md`.

### Residual boundary

- No trustworthy device/ring-drop counter exists. Consumed frames prove progress
  only after software observes them.
- One already-started full redraw or driver call can cross the nominal interval.
- The evidence does not prove indefinite fairness, every possible workload, or
  SMP behavior.
- The pending/telemetry model assumes one core and one consumer.

### Exit criteria

1. Accept missing device-level RX-drop telemetry for v0.2 or land a trustworthy
   counter contract.
2. Accept the one-operation full-redraw boundary or add finer checkpoints and
   evidence.
3. Retain all subsystem and both runtime stress gates.
4. Record a dated visible workflow on the final promotion tree.
5. Resolve or explicitly accept `RISK-018`.
6. Record final promotion tree, runs, tag, accepted residuals, and release notes.

A future wakeable EL1 service is an optional redesign, not a requirement for the
current bounded cooperative contract.

## RISK-018 — Intermittent EL1 VMM data abort during FAT32 smoke

**Severity:** P1  
**Tracking:** issue #63  
**First observation:** PR #62 validation, `Verify ArmoniOS` run `29894678482`,
first attempt

### Observation

One `qemu-fs-test` boot reached the EL0 panel path and then panicked:

```text
ESR 0x96000004
ELR 0x40089cac
FAR 0x00003fdfd5033000
```

The archived ELF resolves the ELR to the `table[index]` load in `next_table()` in
`kernel/mm/vmm.c`.

The same source commit passed its rerun and later matrices, including final PR #62
validation. The observation is intermittent. It is neither attributed to PR #62
nor dismissed as infrastructure-only without stronger evidence.

Possible classes include stale page-table state, premature table reclamation,
allocator corruption/double release, unrelated overwrite, lifecycle use after
transition, timing-sensitive boot interaction, or a bounded external QEMU/runner
cause.

### PR #64 investigation status

Draft PR #64 proposes repeated boots of the unchanged production kernel/FAT image
with live GDB capture on panic.

Proposed evidence includes:

- per-boot storage, mount, and panel assertions;
- QEMU GDB server available without pausing normal execution;
- panic detection while guest state remains inspectable;
- registers, TTBRs, backtrace, current process, PMM globals/bitmap, and
  `next_table` disassembly;
- ELR resolution and retained serial/QEMU stderr logs.

Branch identity:

- branch `agent/vmm-fat32-soak-diagnostics`;
- head `be42617d7286d7ebb6371b0a544072964b332cd7`;
- `Verify ArmoniOS` `29899076138` (#299): success;
- `CI - Tests` `29899076105` (#439): **cancelled by the workflow time limit**.

The cancelled run is not passing evidence. Its artifacts may still be useful for
investigation. PR #64 remains unpromoted and does not change the `main` claim.

Even a completed clean soak would prove only non-reproduction for the tested
sample and conditions; it would not identify the root cause by itself.

### Exit criteria

1. Land a repeatable production FAT32 soak or equivalent reproduction gate.
2. Capture process, table root, virtual address, level/index, and allocator context
   if the failure occurs.
3. Identify and fix the root cause, or bound an external cause with strong
   repeat evidence and rationale.
4. Add focused regression coverage for the demonstrated cause.
5. Preserve size, `.data`, W^X, ABI, and lifecycle contracts.
6. Run the full promotion gate on the final tree.
7. Update current state and v0.2 release disposition.

## RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1

Foundations: fixed mount callbacks, process-local descriptors, bootfs/tmpfs,
primary-MBR FAT32 discovery, bounded block views, and a writable root-only FAT32
8.3 bridge.

Gaps:

- no common normalized path resolver;
- no explicit repeated-slash, `.`/`..`, component, or mount-boundary policy;
- no rich block descriptor with capacity/read-only/flush contract;
- no structured directory/metadata ABI;
- no generic mkdir, truncate, stat, readdir, or filesystem-info operations;
- no FAT long names/subdirectories;
- no ext2 or reboot-persistence gate.

**Exit criteria:** complete v0.3-v0.4 block, path, mount, structured filesystem,
real FAT, malformed-image, nested-directory, and persistence contracts without
FAT policy returning to generic VFS.

## RISK-014 — Desktop applications are incomplete daily tools

**Severity:** P1 for v1  
**Tracking:** issue #2 / v0.6

Seven applications run, but Files is root-only, Editor is 512 bytes with a
caret-line viewport, Shell lacks complete path-aware workflows, Control/Monitor
are narrow, no shared runtime/widget layer exists, and no reboot-persistence
workflow is complete.

v0.6 depends on v0.3 paths/metadata, v0.4 real FAT, and v0.5 userland
runtime/widgets.

## RISK-015 — User copy is not fault-contained

**Severity:** P2

Permission/range validation and kernel-owned payloads exist. Final copy still uses
ordinary EL1 loads/stores; no exception fixup converts a late translation fault
into a syscall error.

**Exit criteria:** recoverable copy helpers, fault/fixup tests, preserved error
semantics, and proof that bad/racy user addresses cannot enter fatal EL1.

## RISK-007 — Raspberry Pi runtime/storage lack physical evidence

**Severity:** P0 hardware track

The tree includes a cross-build backend, EMMC2/SDHCI and mailbox code, diagnostic
telemetry, MBR/block views, an opt-in read-only probe package, and fail-closed
normal capabilities.

It has no promoted physical boot, timer, memory, storage, framebuffer, input, USB,
network, or desktop evidence.

**Exit criteria:** controlled CPU entry/core parking, repeatable cold-boot serial,
exact board/firmware setup, memory/timer evidence, safe card reads, subsystem
validation, and a separate safety milestone for writes.

## RISK-008 — Future address-space hardening

Kernel W^X exists, but every process TTBR0 includes kernel/RAM identity mappings
and switches use broad invalidation.

Future work: TTBR1, user-only TTBR0, ASIDs, scoped invalidation, global/non-global
policy, stale-translation tests, and eventually SMP-safe state where applicable.

## Closed-risk evidence summary

| Risk | Closure evidence |
|---|---|
| RISK-001 | Permission-aware page-table checks and invalid-output regressions |
| RISK-002 | Process-local descriptor ownership and exit cleanup |
| RISK-003 | FAT + GPU wiring and 2026-07-17 visible workflow |
| RISK-004 | Focus marker and Files-to-Editor workflow evidence |
| RISK-005 | Deterministic subsystem scripts with explicit assertions |
| RISK-006 | RPi4 build contract with unsupported capabilities failing closed |
| RISK-009 | KLI1 linker assertions and synthetic rejection tests |
| RISK-010 | Explicit EL0/helper/post-EOI execution documentation |
| RISK-011 | One-command automated baseline and hosted artifacts |
| RISK-012 | Kernel-owned syscall payload and output validation boundaries |
| RISK-016 | Parent-owned zombie/wait lifecycle regression |
