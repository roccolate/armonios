# Roadmap

This roadmap starts from the verified v0.1 QEMU desktop baseline and targets a
usable v1.0 mini desktop OS. It is ordered by architectural dependency, user
value, verification cost, and maintenance risk.

Operational truth lives in `CURRENT_STATE.md`. Active defects and exit criteria
live in `TECHNICAL_RISKS.md`. This file describes planned sequencing and must not
be cited as proof that a feature exists.

## Product target

ArmoniOS v1.0 is a small, usable QEMU desktop operating system.

A v1.0 user should be able to:

- boot reliably to the graphical desktop;
- browse multiple mounted filesystems;
- create directories and use long file names;
- edit multi-line text with scrolling and safe truncating saves;
- copy, move, rename, and delete files;
- use a useful command shell;
- inspect and manage processes and memory;
- change persistent settings;
- reboot and confirm files and configuration survived.

Chosen release defaults:

- primary platform: QEMU `virt`;
- product shape: compact personal desktop OS;
- kernel style: monolithic, explicit, freestanding C with narrow AArch64 assembly;
- compatibility: internal interfaces may change before v1;
- ABI policy: existing public syscall numbers and KLI1 layout stay stable unless
  an incompatibility is explicitly documented;
- writable v1 filesystem: FAT with long names and directories;
- secondary v1 filesystem: ext2, at least read-only;
- Raspberry Pi: separate hardware track, not a v1 QEMU release dependency.

## Current phase summary

| Phase | State | Promotion blocker |
|---|---|---|
| v0.1 baseline | COMPLETE | None for the recorded QEMU baseline |
| v0.2 cleanup/hardening | IN PROGRESS / RELEASE CANDIDATE | RISK-017 runtime budgets, sustained-load evidence, formal promotion record |
| v0.3 storage/VFS platform | NEXT | Depends on v0.2 promotion |
| v0.4 real FAT | PLANNED | Depends on v0.3 filesystem interfaces |
| v0.5 userland runtime/widgets | PLANNED | May start after ABI/storage shapes stabilize |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Depends on v0.3-v0.5 |
| v0.7 ext2 | PLANNED | Depends on v0.3 VFS/filesystem interface |
| v0.8 desktop polish | EARLY PARTIAL | Depends on applications becoming useful |
| v0.9 beta | NOT STARTED | Depends on all QEMU P0/P1 risks being closed or accepted |
| v1.0 | NOT READY | Product workflow and final evidence incomplete |

## Sequencing rules

1. Do not add broad features while a kernel-runtime P1 is open without explicitly
   accepting the risk.
2. Build the storage/VFS platform before polishing Files and Editor around the
   current root-only FAT bridge.
3. Add syscall numbers only with implementation, wrappers, tests, and docs in the
   same change.
4. Keep Raspberry Pi work isolated from the QEMU release line.
5. Every milestone needs automated gates and, where user-visible behavior is
   claimed, dated manual evidence.
6. Do not call userland polish “v1.1” before v1.0 exists. Current application
   usefulness and polish belong to v0.6-v0.8.

## v0.1 — verified QEMU baseline

**State: COMPLETE**

Delivered:

- AArch64 QEMU boot and desktop launch;
- EL0 processes, syscalls, IPC, anonymous memory, and round-robin dispatch;
- kernel W^X mappings;
- process-local VFS descriptors;
- permission-aware user-copy validation;
- kernel-owned syscall payload buffers;
- parent-owned zombie/wait lifecycle;
- KLI1 mutable-storage contract;
- kernel compositor and seven applications;
- root-only writable FAT32 workflow;
- deterministic framebuffer, USB, network, usercopy, focus, and FAT wiring gates;
- hosted CI evidence;
- dated visible create/edit/save/rename/reopen/delete evidence;
- RPi4 build contract that fails unsupported capabilities closed.

The v0.1 claim does not include production hardening, general FAT, ext2, useful
daily applications, or Raspberry Pi hardware support.

## v0.2 — cleanup and runtime hardening

**State: IN PROGRESS / RELEASE CANDIDATE**

### Landed work

- lower subsystems receive kernel-owned syscall buffers instead of caller-owned
  EL0 pointers;
- output pages are permission checked before copying;
- VFS dispatches through generic mount callbacks instead of including FAT32
  policy directly;
- dynamic FAT nodes are invalidated after rename/delete;
- Raspberry Pi normal storage remains fail closed;
- process-owned descriptors and parent/wait lifecycle are verified;
- timer hard-IRQ work is reduced to accounting, rearm, publication, and scheduler
  counters;
- periodic GUI, input, device, and network work is centralized after EOI;
- the full `tools/verify.sh` baseline remains green on the validated code tree.

### Remaining work

- instrument deferred runtime-service duration and high-water marks;
- impose per-pass input, USB/device, network, and redraw budgets;
- preserve pending bits when a budget is exhausted;
- add sustained-load QEMU tests proving EL0 progress and no event loss;
- decide whether the service remains a bounded bottom half or becomes a wakeable
  EL1 service after scheduler support exists;
- create a formal v0.2 promotion/tag record with exact evidence.

### Exit criteria

- `bash tools/verify.sh` passes on the promotion commit;
- no timer callback contains rendering, queue drains, network polling, or device
  polling;
- runtime-service maximum duration and work high-water marks are observable;
- work per pass is bounded and leftover work remains pending;
- sustained input/network load cannot stop an EL0 heartbeat;
- documentation and release notes state the exact post-EOI exception-context
  model;
- RPi4 remains fail closed and no hardware claim is introduced;
- a v0.2 tag or release record names the tested commit and workflow runs.

Fault-recoverable copyin/copyout (`RISK-015`) remains valuable hardening but may be
scheduled after v0.2 if its P2 status is explicitly preserved.

## v0.3 — storage and VFS platform

**State: NEXT**

Goal: replace fixed demo plumbing with a coherent filesystem platform before
adding general FAT or rewriting applications.

### Required architecture

- internal block-device descriptor containing:
  - logical sector size;
  - total sector count;
  - read-only state;
  - read, write, and flush operations;
  - device identity/capability metadata;
- mount table supporting `/fat`, `/ext`, `/tmp`, and `/armonios`;
- common absolute-path resolver with:
  - normalization;
  - repeated slash handling;
  - `.` and `..` policy;
  - parent/child traversal;
  - mount-root boundaries;
  - overflow-safe bounded components;
- filesystem driver interface for:
  - probe and mount;
  - open/close;
  - read/write;
  - readdir/stat;
  - create/mkdir;
  - truncate;
  - unlink/rename;
  - read-only rejection;
- structured kernel-internal directory and metadata types.

### Planned ABI extensions

Numbers are assigned only when implementation and tests land:

- `SYS_MKDIR`;
- `SYS_TRUNCATE`;
- `SYS_STATX` with type, size, and flags;
- `SYS_READDIRX` with structured directory entries;
- `SYS_FSINFO` with filesystem type, read-only state, and capacity.

### Exit criteria

- host tests cover normalization, invalid traversal, multiple mounts, nested
  paths, mount-root boundaries, and read-only mounts;
- Shell can list `/`, `/fat`, `/tmp`, and `/armonios` through the common resolver;
- existing v0.1 syscalls and applications remain functional;
- new structs and syscall numbers are documented before promotion;
- no FAT-specific include or path rule returns to generic VFS code.

## v0.4 — real FAT

**State: PLANNED**

Goal: replace the narrow root-only FAT32 bridge with a tested filesystem driver.

### Scope

- FAT12/16/32 probe and mount where practical;
- VFAT long file names;
- subdirectories;
- create/read/write/list/stat/truncate;
- mkdir, rename, move, and delete;
- file growth and shrink;
- simple MBR partition discovery for QEMU images;
- clear rejection of unsupported or corrupt geometry;
- `/fat` remains the primary writable desktop volume;
- exFAT stays outside v1 unless it can be added without destabilizing the line.

### Verification

- generated and fixture disk-image tests;
- long-name and short-name alias cases;
- nested directory creation and traversal;
- cluster-chain growth, shrink, rename, move, and deletion;
- full-disk and full-directory behavior;
- malformed BPB/FAT/directory rejection;
- QEMU persistence across reboot;
- visible Files/Editor workflow in a subdirectory.

### Exit criteria

- Files and Shell no longer depend on 8.3 names;
- a multi-line file can be saved, closed, reopened, moved, and deleted in a
  nested directory;
- host and QEMU persistence gates pass;
- the old root-only bridge is removed or isolated as test-only compatibility
  code.

## v0.5 — userland runtime and widgets

**State: PLANNED**

Goal: make application development practical without violating KLI1.

### Runtime work

- small heap backed by `SYS_MMAP`;
- dynamic buffer and bounded vector helpers;
- path construction and normalization helpers;
- argv parsing helpers;
- reusable formatting and error/status helpers;
- explicit ownership and cleanup conventions;
- no mutable static `.data` or `.bss` in shipping images.

### Widget work

- labels and status bars;
- buttons;
- text fields;
- list views with selection and scrolling;
- scroll bars or viewport helpers;
- simple modal/confirmation dialogs;
- shared focus and keyboard-navigation behavior.

### Exit criteria

- Files, Editor, Control, and Monitor share common runtime and widget code;
- application image size does not regress without an explicit budget decision;
- `make stack-check` remains green;
- `tests/run_kli1_contract_test.sh` continues to reject mutable static storage;
- widgets have host-testable layout/state logic where practical.

## v0.6 — useful desktop applications

**State: PARTIAL DEMOS ONLY**

Goal: turn the seven existing applications into tools that support the v1
workflow.

### Files

- navigate directories and mount roots;
- show type and size;
- create files and folders;
- copy, move, rename, and delete;
- open files with Editor;
- scroll beyond a fixed eight-entry view;
- report storage and operation errors clearly.

### Editor

- multi-line viewport;
- vertical and horizontal scrolling;
- larger or chunked file loading;
- explicit dirty state;
- safe truncate-on-save;
- Save As;
- useful line/column status;
- predictable caret navigation.

### Shell

Preserve existing commands and add:

- `cp`, `mv`, `rm`, `mkdir`, `touch`;
- `echo`, `edit`, `open`, `df`, `clear`;
- clearer grouped help;
- path-aware error messages;
- enough process output to identify PID and state.

### Control / Settings

Persist at least:

- default path or startup volume;
- clock visibility;
- theme choice;
- input repeat preference;
- hostname or system label.

At least three settings must produce observable behavior after reboot.

### Monitor

- process list with PID and state;
- memory and timer information;
- refresh behavior;
- kill selected process with confirmation.

### Exit criteria

- create a folder;
- create and edit a multi-line file;
- save, close, and reopen;
- copy, move, rename, and delete;
- inspect and terminate a process;
- change three settings;
- reboot and confirm files and settings persist.

## v0.7 — ext2 read-only

**State: PLANNED**

Goal: add a second useful filesystem without risking the main writable path.

### Scope

- superblock and feature validation;
- block groups and inode tables;
- inode lookup;
- direct and supported indirect block reads;
- directories and regular files;
- symlink policy documented explicitly;
- clear rejection of unsupported incompat features;
- mount at `/ext` as read-only.

### Exit criteria

- host tests read known ext2 images with nested files and directories;
- corrupt and unsupported images fail safely;
- Files and Shell navigate `/ext`;
- writes fail with a clear read-only error;
- QEMU mounts and reads a fixture volume.

Ext2 writes are outside v1 unless the read-only path is already stable and a
separate write-safety plan exists.

## v0.8 — desktop polish

**State: EARLY PARTIAL**

Goal: make a normal desktop session coherent and predictable.

- reliable launcher/taskbar/focus/minimize/restore behavior;
- duplicate-launch policy;
- consistent titles, status bars, errors, and confirmations;
- keyboard navigation and cursor regions;
- window resizing and damage repaint tests;
- reduced hardcoded geometry where it blocks use;
- storage and runtime-service status visible through Monitor or diagnostics.

### Exit criteria

- a 30-minute manual QEMU desktop session records no crash, user fault, scheduler
  stall, blank compositor, or data loss;
- repeated launch/focus/minimize/restore/close cycles pass;
- deterministic focus, storage, runtime, and visible-wiring gates remain green;
- visible evidence is recorded in `CURRENT_STATE.md`.

## v0.9 — v1 beta

**State: NOT STARTED**

Goal: stabilize rather than expand.

- freeze syscall numbers and user-visible structures for v1;
- freeze KLI1 v1 layout;
- fuzz or property-test path resolution, VFS, FAT, ext2, and application parsers;
- add reboot-persistence and multi-process lifecycle QEMU gates;
- measure kernel size, PMM pressure, GUI backing memory, runtime-service latency,
  and userland heap use;
- perform documentation claim audit;
- remove obsolete compatibility paths and duplicate docs.

### Exit criteria

- `bash tools/verify.sh` passes on the beta commit;
- final manual workflow is recorded;
- no QEMU-desktop P0/P1 risk remains open unless explicitly accepted in release
  notes;
- ABI and KLI1 documents are frozen and versioned;
- release artifacts and reproducible commands are documented.

## v1.0 — usable QEMU desktop

**State: NOT READY**

v1.0 is ready only when:

- ArmoniOS boots reliably to the QEMU desktop;
- deferred runtime work has measured and enforced bounds;
- `/fat` supports long names and directories;
- `/ext` mounts stable ext2, at least read-only;
- Files, Editor, Shell, Control/Settings, Monitor, Panel, and Clock are useful;
- process/window/resource cleanup remains reliable;
- settings and files persist across reboot;
- syscall and KLI1 v1 contracts are documented and frozen;
- the final manual workflow succeeds:
  1. create a folder;
  2. create a text file;
  3. edit multiple lines;
  4. save, close, and reopen;
  5. copy, move, rename, and delete;
  6. inspect and terminate a process;
  7. change persistent settings;
  8. reboot QEMU;
  9. verify files and settings persisted.

## Hardware track — Raspberry Pi

Raspberry Pi work is independent from the v1 QEMU release line until physical
evidence exists.

Required order:

1. controlled CPU entry and secondary-core parking;
2. repeatable serial marker across cold boots;
3. memory-map and timer validation;
4. read-only controller/card telemetry;
5. sector-zero and FAT geometry evidence on disposable media;
6. mailbox/framebuffer bring-up;
7. input bring-up;
8. writable media only after a separate recovery/safety milestone;
9. desktop workflow only after all earlier milestones pass.

Do not expose normal board capabilities or claim Raspberry Pi support before the
evidence rules in `DOCUMENTATION_POLICY.md` and `PORTING.md` are satisfied.

## Explicit non-goals before v1

- POSIX compatibility;
- libc compatibility;
- dynamic linking;
- package management;
- a browser;
- TCP application sockets unless separately re-scoped;
- audio;
- SMP;
- accelerated graphics;
- Raspberry Pi desktop support;
- ext2 writes;
- exFAT.
