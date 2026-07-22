# Memory Map

This document records the current fixed-address and translation model. It
describes implementation, not the desired hardened architecture.

- Operational evidence: `CURRENT_STATE.md`
- Memory risks: `RISK-008`, `RISK-015`, and `RISK-018` in
  `TECHNICAL_RISKS.md`
- Practical change workflow: `DEVELOPMENT_GUIDE.md`

## QEMU `virt` physical layout

The default QEMU target uses 128 MiB of RAM. The PMM also manages at most 128 MiB,
so memory beyond that limit is ignored even if a future board reports more.

| Region | Current use |
|---|---|
| QEMU-provided DTB area | Firmware/DTB data passed to the kernel and reserved at boot |
| `0x40080000` | Kernel load and link base for `qemu_virt` |
| Kernel image | `.text`, embedded app blobs, `.rodata`, empty loadable `.data`, `.bss`, and bootstrap stack reservation |
| Remaining PMM pages | Page tables, process image/stack pages, anonymous mappings, heap arenas, and GUI backing buffers |

The exact RAM range comes from the DTB. Board physical addresses belong in the
board layer.

## Raspberry Pi 4 physical assumptions

The experimental RPi4 linker script uses `0x80000` as its kernel link address.
That address is build-verified, but no physical firmware/boot configuration is
promoted. The RPi4 memory layout remains a hardware-track assumption until the
criteria in `PORTING.md` and `DOCUMENTATION_POLICY.md` are satisfied.

## QEMU MMIO

| Address/range | Device |
|---|---|
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
| `0x09000000` | PL011 UART0 |
| `0x0a000000` and board-defined strides | virtio-mmio transports |
| Board-owned ECAM/MMIO ranges | PCI discovery and xHCI BAR assignment |

Exact constants live under `drivers/boards/qemu_virt/`.

## Current EL1 virtual layout

The kernel uses an identity-mapped lower-half design.

For the bootstrap table and every process table:

- detected RAM is identity-mapped with kernel W^X permissions;
- board MMIO is identity-mapped as device memory and execute-never;
- TTBR1 is disabled;
- TTBR0 contains user mappings plus the kernel/RAM identity map;
- process activation changes TTBR0 and performs broad EL1 TLB invalidation.

EL0 cannot access kernel mappings merely because they exist in TTBR0: kernel
entries are not user-accessible.

Current kernel mapping policy:

| Region | Permission |
|---|---|
| Kernel text | EL1 read/execute |
| Kernel rodata | EL1 read-only, non-executable |
| Kernel data, BSS, heap, and stack | EL1 read/write, non-executable |
| Remaining RAM | EL1 read/write, non-executable |
| MMIO | Device read/write, non-executable |

Target hardening under `RISK-008`:

- shared kernel mappings through TTBR1;
- user-only TTBR0 roots;
- ASIDs;
- scoped TLB invalidation;
- explicit global/non-global mapping policy;
- stale-translation and lifecycle tests.

## EL0 fixed layout

Values come from `kernel/layout.h` and `kernel/process.h`.

| Virtual range | Purpose |
|---|---|
| `0x0000000000400000 + slot * 0x10000` | Per-process KLI1 image slot |
| 8 KiB image slot | PMM-owned image pages, user read/execute |
| `0x0000000000800000 + slot * 0x10000` | Per-process stack slot |
| 4 KiB stack slot | PMM-owned user stack, user read/write |
| spacing beside stack slot | At least one unmapped page asserted by layout constants |
| `0x0000000100000000` | Anonymous mapping arena base |
| `0x0000000200000000` | Anonymous mapping arena limit |

Static assertions keep image and stack slots disjoint and below the anonymous
mapping arena.

## Page-table structure

AArch64 4 KiB granule, four levels, 48-bit virtual address configuration:

```text
L0[VA 47:39] -> L1 table
L1[VA 38:30] -> L2 table
L2[VA 29:21] -> L3 table
L3[VA 20:12] -> 4 KiB page descriptor
VA[11:0]     -> page offset
```

Child tables are allocated lazily. Table descriptors contain aligned physical
addresses plus valid/table bits. Leaf descriptors contain mapped physical pages
and permission/attribute bits.

## Ownership model

Page-table and leaf-page ownership are intentionally separate:

- `vmm_new_table()` and child-table creation allocate page-table pages;
- `vmm_free_table()` recursively frees table pages only;
- the subsystem that maps a leaf page retains ownership of that leaf page;
- process metadata records image, stack, and anonymous leaf ownership;
- GUI, heap, and driver allocations retain their own ownership contracts.

Process teardown currently releases:

- owned image pages;
- owned stack pages;
- owned anonymous mapping pages;
- the process page-table hierarchy;
- process-owned GUI windows through centralized exit;
- every VFS descriptor owned by the process.

This exactly-once distinction is central to issue #63 / `RISK-018`. One
intermittent FAT32 smoke panic resolved to the `table[index]` load in
`next_table()`. No ownership rule is changed by documentation; the open
investigation must identify stale/corrupt/freed table state or bound an external
cause before the risk is closed.

## Process user-region metadata

Each process owns at most eight registered ranges. A record contains:

- start;
- end;
- physical backing address when known;
- ownership flags.

Region flags express ownership, not effective PTE permissions. User-copy
validation therefore combines:

1. current-process range ownership;
2. page-table readable/writable permission.

Consequences:

- foreign-process-only addresses are rejected;
- crossing a registered-region boundary is rejected;
- zero-length validation is accepted for a non-null process;
- image, stack, and anonymous regions can satisfy ownership checks;
- kernel-to-user output additionally requires writable EL0 leaf entries;
- copies remain non-fault-contained if translation changes unexpectedly after
  validation.

Permission-aware validation closed `RISK-001`. Recoverable final copy remains
`RISK-015`.

## User mappings

### Image

KLI1 image pages are allocated per process, mapped user read/execute, registered
as one fixed image region, and returned during process cleanup.

### Stack

Each process receives one 4 KiB user stack page. The slot stride leaves unmapped
spacing. There is no dynamic stack growth.

### Anonymous mappings

`sys_mmap`:

- accepts only `hint == 0`;
- rounds size to pages;
- allocates contiguous physical pages;
- selects addresses monotonically in the process arena;
- supports read, write, and execute flags at the PTE layer;
- records physical ownership for cleanup.

`sys_munmap` requires an exact mapping match, removes leaf mappings, removes the
region record, and returns owned physical pages.

## Invariants for memory changes

- preserve kernel W^X;
- preserve `.data == 0`;
- use overflow-safe range arithmetic;
- distinguish page-table pages from mapped leaf pages;
- provide rollback for partially constructed mappings;
- reject invalid or unaligned roots/addresses before dereference where the
  contract requires it;
- do not free a live process table;
- test allocation failure, exact unmap, process exit, and repeated cleanup;
- update this document and the risk register when mapping or ownership contracts
  change.
