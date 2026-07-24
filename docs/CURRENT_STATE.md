# Current State

This document is the canonical description of what is implemented on the current
`main` branch. It intentionally avoids historical PR narratives and short-lived
branch names.

For other questions:

- implemented design: `ARCHITECTURE.md`;
- future ordering: `ROADMAP.md`;
- active risks: `TECHNICAL_RISKS.md`;
- detailed syscall contracts: `SYSCALLS.md`;
- storage/VFS implementation detail: `V03_IMPLEMENTATION_STATUS.md`;
- external SDK and KLI1 workflow: `EXTERNAL_APPLICATIONS.md`.

## Executive classification

ArmoniOS is a compact, freestanding AArch64 graphical operating system whose
verified runtime target is QEMU `virt`.

Current release position:

- **v0.1 QEMU baseline:** verified;
- **v0.2 cleanup/runtime hardening:** implementation and automated evidence
  complete;
- **v0.2 formal release record:** pending issue #76, which requires the final
  dated visible workflow, exact validated tree, tag, and release notes;
- **v0.3 storage/VFS platform:** active and capable of mounted FAT32 application
  loading;
- **v0.4 general FAT workflow:** partial; existing nested reads and root mutation
  exist, while general mutation and durability contracts remain incomplete;
- **v0.5 userland platform:** partial; `libkarm`, a generated external SDK, and
  trusted KLI1 sideloading exist, while reusable `libarmdesk` widgets do not;
- **v1.0:** not ready.

Accurate public wording:

> ArmoniOS is a pre-release AArch64 QEMU desktop alpha with a verified graphical
> baseline, bounded runtime-service contracts, an evolving FAT32/VFS platform, a
> public native ABI, a freestanding SDK, and a minimal external KLI1 application
> path.

ArmoniOS is not a production OS, POSIX environment, general FAT implementation,
complete daily-use desktop, hardened third-party application sandbox, or verified
Raspberry Pi operating system.

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
- fixed-capacity process and mapping tables;
- fixed two-page, 8192-byte KLI1 process image slots.

TTBR1, ASIDs, user-only TTBR0 roots, scoped TLB invalidation, variable-size KLI
image allocations, and fault-contained copy fixups remain future hardening.

### Processes and interrupts

- preemptive freestanding EL0 processes;
- process spawn, argv, yield, exit, kill, non-blocking wait, and zombie ownership;
- parent/wait lifecycle and teardown regressions;
- embedded application resolution through `/armonios/<name>`;
- external KLI1 process spawn from an absolute VFS path;
- process creation kept `BLOCKED` until image, VM, stack, argv, and ownership
  transfer are complete;
- rollback of process and owned pages when installation fails;
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

## Public ABI and executable contract

The public kernel/userland boundary lives in:

```text
include/armonios/abi/
```

Current rules:

- syscall numbers and published error values are never silently reused;
- existing public layouts remain stable or receive versioned replacements;
- the global pre-release ABI identifier remains `1.0`;
- kernel, built-in userland, and external SDK consumers use the same public
  definitions;
- userland must not include kernel-private headers for ABI values or layouts.

Implemented public VFS additions include:

- `SYS_STAT_V2 = 49`;
- `SYS_READDIR_V2 = 50`;
- `SYS_FSINFO = 51`;
- versioned stat, directory-entry, and filesystem-information records;
- filesystem-specific native status values such as `EXIST`, `NOTDIR`, `ISDIR`,
  `NOTEMPTY`, `NOSPC`, `ROFS`, `NOTSUP`, and `RANGE`.

Legacy stat and readdir calls remain available.

KLI1 is also a public executable contract:

- little-endian AArch64 flat image;
- 80-byte public header;
- up to eight relative entry offsets;
- exact file size must equal the declared image size;
- the selected entry must be inside the image and four-byte aligned;
- no runtime relocator;
- no mutable static `.data` or `.bss`;
- relocation gates reject absolute, GOT, TLS, and other unsupported fixups.

## Storage and VFS

Implemented foundations:

- process-local descriptors with per-descriptor offsets;
- bootfs and tmpfs;
- generic mount callbacks;
- canonical absolute-path normalization;
- longest component-prefix mount resolution;
- descriptor-free mounted-file reads for kernel-internal consumers;
- static VFS-node precedence over mount-owned direct reads;
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
- direct path reads use a local `fat32_file_t` and do not consume a process FD or
  a persistent materialized-file slot;
- regular KLI1 files can be loaded through the generic VFS application path;
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

### Built-in applications

Seven freestanding KLI1 applications remain embedded:

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

Shell can preserve built-in launch behavior while also passing an absolute VFS
application path. The demonstrated command is:

```text
run /fat/HELLO.KLI
```

Files does not yet execute selected KLI files directly.

### External applications and SDK

The build produces a relocatable console SDK:

```sh
make sdk
```

The bundle contains:

- public ABI headers;
- installable `libkarm` headers;
- `crt0.o`;
- `libkarm.a`;
- generic KLI1 header/end objects;
- the KLI1 linker script;
- the relocation checker;
- a console example.

Permanent tests copy the bundle away from the repository and rebuild the example
using only that isolated SDK tree.

A separate FAT32 application image can be produced without rebuilding or
relinking `kernel.bin`:

```sh
make external-kli-image
```

The visible demonstration target is:

```sh
make qemu-external-kli
```

The automated runtime fixture loads an SDK-built external parent in EL0, has that
parent call `SYS_SPAWN_ARGV` on the same FAT32 path, runs the child in EL0, waits
for a clean child exit, and returns control to EL1.

This proves trusted sideloading and process integration. It does not prove safe
execution of hostile binaries.

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

A compiled `libarmdesk.a`, application/window object model, translated event
loop, damage batching, and reusable controls have not been promoted. The external
SDK is console-only until those pieces exist.

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

The matrix covers:

- QEMU and Raspberry Pi build contracts;
- kernel image size and empty `.data`;
- host suites and public ABI;
- process lifecycle and stack use;
- runtime service and VMM soak;
- FAT32 and VFS behavior;
- descriptor-free direct reads;
- KLI1 public header and generic packaging;
- unsupported-relocation rejection;
- isolated SDK reconstruction;
- byte-identical external KLI placement in FAT32;
- proof that creating the external disk leaves `kernel.bin` unchanged;
- QEMU EL0 execution of an external parent and VFS-spawned child;
- user-copy, focus, framebuffer, USB, network, and stress paths.

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
| KLI1 application image | 8192 bytes in the current fixed slot |
| Physical memory | 128 MiB PMM policy for the current configuration |
| Processes | 16 slots |
| User mappings | Eight recorded regions per process |
| VFS | Fixed nodes, mounts, descriptors, and 64-byte canonical paths |
| FAT32 | Existing nested 8.3 reads; root-only mutation; no long names |
| External apps | Trusted sideloading; no package manager or hostile-code sandbox |
| GUI | Fixed windows, event queues, cursor regions, and damage storage |
| USB | Four direct HID devices; no hubs |
| Network | No sockets, TCP, DNS, HTTP, or public general UDP API |
| Raspberry Pi | Build and diagnostic scaffolding only |

## Active work order

1. Complete issue #76 and publish the exact v0.2 visible/release record.
2. Promote compiled `libarmdesk` application and window objects.
3. Add translated events, redraw batching, Label, and click-based Button in
   userland.
4. Add `libarmdesk.a` to the generated SDK and execute a windowed external KLI.
5. Finish generic seek, truncate, mkdir/rmdir, and rollback-safe nested mutation.
6. Add explicit durability and reboot-persistence evidence.
7. Implement VFAT long names on top of stable mutation contracts.
8. Expand the seven built-in applications into the complete v1 workflow.
9. Add ext2 read-only through the generic mount interface.
10. Stabilize, fuzz, and record long visible-session evidence before beta.
