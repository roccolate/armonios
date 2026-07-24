# Memory map

This document records the implemented fixed-address and translation model. It is
not a proposal for the desired hardened architecture.

Related documents:

- current capability: `CURRENT_STATE.md`;
- implemented ownership: `ARCHITECTURE.md`;
- active memory risks: `TECHNICAL_RISKS.md`;
- syscall mappings: `SYSCALLS.md`.

## QEMU physical memory

The default QEMU `virt` target provides 128 MiB of RAM. The physical memory
manager currently manages at most 128 MiB, so memory beyond that cap is ignored
even if a future target reports more.

| Region | Current use |
|---|---|
| firmware-provided DTB area | parsed at boot and reserved from allocation |
| `0x40080000` | QEMU kernel link/load base |
| kernel image | text, embedded KLI1 images, rodata, empty loadable data, BSS, and bootstrap reservations |
| remaining managed pages | page tables, process images/stacks, anonymous mappings, kernel heap, and GUI backing buffers |

The exact RAM range is discovered from the DTB. Board physical addresses belong
inside the selected board implementation.

## Raspberry Pi assumption

The experimental Raspberry Pi 4 linker script uses `0x80000` as the kernel link
address. That is a build contract only. No physical firmware, memory, or runtime
layout is promoted as supported.

## QEMU MMIO

| Address or range | Device |
|---|---|
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
| `0x09000000` | PL011 UART0 |
| `0x0a000000` plus board-defined stride | virtio-mmio transports |
| board-owned ECAM and BAR ranges | PCI discovery and xHCI |

Exact constants are board-private under `drivers/boards/qemu_virt/`.

## EL1 translation model

The kernel currently uses an identity-mapped lower-half design.

For the bootstrap table and each process table:

- managed RAM is identity-mapped with kernel W^X permissions;
- MMIO is identity-mapped as device memory and execute-never;
- TTBR1 is disabled;
- TTBR0 contains user mappings plus kernel/RAM identity mappings;
- process activation replaces TTBR0 and performs broad EL1 TLB invalidation.

Kernel entries in TTBR0 are not EL0-accessible; their leaf permissions remain
privileged.

Current permission policy:

| Region | Permission |
|---|---|
| kernel text | EL1 read/execute |
| kernel rodata | EL1 read-only, non-executable |
| kernel data, BSS, heap, and stack | EL1 read/write, non-executable |
| remaining RAM identity map | EL1 read/write, non-executable |
| MMIO | device read/write, non-executable |
| user image | EL0 read/execute |
| user stack and ordinary anonymous mapping | EL0 read/write unless a supported protection requests otherwise |

Future hardening under `RISK-008` includes TTBR1 kernel mappings, user-only TTBR0
roots, ASIDs, scoped invalidation, explicit global/non-global policy, and stale
translation tests.

## EL0 fixed layout

Values are defined by kernel layout headers.

| Virtual range | Purpose |
|---|---|
| `0x0000000000400000 + slot * 0x10000` | per-process KLI1 image slot |
| first 8 KiB of image slot | user read/execute image pages |
| `0x0000000000800000 + slot * 0x10000` | per-process stack slot |
| first 4 KiB of stack slot | user read/write stack page |
| spacing beside stack slot | unmapped separation asserted by layout constants |
| `0x0000000100000000` | anonymous mapping arena base |
| `0x0000000200000000` | anonymous mapping arena limit |

Static assertions keep image and stack slots disjoint and below the anonymous
mapping arena.

## Page-table structure

ArmoniOS uses AArch64 stage-1 translation with a 4 KiB granule and four levels:

```text
L0[VA 47:39] -> L1 table
L1[VA 38:30] -> L2 table
L2[VA 29:21] -> L3 table
L3[VA 20:12] -> 4 KiB page
VA[11:0]     -> page offset
```

Child tables are allocated lazily. Table descriptors contain aligned child-table
addresses. Leaf descriptors contain mapped physical pages and permission/memory
attribute bits.

## Ownership model

Page-table pages and mapped leaf pages have different owners:

- the VMM allocates and frees page-table hierarchy pages;
- the subsystem that maps a leaf retains ownership of the mapped physical page;
- process metadata owns image, stack, and anonymous leaf pages;
- GUI, heap, and drivers retain their own allocation contracts;
- `vmm_free_table()` releases table pages, not arbitrary mapped leaf pages.

Process teardown releases:

- process-owned image pages;
- the process stack page;
- owned anonymous mapping pages;
- the process page-table hierarchy;
- process-owned windows;
- process-owned VFS descriptors;
- other lifecycle resources through their central exit hooks.

The exactly-once distinction is a core invariant.

## IRQ-origin protection

The intermittent VMM fault formerly tracked by `RISK-018` was traced to an IRQ
that interrupted EL1 being treated as schedulable EL0 process state. That could
save kernel registers into a process frame and switch process/TTBR0 before
returning to the interrupted kernel path.

The vector-side origin gate now passes a schedulable frame only when SPSR reports
EL0t. IRQs from EL1 still service devices and deferred runtime work, but they do
not enter process save/preemption. Issue #63 and `RISK-018` are closed; the
page-ownership model itself did not change.

## Process user regions

Each process records at most eight disjoint user ranges. A record contains:

- virtual start and end;
- physical backing address when applicable;
- ownership flags.

Region flags express ownership, not effective PTE permission. User-copy checks
combine:

1. ownership by the current process;
2. readable or writable EL0 page-table permission, depending on direction.

Consequences:

- addresses belonging only to another process are rejected;
- a range crossing an ownership boundary is rejected;
- image, stack, and anonymous mappings can satisfy ownership checks;
- output additionally requires writable EL0 pages;
- final copies remain vulnerable to a late translation change after validation.

Permission-aware validation closed the original user-copy permission risk. Fault-
contained copyin/copyout remains `RISK-015`.

## User mappings

### KLI1 image

Image pages are allocated for the process, mapped EL0 read/execute, registered as
one owned region, and returned during process cleanup.

### Stack

Each process receives one 4 KiB stack page. The fixed slot stride leaves unmapped
spacing. Dynamic stack growth is not implemented.

### Anonymous mappings

`SYS_MMAP`:

- accepts only a zero hint;
- rounds size to pages;
- allocates contiguous physical pages;
- selects a monotonically increasing address in the process mapping arena;
- supports the published read/write/execute protection bits;
- rejects writable plus executable mappings;
- records physical ownership for cleanup.

`SYS_MUNMAP` requires an exact mapping match, removes the leaves and ownership
record, and returns the owned physical pages.

## Memory-change invariants

- preserve kernel W^X;
- preserve the empty loadable `.data` contract;
- use overflow-safe range arithmetic;
- distinguish page-table ownership from leaf-page ownership;
- roll back partially constructed mappings;
- reject invalid or unaligned roots and addresses before dereference;
- never preempt through an IRQ frame that originated in EL1;
- do not free a live process table;
- test allocation failure, exact unmap, process exit, and repeated cleanup;
- update this document and the risk register when mapping or ownership changes.
