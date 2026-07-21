# Technical Risk Register

This register tracks correctness defects, architectural limits, missing evidence,
and release blockers. Status terms follow `DOCUMENTATION_POLICY.md`.
Operational truth lives in `CURRENT_STATE.md`; sequencing lives in `ROADMAP.md`.

## Severity

- **P0** — can compromise correctness, isolation, data integrity, or the core
  contract of the affected release.
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required product behavior.
- **P2** — hardening or future-facing limitation that should be scheduled
  deliberately.

Hardware-track severity does not block the QEMU-only line while unsupported
capabilities remain fail closed and no hardware support claim is made.

## Summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-001 | P0 | User-copy permissions | CLOSED | v0.1 isolation |
| RISK-002 | P0 | VFS descriptors | CLOSED | v0.1 multi-process correctness |
| RISK-003 | P1 | Visible FAT workflow | CLOSED | v0.1 desktop evidence |
| RISK-004 | P1 | Desktop focus | CLOSED | v0.1 usability |
| RISK-005 | P1 | Deterministic QEMU gates | CLOSED | v0.1 reproducibility |
| RISK-006 | P1 | Raspberry Pi build contract | CLOSED | Build boundary only |
| RISK-007 | P0 hardware track | Raspberry Pi storage | OPEN | Blocks physical support claims |
| RISK-008 | P2 future | Address-space architecture | PARTIAL / FUTURE | Stronger hardening |
| RISK-009 | P1 | KLI1 mutable storage | CLOSED | Userland image contract |
| RISK-010 | P2 | Scheduling description | CLOSED | Architecture accuracy |
| RISK-011 | P1 | Verification infrastructure | CLOSED | Reproducibility |
| RISK-012 | P1 v0.2 | Syscall buffer ownership | CLOSED | v0.2 cleanup |
| RISK-013 | P1 v1 | Storage/VFS platform | OPEN | Blocks v1 filesystem workflow |
| RISK-014 | P1 v1 | Desktop applications | OPEN | Blocks v1 usability |
| RISK-015 | P2 | Fault-contained user copy | OPEN | Future hostile/racy mapping hardening |
| RISK-016 | P1 | Process lifecycle | CLOSED | Parent/wait correctness |
| RISK-017 | P1 | Deferred runtime execution | OPEN; PHASE 1B CANDIDATE | Blocks formal v0.2 promotion |

## Open risks

### RISK-017 — Deferred runtime execution is measurable but not bounded

**Severity:** P1 runtime hardening  
**Affected scope:** interrupt-to-EL0 latency, GUI responsiveness, input/network
fairness, exception-stack occupancy, and formal v0.2 promotion  
**Tracking:** issue #43  
**Merged aggregate implementation:** PR #44 / `b3fd013`  
**Active work-class candidate:** PR #45

The timer callback performs fixed accounting, rearm, publication, and scheduler
work. The runtime backend executes after EOI but before process dispatch and
`eret`.

#### Aggregate measurements already merged

The internal snapshot records:

- accepted and coalesced requests;
- non-empty and empty consumer invocations;
- requeued passes;
- last, maximum, and cumulative generic-counter duration;
- passes exceeding one timer interval;
- counter frequency, threshold, pending work, and last-consumed work.

Production timing uses `CNTPCT_EL0`; `CNTFRQ_EL0` supplies conversion. At 100 Hz
the current observation threshold is approximately 10 ms. It is not the final
accepted latency budget.

#### Work-class measurements in PR #45

The candidate adds compact indexed last-pass, maximum-pass, and cumulative
counts for:

- input events produced by QEMU virtio-input and direct USB HID;
- input events consumed from the shared queue during the active bottom-half pass;
- successful display redraw submissions.

The shared 64-event input queue also records:

- current depth;
- lifetime high-water;
- rejected full-queue pushes.

Runtime telemetry retains maximum depth observed during a pass, queue high-water,
and cumulative overflow. Reports outside the active runtime-service pass are
ignored, preventing cooperative console work from contaminating bottom-half
measurements.

Candidate implementation tree before documentation:
`0febb1bf2303caa079a54ad4344fb04acb275507`.

Current candidate evidence:

- `Verify ArmoniOS` run `29830679366`: success;
- `CI - Tests` run `29830679347`: final result must be recorded before merge;
- QEMU kernel binary: 107204 bytes against the 108000-byte ceiling.

The candidate includes deterministic runtime metric tests and a standalone input
queue depth/high-water/overflow regression. The final documentation tree must be
validated before merge.

#### Why RISK-017 remains open

During the service pass:

- execution remains inside the IRQ exception path;
- the 288-byte saved frame remains on the EL1 stack;
- nested IRQ helpers restore the vector's prior masked state;
- EL0 remains paused;
- the complete input queue may still be drained;
- USB/device work has no operation budget;
- network work has no frame/packet measurement or budget;
- redraw has no damage/time budget;
- no class or global deadline is enforced;
- no budget-exhaustion pending-bit rule exists;
- no sustained-load QEMU heartbeat proves EL0 progress.

The pending word and telemetry are non-atomic single-core structures. They are
valid only under the current one-consumer model.

**Failure mode:** sustained input, USB, redraw, or network traffic can extend one
exception path and delay EL0 and other normal IRQ handling. Queue loss is now
countable for shared input, but it is not yet prevented.

**Remaining exit criteria:**

1. measure network frames, receive pressure/drops, device operations, damage
   batches, and full-redraw fallbacks;
2. choose independent per-class limits from measured evidence;
3. enforce a global counter-tick deadline;
4. preserve or republish specific pending bits when a class or deadline expires;
5. count every budget exhaustion and loss path;
6. add deterministic QEMU stress with an EL0 heartbeat under combined load;
7. keep all existing subsystem gates green;
8. record a dated visible desktop pass;
9. decide whether the bounded bottom half remains permanent or later becomes a
   wakeable EL1 service.

Measurement is progress, not closure. RISK-017 still blocks formal v0.2.

### RISK-013 — Storage and VFS are too narrow for v1

**Severity:** P1 for v1

Foundations include a fixed mount table, filesystem callbacks, process-local
descriptors, primary-MBR FAT32 discovery, bounded block views, and a writable
root-only FAT32 bridge.

Limits remain:

- 24 VFS nodes, four mounts, eight descriptors/process, 64-byte paths;
- no common normalized path resolver;
- no structured directory/metadata ABI;
- no `mkdir`, truncate, structured stat/readdir, or filesystem-info calls;
- root-only 8.3 FAT32;
- no long names, directories, general FAT variants, ext2, or combined reboot
  persistence gate.

**Exit criteria:** complete v0.3-v0.4 block metadata/flush/read-only contracts,
path resolution, structured filesystem operations and ABI, real FAT
names/directories/truncate, malformed-image tests, and QEMU nested-directory and
persistence gates.

### RISK-014 — Desktop applications are incomplete daily tools

**Severity:** P1 for v1  
**Tracking:** issue #2, **v0.6 useful desktop applications**

The seven applications are real EL0 programs but:

- Files is tied to root-only `/fat` and an eight-entry view;
- Editor uses a 512-byte buffer and renders only the caret line;
- Shell lacks copy/move/remove/mkdir/touch/edit/open/df workflows;
- Control persistence is narrow;
- Monitor is informational rather than operational;
- no shared heap/container layer or widget toolkit exists;
- no final reboot workflow or 30-minute stable visible session exists.

Issue #2 is intentionally v0.6, not v1.1. It depends on v0.3 paths/metadata,
v0.4 real FAT, and v0.5 shared runtime/widgets.

**Exit criteria:** complete v0.5-v0.8 with shared helpers/widgets,
directory-aware Files, multi-line Editor, useful Shell commands, persistent
settings, Monitor process controls, reliable panel/window lifecycle,
reboot-persistence automation, and dated visible evidence.

### RISK-015 — User copy is not fault-contained

Permission-aware validation and kernel-owned payloads are implemented. The final
transfer still uses ordinary EL1 loads/stores; no exception fixup converts an
unexpected translation fault into a syscall error.

**Exit criteria:** recoverable `copy_from_user` / `copy_to_user`, exception/fixup
tests, preserved `ERR_INVAL`/`ERR_PERM`, and proof that a bad or racy address
cannot enter the fatal EL1 path.

### RISK-007 — Raspberry Pi storage lacks physical evidence

The SDHCI/EMMC2 core, mailbox clock query, failure telemetry, MBR discovery,
bounded partition view, and opt-in read-only diagnostic image exist. Normal RPi4
capabilities remain zero. No physical card, sector/FAT, framebuffer, input, or
desktop evidence exists.

**Exit criteria:** controlled CPU entry/core parking, repeatable cold-boot serial,
memory/timer validation, card initialization, sector/FAT reads on disposable
media, and only then considered read-only capability. Writes require a separate
recovery-oriented milestone.

## Future hardening under RISK-008

Kernel W^X is implemented, but every process TTBR0 duplicates kernel/RAM identity
mappings and switches use broad TLB invalidation. Stronger hardening requires
TTBR1, user-only TTBR0 roots, ASIDs, scoped invalidation, global/non-global
policy, and stale-translation tests.

## Closed-risk evidence summary

| Risk | Closure evidence |
|---|---|
| RISK-001 | Page-table permission checks and host/QEMU invalid-output regressions |
| RISK-002 | Process-local owner checks and exit cleanup tests |
| RISK-003 | FAT+GPU wiring and Rocco's 2026-07-17 visible workflow |
| RISK-004 | Focus marker and manual Files-to-Editor evidence |
| RISK-005 | Deterministic framebuffer, USB, network, usercopy, focus, and FAT scripts |
| RISK-006 | Complete RPi4 board contract with fail-closed unsupported paths |
| RISK-009 | Linker assertions and synthetic KLI1 mutable-storage rejection |
| RISK-010 | Explicit preemptive EL0 / cooperative EL1 / bottom-half documentation |
| RISK-011 | One-command local baseline and hosted logs/artifacts |
| RISK-012 | Kernel-owned VFS/argv/IPC/GUI/info payload boundaries |
| RISK-016 | Parent-owned zombie/wait regression and orphan reclamation |
