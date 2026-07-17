# Memory Map

This document records the current fixed-address and translation model. It describes implementation, not the desired hardened architecture.

Operational verification lives in `CURRENT_STATE.md`. Memory-related risks are tracked as `RISK-001` and `RISK-008` in `TECHNICAL_RISKS.md`.

## QEMU `virt` physical layout

The default QEMU target uses 128 MiB of RAM. The PMM is also compiled with a 128 MiB maximum, so memory beyond that limit is not currently managed even if a future board reports more.

| Region | Current use |
|---|---|
| QEMU-provided DTB area | Firmware/DTB data passed to the kernel and reserved at boot |
| `0x40080000` | Kernel load and link base for `qemu_virt` |
| Kernel image | `.text`, embedded application blobs, `.rodata`, `.data`, `.bss`, and bootstrap stack reservation |
| Remaining PMM pages | Page tables, process image/stack pages, anonymous mappings, heap arenas, and GUI backing buffers |

The exact RAM range comes from the DTB. Board physical addresses must remain in the board layer.

## Raspberry Pi 4 planned physical layout

The experimental RPi4 linker script uses `0x80000` as its kernel link address.

This address is a planning assumption only. There is no recorded clean RPi4 build, physical boot, or validated firmware configuration. The RPi4 memory map must not be treated as supported until the hardware criteria in `DOCUMENTATION_POLICY.md` are satisfied.

## QEMU MMIO

| Address/range | Device |
|---|---|
| `0x08000000` | GIC distributor |
| `0x08010000` | GIC CPU interface |
| `0x09000000` | PL011 UART0 |
| `0x0a000000` and following board strides | virtio-mmio transports |
| Board-owned ECAM/MMIO ranges | PCI discovery and xHCI BAR assignment |

The exact constants live in `drivers/boards/qemu_virt/`.

## Current EL1 virtual layout

The kernel currently uses an identity-mapped lower-half design.

For the bootstrap table and every process table:

- the detected RAM range is identity-mapped;
- that RAM mapping is EL1 read/write/execute;
- board MMIO is identity-mapped as device memory and execute-never;
- TTBR1 is disabled;
- TTBR0 contains both user mappings and the kernel identity map;
- a process switch changes TTBR0 and performs a global EL1 TLB invalidation.

EL0 cannot use the kernel mappings merely because they exist in TTBR0: kernel RAM entries are not marked user-accessible. Kernel W^X is now enforced (RISK-008, text RX, data+bss+stack RW+NX, MMIO device+NX, remaining RAM RW+NX) but the design still duplicates kernel mappings for every process and does not use ASIDs (future v1.1 work).

Target direction:

- shared kernel mappings through TTBR1;
- kernel text RX;
- kernel rodata R/NX;
- kernel data, heap, and stacks RW/NX;
- MMIO device RW/NX;
- TTBR0 containing user mappings only;
- ASIDs and scoped TLB invalidation.

This work is tracked by `RISK-008`.

## EL0 fixed layout

The values below come from `kernel/layout.h` and `kernel/process.h`.

| Virtual range | Purpose |
|---|---|
| `0x0000000000400000 + slot * 0x10000` | Per-process KLI1 image slot |
| 8 KiB image slot | PMM-owned image pages, currently mapped user read/execute |
| `0x0000000000800000 + slot * 0x10000` | Per-process stack slot |
| 4 KiB stack slot | PMM-owned user stack, mapped user read/write |
| spacing beside stack slot | At least one unmapped page asserted by layout constants |
| `0x0000000100000000` | Anonymous mapping arena base |
| `0x0000000200000000` | Anonymous mapping arena limit |

Static assertions require fixed image and stack slot ranges not to overlap and keep them below the anonymous mapping arena.

## Process user-region metadata

Each process owns up to eight registered user ranges. A range records:

- start;
- end;
- physical backing address when known;
- ownership flags used for cleanup.

The current region flags do **not** record effective read/write/execute permissions for syscall validation.

Consequences:

- pointers are checked against the current process only;
- crossing a region boundary is rejected;
- a zero-length query is accepted for a non-null process;
- image, stack, and anonymous ranges can all satisfy the same membership test;
- kernel-to-user syscalls do not currently prove that a destination is writable.

Do not describe `sys_user_buf_out()` as validating a writable buffer until `RISK-001` is closed.

## User mappings

### Image

KLI1 image pages are allocated per process and mapped with user read/execute permissions. The full fixed image slot is registered and owned by the process.

### Stack

Each process receives a 4 KiB user stack page. The address stride leaves unmapped spacing, but there is no dynamic stack growth.

### Anonymous mappings

`sys_mmap`:

- accepts only `hint == 0`;
- rounds size to pages;
- allocates contiguous physical pages;
- selects addresses monotonically inside the process's mapping arena;
- supports read, write, and execute flags at the PTE layer;
- records physical-page ownership for cleanup.

`sys_munmap` requires an exact mapping match.

## Page-table structure

AArch64 4 KiB granule, four translation levels, and a 48-bit virtual-address configuration:

```text
L0[VA 47:39] -> L1 table
L1[VA 38:30] -> L2 table
L2[VA 29:21] -> L3 table
L3[VA 20:12] -> 4 KiB page descriptor
VA[11:0]     -> page offset
```

The VMM allocates child tables lazily. Page-table teardown frees table pages recursively but leaves mapped leaf pages to their owning subsystem.

## Cleanup ownership

Process cleanup currently releases:

- owned image pages;
- owned stack pages;
- owned anonymous mapping pages;
- the process page-table hierarchy;
- process-owned GUI windows through the centralized exit path.

File descriptors are not part of process-owned cleanup because the VFS descriptor table is global. This is tracked by `RISK-002`.
