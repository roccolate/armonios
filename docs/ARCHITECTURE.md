# Architecture

This document describes the design implemented on the current `main` branch. It
is not a roadmap, release record, or historical audit.

Related documents:

- current capabilities and limits: `CURRENT_STATE.md`;
- future milestones: `ROADMAP.md`;
- active technical risks: `TECHNICAL_RISKS.md`;
- public syscall contracts: `SYSCALLS.md`;
- exact runtime-service contract: `RUNTIME_SERVICE.md`.

## Design profile

ArmoniOS is a compact monolithic AArch64 operating system with:

- a freestanding C11 EL1 kernel;
- narrow GNU AArch64 assembly boundaries;
- linked-in board and device drivers;
- freestanding KLI1 EL0 applications;
- a native append-only syscall ABI;
- fixed-capacity tables and queues;
- a single-core execution model;
- QEMU-first verification;
- explicit ownership and fail-closed optional capabilities;
- a hard 128 KiB loadable production-image budget.

There is no libc, POSIX layer, dynamic linker, package manager, Linux kernel, X11,
Wayland, or hosted runtime beneath the system.

## System overview

```text
QEMU or board firmware
  -> boot/start.S
  -> kernel_main(dtb)
       -> board, UART, and DTB memory discovery
       -> PMM, heap, page tables, and MMU
       -> process table and public syscall boundary
       -> VFS, bootfs, tmpfs, block device, and FAT32
       -> timer, IRQ routing, and runtime service
       -> optional GPU, network, input, PCI, xHCI, and USB HID
       -> load Panel KLI1 image
       -> activate preemptive EL0 execution

EL0 application
  -> libarmdesk when graphical
  -> libkarm
  -> svc #0
  -> syscall dispatcher
  -> validated kernel-owned request/response boundary
  -> process, VFS, GUI, IPC, memory, or information service
```

## Privilege model

### EL1 kernel

EL1 owns:

- initialization and board orchestration;
- exception and IRQ entry;
- physical and virtual memory management;
- process lifecycle and context activation;
- syscall dispatch and user-copy validation;
- VFS and filesystem implementations;
- the compositor/window manager;
- storage, input, USB, GPU, and network drivers;
- the bounded post-EOI runtime service;
- cooperative kernel helper work.

### EL0 applications

Panel, Shell, Editor, Files, Monitor, Control, and Clock execute as freestanding
KLI1 images. They enter the kernel only through the public syscall ABI.

The userland dependency direction is:

```text
application -> libarmdesk -> libkarm -> include/armonios/abi -> kernel
console app -------------> libkarm -> include/armonios/abi -> kernel
```

`libkarm` cannot depend on desktop code. `libarmdesk` may depend on `libkarm`.

### Higher exception levels

The verified QEMU path transitions to EL1 and remains there. A physical platform
that enters through EL2 or EL3 requires explicit transition, core parking, timer,
MMU, and board setup before the generic EL1 path is valid.

## Boot and initialization

```text
loader
  -> place kernel at the board load address
  -> pass DTB pointer in x0

boot/start.S
  -> preserve DTB
  -> establish early stack and vectors
  -> clear BSS
  -> call kernel_main(dtb)

kernel_main
  -> board early initialization and UART
  -> discover and reserve RAM, kernel, and DTB
  -> initialize PMM, process table, console, and heap
  -> create kernel mappings and enable MMU
  -> initialize VFS, bootfs, and tmpfs
  -> initialize timer, IRQ controller, runtime timing, and scheduler state
  -> expose an optional board block device
  -> discover and mount FAT32 where available
  -> initialize optional GPU, network, input, PCI, xHCI, and USB HID
  -> spawn Panel and activate EL0
```

Optional capabilities report explicit failure. Unsupported board functionality
must not silently appear initialized.

## Memory architecture

### Physical memory

The PMM uses a fixed bitmap and currently manages the configured 128 MiB QEMU
memory range. It supplies pages for:

- page-table hierarchies;
- process images and stacks;
- anonymous EL0 mappings;
- GUI backing buffers;
- kernel allocations with explicit ownership.

The PMM is not reference-counted. Higher layers must release each owned page
exactly once.

### Virtual memory

The VMM implements AArch64 stage-1 translation with 4 KiB pages and a four-level
hierarchy.

Current policy:

| Region | Permissions |
|---|---|
| Kernel text | EL1 read/execute |
| Kernel rodata | EL1 read-only, non-executable |
| Kernel mutable storage and stacks | EL1 read/write, non-executable |
| Remaining RAM identity mapping | EL1 read/write, non-executable |
| MMIO | device memory, non-executable |
| User image | EL0 read/execute |
| User stack and normal anonymous mappings | EL0 read/write, non-executable |

Mutable globals start zeroed and receive non-zero defaults during initialization,
preserving an empty loadable `.data` section.

Each process owns a TTBR0 root. The current root also contains kernel and RAM
identity mappings required by exception handling. Context switches replace TTBR0
and use broad EL1 invalidation.

Future hardening includes TTBR1, user-only TTBR0 roots, ASIDs, scoped invalidation,
and stronger stale-translation testing.

### Page ownership

The VMM owns page-table pages; callers own mapped leaf pages.

Process teardown therefore:

1. releases process-owned image, stack, and anonymous leaf pages;
2. releases the process page-table hierarchy;
3. clears process, descriptor, and GUI ownership state.

This ownership distinction is a central correctness invariant.

### User-copy boundary

Before a syscall reads or writes EL0 memory, the kernel checks:

- the registered process range;
- arithmetic overflow and complete-buffer bounds;
- page-table presence;
- requested read/write permissions.

Syscall payloads are then assembled in kernel-owned storage and copied across the
boundary only after validation.

The final EL1 load/store sequence is not fault-contained by exception-table
fixups. A late translation fault is still fatal and is tracked as future
hardening.

## Process and scheduling architecture

The process table contains 16 fixed slots. A process records:

- PID and parent PID;
- name and state;
- saved register state;
- user SP, PC, and PSTATE;
- TTBR0 root;
- owned user regions;
- descriptor table;
- exit status and zombie ownership;
- anonymous-mapping cursor.

A child zombie remains until its parent completes wait. Kernel-owned or orphaned
zombies may be reclaimed automatically. Descriptor, GUI, mapping, and page-table
cleanup occurs during process teardown.

### IRQ-origin boundary

The exception vector always captures a complete frame, but only an IRQ whose
saved PSTATE returns to EL0 may be treated as a process frame.

```text
IRQ from EL0
  -> service interrupt and runtime work
  -> process context may be saved
  -> scheduler may select another EL0 process

IRQ from EL1
  -> service interrupt and runtime work
  -> no process save
  -> no preemption
  -> no TTBR0 switch
  -> return to the interrupted kernel context
```

This prevents kernel registers captured during a syscall from being mistaken for
user process state.

## Runtime-service architecture

The hard timer IRQ remains small:

```text
physical timer IRQ
  -> account and rearm
  -> publish readiness bits
  -> update fixed scheduler counters
  -> interrupt-controller EOI
  -> measured post-EOI runtime pass
  -> process dispatch
  -> eret
```

The runtime pass groups work into PERIODIC/INPUT and NETWORK phases. Each work
class has a fixed count budget or finite scan. The whole pass also has a
cooperative generic-counter deadline.

On deadline expiry, the service:

1. records one expiry;
2. republishes the original work snapshot;
3. skips later optional work;
4. returns toward process dispatch.

The deadline is cooperative rather than asynchronous. One operation already in
progress can cross the nominal interval before the next checkpoint.

## Public ABI architecture

Public contracts live under:

```text
include/armonios/abi/
```

They include:

- ABI version metadata;
- syscall numbers;
- negative native status values;
- fixed-width base types;
- memory, VFS, process, system, and GUI flags/layouts;
- versioned filesystem metadata and filesystem-information records.

Rules:

- public headers do not include kernel-private headers;
- kernel and userland consume the same published source of truth;
- existing syscall numbers and status values are not reused;
- incompatible structures receive versioned replacements;
- optional capability is reported explicitly rather than inferred from a
  compile-time ABI version.

The pre-release global ABI remains `1.0`. The first official release establishes
the compatibility baseline.

## VFS architecture

### Descriptors and mounts

Each process owns a fixed descriptor table. A descriptor records its selected
node or mount-backed object, flags, offset, and ownership state.

The VFS:

- requires absolute paths;
- canonicalizes repeated separators, `.`, and bounded `..`;
- rejects root escape and overflow;
- selects the owning mount through longest component-prefix matching;
- passes the canonical mount-relative path to the filesystem;
- prevents generic VFS code from embedding FAT-specific path policy.

### Metadata

Filesystem-neutral kernel records describe:

- regular files and directories;
- size and generic attributes;
- bounded directory entries;
- filesystem identity, capacity, limits, and capabilities.

The public ABI exposes versioned `STAT_V2`, `READDIR_V2`, and `FSINFO` adapters
while preserving legacy stat/readdir calls.

### Filesystems

- **bootfs** exposes embedded KLI1 application images;
- **tmpfs** provides fixed-capacity in-memory storage;
- **FAT32** provides the current writable persistent-storage path.

## Block and FAT32 architecture

The generic block-device contract exposes:

- logical block size;
- total block count/capacity;
- read-only state;
- bounded read and write operations;
- explicit flush support;
- stable context and nested bounded views.

The normal QEMU path is:

```text
virtio-blk
  -> block_device_t
  -> whole device or primary MBR partition view
  -> FAT32 geometry validation
  -> FAT32 mount
  -> VFS mount at /fat
```

Current FAT32 semantics:

- 512-byte sectors;
- short 8.3 names;
- traversal of existing nested directory trees;
- nested list, stat, open, and read;
- root-level create, write, rename, and unlink;
- cluster-chain growth for the supported root workflow;
- explicit rejection of unsupported nested mutation;
- capability reporting that does not advertise long names, truncate, or exact
  free bytes when they are unavailable.

A safe general filesystem still requires truncate, mkdir/rmdir, mutation
rollback, long names, malformed-image coverage, and reboot-persistence evidence.

## GUI architecture

The kernel owns:

- window records and backing buffers;
- focus and z-order;
- dragging, minimize, and restore;
- event queues;
- cursor state and registered cursor regions;
- damage tracking and GPU submission.

The public GUI ABI defines event layouts, cursor/button values, and window-state
bits. `libarmdesk` wraps the GUI syscalls and provides geometry and semantic theme
helpers without moving compositor policy into userland.

The current toolkit boundary is intentionally small. Shared controls and layouts
must remain caller-owned, bounded, host-testable, and measured before they are
instantiated in shipping applications.

## Userland runtime architecture

### Build shape

Each application links with:

```text
application.o
application_header.o
crt0.o
libkarm.a
application_end.o
```

`crt0.o` remains explicit because it owns `_start`. GNU `ld` extracts referenced
archive members from `libkarm.a`; function/data sections and `--gc-sections`
remove unused runtime code.

### Allocation and containers

`kli_arena_t` is a caller-owned monotonic allocator. It can use caller-supplied
storage or one `SYS_MMAP` region. Individual allocations cannot be freed;
reset discards all allocations logically and destroy releases mapped storage.

`kli_buffer_t` adds growable binary storage. Growth allocates a new arena block,
copies existing bytes, and leaves old blocks owned by the arena.

`kli_string_t` layers null-terminated text over the buffer while maintaining:

```text
buffer.length < buffer.capacity
buffer.data[buffer.length] == '\0'
```

It treats UTF-8 as bytes and rejects embedded NUL values.

### Complete file transfers

`kli_fd_write_all` handles partial writes until the request completes or an error
occurs.

`kli_file_read_all` and `kli_file_read_text`:

- query structured metadata;
- require a regular file;
- reserve from the reported size but continue until EOF;
- close descriptors on success and failure;
- commit the destination only after the complete transfer and close succeed;
- restore the exact arena offset on failure;
- use a fixed 256-byte stack transfer block.

A path-level replace helper is deliberately absent until truncate or atomic
replacement semantics can prevent stale trailing data.

## Device and board architecture

### QEMU

The verified QEMU `virt` configuration provides:

- generic timer and interrupt controller integration;
- virtio block, GPU, input, and network;
- PCI/xHCI;
- directly attached USB boot keyboard and mouse;
- minimal Ethernet, ARP, IPv4, UDP, and DHCP.

### Raspberry Pi 4

The Raspberry Pi backend provides a build contract, mailbox/EMMC scaffolding,
bounded block views, and an opt-in read-only diagnostic package. Normal
unsupported capabilities fail closed.

There is no promoted physical boot, storage, framebuffer, input, USB, network,
or desktop runtime claim.

## Verification architecture

`tools/verify.sh` is the comprehensive local gate. Permanent focused tests cover
pure models, ABI layouts, lifecycle rules, subsystem contracts, and QEMU-visible
behavior.

Key invariants include:

- QEMU and Raspberry Pi builds remain distinct;
- production `.data` remains empty;
- the production image stays under 128 KiB;
- public ABI values and structures do not drift;
- userland stack use remains bounded;
- unsupported optional capabilities fail explicitly;
- test-only instrumentation does not become a production claim;
- visible/manual evidence is recorded separately from automated gates.
