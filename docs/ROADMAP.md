# Roadmap

This roadmap starts from the verified v0.1 QEMU desktop baseline and targets a
small, usable v1.0 QEMU desktop operating system.

It describes ordering and exit criteria, not current implementation evidence.

- Current verified behavior: `CURRENT_STATE.md`
- Active defects and residuals: `TECHNICAL_RISKS.md`
- Implemented architecture: `ARCHITECTURE.md`
- Practical work slicing: `DEVELOPMENT_GUIDE.md`

## Product target

ArmoniOS v1.0 should let one user:

- boot reliably to a graphical desktop;
- browse multiple mounted filesystems;
- create directories and use long file names;
- edit multi-line text with scrolling and safe truncating saves;
- copy, move, rename, and delete files;
- use a useful path-aware shell;
- inspect and terminate processes;
- inspect memory and storage state;
- change persistent settings;
- reboot and confirm files and settings survived.

Chosen release defaults:

- primary runtime: QEMU `virt`;
- shape: compact personal desktop OS;
- kernel: monolithic, explicit, freestanding C11 with narrow AArch64 assembly;
- compatibility: internal interfaces may change before v1;
- public ABI: append-only unless an incompatibility is explicitly approved and
  documented;
- writable v1 filesystem: FAT with directories and long names;
- secondary v1 filesystem: ext2, at least read-only;
- Raspberry Pi: separate hardware track, not a v1 QEMU dependency.

## Phase summary

| Phase | State | Promotion blocker |
|---|---|---|
| v0.1 baseline | COMPLETE | None for the recorded QEMU baseline |
| v0.2 cleanup/hardening | PROMOTION CANDIDATE | `RISK-017` residual disposition, `RISK-018`/issue #63, final visible pass, exact promotion runs, tag/release record |
| v0.3 storage/VFS platform | NEXT AFTER v0.2 | Depends on formal v0.2 disposition |
| v0.4 real FAT | PLANNED | Depends on v0.3 block/path/filesystem contracts |
| v0.5 userland runtime/widgets | PLANNED | Depends on stable storage and ABI shapes |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Depends on v0.3-v0.5 |
| v0.7 ext2 read-only | PLANNED | Depends on v0.3 filesystem interface |
| v0.8 desktop polish | EARLY PARTIAL | Depends on useful applications and stable persistence |
| v0.9 beta stabilization | NOT STARTED | Depends on all QEMU P0 risks closed and every P1 closed or accepted |
| v1.0 | NOT READY | Complete user workflow and release evidence remain incomplete |

## Sequencing rules

1. Do not add broad product features while a release-blocking kernel P1 remains
   open without an explicit accepted-risk decision.
2. Build generic block, path, mount, and metadata contracts before general FAT or
   application rewrites.
3. Keep FAT-specific behavior out of generic VFS code.
4. Add syscall numbers only with kernel implementation, user wrappers, host tests,
   QEMU evidence where applicable, and ABI documentation in the same cut.
5. Keep Raspberry Pi work isolated from the QEMU release line.
6. Every milestone needs deterministic automated gates and dated visible/manual
   evidence where user-visible behavior is claimed.
7. Preserve the 108000-byte production kernel ceiling unless a deliberate release
   decision defines and justifies a replacement budget. Compact or redesign first.
8. Do not call application usefulness or polish “v1.1” before v1.0 exists.
9. Prefer small, independently testable PRs over phase-sized rewrites.
10. Preserve the v0.1 workflow while migrating foundations unless an explicit
    compatibility break is approved.

## v0.1 — verified QEMU baseline

**State: COMPLETE**

Delivered:

- AArch64 QEMU boot and graphical desktop;
- preemptive EL0 processes, syscalls, IPC, anonymous memory, and process lifecycle;
- kernel W^X;
- process-local VFS descriptors;
- permission-aware user-pointer validation;
- kernel-owned syscall payloads;
- KLI1 mutable-storage contract;
- kernel compositor and seven applications;
- writable root-only FAT32 8.3 workflow;
- deterministic framebuffer, USB, network, user-copy, focus, and FAT gates;
- hosted CI evidence;
- dated visible create/edit/save/rename/reopen/delete evidence;
- RPi4 build contract with unsupported normal capabilities failing closed.

The v0.1 claim excludes production hardening, general FAT, ext2, complete daily
applications, and Raspberry Pi hardware support.

## v0.2 — cleanup and runtime hardening

**State: PROMOTION CANDIDATE**

### Landed implementation

- lower subsystems receive kernel-owned syscall buffers;
- output pages are permission-checked before copying;
- generic VFS mount callbacks replace FAT-specific dispatch in the facade;
- dynamic FAT nodes are invalidated after rename/delete;
- process-owned descriptors and parent/wait lifecycle are verified;
- Raspberry Pi normal optional capabilities remain fail closed;
- timer hard-IRQ work is limited to fixed accounting, rearm, readiness publication,
  and scheduler counters;
- GUI, input, USB, and network work runs in the measured post-EOI service;
- per-class and aggregate telemetry exists;
- input producers, shared input, partial redraw, and network RX are count-bounded;
- one service-wide generic-counter deadline is enforced at safe checkpoints;
- deadline expiry republishes original readiness and skips later optional work;
- network polling and receive are suppressed outside the active NETWORK phase.

### Landed automated evidence

Forced-expiry stress proves repeated EL0 progress while actual republish logic,
input, redraw, DHCP, and network activity occur.

Natural-deadline RX saturation proves:

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

Evidence provenance:

- original GitHub PR #62 merge metadata:
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`;
- current-main runtime replay commit:
  `d5c104a0badc3a2d553516159b2b745737dd252f`;
- final PR #62 head:
  `04f65776d1bbe07545113652342c32f2448bfc7b`;
- original final PR validation:
  - `Verify ArmoniOS` `29896952424` (#295): success;
  - `CI - Tests` `29896952435` (#435): success;
- production kernel 107918 / 108000 bytes;
- existing host, QEMU, RPi4, ABI, stack, `.data`, and size gates retained.

The current audited main tree and current-tree workflows are recorded in
`CURRENT_STATE.md`.

### Promotion checklist

1. Complete or explicitly disposition issue #63 / `RISK-018`.
2. Accept the absence of device-level virtio-net drop telemetry for v0.2 or land a
   trustworthy counter contract.
3. Accept the one-operation full-redraw boundary or add finer checkpoints and
   evidence.
4. Retain all existing automated subsystem and stress gates.
5. Run `bash tools/verify.sh` on the exact final promotion tree.
6. Run a dated visible QEMU workflow on that same tree.
7. Close or explicitly accept `RISK-017` and `RISK-018` with rationale.
8. Create the v0.2 tag and release record naming the tree, runs, manual tester,
   workflow, limitations, and accepted residuals.

A later scheduler may move the bounded bottom half into a wakeable EL1 service.
That redesign is not required for the current cooperative v0.2 contract.

## v0.3 — storage and VFS platform

**State: NEXT AFTER v0.2**

Goal: replace fixed demo plumbing with coherent generic storage, path, mount, and
metadata foundations before adding general FAT or rewriting applications.

### Cut 1 — block-device descriptor

Define a generic descriptor containing:

- logical sector size;
- total sector/byte capacity with overflow-safe conversion;
- stable device identity/type;
- read-only state;
- bounded read operation;
- bounded write operation;
- flush operation and explicit unsupported behavior.

Required evidence:

- invalid sector sizes and capacities rejected;
- range arithmetic and end-of-device access tested;
- read-only writes fail predictably;
- absent flush support is explicit rather than silently successful;
- virtio block, bounded block views, and RPi4 diagnostic adapters preserve current
  behavior.

### Cut 2 — path normalizer

Implement one pure, host-testable absolute-path normalizer with explicit policy
for:

- required leading slash;
- repeated slashes;
- empty components;
- `.`;
- `..` and root escape;
- component and complete-path limits;
- trailing slash behavior;
- overflow-safe output sizing.

Do not tie this helper to FAT semantics.

### Cut 3 — mount resolver

Implement deterministic mount selection with:

- exact component-boundary matching;
- longest valid mount-prefix resolution;
- root mount behavior;
- relative path passed to the selected filesystem;
- read-only mount enforcement;
- tests for `/`, `/fat`, `/tmp`, `/armonios`, and future `/ext`;
- no accidental `/fatx` match for `/fat`.

### Cut 4 — structured kernel filesystem types

Add kernel-internal structures for:

- directory entries;
- file type;
- size and basic attributes;
- filesystem/device information;
- iterator or bounded readdir state;
- explicit ownership and lifetime rules.

Keep these internal until the implementation shape is stable enough for a public
ABI.

### Cut 5 — filesystem operations

Extend the generic filesystem interface deliberately:

- probe and mount;
- open and close;
- read and write;
- create and unlink;
- rename;
- mkdir;
- truncate;
- stat;
- readdir;
- filesystem information;
- flush where applicable.

Each operation must specify read-only behavior, partial progress, rollback,
capacity limits, and errors.

### Cut 6 — append-only public ABI

Only after kernel implementation and tests exist, add the required user ABI in
small cuts. Candidate calls include:

- `SYS_MKDIR`;
- `SYS_TRUNCATE`;
- `SYS_STATX`;
- `SYS_READDIRX`;
- `SYS_FSINFO`.

Every call must land with:

- append-only number;
- kernel dispatcher and implementation;
- kernel-owned input/output handling;
- `libkarm` wrapper;
- public structure layout assertions;
- host ABI tests;
- at least one real userland consumer;
- `SYSCALLS.md` update.

### Cut 7 — compatibility migration

Route the existing bootfs, tmpfs, and root-only FAT32 workflow through the new
path and mount contracts without changing the v0.1 user workflow.

Exit criteria for v0.3:

- Shell lists `/`, `/fat`, `/tmp`, and `/armonios` through the common resolver;
- normalization, traversal, mount-boundary, multiple-mount, and read-only tests
  pass;
- existing v0.1 applications and FAT workflow remain functional;
- generic VFS contains no FAT-specific path policy;
- new ABI is fully documented and tested;
- full automated gate and a dated visible filesystem-navigation workflow pass.

## v0.4 — real FAT

**State: PLANNED**

Build on v0.3 rather than expanding the compatibility bridge in place.

Required scope:

- FAT12/16/32 probe and mount where practical;
- VFAT long file names;
- subdirectories;
- create, read, write, list, stat, truncate, mkdir, rename, move, and delete;
- file growth and shrink;
- MBR-backed QEMU images;
- safe rejection of unsupported/corrupt geometry;
- malformed-image and interoperability fixtures;
- reboot-persistence gates;
- removal or isolation of root-only compatibility policy.

Exit workflow:

1. create nested directories;
2. create and edit a long-name file;
3. close and reopen it;
4. rename and move it;
5. reboot;
6. verify content and hierarchy;
7. remove the file and directories safely.

## v0.5 — userland runtime and widgets

**State: PLANNED**

- small heap backed by `SYS_MMAP`;
- dynamic byte/string buffers;
- bounded vectors and ownership helpers;
- path, argv, formatting, and error helpers;
- reusable labels, buttons, text fields, list views, scrolling, and dialogs;
- no mutable static `.data` or `.bss` in shipping images;
- unchanged KLI1 and stack gates;
- versioned diagnostic interfaces before Monitor consumes kernel internals.

Exit criteria:

- at least two applications use each promoted shared component;
- ownership and failure behavior are tested;
- total embedded application size remains controlled;
- no hidden libc/POSIX assumption enters userland.

## v0.6 — useful applications

**State: PARTIAL DEMOS ONLY**  
**Tracking:** issue #2

Depends on v0.3-v0.5.

Required integrated workflow:

- navigate directories and mount roots in Files;
- create folders and long-name files;
- edit multi-line text with scrolling and safe truncate/save;
- copy, move, rename, and delete;
- use path-aware shell commands;
- inspect and terminate a process;
- inspect memory and storage;
- persist at least three observable settings;
- reboot and confirm files and settings survived.

Applications should share runtime and widget foundations rather than duplicating
new fixed-size local solutions.

## v0.7 — ext2 read-only

**State: PLANNED**

- validate superblock and feature bits;
- read block groups, inodes, directories, regular files, and supported indirect
  blocks;
- define symlink policy;
- reject unsupported incompat features;
- mount `/ext` read-only through the v0.3 interface;
- test valid fixtures, malformed/corrupt images, mount boundaries, and QEMU access.

Ext2 writes are outside v1 unless read-only support is already stable and a
separate write-safety plan is approved.

## v0.8 — desktop polish

**State: EARLY PARTIAL**

- reliable launcher/taskbar/focus/minimize/restore behavior;
- duplicate-launch policy;
- consistent titles, status, errors, and confirmations;
- keyboard navigation and visible focus;
- scrolling and cursor regions;
- resizing and damage repaint evidence;
- reduced hardcoded geometry where it blocks use;
- deliberate storage/runtime diagnostics;
- stable settings and recovery UX.

Exit criteria include a documented 30-minute visible QEMU session without panic,
scheduler stall, blank compositor, stuck input, or data loss.

## v0.9 — beta stabilization

**State: NOT STARTED**

- freeze syscall and KLI1 ABI for v1;
- fuzz path, filesystem, image, and syscall inputs;
- run sustained process, mapping, window, storage, input, and network stress;
- automate reboot persistence;
- investigate every fatal EL1 observation;
- complete install/run/recovery documentation;
- close all QEMU P0 risks;
- close or explicitly accept every QEMU P1 risk with rationale;
- record a beta image hash and complete evidence matrix.

## v1.0 — usable QEMU mini desktop

**State: NOT READY**

Final acceptance requires:

- reliable graphical boot and daily desktop workflow;
- writable FAT with directories and long names;
- read-only ext2;
- useful Files, Editor, Shell, Control/Settings, Monitor, Panel, and Clock;
- reboot persistence for files and settings;
- bounded runtime execution with sustained-load evidence;
- fault behavior and recovery documented;
- zero open QEMU P0 risks;
- every P1 closed or explicitly accepted;
- full automated gate on the release tree;
- dated visible acceptance workflow;
- tagged release with exact artifact hashes and evidence.
