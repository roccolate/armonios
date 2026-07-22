# Architecture

ArmoniOS is a compact monolithic AArch64 operating system. Its verified runtime
platform is QEMU `virt`; Raspberry Pi 4 remains a separate build/host-verified,
fail-closed hardware track.

This document describes implemented architecture, not roadmap intent.
Operational evidence lives in `CURRENT_STATE.md`, active risks in
`TECHNICAL_RISKS.md`, and milestone ordering in `ROADMAP.md`.

## Design profile

- freestanding AArch64 kernel written primarily in C with narrow assembly entry
  boundaries;
- monolithic EL1 kernel and linked-in drivers;
- freestanding EL0 applications in the KLI1 image format;
- no libc, POSIX layer, dynamic linker, package manager, or hosted runtime;
- single-core execution model;
- QEMU-first verification policy;
- fixed-capacity tables and queues chosen for readability and bounded memory.

## Privilege and execution modes

### EL1

EL1 contains:

- kernel initialization and board orchestration;
- exception and IRQ entry/dispatch;
- PMM, VMM, heap, and process management;
- syscall dispatch and user-copy helpers;
- VFS and filesystem adapters;
- GUI compositor and window manager;
- input, storage, USB, and network drivers;
- the post-EOI deferred runtime bottom half;
- cooperative helper threads.

### EL0

EL0 runs KLI1 applications such as Panel, Shell, Editor, Files, Monitor, Control,
and Clock. Applications invoke the kernel with `svc #0` through `libkarm` and
`libkarmdesk` wrappers.

### EL2 and EL3

The verified QEMU path does not use EL2 or EL3 after entry. Physical boards may
enter at a higher exception level and require deliberate transition and core
parking before the normal EL1 path is valid.

## Boot flow

```text
QEMU loader
  -> load kernel at board linker address
  -> pass DTB pointer in x0

boot/start.S
  -> preserve DTB pointer
  -> install early stack
  -> install exception vectors
  -> clear BSS
  -> call kernel_main(dtb)

kernel_main
  -> board early initialization and UART
  -> DTB memory discovery
  -> PMM and process table
  -> console and heap
  -> VMM/MMU identity mappings
  -> VFS, bootfs, tmpfs
  -> timer, IRQ, process dispatch, runtime pending state
  -> optional block storage and FAT32
  -> optional virtio GPU
  -> optional network
  -> input, PCI, xHCI, USB HID
  -> launch Panel in EL0
  -> enter normal process/IRQ runtime
```

Initialization phase status is recorded through `kernel/init_status.{c,h}` and
reported on the kernel console.

## Memory architecture

### Physical memory manager

The PMM uses one fixed bitmap and manages at most 128 MiB. It reserves the loaded
kernel and DTB before allocating pages for:

- page tables;
- application images;
- user stacks;
- anonymous mappings;
- GUI backing buffers;
- kernel heap arenas.

The 128 MiB cap matches the current default QEMU configuration and is not a
general physical-board memory policy.

### Virtual memory

The VMM implements AArch64 stage-1 translation with 4 KiB pages and four-level
page tables.

Current mapping policy:

- kernel text: read/execute;
- kernel rodata: read-only, non-executable;
- kernel data, BSS, and stack: read/write, non-executable;
- remaining RAM: read/write, non-executable;
- MMIO: device memory, non-executable;
- process image: user read/execute;
- process stack and ordinary anonymous mappings: user read/write unless another
  supported protection is requested.

Mutable kernel globals are zero-initialized and subsystems establish non-zero
defaults during initialization. This keeps the loadable `.data` section empty
while preserving W^X boundaries.

Each process owns a TTBR0 root, but that root currently also contains the kernel
and RAM identity map required while handling exceptions. TTBR1 is disabled.
Process switches replace TTBR0 and invalidate the complete EL1 TLB.

This is adequate for the current v0.1 isolation baseline, but it is not a
hardened split-address-space design. Future hardening requires TTBR1, user-only
TTBR0 roots, ASIDs, and scoped TLB invalidation.

### Process user regions

Each process records at most eight disjoint user regions. Region records contain
virtual range, physical base, and ownership flags. They support:

- image and stack ownership checks;
- anonymous mapping allocation and release;
- syscall pointer range validation;
- cleanup during process exit.

The syscall boundary checks both the registered range and the process page table.
Readable input and writable output permissions are distinguished. The remaining
limitation is that the final byte copy is an ordinary EL1 load/store sequence,
not a fault-recoverable copy primitive.

## Processes and scheduling

### Process model

The process table contains 16 fixed slots. A process records:

- PID and parent PID;
- name and state;
- saved x0-x30 registers;
- user SP, PC, and PSTATE;
- TTBR0 root;
- user regions and owned pages;
- exit code and zombie state;
- next anonymous mapping address.

Spawn records the current process as parent. A child zombie remains observable
until its parent successfully calls non-blocking `sys_wait`. Automatic zombie
reclamation is restricted to kernel-owned or orphaned processes.

### EL0 dispatch

EL0 processes are preemptive:

1. IRQ entry masks normal IRQs and saves a 288-byte exception frame on the EL1
   stack.
2. `irq_handler_frame()` saves the interrupted process state.
3. The acknowledged IRQ handler runs.
4. The board interrupt controller receives EOI.
5. One deferred runtime pass runs if work is pending.
6. `process_dispatch_next()` may select another ready process.
7. The selected TTBR0 and trap frame are activated.
8. `eret` restores EL0 execution and the saved interrupt-mask state.

Voluntary yield, exit, kill, and lower-EL fault paths use the same process context
activation machinery where applicable.

### EL1 helper threads

The scheduler under `kernel/sched/` is cooperative. EL1 helper threads change
only through explicit yield or exit boundaries. Timer ticks update counters but
do not preempt an executing helper.

The current helper scheduler cannot wake and interleave a kernel service while a
long-lived EL0 process remains active. That is why periodic desktop work is not
yet implemented as a helper thread.

### Deferred runtime bottom half

The timer callback publishes `RUNTIME_WORK_PERIODIC` into one coalescing pending
bitmask. After EOI, the IRQ dispatcher snapshots and clears pending bits, then
calls the backend.

Current backend work:

- poll UART, board, and direct USB HID input producers;
- drain the shared input queue into GUI routing;
- flush dirty GUI redraw;
- poll the small network stack.

Clearing the snapshot before backend execution preserves a request published
during the current pass.

Important semantics:

- EOI has occurred, but CPU exception return has not;
- IRQs remain masked by the vector entry;
- EL0 remains paused;
- the exception frame remains on the EL1 stack;
- the bottom half is not preemptible by normal IRQs;
- no per-pass work or duration budget currently exists.

The pending word is safe only for the current single-core, masked-IRQ,
one-consumer model. `volatile` does not provide SMP-safe atomicity.

See `RUNTIME_SERVICE.md` and `RISK-017`.

The accurate runtime description is:

> preemptive EL0 processes, a non-preemptible post-EOI runtime bottom half, and
> cooperative EL1 helper threads.

## Exceptions and faults

- `svc #0` from EL0 enters syscall dispatch.
- Other synchronous lower-EL exceptions mark the current process exited and try
  to dispatch another ready process.
- Unexpected EL1 exceptions enter a fatal diagnostic path and wait forever.
- Normal IRQ entry prevents nesting until exception return.

The kernel does not yet have exception-table fixups for user-copy faults.

## Syscall boundary

The ABI uses:

```text
x8      syscall number
x0..x6  arguments
x0      return value or negative kernel error
```

Numbers are frozen in `kernel/syscall_numbers.h`; new calls must append numbers.

Public pointer handling is centralized in `kernel/syscall_helpers.{c,h}`.
Covered syscall paths:

- validate process-owned ranges;
- walk user page tables;
- distinguish readable and writable mappings;
- copy c-strings and bounded byte ranges;
- import argv into pointer-free kernel storage;
- build output in kernel-owned buffers;
- validate complete output destinations before consuming IPC or GUI events.

Fault-contained copying remains future hardening.

See `SYSCALLS.md` for the exact ABI.

## VFS and file descriptors

The VFS is a fixed-capacity kernel facade:

- 24 nodes;
- four mounts;
- 64-byte absolute paths;
- eight descriptors per process;
- a global internal handle pool sized for all process slots.

Descriptors `3..10` are local to the current process. Internal handles store the
owner PID, local descriptor, node, offset, and flags. Foreign use is rejected,
dead owners can be reaped, and `process_mark_exited()` closes all descriptors for
the exiting PID.

Mount callbacks currently support open, list, unlink, and rename. Generic VFS
code selects the mount and does not include FAT32 policy directly.

The VFS does not yet provide:

- common path normalization/traversal;
- structured directory entries;
- rich metadata;
- mkdir or truncate;
- general filesystem driver lifecycle;
- POSIX semantics.

## Filesystems and storage

### bootfs

Shipping KLI1 application images are embedded in the kernel and exposed at
`/armonios/<name>`.

### tmpfs

A small fixed in-memory filesystem supports simple VFS and test workflows.

### FAT32 bridge

The current writable FAT path supports:

- 512-byte sectors;
- one FAT32 volume;
- root-directory short 8.3 names;
- list/open/create/read/write/rename/delete;
- cluster-chain growth;
- dynamic `/fat/<name>` nodes;
- dynamic-node invalidation after rename/delete.

It does not support long names, subdirectories, FAT12/16, GPT, extended
partitions, journaling, crash recovery, or broad interoperability testing.

A reusable primary-MBR FAT32 parser and bounded block view exist. The RPi4
read-only diagnostic path uses those components without exposing normal writable
hardware storage.

The QEMU path is verified through storage smoke and visible-target wiring tests.
The latest visible create/edit/save/rename/reopen/delete evidence is dated
2026-07-17.

### ext2

No ext2 implementation currently exists.

## GUI architecture

The GUI is a kernel compositor/window manager rather than a userland display
server.

Responsibilities are separated into:

- `gui_events`: 32-entry per-window queues;
- `gui_cursor`: cursor shape, buttons, and drag state;
- `gui_input`: hit testing and event routing;
- `gui_backing`: lazily allocated content buffers;
- `gui_pool`: 16-window lifecycle, ownership, lookup, z-order, and focus;
- `gui_compositor`: damage tracking and rendering.

Windows carry owner PID. Most mutating syscalls require ownership. Panel-facing
focus, restore, lookup, and state calls are intentionally cross-process.

Applications draw into backing buffers and flush damage rectangles. When damage
tracking fills, the compositor may collapse work to a full redraw.

Timer-originated input routing and redraw now run through the deferred runtime
bottom half. Redraw cost is not yet budgeted.

There is no shared userland widget toolkit. Each application currently implements
its own layout and interaction state.

## Input architecture

One shared 64-event producer queue receives events from:

- UART/ANSI keyboard translation;
- virtio-input;
- directly attached USB boot-protocol keyboard and mouse devices.

The runtime service currently drains all available producer events and routes
them to the GUI. Queue overflow/fairness and per-pass limits require stronger
instrumentation.

USB support is a basic QEMU xHCI path. Hubs and general USB class support are not
claimed.

## Networking

The direct virtio-net stack contains enough:

- Ethernet;
- ARP;
- IPv4;
- UDP;
- DHCP

to obtain a QEMU user-network lease.

There is no application socket ABI, TCP, general UDP API, DNS query interface, or
HTTP client. Network polling currently contributes to the unbounded runtime-pass
risk.

## Board boundary

Generic kernel code uses `drivers/board.h`. Board-specific addresses, interrupt
controller details, capability reporting, and device initialization live under
`drivers/boards/<board>/`.

QEMU is the reference backend and implements the verified display, input,
storage, and network paths.

The RPi4 backend:

- satisfies the build contract;
- exposes safe failure for unsupported display/input;
- contains SDHCI, mailbox clock, telemetry, MBR, block-view, and read-only probe
  scaffolding;
- advertises no normal capabilities requiring unproven hardware behavior.

No physical Raspberry Pi runtime claim is valid until `RISK-007` exit criteria
are met.

## Userland and KLI1

Applications link against:

- `programs/libkarm`: startup, syscall trampolines, I/O, strings, and small
  helpers;
- `programs/libkarmdesk`: typed GUI wrappers.

Shipping applications:

- `panel`;
- `shell`;
- `editor`;
- `files`;
- `monitor`;
- `control`;
- `clock`.

KLI1 is a flat image with fixed header and entry offsets. Shipping images may
contain header, text, and rodata, but mutable `.data` and `.bss` are forbidden by
linker assertions and regression tests. Large mutable application state uses
`SYS_MMAP`.

The applications demonstrate process, GUI, storage, and syscall behavior but are
not yet complete daily tools. See `CURRENT_STATE.md` for exact limits.

## Testing architecture

Evidence is divided into distinct classes:

- static inspection;
- native host tests;
- build/link verification;
- deterministic QEMU serial-marker tests;
- visible manual QEMU use;
- physical hardware evidence.

Host tests validate pure-C contracts and mocked driver paths, but cannot prove
real MMIO or complete exception-return timing.

Deterministic QEMU tests cover:

- FAT storage smoke;
- invalid user-copy rejection;
- focus transitions;
- framebuffer/window readiness;
- USB initialization and two HID devices;
- DHCP lease;
- FAT + display + panel wiring.

The runtime-service host gate covers coalescing, requeue preservation, mocked EOI
ordering, and a static timer-source boundary. It does not prove latency or
fairness under load.

`bash tools/verify.sh` is the full promotion gate.

## Near-term design direction

1. Instrument and bound the deferred runtime service.
2. Record formal v0.2 promotion evidence.
3. Build the v0.3 block-device, path, mount, and structured filesystem platform.
4. Replace the root-only bridge with real FAT long-name/directory support.
5. Add a shared userland runtime and widgets.
6. Make the seven applications support the v1 workflow.
7. Add ext2 read-only support.
8. Harden address spaces, user copy, and hardware ports without overstating
   evidence.
