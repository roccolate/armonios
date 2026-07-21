# Architecture

ArmoniOS is a compact monolithic AArch64 kernel whose verified development platform is QEMU `virt`. The Raspberry Pi 4 board layer and read-only EMMC2 diagnostic probe are build/host verified, but physical hardware behavior remains unverified and fail-closed.

This document describes the implementation as it exists, including known architectural limitations. Operational status lives in `CURRENT_STATE.md`; active defects and exit criteria live in `TECHNICAL_RISKS.md`.

## Privilege model

- **EL1:** kernel, drivers, VFS, GUI compositor, exception handling, syscall dispatch, the post-EOI deferred runtime service, and cooperative helper threads.
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
  -> timer, IRQs, EL0 dispatch, deferred-runtime state
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
- kernel text is mapped RX, rodata R/NX, data+bss+stack RW/NX, and remaining RAM RW/NX;
- mutable kernel globals use zero-initialized BSS and subsystem init functions establish non-zero defaults, keeping the loadable `.data` section empty while preserving the page-aligned W^X boundary;
- board MMIO is identity-mapped as device memory;
- each EL0 process has a separate TTBR0 root;
- every process TTBR0 also contains the full kernel/RAM identity map needed while handling exceptions;
- process image pages are user read/execute;
- process stack and ordinary anonymous pages are user read/write unless callers request other supported flags;
- TTBR1 is disabled;
- changing process TTBR0 invalidates the complete EL1 TLB.

This provides basic EL0 separation and kernel-page W^X, but it is not a hardened split-address-space design. The remaining TTBR1, ASID, and scoped-invalidation work is future hardening material and is not part of the current v0.1 claim.

### Process user regions

Each process records a fixed set of disjoint user virtual ranges. These records are used to decide whether a syscall pointer belongs to the current process.

The syscall helper layer first checks the registered process range, then walks the process page table. Input buffers require valid user-readable leaves; output buffers also require writable leaves and return `ERR_PERM` on read-only pages before writing any byte. Syscall entry points now import VFS buffers, path strings, argv, IPC payloads, GUI outputs, and information outputs through kernel-owned temporaries before lower layers operate. The remaining limitation is that the final byte transfer is not fault-contained against an unexpected translation fault.

## Processes and scheduling

`kernel/process.{c,h}` owns:

- fixed process slots;
- PID, parent PID, and state;
- saved EL0 registers, PC, SP, and PSTATE;
- per-process TTBR0 root;
- registered user regions and owned physical pages;
- zombie state and cleanup.

EL0 dispatch uses `process_dispatch_next()` and a round-robin scan of ready slots. Spawn records the current process as parent. A child zombie remains in the fixed table until that parent calls `sys_wait`; automatic reclamation is limited to kernel-owned or orphaned zombies, so later spawns cannot erase an observable exit status.

### EL0 processes

EL0 processes are preemptive in the current architecture:

- IRQ entry saves the interrupted process frame;
- the physical timer callback accounts/rearms and publishes one coalescible periodic-work bit;
- the generic IRQ dispatcher sends EOI and runs one deferred runtime-service pass;
- the dispatcher may select another ready process;
- the selected process TTBR0 and trap frame are activated before exception return.

Voluntary yield and exit/fault paths share the same process activation logic.

### Deferred runtime service

Timer-originated device, GUI, and network work is represented by a zero-initialized pending bitmask. Repeated periodic requests coalesce. The consumer clears its snapshot before invoking the backend, so work published during a pass remains pending for a later pass rather than being lost.

The current consumer runs after `board_irq_end()` and before EL0 dispatch. This removes subsystem polling from the physical timer callback and makes the hard-IRQ handler bounded, but it is still a non-preemptible EL1 bottom half. It is not yet a separately scheduled kernel thread. `RISK-017` tracks duration measurement, per-pass budgets, starvation resistance, and the later decision to promote it into a wakeable thread. See `RUNTIME_SERVICE.md` for the exact contract and verification boundary.

### EL1 helper threads

The scheduler under `kernel/sched/` is cooperative. It is used for EL1 helper work such as console input. The timer updates scheduler counters but does not preempt an executing EL1 helper thread. The deferred runtime service is separate from this scheduler because the current helper-thread model does not interleave a wakeable EL1 service while the long-lived panel executes in EL0.

Therefore the accurate description is:

> preemptive EL0 processes, a post-EOI deferred runtime bottom half, and cooperative EL1 helper threads.

`RISK-010` tracks the helper-thread scheduling distinction; `RISK-017` tracks deferred-runtime budgeting.

## Exceptions and faults

- `svc #0` from EL0 enters syscall dispatch.
- Other synchronous lower-EL exceptions mark the current process exited and attempt to dispatch another process.
- An unexpected EL1 exception enters the fatal diagnostic path and waits forever.

Lower subsystems no longer receive caller-owned EL0 pointers for the covered syscall payloads. The syscall boundary performs bounded copies after range and PTE checks. Those copies are still ordinary EL1 loads/stores and are not recoverable if an unexpected translation fault occurs after validation.

## Syscall boundary

The ABI uses:

```text
x8      syscall number
x0..x6  arguments
x0      return value; negative values are kernel error codes
```

Numbers are frozen in `kernel/syscall_numbers.h` and exercised by host ABI tests.

Public pointer handling is centralized in `kernel/syscall_helpers.{c,h}`. The helpers provide process-range validation, page-table permission checks, c-string copying, argv import, and checked byte copies. State-consuming outputs validate the whole destination before dequeueing, then perform a bounded final copy. Fault-contained copy is still future work.

See `SYSCALLS.md` for exact calls and current limitations.

## VFS and file descriptors

The VFS is a fixed-capacity in-kernel facade with static nodes plus a small mount table. Mount callbacks dispatch open, list, unlink, and rename without embedding FAT32 knowledge in the generic layer. It supports bootfs, tmpfs, and dynamic FAT32 root-file nodes.

Current descriptor architecture:

- a global kernel handle pool stores node pointer, offset, flags, owner PID, and local fd;
- each process sees local descriptor numbers `3..10`;
- VFS operations translate the caller's local fd through the current PID;
- dead owners are reaped lazily and `process_mark_exited()` closes all descriptors for the exiting PID.

`RISK-002` is closed for process-owned descriptors. The VFS remains a fixed-table kernel facade, not a POSIX filesystem layer.

The current mount table and filesystem callback boundary are intentionally small. The v1 roadmap still requires a common path resolver, structured metadata/directory ABI, richer block-device metadata, and filesystem semantics beyond root-only FAT32.

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

It does not claim long-file-name support, subdirectories, GPT or extended-partition discovery, journaling, crash recovery, or broad compatibility testing. A reusable MBR parser and bounded block view can locate and validate one supported primary FAT32 partition; that path is currently used by the opt-in RPi4 read-only probe, not exposed as normal writable board storage.

The QEMU block path is verified through `make qemu-fs-test`. The visible desktop target also attaches the generated FAT32 block image; `tools/qemu_fb_fat_test.sh` verifies the FAT + display + panel wiring. The visible create/edit/save/rename/reopen/delete workflow has existing manual evidence from rocco on 2026-07-17; newer automated baselines must not imply a newer manual desktop pass unless one is recorded.

v1 storage direction is to replace this narrow bridge with real writable FAT
behind the filesystem interface, then mount ext2 at `/ext` at least read-only.
There is no current ext2 implementation.

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

Normal application wrappers request focus after creating a window. The kernel still enforces `GUI_WINDOW_NO_FOCUS`, and `tools/qemu_focus_test.sh` verifies the focus syscall path. The files-to-editor focus workflow has existing manual confirmation from rocco on 2026-07-17.

Timer-originated GUI event routing and dirty redraw now execute through the deferred runtime service after EOI. Syscalls may still mark compositor state dirty immediately, but the timer callback itself does not render or drain GUI input. The current service pass can still process an unbounded queue and a full redraw; that limitation is tracked by `RISK-017`.

## Input and device polling

Input events enter one shared queue from:

- UART keyboard translation;
- virtio-input;
- directly attached USB boot-protocol HID devices.

The physical timer callback no longer invokes UART, board-input, USB-HID, GUI, or network routines. It publishes `RUNTIME_WORK_PERIODIC`; after EOI, the single deferred runtime service polls board/USB producers, routes queued GUI events, flushes dirty redraw, and polls the network. The serial-console helper and syscall boundary retain their explicit input servicing paths for compatibility, but timer-originated periodic work has one consumer and one coalescing pending state.

The service currently drains all available input events in one pass. There is no event budget, duration metric, or sustained-load progress proof yet. Those limits are explicit under `RISK-017` rather than being hidden inside the IRQ path.

USB support is currently a basic QEMU xHCI path with directly attached keyboard/mouse devices. USB hubs are not supported.

## Networking

The network code is a small direct stack over virtio-net containing enough Ethernet, ARP, IPv4, UDP, and DHCP behavior to obtain a QEMU user-network lease.

Timer-originated `net_poll()` execution is deferred until after EOI through the runtime service. The service does not yet cap receive work per pass or expose a socket scheduler; network load can therefore contribute to the open runtime-budget risk.

There is no application socket ABI, TCP, general UDP API, DNS query implementation, or HTTP client.

Use `bash tools/qemu_marker_test.sh net` or `bash tools/verify.sh` for deterministic network-marker evidence. The plain `qemu-net` target remains a launch command.

## Board boundary

Generic kernel code includes `drivers/board.h`. Physical addresses and board-specific initialization belong under `drivers/boards/<board>/`.

The QEMU board is the reference implementation.

The board contract now exposes generic storage, display, input, IRQ, and capability entry points. QEMU implements display/input through virtio internally; generic kernel orchestration calls `board_display_*` and `board_input_*`. RPi4 returns explicit safe failures for missing display/input while still compiling and linking under `tests/run_board_build_test.sh`.

The RPi4 board directory is still not a supported hardware implementation. The SDHCI controller core, firmware clock query, MBR parser, bounded block view, and minimal read-only probe are implemented and host/build verified. Normal board capabilities remain zero and storage stays fail-closed until repeatable physical serial evidence closes `RISK-007`.

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
- `control`
- `clock`

KLI1 is a small flat-image format with a fixed header and entry offsets. The current linker script explicitly orders the image header, text, rodata, and end marker.

Mutable `.data` and `.bss` are forbidden for shipping app images. `programs/apps/image.ld` asserts that contract, and `tests/run_kli1_contract_test.sh` verifies both the seven shipping apps and a synthetic violation.

The current applications demonstrate the desktop and persistence paths but are
not yet complete daily-use tools. The v1 line needs shared `libkarm` runtime
helpers, `libkarmdesk` widgets, a multi-line Editor, directory-aware Files,
stronger Shell commands, observable Settings behavior, and a more useful
Monitor while preserving the KLI1 storage contract.

## Testing architecture

Native host tests exercise pure-C contracts and mocked driver paths. They provide good regression coverage for logic, but they cannot prove device MMIO or full exception-return behavior.

`tests/run_runtime_service_test.sh` verifies coalescing, requeue preservation, post-EOI ordering, and a static forbidden-call boundary for `kernel/timer/timer.c`. `qemu-fs-test`, `tools/qemu_marker_test.sh`, `tools/qemu_focus_test.sh`, `tools/qemu_usercopy_test.sh`, and `tools/qemu_fb_fat_test.sh` capture guest serial output and check required markers. They do not replace visible manual workflow evidence or sustained-load latency tests.

Release evidence must always state whether it came from:

- static inspection;
- host tests;
- build/link;
- deterministic QEMU output;
- visible manual QEMU use;
- physical hardware.

## Design direction

Near-term architectural priorities are:

1. bound the deferred runtime service: duration instrumentation, per-pass work budgets, preserved pending work, and EL0-progress stress tests;
2. finish v0.2 hardening: fault-contained user copies and any remaining syscall-boundary audits;
3. v0.3 storage platform: richer block-device metadata, common path resolution, and structured filesystem ABI;
4. v0.4 real FAT: long names, directories, broader partition handling, and persistence tests;
5. v0.5-v0.6 userland runtime, widgets, and useful desktop applications;
6. v0.7 ext2 read-only support;
7. ongoing hardening: TTBR1 kernel mappings, ASIDs, scoped TLB invalidation, and physical RPi evidence when the hardware track resumes.
