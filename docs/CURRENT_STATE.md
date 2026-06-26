# Current State

This is the short live snapshot of the QEMU desktop and its kernel support.
Historical cleanup details live in `docs/TECH_DEBT_REVIEW.md`.

## Boot And Processes

- The default board is QEMU `virt`.
- The kernel runs in EL1 with an identity-mapped kernel/MMIO bootstrap.
- EL0 apps run from per-process `TTBR0_EL1` page tables.
- Each process owns tracked user regions for image, stack, and mmap memory.
  `PROCESS_MAX_USER_REGIONS` is 8.
- App image and stack pages are allocated per spawn from PMM and released by
  process cleanup.
- The fixed user image/stack virtual layout is centralized in
  `kernel/layout.h`.

## Userland

- Shipping apps are C programs under `programs/apps/`: `panel`, `shell`,
  `editor`, `monitor`, and `clock`.
- Apps link against `programs/libkarm` and `programs/libkarmdesk`.
- App images use the KLI1 flat format and are embedded in bootfs.
- When `make qemu-blk` or `make qemu-fs-test` provides the generated FAT32
  virtio-blk disk, the kernel can select FAT32-backed app images through VFS.
- `make stack-check` measures per-function userland C stack usage.

## GUI

- The old `kernel/gui.c` monolith is gone.
- GUI code is split across `kernel/gui_events`, `gui_cursor`, `gui_input`,
  `gui_backing`, `gui_pool`, and `gui_compositor`.
- `kernel/gui.h` is the public aggregate header and no longer includes the
  framebuffer driver header directly.
- The compositor is still kernel-owned. There is no separate desktop server.
- Windows have per-process ownership, title bars, close/minimize/restore,
  focus, z-order, drag, per-window backing buffers, and damage rectangles.
- Cursor shape is arrow/hand, with per-window cursor-region registration for
  owner-drawn controls.

## Input And Drivers

- UART, virtio-input, and USB HID feed the common input queue.
- QEMU `virt` supports virtio-gpu, virtio-blk, virtio-net/DHCP, and xHCI USB
  HID on the development path.
- The network stack is hand-written under `kernel/net/`; DHCP option parsing
  has focused host tests.
- FAT32 parser/VFS behavior has host tests, and the storage integration path is
  covered by `make qemu-fs-test`.

## Syscalls And ABI

- Syscall numbers are pinned in `kernel/syscall_numbers.h` with
  `_Static_assert`s.
- `SYSCALLS.md` is the syscall reference.
- User-pointer validation funnels through `kernel/syscall_helpers.{c,h}`.
- Owner-only window syscall lookup funnels through shared syscall helpers.
- `tests/test_syscall_abi.c` and `tests/test_window_abi.c` pin the ABI details
  that apps depend on.

## Known Product Gaps

These are not technical-debt blockers; they are future product work:

- no TCP/HTTP client;
- no USB hub support;
- no automated framebuffer screenshot diff test;
- no real Raspberry Pi hardware boot yet;
- no audio or multimedia runtime yet;
- no SMP.
