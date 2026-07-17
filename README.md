# ArmoniOS

> A small freestanding AArch64 operating system with a graphical QEMU desktop, inspired by KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License/GPL%202.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch/AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang/C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status/QEMU%20desktop%20alpha-blue.svg)]()

## Project status

ArmoniOS is currently a **v0.9 QEMU desktop alpha**. It is a real bare-metal operating system, not a hosted application or Linux distribution, but it is not yet a stable v1.0 release candidate.

The current codebase includes:

- an AArch64 EL1 kernel and freestanding EL0 applications;
- per-process page tables, process dispatch, syscalls, IPC, and anonymous mappings;
- permission-aware kernel-to-user copy helpers with per-page PTE write checks;
- per-process VFS file descriptors with central cleanup on exit, fault, and kill;
- a kernel-owned window compositor with input, dragging, focus, backing buffers, and damage tracking;
- `panel`, `shell`, `editor`, `files`, `monitor`, and `clock` applications;
- bootfs, tmpfs, and a small FAT32 root-filesystem implementation;
- QEMU virtio block, GPU, input, and network paths;
- PCI/xHCI and basic USB HID support;
- native host tests for core kernel, VFS, filesystem, driver-parser, GUI, and ABI logic;
- an automated `tools/verify.sh` baseline that includes the EL0 user-copy permissions host and QEMU gates and the process-local VFS descriptor host gate.

Important limitations remain. In particular, the visible FAT/editor workflow has not been rerun on the current commit by a named tester, GitHub Actions is blocked before checkout, and Raspberry Pi support is not build- or hardware-verified.

Read these before making or evaluating claims:

- [Current State](docs/CURRENT_STATE.md) - audited operational truth and verification evidence
- [Technical Risks](docs/TECHNICAL_RISKS.md) - active blockers and correctness risks
- [Documentation Policy](docs/DOCUMENTATION_POLICY.md) - evidence and maintenance rules
- [Roadmap](docs/ROADMAP.md) - ordered release work

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

### Build and run the automated baseline

```bash
git clone https://github.com/roccolate/armonios
cd armonios
bash tools/verify.sh
```

The script records the current commit, runs the kernel build, binary-size gate, native host tests, process-local VFS descriptor host gate, the standalone user-copy permissions host gate, userland stack check, the FAT32 QEMU serial-marker storage smoke test, and the EL0 user-copy permissions QEMU regression. It stops on the first failure.

Equivalent individual commands are:

```bash
make
make size
make -C tests test
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
make stack-check
make qemu-fs-test
bash tools/qemu_usercopy_test.sh
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

### Run the user-copy permissions regression

```bash
bash tools/qemu_usercopy_test.sh
```

This QEMU gate boots a kernel whose EL0 crt0 probes a read-only image address through `sys_user_buf_out`, requires at least two `USERCOPY: RX output rejected` markers, and asserts that `panel: ready` and `clock: starting` still appear, proving the kernel rejected the invalid output buffer without halting. The serial log is written to `build-usercopy-test/qemu-usercopy-test.log`.

Other QEMU launch targets exist:

```bash
make qemu-fb
make qemu-usb
make qemu-net
```

At present these are runtime launch targets, not complete deterministic tests. A timeout alone is not proof that the associated subsystem passed.

## Current verified local baseline

The latest local verification recorded on commit `4494c554df212401ffbd294d1a44b5977696fa0b` reports:

```text
make                              passed
make size                         kernel.bin: 97344 bytes, limit 100000
make -C tests test                ALL TESTS PASSED (0)
tests/run_vfs_process_fd_test.sh  process-local VFS descriptors + exit cleanup PASS
tests/run_user_copy_permissions_test.sh  writable / ERR_PERM / mixed atomicity PASS
make stack-check                  maximum 368 bytes, limit 3072
make qemu-fs-test                 passed after direct QEMU serial-file capture
tools/qemu_usercopy_test.sh       6 EL0 probes rejected, panel: ready + clock: starting
```

Record the exact commit and serial log when promoting the baseline. The full visible files/editor/FAT workflow and deterministic framebuffer, USB, and network gates remain incomplete. See `docs/CURRENT_STATE.md` for the per-subsystem evidence and scope.

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

Output-producing syscalls route user-pointer writes through `kernel/syscall_helpers.c`, which validates every covered page of the destination against the process's own L3 page table and rejects non-writable ranges with `ERR_PERM` before any byte is copied. The VFS layer records an `owner_pid` per open file and routes every fd operation through that owner check; descriptors are reclaimed through `process_mark_exited`.

See [Architecture](docs/ARCHITECTURE.md), [Memory Map](docs/MEMORY_MAP.md), and [Syscalls](docs/SYSCALLS.md).

## Filesystem scope

The current FAT32 implementation supports:

- 512-byte sectors;
- short 8.3 names;
- root-directory files;
- create, read, write, rename, delete, and list;
- dynamic `/fat/<name>` VFS nodes.

It does not claim long-file-name support, directories, partition discovery, crash consistency, or broad FAT32 interoperability.

Per-process VFS file descriptors are now isolated and reclaimed on exit/fault/kill.

## Application format

Shipping applications are linked as flat KLI1 images with a fixed header and entry table. The current format is tested for the six included applications, but mutable static `.data` and `.bss` are not yet a defined application contract. Apps currently use restricted freestanding patterns and anonymous mappings for larger mutable state.

## Raspberry Pi status

Raspberry Pi 4 and 5 are planned targets only.

The repository contains an early `rpi4` board directory, linker script, mailbox scaffolding, and experimental eMMC code. This does not constitute support. The RPi4 backend has not been recorded as building and linking cleanly, has not reached a physical serial milestone, and its storage code must not be treated as validated.

Do not describe ArmoniOS as running on Raspberry Pi until the hardware criteria in `docs/DOCUMENTATION_POLICY.md` and `docs/PORTING.md` are satisfied.

## Release direction

The next release goal is a repeatable **v1.0 QEMU desktop release candidate**. The immediate work is correctness and reproducibility, not new multimedia or hardware scope.

Both syscall-boundary P0 risks (`RISK-001` and `RISK-002`) are closed on `4494c55`; see `docs/TECHNICAL_RISKS.md` for the recorded evidence.

Major remaining blockers include:

1. verification of the visible FAT attachment on the current commit (`RISK-003`);
2. verification of initial focus for spawned app windows (`RISK-004`);
3. deterministic framebuffer, USB, and network QEMU gates (`RISK-005`);
4. successful end-to-end files/editor/FAT verification;
5. restoring GitHub Actions runner execution and logs (`RISK-011`).

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
