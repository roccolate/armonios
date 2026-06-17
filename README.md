# KolibriARM

> A minimal, fast, assembly-first operating system for AArch64 — inspired by KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang-C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-pre--alpha-red.svg)]()

---

## What is KolibriARM?

KolibriARM is a bare-metal operating system for ARM64 (AArch64) processors, written entirely in C and AArch64 assembly. It takes direct inspiration from [KolibriOS](https://kolibrios.org) and [MenuetOS](https://www.menuetos.net/) — two x86 operating systems celebrated for their extreme compactness, speed, and elegance — and brings those principles to modern ARM hardware.

**Core philosophy:**
- Every byte is intentional. No unnecessary abstraction layers.
- The kernel fits in your head. Small enough to read in a weekend.
- No libc. No POSIX. No Linux compatibility layer. Just clean system calls.
- Boots in under 3 seconds on real hardware.
- Runs comfortably on 64 MB of RAM.

---

## Current Status

> **Pre-alpha.** The kernel boots on QEMU `virt`, brings up memory management, enables an identity-mapped MMU, runs EL0 demo processes with syscall and timer-IRQ context switches, and drops into a minimal EL0 shell. One EL0 demo intentionally faults to verify that the kernel can terminate a bad user process and continue scheduling. With `make qemu-blk`, QEMU boots with a generated FAT32 virtio-blk image and reloads the EL0 demo through the VFS path.

| Component         | Status       | Notes                                  |
|-------------------|-------------|----------------------------------------|
| Bootloader        | Working      | AArch64 ASM, QEMU virt tested          |
| Physical memory   | Working      | Bitmap allocator, host tests           |
| Virtual memory    | Working      | Identity map, user flags, unmap tested |
| Scheduler         | Early        | Kernel threads plus EL0 timer preemption |
| IRQ dispatch      | Early        | GICv2, timer PPI, C handler table      |
| UART driver       | Working      | PL011 TX polling, RX IRQ ring, QEMU console input echo |
| Syscalls          | Early        | `svc #0`, process, memory, UART/stdin, VFS read/write/seek, and fixed-message IPC calls |
| Userland          | Early        | EL0 demos plus a shell, line editor for `/fat/edit.txt`, and process monitor |
| Framebuffer       | Early        | Linear primitives plus virtio-gpu demo windows |
| Storage           | Early        | virtio-blk MMIO probe and FAT32 image smoke |
| Filesystem        | Early        | Fixed VFS, bootfs seed, tmpfs foundation, FAT32 root 8.3 lookup/listing plus limited overwrite of existing files |
| GUI               | Early        | Kernel-managed demo windows, bitmap text, focused key delivery |
| Networking        | Planned      | lwIP integration                       |

## Current Milestone

The current milestone is the Phase 5 GUI foundation.

Initial scope:
- [x] Add a syscall path for `svc #0` with syscall number in `x8`.
- [x] Implement `sys_write`, `sys_exit`, `sys_yield`, and `sys_getpid` first.
- [x] Build an initial EL0 context with a user stack and `eret` into an embedded hello program.
- [x] Print `Hello from EL0` through `sys_write`, then return to the kernel through `sys_exit`.
- [x] Move the current `kernel_main()` smoke tests behind smaller debug/demo helpers.
- [x] Track the embedded demo with initial process-owned metadata.
- [x] Run multiple embedded EL0 processes and preempt one with the timer IRQ.
- [x] Convert a lower-EL memory fault into process exit while continuing to schedule.
- [x] Route embedded EL0 programs through a tiny loader-owned image descriptor.
- [x] Copy the embedded EL0 blob into loader-owned executable slots before entering user mode.
- [x] Move the EL0 demo payload into `programs/` while keeping the kernel EL0 transition code separate.
- [x] Add a tiny flat user-image header with image size and entry offsets.
- [x] Build the EL0 demo as `build/programs/user_demo.bin` and embed that serialized image as a kernel blob.
- [x] Add bootfs and VFS paths for named loader-owned program images.
- [x] Load and execute the demo from a generated FAT32 virtio-blk image.
- [x] Exchange a fixed-size IPC message between two EL0 processes.
- [x] Render two overlapping GUI windows through `virtio-gpu`.
- [x] Render built-in bitmap text in GUI windows.
- [x] Route early UART keyboard characters to the focused demo window.

Out of scope for this milestone:
- Porting KolibriOS applications directly.
- Full POSIX-like process/userland APIs.
- Per-process userland GUI ownership and full application windows.

---

## Target Hardware

**Primary target:** QEMU `virt` machine (AArch64, Cortex-A72)
**Secondary target:** Raspberry Pi 4 / 5 (BCM2711 / BCM2712)
**Future targets:** Any Cortex-A board with open peripheral documentation

---

## Building

### Requirements

- WSL2 (Ubuntu 22.04+) or any Linux system
- `gcc-aarch64-linux-gnu` — AArch64 cross-compiler
- `qemu-system-aarch64` — ARM emulator
- `gdb-multiarch` — debugger
- `make`

### Install dependencies (Ubuntu / WSL2)

```bash
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make
```

### Build and run

```bash
# Clone the repository
git clone https://github.com/yourname/kolibriarm
cd kolibriarm

# Build the default QEMU virt board
make

# Equivalent explicit board selection
make BOARD=qemu_virt

# Run in QEMU
make qemu

# At the EL0 shell prompt, try:
# help
# ls
# ls /fat
# ps
# ticks
# pwd
# cd /
# mem
# cat /tmp/note
# cat /fat/edit.txt
# run hello
# run loop
# edit /fat/edit.txt
# monitor
# kill last
# exit

# Run in QEMU with a generated FAT32 virtio-blk image
make qemu-blk

# Run in QEMU with virtio-gpu headless and draw the GUI window demo
make qemu-fb

# Run in QEMU with a visible virtio-gpu window
make qemu-fb-visible

# Exit QEMU: Ctrl+A then X
```

### Debug with GDB

```bash
# Terminal 1: launch QEMU in debug mode (CPU paused)
make qemu-debug

# Terminal 2: attach GDB
gdb-multiarch build/kernel.elf
(gdb) target remote :1234
(gdb) break kernel_main
(gdb) continue
```

---

## Project Structure

```
kolibriarm/
├── boot/               # Bootloader (AArch64 ASM only)
│   └── start.S         # Entry point, MMU init, jump to kernel
├── kernel/             # Kernel core (C + inline ASM)
│   ├── kernel.c        # kernel_main, early init
│   ├── mm/             # Memory management
│   ├── sched/          # Scheduler
│   ├── ipc.c           # Fixed-message IPC queue
│   └── gui.c           # Early kernel-managed window compositor
├── drivers/            # Hardware drivers (C + minimal ASM)
│   ├── board.h         # Generic board/platform interface
│   ├── boards/         # Board/platform glue, starting with qemu_virt
│   ├── uart/           # PL011 UART
│   ├── fb/             # Linear framebuffer primitives
│   ├── irq/            # GICv2 interrupt controller
│   ├── usb/            # USB HID (keyboard, mouse)
│   └── net/            # Ethernet driver
├── programs/           # Userland applications
├── docs/               # Documentation
│   ├── ARCHITECTURE.md
│   ├── SYSCALLS.md
│   ├── MEMORY_MAP.md
│   └── PORTING.md
├── linker.ld           # Linker script
├── Makefile            # Build system
├── ROADMAP.md          # Development roadmap
├── CONTRIBUTING.md     # Contribution guidelines
└── LICENSE             # GPL-2.0
```

---

## Design Decisions

### Why AArch64?

AArch64 (ARM64) is the cleanest ISA to write an OS in from scratch:
- 31 general-purpose registers (no x86 legacy baggage)
- Clean, orthogonal instruction set
- Well-documented exception model (EL0/EL1/EL2)
- MMU with 4-level page tables, standard and predictable
- No real mode, no A20 gate, no interrupt legacy hell

### Why no POSIX?

POSIX compatibility layers add complexity without adding value for a purpose-built OS. KolibriARM defines its own minimal syscall ABI (inspired by KolibriOS's ~100-syscall design), using the `svc` instruction with function number in `x8` and arguments in `x0`–`x7`. This keeps the kernel small and the syscall path fast.

### Why C + ASM and not Rust?

The goal is to keep the codebase readable to anyone who knows C and assembly. Rust would add compile-time safety guarantees but also toolchain complexity, longer onboarding, and a runtime (even `no_std` has one). The entire kernel is meant to be understandable — Rust's borrow checker, while valuable, works against that goal at this stage.

---

## License

KolibriARM is licensed under the [GNU General Public License v2.0](LICENSE), the same license as KolibriOS.

---

## Acknowledgements

- [KolibriOS](https://kolibrios.org) — direct inspiration for philosophy and design
- [MenuetOS](https://www.menuetos.net/) — the original single-file OS concept
- [Raspberry Pi bare metal examples](https://github.com/isometimes/rpi4-osdev) — hardware reference
- [OSDev Wiki](https://wiki.osdev.org) — the indispensable community reference
