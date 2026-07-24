# ArmoniOS

> A compact, freestanding AArch64 desktop operating system for QEMU, inspired by
> the direct and resource-conscious design of systems such as KolibriOS and
> MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/license-GPL--2.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/language-C11%20%2B%20AArch64%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-pre--release%20alpha-blue.svg)]()

<p align="center">
  <img src="docs/assets/armin.png" alt="Armin, the ArmoniOS mascot" width="128">
</p>

## What ArmoniOS is

ArmoniOS is a real bare-metal operating system. It boots its own AArch64 kernel,
enters EL1, creates freestanding EL0 processes, exposes an ArmoniOS-native syscall
ABI, and renders a graphical desktop without Linux, libc, POSIX, X11, Wayland, or
a hosted runtime underneath it.

The verified runtime target is QEMU `virt`. Raspberry Pi 4 support is a separate,
fail-closed hardware track and is not yet a physical-runtime claim.

![ArmoniOS QEMU desktop preview](docs/assets/qemu-desktop-preview.png)

## Current status

ArmoniOS is a pre-release alpha with several foundations already in place:

- the v0.1 QEMU desktop baseline is verified;
- the v0.2 cleanup and runtime-hardening implementation is complete;
- issue #76 still tracks the final visible validation, tag, and v0.2 release record;
- the v0.3 storage/VFS platform is in progress;
- the v0.5 userland-runtime foundation has started through `libkarm`;
- seven graphical and system applications run, but they remain compact demos
  rather than complete daily-use tools.

Do not describe the current tree as production-ready, POSIX-compatible, a general
FAT implementation, or a verified Raspberry Pi desktop.

## Implemented system

### Kernel and processes

- AArch64 EL1 boot with DTB handoff;
- physical and virtual memory management with 4 KiB page tables;
- kernel W^X and an empty loadable `.data` contract;
- preemptive EL0 processes with private images and stacks;
- anonymous mappings, parent/wait, exit, kill, zombies, and cleanup;
- permission-aware user-buffer validation and kernel-owned syscall payloads;
- IRQ-origin gating so EL1 interrupt frames cannot enter process preemption.

### Runtime and devices

- bounded post-EOI runtime service with count budgets and a cooperative deadline;
- virtio block, GPU, input, and network devices on QEMU;
- PCI/xHCI with directly attached boot-protocol keyboard and mouse;
- Ethernet, ARP, IPv4, UDP, and DHCP sufficient for the QEMU desktop path;
- deterministic host, build, QEMU, stress, ABI, stack, storage, GUI, USB, network,
  and board-contract gates.

### Storage and VFS

- process-local file descriptors;
- bootfs and tmpfs;
- generic block-device capacity, read-only, flush, and bounded-view contracts;
- whole-device and primary-MBR FAT32 discovery;
- canonical absolute paths and longest-prefix mount resolution;
- existing nested FAT32 8.3 directories can be traversed, listed, statted, and read;
- file creation, write, rename, and deletion remain limited to the FAT volume root;
- versioned structured metadata through `SYS_STAT_V2` and `SYS_READDIR_V2`;
- filesystem identity and capabilities through `SYS_FSINFO`.

Long names, truncate, directory creation/removal, nested mutation transactions,
and proven reboot durability are not implemented yet.

### Userland

The desktop currently includes:

- **Panel** — launcher, taskbar, focus, minimize, and restore;
- **Shell** — basic process, file, and system commands;
- **Editor** — small text editor with save support;
- **Files** — FAT browser and filesystem-information consumer;
- **Monitor** — process and memory information;
- **Control** — compact desktop settings editor;
- **Clock** — time display.

The public dependency direction is:

```text
applications -> libarmdesk -> libkarm -> public ABI -> kernel
console apps -------------> libkarm -> public ABI -> kernel
```

`libkarm` is built as a static archive and currently provides syscall wrappers,
small I/O/string helpers, monotonic arenas, growable binary buffers, dynamic
null-terminated strings, complete descriptor writes, and rollback-safe complete
file reads.

`libarmdesk` is the canonical desktop-facing layer. Its current foundation
contains typed GUI wrappers, rectangle/clipping helpers, and semantic theme
tokens. A reusable widget toolkit has not yet been promoted.

## Build and run

### Requirements

On Ubuntu or WSL2:

```sh
sudo apt update
sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make
```

### Verify everything

```sh
git clone https://github.com/roccolate/armonios.git
cd armonios
bash tools/verify.sh
```

### Run the serial target

```sh
make qemu
```

Exit QEMU with `Ctrl+A`, then `X`.

### Run the visible desktop

```sh
make qemu-fb-visible
```

The default production image budget is **128 KiB (131072 bytes)** and is enforced
by `make size` and the verification workflows.

## Repository map

```text
boot/                  AArch64 entry and early setup
drivers/               board and device drivers
include/armonios/abi/  public kernel/userland ABI
kernel/                exceptions, memory, processes, VFS, GUI, and runtime
programs/apps/          freestanding KLI1 applications
programs/libkarm/       GUI-independent userland runtime
programs/libarmdesk/    canonical desktop-facing userland layer
programs/libkarmdesk/   temporary compatibility include path
tests/                 host and contract tests
tools/                 verification, QEMU, diagnostics, and packaging
docs/                  architecture, state, roadmap, risks, and references
```

## Documentation

Start with [the documentation index](docs/README.md).

The canonical documents have distinct responsibilities:

- [Current State](docs/CURRENT_STATE.md) — what exists on `main` now;
- [Architecture](docs/ARCHITECTURE.md) — how the implemented system is organized;
- [Roadmap](docs/ROADMAP.md) — future milestones and their exit criteria;
- [Technical Risks](docs/TECHNICAL_RISKS.md) — active risks and closed-risk summary;
- [Development Guide](docs/DEVELOPMENT_GUIDE.md) — practical contribution workflow;
- [Syscall Reference](docs/SYSCALLS.md) — public syscall contracts;
- [v0.3 Implementation Status](docs/V03_IMPLEMENTATION_STATUS.md) — detailed
  storage/VFS checkpoint.

## Design principles

- Keep the kernel small, explicit, freestanding, and testable.
- Prefer direct C11 and narrow assembly boundaries.
- Treat QEMU as the regression platform until hardware is proven separately.
- Keep public ABI definitions outside kernel-private headers.
- Keep `libkarm` independent from desktop code.
- Use caller-owned, bounded state and fail closed on unsupported capabilities.
- Separate current implementation, future intent, risks, and historical evidence.
- Add tests and documentation in the same change that alters a contract.

## License

ArmoniOS is licensed under the [GNU General Public License v2.0](LICENSE).
