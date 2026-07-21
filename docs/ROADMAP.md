# Roadmap

This roadmap starts from the verified v0.1 QEMU desktop baseline and targets a
usable v1.0 mini desktop OS. It is ordered by architectural dependency, user
value, verification cost, and maintenance risk.

Operational truth lives in `CURRENT_STATE.md`. Active defects and exit criteria
live in `TECHNICAL_RISKS.md`. This file describes future sequencing and must not
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
- ABI policy: existing syscall numbers and KLI1 layout remain stable unless an
  incompatibility is explicitly documented;
- writable v1 filesystem: FAT with long names and directories;
- secondary v1 filesystem: ext2, at least read-only;
- Raspberry Pi: separate hardware track, not a v1 QEMU release dependency.

## Current phase summary

| Phase | State | Promotion blocker |
|---|---|---|
| v0.1 baseline | COMPLETE | None for the recorded QEMU baseline |
| v0.2 cleanup/hardening | IN PROGRESS / RELEASE CANDIDATE | Input, USB, redraw, global-time bounds; sustained-load evidence; promotion record |
| v0.3 storage/VFS platform | NEXT | Depends on v0.2 promotion |
| v0.4 real FAT | PLANNED | Depends on v0.3 filesystem interfaces |
| v0.5 userland runtime/widgets | PLANNED | Depends on stable storage and ABI shapes |
| v0.6 useful applications | PARTIAL DEMOS ONLY | Depends on v0.3-v0.5 |
| v0.7 ext2 | PLANNED | Depends on v0.3 filesystem interface |
| v0.8 desktop polish | EARLY PARTIAL | Depends on useful applications |
| v0.9 beta | NOT STARTED | Depends on all QEMU P0/P1 risks being closed or accepted |
| v1.0 | NOT READY | Product workflow and final evidence incomplete |

## Sequencing rules

1. Do not add broad product features while a kernel-runtime P1 remains open
   without explicitly accepting the risk.
2. Build storage/VFS foundations before polishing Files and Editor around the
   root-only FAT bridge.
3. Add syscall numbers only with implementation, wrappers, tests, and docs in the
   same change.
4. Keep Raspberry Pi work isolated from the QEMU release line.
5. Every milestone needs automated gates and dated manual evidence where visible
   behavior is claimed.
6. Do not call userland polish “v1.1” before v1.0 exists. Application usefulness
   and polish belong to v0.6-v0.8.
7. Preserve the 108000-byte kernel limit unless a deliberate release decision,
   evidence, and replacement size budget are documented. Compact first.

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
- deterministic framebuffer, USB, network, usercopy, focus, and FAT gates;
- hosted CI evidence;
- dated visible create/edit/save/rename/reopen/delete evidence;
- RPi4 build contract that fails unsupported capabilities closed.

The v0.1 claim does not include production hardening, general FAT, ext2, useful
daily applications, or Raspberry Pi hardware support.

## v0.2 — cleanup and runtime hardening

**State: IN PROGRESS / RELEASE CANDIDATE**

### Landed cleanup

- lower subsystems receive kernel-owned syscall buffers;
- output pages are permission checked before copying;
- VFS dispatches through generic mount callbacks;
- dynamic FAT nodes are invalidated after rename/delete;
- Raspberry Pi normal storage remains fail closed;
- process-owned descriptors and parent/wait lifecycle are verified;
- timer hard-IRQ work is limited to accounting, rearm, publication, and scheduler
  counters;
- periodic GUI, input, USB, and network work is centralized after EOI.

### Landed runtime measurement

The post-EOI service measures:

- request, coalescing, requeue, non-empty, and empty pass counts;
- last, maximum, and cumulative generic-counter duration;
- one-timer-interval overruns;
- input events produced and consumed;
- input queue depth, lifetime high-water, and overflow;
- USB HID polling operations;
- valid virtio-net RX frames consumed;
- redraw submissions, partial-damage batches, and full-redraw fallbacks.

Phase 1B was completed through PRs #44-#48.

### Landed network bound

PR #50 introduced the first enforced class budget:

- `RUNTIME_WORK_NETWORK` is independent from periodic readiness;
- active post-EOI network receive is capped at 16 valid frames per pass;
- reaching the cap increments a network exhaustion counter;
- network readiness is conservatively republished;
- a seventeenth frame continues later;
- exactly sixteen frames may produce one empty follow-up pass;
- cooperative network polling outside the runtime service remains unbudgeted.

Evidence:

- validated head `e3765864e6719c0b6373a4c9b1b7db59dfaa0202`;
- `Verify ArmoniOS` run `29849603386`: success;
- `CI - Tests` run `29849603374`: success;
- loadable kernel: 107548 / 108000 bytes;
- merge `3797f7e7cf3dfb825d927e399aa4769b27020e29`.

The virtio-net path still exposes no trustworthy device-level RX-drop counter.
This remains an explicit evidence gap.

### Remaining runtime work

1. Split input/GUI readiness where independent continuation is needed.
2. Bound shared input queue consumption per pass.
3. Split and bound USB HID/device polling.
4. Bound redraw and damage work.
5. Enforce a global generic-counter deadline.
6. Preserve or republish every exhausted class and count each exhaustion.
7. Add sustained-load QEMU tests proving EL0 heartbeat progress and explicit loss
   accounting.
8. Decide whether the fully bounded bottom half remains permanent or later becomes
   a wakeable EL1 service.
9. Create a formal v0.2 promotion/tag record with exact evidence.

### Exit criteria

- `bash tools/verify.sh` passes on the promotion commit;
- no hard timer callback contains rendering, queue drains, network polling, or
  device polling;
- every budget-relevant class and total service duration are observable;
- input, USB, network, redraw, and global time are bounded;
- leftover class work remains pending;
- every exhaustion is counted;
- sustained combined load cannot stop an EL0 heartbeat;
- documentation states the exact post-EOI exception-context model;
- RPi4 remains fail closed and no hardware claim is introduced;
- a dated visible QEMU pass is recorded;
- a v0.2 tag or release record names the tested commit and workflow runs.

Fault-recoverable copyin/copyout (`RISK-015`) remains valuable P2 hardening and may
be scheduled after v0.2 if its status is preserved explicitly.

## v0.3 — storage and VFS platform

**State: NEXT**

Goal: replace fixed demo plumbing with a coherent filesystem platform before
adding general FAT or rewriting applications.

Required architecture:

- block-device descriptor with sector size, capacity, read-only state, identity,
  read, write, and flush operations;
- mount table supporting `/fat`, `/ext`, `/tmp`, and `/armonios`;
- common absolute-path resolver with normalization, repeated slash handling,
  `.`/`..` policy, mount boundaries, and bounded components;
- filesystem interface for probe, mount, open, close, read, write, readdir, stat,
  create, mkdir, truncate, unlink, and rename;
- structured kernel directory and metadata types.

Planned ABI extensions are assigned only with implementation and tests:

- `SYS_MKDIR`;
- `SYS_TRUNCATE`;
- `SYS_STATX`;
- `SYS_READDIRX`;
- `SYS_FSINFO`.

Exit criteria:

- host tests cover normalization, traversal rejection, multiple mounts, nested
  paths, mount boundaries, and read-only mounts;
- Shell lists `/`, `/fat`, `/tmp`, and `/armonios` through the common resolver;
- existing v0.1 syscalls and applications remain functional;
- new structs and syscall numbers are documented before promotion;
- no FAT-specific policy returns to generic VFS code.

## v0.4 — real FAT

**State: PLANNED**

Goal: replace the root-only FAT32 bridge with a tested filesystem driver.

- FAT12/16/32 probe and mount where practical;
- VFAT long file names and subdirectories;
- create, read, write, list, stat, truncate, mkdir, rename, move, and delete;
- file growth and shrink;
- MBR discovery for QEMU images;
- safe rejection of unsupported or corrupt geometry;
- host-image and QEMU reboot-persistence gates.

Exit criteria include a nested directory workflow with long names and removal or
isolation of the root-only compatibility bridge.

## v0.5 — userland runtime and widgets

**State: PLANNED**

- small heap backed by `SYS_MMAP`;
- dynamic buffers and bounded vectors;
- path, argv, formatting, error, and ownership helpers;
- reusable labels, buttons, text fields, list views, scrolling, and dialogs;
- no mutable static `.data` or `.bss` in shipping images;
- stack and KLI1 gates remain green.

## v0.6 — useful desktop applications

**State: PARTIAL DEMOS ONLY**  
**Tracking:** issue #2

This milestone is intentionally v0.6, not v1.1. It depends on v0.3-v0.5.

Required user workflow:

- navigate directories and mount roots;
- create folders and multi-line files;
- save, reopen, copy, move, rename, and delete;
- use path-aware shell commands;
- inspect and terminate a process;
- persist at least three observable settings;
- reboot and confirm files and settings survived.

## v0.7 — ext2 read-only

**State: PLANNED**

- validate superblock and features;
- read block groups, inodes, directories, regular files, and supported indirect
  blocks;
- document symlink policy;
- reject unsupported incompat features;
- mount `/ext` read-only;
- verify fixture and corrupt images in host tests and QEMU.

Ext2 writes are outside v1 unless read-only support is already stable and a
separate write-safety plan exists.

## v0.8 — desktop polish

**State: EARLY PARTIAL**

- reliable launcher/taskbar/focus/minimize/restore behavior;
- duplicate-launch policy;
- consistent titles, errors, status bars, and confirmations;
- keyboard navigation and cursor regions;
- resizing and damage repaint tests;
- reduced hardcoded geometry where it blocks use;
- storage and runtime status through a deliberate diagnostic ABI.

Exit criteria include a 30-minute manual QEMU session without crash, scheduler
stall, blank compositor, or data loss.

## v0.9 — beta stabilization

**State: NOT STARTED**

- freeze syscall and KLI1 ABI for v1;
- fuzz path, filesystem, image, and syscall inputs;
- run sustained process, window, storage, input, and network stress;
- automate reboot persistence;
- complete install/run/recovery documentation;
- close or explicitly accept all QEMU P0/P1 risks.

## v1.0 — usable QEMU mini desktop

**State: NOT READY**

Final acceptance requires:

- reliable graphical boot and desktop workflow;
- writable FAT with directories and long names;
- read-only ext2;
- useful Files, Editor, Shell, Settings, Monitor, Panel, and Clock;
- reboot persistence for files and settings;
- bounded runtime execution with sustained-load evidence;
- complete user, developer, ABI, and recovery documentation;
- dated automated and manual release evidence.

Raspberry Pi remains a separate hardware-support track until physical evidence is
repeatable.
