# Architecture

ArmoniOS is a compact monolithic AArch64 kernel whose verified development platform is QEMU `virt`. Raspberry Pi code exists only as an experimental future port.

This document describes the implementation as it exists, including known architectural limitations. Operational status lives in `CURRENT_STATE.md`; active defects and exit criteria live in `TECHNICAL_RISKS.md`.

## Privilege model

- **EL1:** kernel, drivers, VFS, GUI compositor, exception handling, syscall dispatch, and cooperative helper threads.
- **EL0:** freestanding KLI1 applications.
- **EL2/EL3:** not used by the verified QEMU path. A physical board may enter at EL2 and needs a deliberate transition before normal kernel initialization.

Drivers are linked into the kernel. There is no driver isolation, libc, POSIX layer, or hosted runtime.

## Boot flow

QEMU loads the kernel at the board linker address and passes the DTB pointer in `x0`.

```text
boot/start.S
  -> preserve DTB pointer
  -> install early stack
  -> install exception vectors
  -> clear BSS
  -> call kernel_main(dtb)

kernel_main
  -> board early init and UART
  -> DTB memory discovery
  -> PMM and process table
  -> console and heap
  -> VMM/MMU identity mapping
  -> VFS, bootfs, tmpfs
  -> timer, IRQs, EL0 dispatch
  -> optional storage and FAT32
  -> optional virtio GPU
  -> optional network
  -> input, PCI, USB/HID
  -> launch panel in EL0
  -> start cooperative EL1 helper scheduler
```

Boot phases are recorded in `kernel/init_status.{c,h}` and exposed through the kernel console.

## Memory architecture

### Physical memory manager

The PMM uses a fixed bitmap and currently manages at most 128 MiB. This matches the default QEMU configuration but is not a general Raspberry Pi memory policy.

The kernel reserves its loaded image and DTB before allocating pages for page tables, application images, stacks, anonymous mappings, GUI backing buffers, and kernel heap arenas.

### Virtual memory

The VMM implements AArch64 4 KiB, four-level stage-1 translation tables.

Current behavior:

- the early kernel builds an identity map of the detected RAM range;
- that RAM range is mapped for EL1 as read/write/execute;
- board MMIO is identity-mapped as device memory;
- each EL0 process has a separate TTBR0 root;
- every process TTBR0 also contains the full kernel/RAM identity map needed while handling exceptions;
- process image pages are user read/execute;
- process stack and ordinary anonymous pages are user read/write unless callers request other supported flags;
- TTBR1 is disabled;
- changing process TTBR0 invalidates the complete EL1 TLB.

This provides basic EL0 separation, but it is not a hardened split-address-space design. `RISK-008` tracks the intended TTBR1, W^X, ASID, and scoped-invalidation work.

### Process user regions

Each process records a fixed set of disjoint user virtual ranges. These records are used to decide whether a syscall pointer belongs to the current process.

Important current limitation: the region metadata used by syscall helpers does not distinguish readable from writable ranges. `sys_user_buf_in()` and `sys_user_buf_out()` currently apply the same membership check. This means the phrase “validated writable user buffer” must not be used until `RISK-001` is closed.

## Processes and scheduling

`kernel/process.{c,h}` owns:

- fixed process slots;
- PID and state;
- saved EL0 registers, PC, SP, and PSTATE;
- per-process TTBR0 root;
- registered user regions and owned physical pages;
- zombie state and cleanup.

EL0 dispatch uses `process_dispatch_next()` and a round-robin scan of ready slots.

### EL0 processes

EL0 processes are preemptive in the current architecture:

- IRQ entry saves the interrupted process frame;
- timer handling runs;
- the dispatcher may select another ready process;
- the selected process TTBR0 and trap frame are activated before exception return.

Voluntary yield and exit/fault paths share the same process activation logic.

### EL1 helper threads

The scheduler under `kernel/sched/` is cooperative. It is used for EL1 helper work such as console/input polling. The timer updates scheduler counters but does not preempt an executing EL1 helper thread.

Therefore the accurate description is:

> preemptive EL0 processes with cooperative EL1 helper threads.

`RISK-010` tracks this documentation and future-design distinction.

## Exceptions and faults

- `svc #0` from EL0 enters syscall dispatch.
- Other synchronous lower-EL exceptions mark the current process exited and attempt to dispatch another process.
- An unexpected EL1 exception enters the fatal diagnostic path and waits forever.

Because kernel syscall bodies currently dereference validated user virtual addresses directly, a bad permission assumption at the user-copy boundary can become an EL1 fault rather than a recoverable user error. This is part of `RISK-001`.

## Syscall boundary

The ABI uses:

```text
x8      syscall number
x0..x6  arguments
x0      return value; negative values are kernel error codes
```

Numbers are frozen in `kernel/syscall_numbers.h` and exercised by host ABI tests.

Public pointer handling is centralized in `kernel/syscall_helpers.{c,h}`. The current helpers provide process-range validation and c-string copying, but not permission-aware or fault-contained user copies.

See `SYSCALLS.md` for exact calls and current limitations.

## VFS and file descriptors

The VFS is a fixed-table in-kernel facade over mounted node callbacks. It supports bootfs, tmpfs, and dynamic FAT32 root-file nodes.

Current descriptor architecture:

- one global table of eight VFS open-file entries;
- each entry stores node pointer, offset, flags, and used state;
- syscall-visible descriptor `3+` indexes that global table;
- no owner PID is stored;
- process exit does not close descriptors automatically.

This is suitable for early single-process bring-up but not a correct multi-process resource model. `RISK-002` blocks v1.0 until descriptors are process-owned and reclaimed.

## Filesystems and storage

### bootfs

Shipping application images are embedded into the kernel and exposed under `/armonios/<name>`.

### tmpfs

The current tmpfs is a small fixed in-memory file implementation used for simple kernel/VFS validation.

### FAT32

The current FAT32 bridge supports:

- 512-byte sectors;
- short 8.3 root names;
- root-directory list/open/create/read/write/rename/delete;
- cluster-chain growth for the supported file path;
- dynamic `/fat/<name>` VFS nodes;
- invalidation of dynamic nodes after rename/delete.

It does not claim long-file-name support, subdirectories, arbitrary partition discovery, journaling, crash recovery, or broad compatibility testing.

The QEMU block path is verified through `make qemu-fs-test`. The current visible desktop target does not attach the block image; see `RISK-003`.

## GUI architecture

The GUI is a kernel-owned compositor rather than a userland display server.

Responsibilities are split across:

- `gui_events` — per-window queues;
- `gui_cursor` — cursor state, drag state, and cursor regions;
- `gui_input` — hit testing and input dispatch;
- `gui_backing` — per-window content buffers;
- `gui_pool` — lifecycle, ownership, lookup, and focus;
- `gui_compositor` — z-order drawing and damage tracking.

Windows carry an owner PID. Most mutating syscalls require ownership. A small set of presentation calls is intentionally cross-process so the panel can find, focus, restore, and display state for application windows.

Normal application creation does not currently guarantee that a new window receives focus when another window is focused. This is visible in the files-to-editor workflow and tracked by `RISK-004`.

## Input and device polling

Input events enter one shared queue from:

- UART keyboard translation;
- virtio-input;
- directly attached USB boot-protocol HID devices.

The kernel polls several devices from both the timer path and helper loop. This is intentionally simple but should be treated carefully if concurrency, SMP, or more EL1 threads are introduced.

USB support is currently a basic QEMU xHCI path with directly attached keyboard/mouse devices. USB hubs are not supported.

## Networking

The network code is a small direct stack over virtio-net containing enough Ethernet, ARP, IPv4, UDP, and DHCP behavior to obtain a QEMU user-network lease.

There is no application socket ABI, TCP, general UDP API, DNS query implementation, or HTTP client.

`qemu-net` currently launches the VM but is not a deterministic release test; see `RISK-005`.

## Board boundary

Generic kernel code includes `drivers/board.h`. Physical addresses and board-specific initialization belong under `drivers/boards/<board>/`.

The QEMU board is the reference implementation.

The RPi4 board directory is not a supported implementation. It does not currently satisfy all functions declared by the generic board contract and the eMMC code is experimental. See `PORTING.md`, `RISK-006`, and `RISK-007`.

A future board interface should distinguish mandatory capabilities from optional ones instead of naming generic operations after virtio-specific devices.

## Userland and KLI1

User programs live under `programs/apps/` and link against:

- `programs/libkarm` for startup, syscall trampolines, I/O, and small helpers;
- `programs/libkarmdesk` for GUI wrappers.

The shipping set is:

- `panel`
- `shell`
- `editor`
- `files`
- `monitor`
- `clock`

KLI1 is a small flat-image format with a fixed header and entry offsets. The current linker script explicitly orders the image header, text, rodata, and end marker.

Mutable `.data` and `.bss` are not yet an explicit tested contract. Current applications avoid relying on general static mutable storage. `RISK-009` requires the format either to reject these sections or define how the loader initializes them.

## Testing architecture

Native host tests exercise pure-C contracts and mocked driver paths. They provide good regression coverage for logic, but they cannot prove device MMIO or full exception-return behavior.

`qemu-fs-test` is deterministic because it captures guest serial output and checks required markers. The framebuffer, USB, and network launch targets currently lack equivalent final assertions.

Release evidence must always state whether it came from:

- static inspection;
- host tests;
- build/link;
- deterministic QEMU output;
- visible manual QEMU use;
- physical hardware.

## Design direction

Near-term architectural priorities are:

1. permission-aware and fault-contained user copies;
2. process-owned kernel resources, beginning with file descriptors;
3. deterministic QEMU runtime tests;
4. an explicit KLI1 mutable-storage contract;
5. shared TTBR1 kernel mappings and section permissions;
6. a capability-oriented board interface before serious hardware work.
