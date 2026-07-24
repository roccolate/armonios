# Technical Risk Register

This register contains active correctness, architecture, product, and hardware
risks. Closed investigations are summarized without retaining their full working
notes in the live register.

- current implementation: `CURRENT_STATE.md`;
- implemented design: `ARCHITECTURE.md`;
- future milestone order: `ROADMAP.md`;
- evidence terminology: `DOCUMENTATION_POLICY.md`.

A risk remains open until its exit criteria are met or a named release explicitly
accepts the residual with rationale.

## Severity and status

- **P0** — can compromise correctness, isolation, data integrity, or a core
  contract of the affected release;
- **P1** — materially affects stability, responsiveness, reproducibility,
  maintainability, or required product behavior;
- **P2** — deliberate future hardening that does not block the current baseline.

| Status | Meaning |
|---|---|
| `OPEN` | Exit criteria are incomplete. |
| `PARTIAL / FUTURE` | A usable foundation exists; a stronger design is deferred. |
| `ACCEPTED FOR <release>` | A bounded residual is explicitly accepted for one release. |
| `CLOSED` | Root cause or completion criteria and evidence are recorded. |

Hardware-track risks do not block the QEMU release while unsupported capabilities
fail closed and no physical support claim is made.

## Current summary

| ID | Severity | Area | Status | Release impact |
|---|---:|---|---|---|
| RISK-007 | P0 hardware | Raspberry Pi physical runtime | OPEN | Blocks physical support claims only |
| RISK-008 | P2 | Address-space architecture | PARTIAL / FUTURE | Stronger isolation and scaling |
| RISK-013 | P1 for v1 | Storage and VFS completeness | OPEN | Blocks the v1 filesystem workflow |
| RISK-014 | P1 for v1 | Desktop application usefulness | OPEN | Blocks v1 usability |
| RISK-015 | P2 | Fault-contained user copy | OPEN | Future syscall-boundary hardening |

Release task issue #76 is separate from the technical register. It is the only
remaining v0.2 promotion task: a dated visible workflow, exact validated tree,
tag, and release record.

## RISK-013 — Storage and VFS are incomplete for v1

**Severity:** P1 for v1  
**Status:** OPEN

### Implemented foundation

- process-local descriptors;
- bootfs and tmpfs;
- generic mounts and canonical paths;
- longest-prefix mount resolution;
- generic block-device capacity, read-only, flush, and bounded views;
- whole-device and primary-MBR FAT32 discovery;
- nested read traversal of existing FAT32 8.3 directories;
- native structured metadata and directory entries;
- public `STAT_V2`, `READDIR_V2`, and `FSINFO`;
- filesystem-specific native error values.

### Remaining risk

The system cannot yet provide the v1 storage workflow because it lacks:

- complete seek behavior;
- truncate and safe file shrink/grow;
- mkdir/rmdir;
- rollback-safe nested create, unlink, rename, and move;
- VFAT long names;
- exact free-space accounting;
- explicit durable flush/fsync behavior;
- reboot-persistence proof;
- ext2 read-only support;
- broad malformed-image and interoperability coverage.

The current FAT path supports existing nested reads but root-only mutation. A
power loss or reboot persistence claim is not yet justified.

### Exit criteria

1. Complete the v0.3 generic seek, truncate, directory mutation, rollback, and
   durability contracts.
2. Complete the v0.4 FAT long-name and interoperability workflow.
3. Add malformed-image, capacity, rollback, and reboot-persistence fixtures.
4. Keep FAT-specific policy outside generic VFS.
5. Record a dated visible nested create/edit/move/reboot/delete workflow.
6. Add ext2 read-only or explicitly revise the v1 product scope.

## RISK-014 — Desktop applications are incomplete daily tools

**Severity:** P1 for v1  
**Tracking:** issue #2  
**Status:** OPEN

Seven applications run, but the product workflow remains incomplete:

- Files is centered on `/fat`, short names, and root mutation;
- Editor has a small fixed text model and limited viewport behavior;
- Shell lacks the complete path-aware file workflow;
- Monitor and Control expose narrow demonstrations;
- settings and files do not yet have a complete reboot-persistence story;
- reusable `libarmdesk` controls and layouts are not promoted;
- long visible-session evidence is incomplete.

The early `libkarm` runtime foundation reduces duplicated allocation, container,
and file-read logic, but it does not by itself make the applications complete.

### Exit criteria

1. Complete v0.3 storage/VFS and v0.4 FAT prerequisites.
2. Promote only the `libkarm` and `libarmdesk` components required by real apps.
3. Deliver the application workflows listed under v0.6 in `ROADMAP.md`.
4. Surface useful, specific, recoverable errors.
5. Prove persistence where applications claim it.
6. Record a dated multi-application visible workflow without freeze or corruption.

## RISK-015 — User copy is not fault-contained

**Severity:** P2  
**Status:** OPEN

### Implemented foundation

- complete range and overflow validation;
- registered process-region validation;
- page-table presence and permission checks;
- kernel-owned syscall request/response buffers;
- output validation before provider execution;
- invalid-output and permission regressions.

### Remaining risk

The final copy still uses ordinary EL1 loads and stores. No exception-table or
fault-fixup mechanism converts a late translation fault into a normal syscall
error. A racing or unexpectedly invalid mapping could therefore still become a
fatal EL1 exception after validation.

### Exit criteria

1. Add recoverable copyin/copyout helpers.
2. Define the exception/fixup contract and allowed fault classes.
3. Add deterministic invalidation/race fixtures.
4. Preserve existing public error and partial-progress semantics.
5. Prove a bad user address cannot convert an otherwise recoverable syscall into
   a kernel panic.

## RISK-007 — Raspberry Pi physical runtime is unverified

**Severity:** P0 hardware track  
**Status:** OPEN

### Implemented foundation

- Raspberry Pi cross-build target;
- mailbox and EMMC2/SDHCI scaffolding;
- CSD-derived diagnostic capacity;
- MBR parsing and bounded block views;
- opt-in read-only probe package;
- normal unsupported capabilities fail closed.

### Remaining risk

There is no promoted physical evidence for:

- controlled CPU entry and core parking;
- repeatable cold boot and serial output;
- memory discovery and timer behavior;
- normal storage mounting;
- framebuffer and desktop rendering;
- input, USB, or network;
- safe writable media behavior.

### Exit criteria

1. Record exact board revision, firmware, boot files, power, and media setup.
2. Prove repeatable cold boot and serial output.
3. Validate exception level, core parking, memory, timer, and interrupt routing.
4. Validate read-only media discovery and bounded reads.
5. Promote framebuffer, input, USB, and network independently.
6. Treat writable storage as a separate safety milestone with sacrificial-media
   tests and recovery procedures.

## RISK-008 — Address-space architecture needs future hardening

**Severity:** P2  
**Status:** PARTIAL / FUTURE

Kernel W^X and per-process TTBR0 roots exist, but the current process address
space also contains the kernel/RAM identity mappings needed by exception
handling. Context switches use broad TLB invalidation.

Future work:

- TTBR1 for kernel mappings;
- user-only TTBR0 roots;
- ASIDs;
- global/non-global mapping policy;
- scoped invalidation;
- stale-translation tests;
- explicit SMP-safe state if multi-core execution is introduced.

### Exit criteria

1. Define the intended kernel/user virtual-address split.
2. Move kernel mappings to TTBR1 without breaking exception entry.
3. Add ASID allocation and rollover policy.
4. Replace broad invalidation where safe.
5. Add cross-process stale-translation and teardown tests.

## Closed risks

| ID | Area | Closure summary |
|---|---|---|
| RISK-001 | User-copy permissions | Page-table permission checks and invalid-output regressions |
| RISK-002 | VFS descriptors | Process-local ownership and exit cleanup |
| RISK-003 | Visible FAT workflow | Recorded v0.1 FAT + GPU visible workflow |
| RISK-004 | Desktop focus | Focus and Files-to-Editor workflow gates |
| RISK-005 | Deterministic QEMU gates | Explicit subsystem scripts and hosted evidence |
| RISK-006 | Raspberry Pi build contract | Unsupported normal capabilities fail closed |
| RISK-009 | KLI1 mutable storage | Linker assertions and rejection fixtures |
| RISK-010 | Scheduling description | EL0, helper, IRQ, and post-EOI execution are separated accurately |
| RISK-011 | Verification infrastructure | One-command baseline plus hosted artifacts |
| RISK-012 | Syscall buffer ownership | Kernel-owned payload and complete output validation |
| RISK-016 | Process lifecycle | Parent-owned zombie/wait and teardown regression |
| RISK-017 | Deferred runtime execution | Count bounds, cooperative deadline, continuation, routing, telemetry, and stress evidence; issue #43 closed |
| RISK-018 | Intermittent EL1 VMM fault | EL1 IRQ frames were entering process preemption; origin gate fixed the defect, issue #63 closed, repeated FAT32 soak retained |

## Closed-risk details that remain architectural invariants

### RISK-017

The timer hard IRQ remains limited to fixed accounting, rearm, readiness
publication, and scheduler counters. Post-EOI work remains count-bounded and
subject to a cooperative whole-service deadline. Work interrupted by the
deadline is republished rather than silently discarded.

The current virtio-net interface still lacks a trustworthy device-level drop
counter, and one already-started full redraw or driver operation is not
asynchronously preempted. These are documented boundaries of the closed v0.2
contract rather than claims of nonexistent telemetry or hard real-time behavior.

### RISK-018

The intermittent VMM panic occurred when an IRQ taken during EL1 execution was
passed into the process-preemption path as if it were an EL0 frame. The origin
gate now supplies a schedulable frame only when the saved PSTATE returns to EL0.
EL1-origin interrupts cannot save process state, switch TTBR0, or preempt the
interrupted kernel path.

The semantic/assembly regression and repeated production FAT32 boot coverage
must remain permanent gates.

## Maintenance rules

- Do not reopen a closed risk merely because its detailed history moved out of
  this file; reopen it only with new contradictory evidence.
- Add a new risk when the failure mode, severity, and exit criteria are distinct.
- Do not use green reruns as a substitute for root-cause analysis.
- Keep release-record tasks separate from implementation risks.
- Update this register in the same change that closes, accepts, or materially
  changes a risk boundary.
