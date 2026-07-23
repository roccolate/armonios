# Architecture

> **Implementation update — 2026-07-23:** The older audit sections in this document predate merged v0.3 PRs #80, #81, #82, #90, and #93. Use `V03_IMPLEMENTATION_STATUS.md` for the current storage/VFS checkpoint. Issue #63 is closed; issue #76 remains the manual v0.2 validation and release-record task.

ArmoniOS is a compact monolithic AArch64 operating system. Its verified runtime
platform is QEMU `virt`. Raspberry Pi 4 remains a separate build/host-verified,
fail-closed hardware track.

This document describes implemented design and current contracts. It does not
serve as release evidence.

- Operational evidence: `CURRENT_STATE.md`
- Open risks: `TECHNICAL_RISKS.md`
- Milestone sequencing: `ROADMAP.md`
- Practical development workflow: `DEVELOPMENT_GUIDE.md`
- Exact runtime-service contract: `RUNTIME_SERVICE.md`

## Design profile

- freestanding AArch64 kernel, primarily C11 with narrow GNU assembly boundaries;
- monolithic EL1 kernel with linked-in drivers;
- freestanding EL0 KLI1 applications;
- no libc, POSIX layer, dynamic linker, package manager, or hosted runtime;
- single-core execution model;
- fixed-capacity tables and queues;
- QEMU-first verification policy;
- explicit ownership and fail-closed optional capabilities;
- small production image constrained by a fixed 108000-byte ceiling.

## System overview

```text
QEMU / board firmware
  -> boot/start.S
  -> kernel_main(dtb)
       -> board + UART + DTB memory
       -> PMM + heap + page tables + MMU
       -> process table + VFS + filesystems
       -> timer + IRQ + runtime service
       -> optional storage, GPU, network, input, PCI, xHCI
       -> load Panel KLI1 image
       -> enter preemptive EL0 runtime

EL0 application
  -> svc #0
  -> syscall dispatcher
  -> kernel-owned validation/copy boundary
  -> VFS / GUI / IPC / process service

Timer IRQ
  -> fixed timer callback
  -> EOI
  -> bounded post-EOI runtime pass
  -> process dispatch
  -> eret
```

## Privilege and execution modes

### EL1 kernel

EL1 owns:

- initialization and board orchestration;
- exception and IRQ entry/dispatch;
- physical and virtual memory management;
- process lifecycle and context activation;
- syscall dispatch and user-copy validation;
- VFS and filesystem adapters;
- kernel compositor/window manager;
- storage, input, USB, and network drivers;
- the measured post-EOI runtime service;
- cooperative helper threads.

### EL0 applications

Panel, Shell, Editor, Files, Monitor, Control, and Clock run as freestanding KLI1
images. They call the kernel through `svc #0` wrappers in `programs/libkarm/` and
GUI wrappers in `programs/libkarmdesk/`.

### EL2 and EL3

The verified QEMU path transitions to and remains in EL1 after entry. Physical
boards may enter at a higher exception level and require deliberate transition,
core parking, and board-specific setup before the generic EL1 path is valid.

## Boot and initialization

```text
loader
  -> kernel at board load address
  -> DTB pointer in x0

boot/start.S
  -> preserve DTB
  -> establish early stack
  -> establish exception vectors
  -> clear BSS
  -> call kernel_main(dtb)

kernel_main
  -> board early init and UART
  -> DTB memory discovery
  -> reserve kernel and DTB
  -> PMM, process table, console, heap
  -> kernel page tables and MMU
  -> VFS, bootfs, tmpfs
  -> timer, IRQ, runtime timing, scheduler state
  -> optional block device and FAT32
  -> optional virtio GPU
  -> optional virtio network
  -> input, PCI, xHCI, USB HID
  -> spawn Panel and activate EL0
```

Initialization status is recorded through `kernel/init_status.{c,h}`. Optional
capabilities must report explicit failure rather than silently succeeding.

## Memory architecture

### Physical memory manager

`kernel/mm/pmm.c` uses a fixed bitmap and manages at most 128 MiB. The current
limit matches the default QEMU configuration and is not a general hardware memory
policy.

The PMM supplies physical pages for:

- page-table hierarchies;
- application images and user stacks;
- anonymous user mappings;
- GUI backing buffers;
- kernel heap arenas;
- other explicitly owned kernel allocations.

The PMM records used/free state but is not a reference-counting allocator. Higher
layers must provide correct ownership and exactly-once release.

### Virtual memory manager

`kernel/mm/vmm.c` implements AArch64 stage-1 translation with 4 KiB pages and a
four-level table hierarchy.

Current mapping policy:

| Region | Permissions |
|---|---|
| Kernel text | EL1 read/execute |
| Kernel rodata | EL1 read-only, non-executable |
| Kernel data/BSS/stack | EL1 read/write, non-executable |
| Remaining RAM identity map | EL1 read/write, non-executable |
| MMIO | Device memory, non-executable |
| User image | EL0 read/execute |
| User stack and ordinary anonymous mappings | EL0 read/write unless another supported protection is requested |

Mutable kernel globals are zero-initialized and non-zero defaults are established
at runtime. This preserves an empty loadable `.data` section.

Each process owns one TTBR0 root. That root currently includes the kernel/RAM
identity mappings required while handling exceptions. TTBR1 is disabled. Context
switches replace TTBR0 and use broad EL1 TLB invalidation.

This is sufficient for the current baseline but not a hardened split address
space. Future work under `RISK-008` includes TTBR1, user-only TTBR0 roots, ASIDs,
scoped invalidation, and stale-translation tests.

### Page ownership

The VMM owns page-table pages. The caller owns physical leaf pages mapped by those
tables.

Examples:

- process image/stack pages are registered as process-owned regions;
- anonymous mapping pages are returned by user-VM cleanup;
- `vmm_free_table()` releases only table pages;
- process teardown releases owned mapped pages and then the table hierarchy.

This distinction is a core correctness invariant. The intermittent VMM fault in
`RISK-018` is investigated without changing this ownership contract until a root
cause is demonstrated.

### Process user regions

Each process records at most eight disjoint user regions containing:

- virtual start and end;
- physical base where applicable;
- ownership flags.

They support:

- image and stack ownership;
- anonymous mapping allocation/release;
- user-pointer range validation;
- mapping-permission checks;
- cleanup on exit.

The syscall boundary validates both registered ranges and page-table permissions.
The final byte copy still uses ordinary EL1 loads/stores and is not protected by
exception-table fixups (`RISK-015`).

## Process and scheduling architecture

### Process model

The fixed process table contains 16 slots. A process records:

- PID and parent PID;
- name and state;
- saved x0-x30 register state;
- user SP, PC, and PSTATE;
- TTBR0 root;
- user regions and physical ownership;
- exit code and zombie state;
- next anonymous mapping address.

Spawn records the current process as parent. A child zombie remains visible until
its parent completes non-blocking wait. Automatic reclamation is limited to
kernel-owned or orphaned zombies.

Process cleanup closes owned VFS descriptors, destroys owned GUI windows, releases
owned user pages, frees page-table pages, and returns the fixed slot.

### EL0 dispatch

EL0 processes are preemptive:

1. IRQ entry saves a 288-byte exception frame on the EL1 stack.
2. The interrupted process context is saved.
3. The acknowledged IRQ handler runs.
4. The board interrupt controller receives EOI.
5. A pending post-EOI runtime pass executes.
6. `process_dispatch_next()` selects a ready process when required.
7. The selected TTBR0 and frame are activated.
8. `eret` restores EL0 and the saved interrupt-mask state.

Yield, exit, kill, and lower-EL fault paths use the same context activation model
where applicable.

### Cooperative EL1 helpers

`kernel/sched/` provides cooperative EL1 helper threads. They switch only through
explicit yield or exit. Timer ticks update their counters but do not preempt an
executing helper.

The current helper scheduler does not independently wake and interleave a kernel
service while an EL0 process runs. Periodic GUI/input/network work therefore uses
the post-EOI runtime service rather than an EL1 helper thread.

## Deferred runtime service

The runtime service is a non-preemptible post-EOI EL1 bottom half, not a thread.
The timer publishes readiness; the IRQ dispatcher consumes it after EOI and before
process selection and `eret`.

### Pending work

One coalescing pending word contains:

```text
RUNTIME_WORK_PERIODIC
RUNTIME_WORK_INPUT
RUNTIME_WORK_NETWORK
```

The timer publishes all three readiness classes. The service snapshots and clears
the word before running, so requests published during the pass survive for the
next pass.

Readiness bits are not exact event counters. Exact continuation remains in native
structures such as device rings, the shared input queue, and the compositor damage
list.

### Deadline

At 100 Hz, QEMU configures one nominal timer interval as the service budget:

```text
deadline = start_CNTPCT_EL0 + CNTFRQ_EL0 / timer_hz
```

At safe checkpoints, expiry:

1. marks the active phase as deadline-exhausted;
2. increments `over_budget_count` once;
3. republishes the original work snapshot;
4. skips later optional classes;
5. returns toward process dispatch.

The deadline is cooperative. It cannot interrupt one already-started operation.
A full redraw or driver call may cross the nominal interval before the next
checkpoint.

### Count budgets

| Work class | Bound | Continuation |
|---|---:|---|
| Virtio-input producer | `min(negotiated ring, 16)` descriptors/call | Later used descriptors stay in the ring. |
| USB HID producer | Four device visits/call | All supported direct slots fit in one scan. |
| Shared input consumer | 16 events/active pass | INPUT requeued when events remain. |
| Partial compositor damage | Eight rectangles/successful submission | Ordered remainder stays dirty; failed submission consumes none. |
| Virtio-net RX | 16 valid frames/NETWORK pass | NETWORK conservatively requeued at cap. |

### Strict network routing

Existing network calls are routed through runtime wrappers. Both network polling
and descriptor receive consume nothing unless the active phase is NETWORK. The
legacy cooperative console poll therefore cannot bypass the count and time
contracts.

### Telemetry

The kernel-internal snapshot records:

- requests, coalescing, empty/non-empty passes, and requeues;
- last, maximum, and cumulative pass duration;
- deadline exhaustion;
- produced and consumed input events;
- input queue depth, high-water, and overflow;
- USB HID poll operations;
- valid network frames consumed;
- redraws, damage items, full redraws, and redraw exhaustion;
- input and network count-budget exhaustion;
- configured counter frequency, budget, pending work, and last work.

Reports are accepted only while the service is active. The layout is not a public
ABI; Monitor integration requires a versioned diagnostic interface.

The exact contract and stress evidence are documented in `RUNTIME_SERVICE.md`.

## Exceptions and faults

- `svc #0` from EL0 enters syscall dispatch.
- Other synchronous lower-EL exceptions mark the current process exited and try
  to dispatch another ready process.
- Unexpected EL1 exceptions enter a fatal serial diagnostic path and halt.
- Normal IRQ entry keeps normal IRQs masked until exception return.

Fatal EL1 diagnostics report ESR, ELR, FAR, SP, exception kind, and a panic marker.
Issue #63 / `RISK-018` tracks one intermittent data abort resolved to
`next_table()` during a FAT32 smoke run.

The kernel does not yet provide exception-table fixups for user-copy faults.

## Syscall and user-copy boundary

Register ABI:

```text
x8      syscall number
x0..x6  arguments
x0      result or negative kernel error
```

Syscall numbers are append-only in `kernel/syscall_numbers.h`.

`kernel/syscall_helpers.{c,h}` centralizes public pointer handling:

- process-owned range validation;
- user page-table walks;
- readable/writable distinction;
- bounded strings and byte copies;
- pointer-free argv import;
- kernel-owned output construction;
- full destination validation before consuming IPC or GUI events.

Lower subsystems should receive kernel-owned data, not user pointers.

See `SYSCALLS.md` and `GUI_ABI_NOTES.md` for exact public contracts.

## VFS and file descriptors

The VFS is a fixed-capacity kernel facade:

- 24 nodes;
- four mounts;
- 64-byte absolute paths;
- eight descriptors per process;
- a global internal handle pool covering all process slots.

Descriptors `3..10` are local to the owning process. Internal handles store owner
PID, local descriptor, node, offset, and flags. Foreign descriptor use is rejected
and process exit closes all owned handles.

Mount callbacks currently provide the narrow operations required by bootfs,
tmpfs, and the FAT32 bridge. Generic VFS code selects the mount; it must not embed
FAT-specific policy.

Landed v0.3 foundations:

- canonical absolute-path normalization and longest-prefix mount resolution;
- block-device capacity/read-only/flush contracts and bounded views;
- whole-device and primary-MBR FAT32 mounting;
- read-only traversal of existing nested FAT32 8.3 directory trees.

Remaining foundations include native structured metadata promotion, filesystem
information, generic mkdir/rmdir and truncate, nested mutation rollback, and an
explicit durable-flush contract.

## Storage and filesystems

### bootfs

Shipping KLI1 images are embedded in the kernel and exposed under
`/armonios/<name>`.

### tmpfs

A small fixed in-memory filesystem supports tests and temporary workflows.

### FAT32 compatibility bridge

The current writable FAT path supports:

- 512-byte sectors;
- primary-MBR FAT32 discovery for QEMU images;
- one mounted FAT32 volume;
- short 8.3 names;
- traversal, listing, stat, open, and read for existing nested directories;
- root-level create, write, rename, and delete;
- cluster-chain growth;
- dynamic VFS nodes and invalidation after rename/delete.

It does not support long names, nested mutation transactions, mkdir/rmdir,
FAT12/16, GPT, extended partitions, journaling, crash recovery, or broad
interoperability.

Reusable MBR parsing and bounded block views are shared with the opt-in Raspberry
Pi read-only diagnostic path. Normal Raspberry Pi storage remains unavailable.

### ext2

No ext2 implementation exists.

## GUI architecture

The GUI is a kernel compositor/window manager rather than a userland display
server.

Main responsibilities:

| Component | Responsibility |
|---|---|
| `gui_pool` | Fixed 16-window lifecycle, ownership, lookup, z-order, focus |
| `gui_events` | 32-entry per-window event queues |
| `gui_cursor` | Cursor shape, buttons, drag state |
| `gui_input` | Hit testing and event routing |
| `gui_backing` | Lazily allocated content buffers |
| `gui_damage` | Ordered damage-list management |
| `gui_compositor` | Partial/full rendering and board submission |

Windows carry owner PIDs. Most mutations require ownership. Panel-facing focus,
restore, lookup, and state operations are intentionally cross-process.

Applications draw into backing buffers and submit damage. Partial redraw consumes
at most eight ordered rectangles after a successful board submission. Failed
submissions preserve all damage. Damage-list overflow may collapse to one full
redraw, which remains one non-preemptible operation.

No shared userland widget toolkit exists; applications currently implement their
own layout and interaction state.

## Input architecture

One shared 64-event queue receives events from:

- UART/ANSI keyboard translation;
- virtio-input;
- directly attached xHCI boot-protocol keyboard and mouse devices.

Producer work and shared consumption run through the runtime service. Producer,
consumer, device-visit, queue-pressure, and overflow metrics are explicit.
Overflow is counted but not prevented.

USB support is intentionally narrow: four direct HID slots, no hubs, and no
general USB class framework.

## Networking

The direct virtio-net stack implements enough:

- Ethernet;
- ARP;
- IPv4;
- UDP;
- DHCP

to acquire a QEMU user-network lease.

There is no application socket ABI, TCP, DNS interface, HTTP client, or general
user UDP API.

Valid RX consumption is bounded and strictly routed through NETWORK phase. The
current device interface does not expose trustworthy dropped/overwritten frame
counters. Software consumption metrics therefore do not prove loss-free delivery.

## Board abstraction

Generic kernel code uses `drivers/board.h`. Board-specific physical addresses,
interrupt-controller behavior, capabilities, and device initialization belong
under `drivers/boards/<board>/`.

### QEMU `virt`

The reference backend implements the verified display, input, storage, network,
timer, and interrupt paths.

### Raspberry Pi 4

The RPi4 backend:

- satisfies the cross-build contract;
- fails unsupported normal display/input/storage capabilities closed;
- contains EMMC2, mailbox-clock, telemetry, MBR, block-view, and read-only probe
  scaffolding;
- has no promoted physical runtime evidence.

No physical support claim is valid until `RISK-007` exit criteria are met.

## Userland and KLI1

Applications link against:

- `programs/libkarm`: startup, syscall trampolines, I/O, strings, and small helpers;
- `programs/libkarmdesk`: typed GUI wrappers.

Shipping applications:

- Panel;
- Shell;
- Editor;
- Files;
- Monitor;
- Control;
- Clock.

KLI1 is a flat image with a fixed header and entry offsets. Shipping images may
contain header, text, and rodata. Mutable static `.data` and `.bss` are forbidden
by linker assertions and regression tests. Large mutable state uses `SYS_MMAP`.

The applications demonstrate process, GUI, storage, and syscall behavior but are
not yet complete daily tools.

## Verification architecture

Evidence classes remain separate:

- static inspection;
- native host tests;
- build/link/size/stack checks;
- deterministic QEMU serial assertions;
- QEMU stress tests;
- visible manual QEMU workflows;
- physical hardware evidence.

Host tests prove pure-C or mocked contracts, not MMIO or complete exception timing.
A successful build proves compatibility, not boot. A serial marker proves the
asserted guest state, not visible usability.

`bash tools/verify.sh` is the full automated promotion gate. It covers the normal
build, size and `.data`, RPi4 build/diagnostic contracts, host suites, runtime
contracts, storage smoke, user-copy/focus, device markers, both runtime stress
modes, and visible-target FAT + GPU wiring.

`make qemu-fb-visible` is a separate manual evidence path.

## Architectural invariants

- Keep the hard timer callback fixed and bounded.
- Keep post-EOI work count- and time-bounded at documented checkpoints.
- Preserve continuation in native queues/rings/lists.
- Preserve strict NETWORK-phase routing.
- Preserve page, table, descriptor, window, mapping, and process ownership.
- Keep generic VFS free of FAT-specific policy.
- Keep board addresses and behavior behind the board boundary.
- Preserve `.data == 0`, W^X, the production size ceiling, KLI1, and stack gates.
- Append public ABI; do not silently rewrite it.
- Do not upgrade QEMU evidence into hardware evidence.

## Near-term design direction

1. Complete or disposition issue #63 / `RISK-018`.
2. Record the final v0.2 visible workflow and residual-risk decisions.
3. Promote and tag v0.2 with exact automated and manual evidence.
4. Build v0.3 as staged block-descriptor, path-normalizer, mount-resolver, and
   structured-metadata cuts.
5. Replace the root-only compatibility bridge with real FAT names/directories.
6. Add a shared userland runtime and widget layer.
7. Complete the seven applications around the v1 workflow.
8. Add ext2 read-only support.
9. Harden address spaces, user-copy faults, and physical-board ports without
   overstating evidence.
