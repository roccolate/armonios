# ArmoniOS

> A small freestanding AArch64 operating system with a graphical QEMU desktop, inspired by KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL%202.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang-C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-QEMU%20desktop%20alpha-blue.svg)]()

## Project status

ArmoniOS is currently a **v0.9 QEMU desktop alpha**. It is a real bare-metal operating system, not a hosted application or Linux distribution, but it is not yet a stable v1.0 release candidate.

The current codebase includes:

- an AArch64 EL1 kernel and freestanding EL0 applications;
- per-process page tables, process dispatch, syscalls, IPC, and anonymous mappings;
- a kernel-owned window compositor with input, dragging, focus, backing buffers, and damage tracking;
- `panel`, `shell`, `editor`, `files`, `monitor`, and `clock` applications;
- bootfs, tmpfs, and a small FAT32 root-filesystem implementation;
- QEMU virtio block, GPU, input, and network paths;
- PCI/xHCI and basic USB HID support;
- native host tests for core kernel, VFS, filesystem, driver-parser, GUI, and ABI logic.

Important limitations remain. In particular, user-output buffers on `main` are not yet checked for write permission, VFS file descriptors are global rather than process-owned, the repaired visible FAT/editor workflow has not yet been rerun, GitHub Actions is blocked before checkout, and Raspberry Pi support is not build- or hardware-verified.
Important limitations remain. In particular, user-output buffers are not yet checked for write permission, VFS file descriptors are global rather than process-owned, the visible desktop target does not currently attach its FAT disk, and Raspberry Pi support is not build- or hardware-verified.

Read these before making or evaluating claims:

- [Current State](docs/CURRENT_STATE.md) — audited operational truth and verification evidence
- [Technical Risks](docs/TECHNICAL_RISKS.md) — active blockers and correctness risks
- [Documentation Policy](docs/DOCUMENTATION_POLICY.md) — evidence and maintenance rules
- [Roadmap](docs/ROADMAP.md) — ordered release work

## What ArmoniOS is

ArmoniOS is a compact monolithic ARM64 kernel intended to remain understandable and direct. It does not provide libc, POSIX compatibility, dynamic linking, or a hosted runtime. Applications are freestanding KLI1 images that invoke a small syscall ABI through `svc #0`.

The primary supported development target is currently **QEMU `virt` with a Cortex-A72 CPU model**.

## Quick start

### Requirements

On Ubuntu or WSL2:

```bash
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make
```

### Run the automated local baseline
### Build

```bash
git clone https://github.com/roccolate/armonios
cd armonios
bash tools/verify.sh
```

The script records the current commit and runs the kernel build, binary-size gate, native host tests, userland stack check, and FAT32 QEMU serial-marker smoke test. It stops on the first failure.

Equivalent individual commands are:

```bash
make
make size
make -C tests test
make stack-check
make qemu-fs-test
```

### Run the serial target

```bash
make qemu
```

Exit QEMU with `Ctrl+A`, then `X`.

### Run the visible desktop

```bash
make qemu-fb-visible
```

This target now builds and attaches the generated FAT32 virtio block image together with GPU, USB keyboard, and mouse devices. The wiring change is implemented, but the complete create/edit/save/rename/reopen/delete workflow has not yet been rerun on the current commit. Treat the visible result as `UNVERIFIED` until a named tester records it in issue #1.

### Run the storage smoke test

```bash
make qemu-fs-test
```

This is the strongest current QEMU integration test because it captures guest serial output and checks explicit storage/FAT markers.

Other QEMU launch targets exist:

```bash
make qemu-fb
make qemu-usb
make qemu-net
```

At present these are runtime launch targets, not complete deterministic tests. A timeout alone is not proof that the associated subsystem passed.

## Current verified local baseline

The latest verification recorded in issue #1, before the visible-recovery merge, reports:
make
make size
make -C tests test
```

### Run the serial target

```bash
make qemu
```

Exit QEMU with `Ctrl+A`, then `X`.

### Run the visible desktop

```bash
make qemu-fb-visible
```

This target currently provides GPU and input devices. It does **not** yet attach the generated FAT32 block image, so the `files` application will not expose the documented FAT workflow until that build-target defect is fixed. See `RISK-003` in `docs/TECHNICAL_RISKS.md`.

### Run the storage smoke test

```bash
make qemu-fs-test
```

This is the strongest current QEMU integration test because it captures guest serial output and checks explicit storage/FAT markers.

Other QEMU launch targets exist:

```bash
make qemu-fb
make qemu-usb
make qemu-net
```

At present these are runtime launch targets, not complete deterministic tests. A timeout alone is not proof that the associated subsystem passed.

## Current verified local baseline

The latest verification recorded in issue #1 reports:

```text
make                  passed
make size             kernel.bin: 92696 bytes, limit 100000
make -C tests test    ALL TESTS PASSED (0)
make stack-check      maximum 368 bytes, limit 3072
make qemu-fs-test     passed after direct QEMU serial-file capture
```

Those results are historical evidence, not proof for the current `main`. Run `bash tools/verify.sh` and record its exact commit before promoting the current baseline. The full visible files/editor/FAT workflow and deterministic framebuffer, USB, and network gates remain incomplete. See `docs/CURRENT_STATE.md` for exact evidence and scope.
The full visible files/editor/FAT workflow is still incomplete and the remaining QEMU targets do not yet have deterministic pass records. See `docs/CURRENT_STATE.md` for exact evidence and scope.

## Architecture summary

```text
boot/start.S
  -> early stack, exception vectors, BSS clear
  -> kernel_main
       -> board/UART and DTB memory discovery
       -> PMM, heap, VMM/MMU
       -> VFS, bootfs, tmpfs
       -> IRQ, timer, process dispatch
       -> optional storage, display, network, input, USB
       -> launch panel in EL0
       -> cooperative EL1 helper scheduler
```

EL0 processes are preempted through IRQ trap frames. EL1 helper threads are currently cooperative.

Each process has its own TTBR0 root, image, stack, and anonymous mappings. The current process tables also identity-map the full detected RAM range for EL1 as read/write/execute. A future hardening step should move shared kernel mappings to TTBR1 and apply section-specific permissions.

See [Architecture](docs/ARCHITECTURE.md) and [Memory Map](docs/MEMORY_MAP.md).

## Filesystem scope

The current FAT32 implementation supports:

- 512-byte sectors;
- short 8.3 names;
- root-directory files;
- create, read, write, rename, delete, and list;
- dynamic `/fat/<name>` VFS nodes.

It does not claim long-file-name support, directories, partition discovery, crash consistency, or broad FAT32 interoperability.

The current VFS has eight global file descriptors shared by the kernel. They are not yet isolated per process. See `RISK-002`.

## Application format

Shipping applications are linked as flat KLI1 images with a fixed header and entry table. The current format is tested for the six included applications, but mutable static `.data` and `.bss` are not yet a defined application contract. Apps currently use restricted freestanding patterns and anonymous mappings for larger mutable state.

## Raspberry Pi status

Raspberry Pi 4 and 5 are planned targets only.

The repository contains an early `rpi4` board directory, linker script, mailbox scaffolding, and experimental eMMC code. This does not constitute support. The RPi4 backend has not been recorded as building and linking cleanly, has not reached a physical serial milestone, and its storage code must not be treated as validated.

Do not describe ArmoniOS as running on Raspberry Pi until the hardware criteria in `docs/DOCUMENTATION_POLICY.md` and `docs/PORTING.md` are satisfied.

## Release direction

The next release goal is a repeatable **v1.0 QEMU desktop release candidate**. The immediate work is correctness and reproducibility, not new multimedia or hardware scope.

Major blockers include:

1. permission-aware user-copy helpers and a QEMU negative regression;
2. process-owned file descriptors and exit cleanup;
3. verification of the visible FAT attachment on the current commit;
4. verification of initial focus for spawned app windows;
5. deterministic framebuffer, USB, and network QEMU gates;
6. successful end-to-end files/editor/FAT verification;
7. restoring GitHub Actions runner execution and logs.
1. permission-aware user-copy helpers;
2. process-owned file descriptors and exit cleanup;
3. a visible desktop target with FAT storage;
4. correct initial window focus for spawned apps;
5. deterministic framebuffer, USB, and network QEMU gates;
6. successful end-to-end files/editor/FAT verification.

See [Roadmap](docs/ROADMAP.md).

## Project structure

```text
boot/                 AArch64 entry and early setup
kernel/               kernel core, memory, processes, syscalls, VFS, GUI
kernel/mm/            PMM, VMM, heap
kernel/sched/         cooperative EL1 helper-thread scheduler
drivers/              board boundary and device drivers
drivers/boards/       QEMU reference board and experimental board ports
programs/apps/         freestanding desktop applications
programs/libkarm/      syscall and small userland runtime helpers
programs/libkarmdesk/  GUI wrappers
tests/                native host test suite
tools/                host build and verification utilities
tools/                host build utilities
linker/               kernel linker scripts
docs/                 architecture, ABI, status, risks, and maintenance policy
```

## Design principles

- Keep the kernel small and readable.
- Prefer explicit C and narrow AArch64 assembly boundaries.
- Treat QEMU as the regression platform until hardware is proven.
- Do not confuse implemented code with verified behavior.
- Add tests and update documentation in the same change as public behavior.
- Port ideas from classic small operating systems, not architecture-specific source code.

## License

ArmoniOS is licensed under [GNU GPL-2.0](LICENSE).
