# Current State

This document is the canonical description of what is implemented on the current
`main` branch. It intentionally avoids historical PR narratives and short-lived
branch names.

For other questions:

- implemented design: `ARCHITECTURE.md`;
- future ordering: `ROADMAP.md`;
- active risks: `TECHNICAL_RISKS.md`;
- detailed syscall contracts: `SYSCALLS.md`;
- storage/VFS implementation detail: `V03_IMPLEMENTATION_STATUS.md`.

## Executive classification

ArmoniOS is a compact, freestanding AArch64 graphical operating system whose
verified runtime target is QEMU `virt`.

Current release position:

- **v0.1 QEMU baseline:** verified;
- **v0.2 cleanup/runtime hardening:** implementation and automated evidence
  complete;
- **v0.2 formal release record:** pending issue #76, which requires the final
  dated visible workflow, exact validated tree, tag, and release notes;
- **v0.3 storage/VFS platform:** in progress;
- **v0.4 general FAT workflow:** early partial foundations only;
- **v0.5 userland runtime/widgets:** early partial; runtime primitives exist,
  reusable widgets do not;
- **v1.0:** not ready.

Accurate public wording:

> ArmoniOS is a pre-release AArch64 QEMU desktop alpha with a verified graphical
> baseline, bounded runtime-service contracts, an evolving storage/VFS platform,
> a public native ABI, and an early freestanding userland runtime.

ArmoniOS is not a production OS, POSIX environment, general FAT implementation,
complete daily-use desktop, or verified Raspberry Pi operating system.

## Implemented platform

### Execution and memory

- AArch64 entry and DTB handoff;
- EL1 monolithic kernel with narrow assembly boundaries;
- single-core execution model;
- physical memory manager for the configured 128 MiB QEMU environment;
- four-level 4 KiB stage-1 translation;
- kernel W^X;
- empty loadable `.data` contract;
- process-owned image, stack, and anonymous-mapping pages;
- permission-aware user-range and page-table checks;
- fixed-capacity process and mapping tables.

TTBR1, ASIDs, user-only TTBR0 roots, scoped TLB invalidation, and fault-contained
copy fixups remain future hardening.

### Processes and interrupts

- preemptive freestanding EL0 processes;
- process spawn, argv, yield, exit, kill, non-blocking wait, and zombie ownership;
- parent/wait lifecycle and teardown regressions;
- IRQ-origin classification: only an interrupt returning to EL0 may provide a
  schedulable process frame;
- an IRQ that interrupted EL1 services devices/runtime work and returns to the
  exact kernel context without process save, preemption, or TTBR0 replacement.

The previously intermittent EL1 VMM fault is closed as the IRQ-origin defect
fixed by this boundary and backed by repeated FAT32 boot coverage.

### Runtime service

The physical timer callback performs fixed accounting, rearm, readiness
publication, and scheduler-counter work. After interrupt-controller EOI, a
measured runtime-service pass handles periodic desktop/device work before process
dispatch and `eret`.

Current bounds:

| Work class | Bound |
|---|---:|
| Whole service | One nominal timer interval at cooperative checkpoints |
| Virtio input | At most the negotiated ring length and no more than 16 descriptors per call |
| USB HID | Four fixed device visits per call |
| Shared input | 16 queued events per active pass |
| Partial redraw | Eight ordered damage rectangles per successful submission |
| Virtio network RX | 16 valid frames per NETWORK pass |

Deadline expiry records the event, republishes the original work snapshot, skips
later optional work, and returns toward process dispatch. One already-started
driver operation or full redraw is not asynchronously preempted.

The runtime implementation and sustained-load evidence are complete. Historical
measurement identities belong in the associated issues and release record rather
than in this live-state document.

## Public ABI

The public kernel/userland boundary lives in:

```text
include/armonios/abi/
```

Current rules:

- syscall numbers and published error values are never silently reused;
- existing public layouts remain stable or receive versioned replacements;
- the global pre-release ABI identifier remains `1.0`;
- kernel and userland consume the same public definitions;
- userland must not include kernel-private headers for ABI values or layouts.

Implemented public VFS additions include:

- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`;
- `SYS_FSINFO = 51`;
- versioned stat, directory-entry, and filesystem-information records;
- filesystem-specific native status values such as `EXIST`, `NOTDIR`, `ISDIR`,
  `NOTEMPTY`, `NOSPC`, `ROFS`, `NOTSUP`, and `RANGE`.

Legacy stat and readdir calls remain available.

## Storage and VFS

Implemented foundations:

- process-local descriptors with per-descriptor offsets;
- bootfs and tmpfs;
- generic mount callbacks;
- canonical absolute-path normalization;
- longest component-prefix mount resolution;
- generic block-device capacity, block-size, read-only, read/write, flush, and
  bounded-view contracts;
- QEMU virtio-blk adapter;
- Raspberry Pi diagnostic EMMC adapter;
- whole-device and primary-MBR FAT32 discovery;
- bounded partition views;
- filesystem-neutral native metadata and directory-entry records;
- filesystem-information and capability reporting.

Current FAT32 behavior:

- 512-byte sectors;
- one mounted FAT32 volume on the normal QEMU path;
- short 8.3 names;
- existing nested directories can be traversed, listed, statted, and read;
- root-level create, write, rename, and delete remain available;
- nested mutation is deliberately rejected;
- transport read-only and real flush support are reported through `FSINFO`.

Not implemented:

- complete seek semantics across every backend;
- truncate and safe cluster shrink/grow;
- mkdir/rmdir;
- nested create, unlink, rename, or move transactions;
- VFAT long names;
- exact free-space reporting;
- application-visible fsync semantics and reboot-persistence proof;
- ext2.

## Userland

### Applications

Seven freestanding KLI1 applications run:

- Panel;
- Shell;
- Editor;
- Files;
- Monitor;
- Control;
- Clock.

They demonstrate the desktop and ABI but remain bounded tools. Editor uses a
small fixed text buffer; Files is focused on `/fat`; Shell and system utilities
do not yet provide the complete v1 workflow.

### libkarm

`libkarm` is the GUI-independent freestanding runtime. The build produces an
explicit `crt0.o` startup object and a static `libkarm.a` archive.

Implemented runtime components include:

- syscall wrappers and typed public-ABI adapters;
- small output, memory, string, and integer helpers;
- explicit caller-owned monotonic arenas using supplied or `SYS_MMAP` storage;
- growable arena-backed binary buffers;
- arena-backed null-terminated dynamic strings;
- complete descriptor writes with partial-progress handling;
- complete binary and text file reads with descriptor cleanup and exact arena
  rollback on failure.

Current limitations:

- no general `malloc`/`free` heap;
- no formatted-output subsystem;
- no safe path-level replace/write helper while truncate is unavailable;
- no libc or POSIX compatibility promise.

### libarmdesk

`libarmdesk` is the canonical desktop-facing userland layer and may depend on
`libkarm`.

Implemented foundation:

- typed GUI syscall wrappers;
- backend-neutral rectangle and clipping helpers;
- semantic theme tokens;
- public GUI ABI shared with the kernel;
- compatibility include through `programs/libkarmdesk/`.

Reusable controls, layouts, dialogs, list models, text fields, and icons have not
been promoted. The abandoned Control-widget draft did not enter `main`.

## Devices and networking

Verified QEMU components:

- virtio block;
- virtio GPU;
- virtio input;
- virtio network;
- PCI enumeration;
- xHCI with directly attached boot-protocol keyboard and mouse;
- Ethernet, ARP, IPv4, UDP, and DHCP sufficient to obtain a QEMU lease.

There is no socket ABI, TCP, DNS API, HTTP stack, or general user-facing UDP API.
USB hubs are not supported.

## Build and verification

The normal comprehensive gate is:

```sh
bash tools/verify.sh
```

The matrix covers the QEMU and Raspberry Pi build contracts, kernel image size,
empty `.data`, host suites, public ABI, process lifecycle, stack use, runtime
service, VMM soak, FAT32, user-copy, focus, framebuffer wiring, USB, network, and
stress paths.

The default loadable production-image ceiling is:

```text
128 KiB / 131072 bytes
```

The ceiling is a regression budget, not permission to consume all remaining
space without measurement.

## Fixed and deliberate limits

| Area | Current boundary |
|---|---|
| Runtime target | QEMU `virt` is the verified platform |
| Production image | 128 KiB hard default ceiling |
| Physical memory | 128 MiB PMM policy for the current configuration |
| Processes | 16 slots |
| User mappings | Eight recorded regions per process |
| VFS | Fixed nodes, mounts, descriptors, and 64-byte canonical paths |
| FAT32 | Existing nested 8.3 reads; root-only mutation; no long names |
| GUI | Fixed windows, event queues, cursor regions, and damage storage |
| USB | Four direct HID devices; no hubs |
| Network | No sockets, TCP, DNS, HTTP, or public general UDP API |
| Raspberry Pi | Build and diagnostic scaffolding only |

## Active work order

1. Complete issue #76 and publish the exact v0.2 visible/release record.
2. Finish generic seek and truncate semantics.
3. Add mkdir/rmdir and rollback-safe nested mutation.
4. Add explicit durability and reboot-persistence evidence.
5. Implement VFAT long names on top of the stable mutation contracts.
6. Continue `libkarm` and `libarmdesk` only through measured, consumer-driven
   slices.
7. Expand the seven applications into the complete v1 workflow.
8. Add ext2 read-only through the generic mount interface.
9. Stabilize, fuzz, and record long visible-session evidence before beta.
