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
- ABI policy: existing syscall numbers and KLI1 layout remain stable unless an
  incompatibility is explicitly documented;
- writable v1 filesystem: FAT with long names and directories;
- secondary v1 filesystem: ext2, at least read-only;
- Raspberry Pi: separate hardware track, not a v1 QEMU release dependency.

## Current phase summary

| Phase | State | Promotion blocker |
|---|---|---|
| v0.1 baseline | COMPLETE | None for the recorded QEMU baseline |
| v0.2 cleanup/hardening | IN PROGRESS / RELEASE CANDIDATE | Runtime budgets, exhausted-work preservation, sustained-load evidence, formal promotion record |
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

- lower subsystems receive kernel-owned syscall buffers instead of caller-owned
  EL0 pointers;
- output pages are permission checked before copying;
- VFS dispatches through generic mount callbacks instead of FAT32 policy;
- dynamic FAT nodes are invalidated after rename/delete;
- Raspberry Pi normal storage remains fail closed;
- process-owned descriptors and parent/wait lifecycle are verified;
- timer hard-IRQ work is limited to accounting, rearm, publication, and scheduler
  counters;
- periodic GUI, input, device, and network work is centralized after EOI.

### Landed runtime measurement

The post-EOI service now measures:

- request, coalescing, requeue, non-empty, and empty pass counts;
- last, maximum, and cumulative generic-counter duration;
- one-timer-interval overruns;
- input events produced and consumed;
- input queue depth, lifetime high-water, and overflow;
- USB HID polling operations;
- valid virtio-net RX frames consumed;
- successful redraw submissions;
- partial-damage rectangle batches and full-redraw fallbacks.

Latest validated Phase 1B head:
`6634c3a6f527433643a56f2c90cc6af8bad62c1d`.

- `Verify ArmoniOS` run `29840410727`: success;
- `CI - Tests` run `29840411044`: success;
- loadable kernel: 107370 / 108000 bytes.

The current virtio-net path exposes no trustworthy device-level RX-drop counter.
This is documented as unavailable rather than replaced with an inferred value.

### Remaining runtime work

1. Split the single periodic readiness bit into independently pending input/GUI,
   device, and network work.
2. Bound virtio-net receive draining first; it is the clearest unbounded loop and
   the queue has 16 descriptors.
3. Bound input consumption, USB HID polling, and redraw/damage work.
4. Add a global generic-counter deadline.
5. Preserve or republish the relevant pending bit whenever a class or deadline
   expires.
6. Count every class-budget and global-deadline exhaustion.
7. Add sustained-load QEMU tests proving EL0 heartbeat progress and explicit loss
   accounting.
8. Decide whether the bounded bottom half remains permanent or later becomes a
   wakeable EL1 service.
9. Create a formal v0.2 promotion/tag record with exact evidence.

### Exit criteria

- `bash tools/verify.sh` passes on the promotion commit;
- no hard timer callback contains rendering, queue drains, network polling, or
  device polling;
- runtime duration and every current budget-relevant work class are observable;
- work per pass is bounded;
- leftover class work remains pending;
- every exhaustion is counted;
- sustained input/network/redraw load cannot stop an EL0 heartbeat;
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

Scope:

- FAT12/16/32 probe and mount where practical;
- VFAT long file names;
- subdirectories;
- create, read, write, list, stat, truncate, mkdir, rename, move, and delete;
- file growth and shrink;
- simple MBR discovery for QEMU images;
- clear rejection of unsupported or corrupt geometry;
- `/fat` remains the primary writable volume;
- exFAT remains outside v1 unless it can be added safely.

Exit criteria:

- Files and Shell no longer depend on 8.3 names;
- a multi-line file can be saved, reopened, moved, and deleted in a nested
  directory;
- host image and QEMU reboot-persistence gates pass;
- the root-only bridge is removed or isolated as compatibility code.

## v0.5 — userland runtime and widgets

**State: PLANNED**

Goal: make application development practical without violating KLI1.

Runtime work:

- small heap backed by `SYS_MMAP`;
- dynamic buffers and bounded vectors;
- path construction and normalization helpers;
- argv parsing, formatting, error, and ownership helpers;
- no mutable static `.data` or `.bss` in shipping images.

Widget work:

- labels and status bars;
- buttons and text fields;
- list views with selection and scrolling;
- viewport/scrollbar helpers;
- modal confirmation dialogs;
- shared focus and keyboard navigation.

Exit criteria:

- Files, Editor, Control, and Monitor share runtime and widget code;
- image size changes are budgeted deliberately;
- stack and KLI1 gates remain green;
- layout/state logic is host-testable where practical.

## v0.6 — useful desktop applications

**State: PARTIAL DEMOS ONLY**  
**Tracking:** issue #2

This milestone is intentionally v0.6, not v1.1. It depends on v0.3-v0.5.

### Files

- navigate directories and mount roots;
- show type and size;
- create files and folders;
- copy, move, rename, and delete;
- open files with Editor;
- scroll beyond eight entries;
- report storage errors clearly.

### Editor

- multi-line viewport;
- vertical and horizontal scrolling;
- larger or chunked loading;
- dirty state and Save As;
- safe truncate-on-save;
- line/column status and predictable caret navigation.

### Shell

Preserve current commands and add `cp`, `mv`, `rm`, `mkdir`, `touch`, `echo`,
`edit`, `open`, `df`, and `clear`, with path-aware errors and clearer process
output.

### Control and Monitor

- persist at least three observable settings across reboot;
- show PID, state, memory, and timer information;
- refresh and kill a selected process with confirmation.

Exit criteria:

- create a folder and multi-line file;
- save, close, reopen, copy, move, rename, and delete;
- inspect and terminate a process;
- change three settings;
- reboot and confirm files and settings persist.

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
- storage and runtime status visible through a deliberate diagnostic ABI.

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

- reliable graphical boot;
- directory-aware writable FAT;
- read-only ext2;
- useful Files, Editor, Shell, Control, Monitor, Panel, and Clock;
- persistent files and settings across reboot;
- bounded deferred runtime work with sustained-load EL0 proof;
- deterministic automated gates;
- dated visible end-to-end evidence;
- exact release commit, workflow runs, limitations, and recovery instructions.

Raspberry Pi support remains a separate hardware track until physical evidence
exists.
