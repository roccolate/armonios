# Memory Map

This is the current fixed-address reference for KolibriARM. C-owned user layout
constants live in `kernel/layout.h`; kernel link addresses still live in
`linker.ld`.

## QEMU virt Physical Layout

The default QEMU target uses 128 MB of RAM. The kernel still runs from an
identity-mapped lower-half bootstrap layout.

| Region | Current use |
|--------|-------------|
| `0x00040000` area | QEMU-provided DTB / firmware data, discovered at boot |
| `0x40000000` | Kernel load/link base for QEMU `virt` |
| kernel image range | `.text`, `.rodata`, `.data`, `.bss`, embedded app blobs |
| PMM-managed pages | Kernel allocations, page tables, EL0 image/stack pages, mmap pages |

The exact free range comes from the DTB memory map and PMM reservations during
boot. Do not add new generic-kernel constants for board physical addresses.

## QEMU virt MMIO

Board-specific MMIO constants belong in `drivers/boards/qemu_virt/`, exposed to
generic code through `drivers/board.h`.

| Address | Device |
|---------|--------|
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
| `0x09000000` | PL011 UART0 |
| `0x0a000000` and following strides | virtio-mmio transports |

## Kernel Virtual Layout

Current implementation:

- kernel and board MMIO are identity-mapped in the active bootstrap tables;
- each EL0 process has its own `TTBR0_EL1` page table;
- EL1 mappings needed for exception return are installed in process tables.

Target direction remains a shared higher-half kernel table through `TTBR1_EL1`,
but the current code should be documented and tested as identity-mapped until
that cleanup lands.

## EL0 Fixed Layout

Defined by `kernel/layout.h` and `kernel/process.h`.

| Virtual range | Purpose |
|---------------|---------|
| `0x0000000000400000` + `slot * 0x10000` | Per-process KLI1 image slot |
| 8 KB per image slot | PMM-owned image pages |
| `0x0000000000800000` + `slot * 0x10000` | Per-process stack slot |
| 4 KB per stack slot | PMM-owned user stack |
| at least 4 KB beside each stack slot | Unmapped guard spacing asserted at build time |
| `0x0000000100000000` | Anonymous `sys_mmap` arena base |
| `0x0000000200000000` | Anonymous `sys_mmap` arena limit |

Static assertions require image and stack slot ranges not to overlap and require
fixed slots to stay below the mmap arena.

## User-Region Contract

Each process owns up to `PROCESS_MAX_USER_REGIONS` disjoint user regions. The
current limit is 8. Syscall user-pointer checks validate against the current
process only:

- image and stack regions are installed by the loader;
- anonymous mmap regions are installed by `sys_mmap`;
- owned physical pages are released by process cleanup;
- `size == 0` checks remain vacuously valid;
- pointers crossing a region boundary are rejected.

## Stack Layout

- Kernel code currently uses the boot/kernel stack and exception frames.
- EL0 stacks are 4 KB PMM-owned pages in the fixed stack slot range.
- Each fixed stack stride leaves at least one unmapped guard page of spacing.

## Page Tables

AArch64 4 KB granule, 4-level page tables, 48-bit virtual addresses:

```text
PGD[VA[47:39]] -> PUD
PUD[VA[38:30]] -> PMD
PMD[VA[29:21]] -> PTE
PTE[VA[20:12]] -> physical page + flags
VA[11:0]       -> page offset
```

See `ARCHITECTURE.md` for the broader VM model and `SYSCALLS.md` for the
user-pointer isolation contract.
