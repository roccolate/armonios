# Roadmap

This roadmap describes future ordering and exit criteria. It does not duplicate
current implementation detail or historical workflow evidence.

- Current implementation: `CURRENT_STATE.md`
- Implemented design: `ARCHITECTURE.md`
- Active technical risks: `TECHNICAL_RISKS.md`
- Practical contribution workflow: `DEVELOPMENT_GUIDE.md`

## Product target

ArmoniOS v1.0 should let one user:

- boot reliably into a graphical QEMU desktop;
- browse multiple mounted filesystems;
- create directories and use long file names;
- create, edit, truncate, copy, move, rename, and delete files;
- close, reopen, and reboot without losing committed content;
- use a path-aware shell;
- inspect and terminate processes;
- inspect memory and storage state;
- change persistent desktop settings;
- use a coherent set of small but practical applications.

Chosen v1 boundaries:

- primary verified runtime: QEMU `virt`;
- product shape: compact personal desktop OS;
- kernel: monolithic, freestanding C11 with narrow AArch64 assembly;
- public ABI: native and append-only unless an incompatibility is explicitly
  approved and versioned;
- writable filesystem: FAT with directories and long names;
- secondary filesystem: ext2, at least read-only;
- Raspberry Pi: separate hardware track, not a dependency of the QEMU v1 release.

## Phase summary

| Phase | State | Remaining promotion boundary |
|---|---|---|
| v0.1 verified baseline | COMPLETE | None for the recorded QEMU baseline |
| v0.2 cleanup/runtime hardening | IMPLEMENTATION COMPLETE; RELEASE RECORD PENDING | Issue #76 visible validation, exact tag, and release notes |
| v0.3 storage/VFS platform | IN PROGRESS | Seek, truncate, mkdir/rmdir, nested mutation, and durability |
| v0.4 real FAT | EARLY PARTIAL | VFAT long names and complete mutation/persistence workflow |
| v0.5 userland runtime/widgets | EARLY PARTIAL | General runtime gaps and promoted reusable widget models |
| v0.6 useful applications | PARTIAL DEMOS | Complete daily workflows on v0.3-v0.5 foundations |
| v0.7 ext2 read-only | PLANNED | Generic read-only driver and interoperability gates |
| v0.8 desktop polish | EARLY PARTIAL | Coherent interaction, visuals, diagnostics, and long-session evidence |
| v0.9 beta stabilization | NOT STARTED | ABI/release freeze, fuzzing, persistence, and accepted residuals |
| v1.0 | NOT READY | Complete verified workflow and release record |

## Sequencing rules

1. Preserve the verified QEMU baseline while replacing foundations.
2. Complete generic contracts before adding filesystem-specific product features.
3. Keep FAT-specific behavior outside generic VFS policy.
4. Land every public ABI addition with implementation, wrappers, tests,
   documentation, and at least one real consumer.
5. Keep Raspberry Pi work isolated from the QEMU release line.
6. Require deterministic automated gates and dated visible evidence for promoted
   user-visible behavior.
7. Preserve the 128 KiB production-image ceiling unless an explicit project
   decision replaces it.
8. Prefer small independently testable cuts over phase-sized rewrites.
9. Add shared runtime or UI components only when a concrete consumer exists and
   binary/stack deltas are measured.
10. Do not call a later phase complete merely because one supporting primitive
    landed early.

## v0.1 — verified QEMU baseline

**State: COMPLETE**

Delivered:

- AArch64 boot and graphical desktop;
- preemptive EL0 processes and native syscalls;
- memory, process, user-copy, descriptor, GUI, input, USB, network, and FAT gates;
- seven KLI1 applications;
- writable root-level FAT32 8.3 workflow;
- deterministic hosted CI and dated visible evidence;
- Raspberry Pi build contract with unsupported normal capabilities failing closed.

The v0.1 claim excludes production hardening, general FAT, ext2, complete daily
applications, and physical Raspberry Pi support.

## v0.2 — cleanup and runtime hardening

**State: IMPLEMENTATION COMPLETE; RELEASE RECORD PENDING**

Landed:

- kernel-owned syscall payload boundaries;
- permission-aware output validation;
- generic VFS mount dispatch;
- process-owned descriptors and parent/wait lifecycle;
- fail-closed optional board capabilities;
- fixed timer hard-IRQ work;
- bounded post-EOI input, USB, redraw, and network work;
- service-wide cooperative deadline and continuation;
- strict network-phase routing;
- sustained-load EL0 progress evidence;
- IRQ-origin gating that prevents EL1 frames entering process preemption;
- repeated production FAT32 VMM soak coverage;
- 128 KiB image-budget policy;
- initial `libarmdesk` and public ABI boundaries.

Remaining release steps:

1. update a local checkout to the exact final `main` tree;
2. run the complete automated gate;
3. run and record the visible desktop/FAT workflow from issue #76;
4. record tester, date, host setup, exact commit, result, and limitations;
5. create the annotated `v0.2` tag;
6. publish concise release notes.

No additional kernel feature is required to perform this release record.

## v0.3 — storage and VFS platform

**State: IN PROGRESS**

Goal: provide coherent generic storage, paths, mounts, metadata, mutation, and
durability contracts before general FAT and application expansion.

### Landed foundations

- generic block-device capacity, block-size, read-only, read/write, flush, and
  bounded-view contract;
- QEMU virtio-blk and Raspberry Pi diagnostic adapters;
- whole-device and primary-MBR FAT32 discovery;
- canonical absolute paths;
- longest-prefix mount resolution;
- nested read traversal of existing FAT32 8.3 directory trees;
- filesystem-neutral native metadata and directory entries;
- public `STAT_V2`, `READDIR_V2`, and `FSINFO` contracts;
- native filesystem-specific error values;
- Files as a real structured-metadata and filesystem-information consumer.

### Cut 1 — complete seek semantics

Required:

- consistent `SEEK_SET`, `SEEK_CUR`, and `SEEK_END` behavior;
- overflow-safe signed offset arithmetic;
- explicit directory and unsupported-backend behavior;
- unchanged descriptor offset after failure;
- host tests plus a real userland consumer.

### Cut 2 — truncate

Required:

- descriptor and/or path contract chosen explicitly;
- regular-file-only enforcement;
- safe grow and shrink behavior;
- FAT cluster-chain release and allocation rollback;
- zero-fill policy for growth;
- unchanged file and descriptor state on failure;
- exact error propagation;
- public ABI, wrapper, tests, and consumer in the same cut.

### Cut 3 — mkdir and rmdir

Required:

- explicit parent-directory validation;
- `EXIST`, `NOTDIR`, `NOTEMPTY`, `ROFS`, `NOSPC`, and `NOTSUP` semantics;
- `.` and `..` handling;
- rollback for partially updated directory/FAT state;
- root and nested tests;
- malformed and full-directory fixtures.

### Cut 4 — nested mutation

Extend create, unlink, rename, and move below the FAT root only after directory
mutation and rollback rules are stable.

Required:

- canonical source and destination resolution;
- cross-mount rejection;
- directory-cycle prevention;
- source/destination rollback;
- dynamic-node invalidation;
- open-descriptor behavior documented and tested.

### Cut 5 — durability

Required:

- explicit flush/fsync contract;
- truthful capability reporting;
- transport flush propagation;
- defined ordering for FAT and directory updates;
- reboot-persistence QEMU fixture;
- documented behavior when the transport cannot flush.

### v0.3 exit criteria

- generic VFS contains no FAT-specific path policy;
- seek, truncate, mkdir/rmdir, and nested mutation contracts pass host and QEMU
  coverage;
- the root-level v0.1 workflow remains compatible;
- durability capability is explicit rather than assumed;
- a dated visible nested-directory workflow passes on the exact promotion tree;
- `SYSCALLS.md`, architecture, current state, and public headers agree.

## v0.4 — real FAT

**State: EARLY PARTIAL**

Build on v0.3 rather than expanding the compatibility policy in place.

Required scope:

- VFAT long file names;
- interoperable directory creation and removal;
- complete create/read/write/list/stat/truncate/rename/move/delete workflows;
- safe file growth and shrink;
- malformed-image rejection;
- external FAT interoperability fixtures;
- reboot-persistence gates;
- removal or isolation of root-only compatibility behavior.

Optional only after FAT32 is stable:

- FAT12/FAT16 support where practical;
- broader partition-table support.

Exit workflow:

1. create nested directories;
2. create and edit a long-name file;
3. close and reopen it;
4. truncate it shorter and grow it again;
5. rename and move it;
6. reboot;
7. verify content and hierarchy;
8. delete the file and directories safely.

## v0.5 — userland runtime and widgets

**State: EARLY PARTIAL**

### Runtime foundation already landed

- public ABI headers independent from kernel-private implementation;
- typed syscall wrappers;
- `crt0.o` plus `libkarm.a` static-library build shape;
- monotonic caller-owned arenas;
- growable binary buffers;
- dynamic null-terminated strings;
- complete descriptor writes;
- rollback-safe complete binary/text file reads.

### Remaining runtime work

Promote only as required by real consumers:

- safe formatted output;
- line-oriented input and parsing helpers;
- bounded vectors or other ownership helpers;
- path and argv utilities not already covered by the VFS/public ABI;
- safe path-level replace/write after truncate or atomic replacement exists;
- optional reusable heap only if arena lifetimes cannot express the consumer.

A global hidden allocator is not the default design. Caller-owned lifetime and
explicit failure behavior remain preferred.

### Desktop toolkit work

The canonical layer is `programs/libarmdesk/`. The current foundation contains
GUI wrappers, rectangle/clipping helpers, and semantic theme tokens.

Remaining promoted components may include:

- labels and buttons;
- bounded row/column layouts;
- text fields and editing models;
- list views and scrolling;
- dialogs and notifications;
- icon masks and bitmap drawing;
- logical event adapters and replay fixtures.

The closed Control-widget draft is not part of `main`; any new widget cut must be
rebuilt from current main and justified by at least one real consumer.

### v0.5 exit criteria

- every shared component has explicit ownership and failure semantics;
- pure models are host-testable without syscalls;
- platform drawing/adapters are separate from neutral state;
- at least two applications use each broadly promoted component, unless a narrow
  one-consumer abstraction has a documented reason;
- per-application binary and stack deltas remain measured;
- no hidden libc/POSIX or mutable-global dependency enters userland;
- compatibility `programs/libkarmdesk/` is removed only after all consumers move.

## v0.6 — useful applications

**State: PARTIAL DEMOS**

Target applications:

- **Files:** multiple mounts, nested navigation, long names, copy/move, details,
  confirmations, and useful errors;
- **Editor:** dynamic multi-line text, scrolling, selection, safe save/replace,
  and dirty-state prompts;
- **Shell:** canonical paths, useful pipelines of built-in operations, complete
  filesystem workflow, and clear status output;
- **Panel:** scalable launcher/task model and coherent window state;
- **Monitor:** versioned process/memory/storage diagnostics and process control;
- **Control:** persistent settings with validation and rollback;
- **Clock:** stable time/date presentation through public system information.

Exit criteria:

- the v1 product workflow can be completed without test-only tools;
- all application persistence survives reboot where promised;
- failures are visible and recoverable;
- applications use shared runtime/toolkit components instead of duplicating
  unsafe local logic;
- a dated visible multi-application workflow is recorded.

## v0.7 — ext2 read-only

**State: PLANNED**

Required:

- mount through the generic block/mount interface;
- superblock and feature validation;
- inode, direct/indirect block, directory, stat, and file-read support;
- explicit rejection of unsupported features and all mutation;
- malformed-image and interoperability fixtures;
- one real userland navigation/read workflow.

## v0.8 — desktop polish

**State: EARLY PARTIAL**

Required:

- consistent semantic theme use;
- coherent controls, focus, keyboard navigation, and pointer behavior;
- improved panel and window interactions;
- useful icons/bitmap path without kernel policy leakage;
- diagnostic surfaces based on versioned public interfaces;
- long visible-session evidence without freeze, corruption, or unbounded growth.

## v0.9 — beta stabilization

**State: NOT STARTED**

Required:

- freeze the intended v1 public ABI surface;
- preserve old-SDK/binary fixtures;
- fuzz parsers, paths, metadata, and filesystem images;
- stress process, GUI, input, storage, and persistence lifecycles;
- close every QEMU P0 and close or explicitly accept each relevant P1;
- record a release-quality verification matrix and known-limit list;
- remove stale compatibility and diagnostic-only code.

## v1.0 — compact QEMU desktop

**State: NOT READY**

Release only when:

- the complete product workflow passes automated and visible verification;
- storage survives the promised reboot workflow;
- applications are practical within their documented scope;
- the public ABI and SDK boundary are documented and tested;
- ext2 read-only is available or explicitly removed from v1 scope by a recorded
  project decision;
- QEMU risks are closed or accepted with rationale;
- exact source, artifacts, runs, tester, date, limitations, tag, and release notes
  are published.

Physical Raspberry Pi support remains a separate release line until it has its
own controlled hardware evidence.
