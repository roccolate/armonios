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
- `panel`, `shell`, `editor`, `files`, `monitor`, `control`, and `clock` applications;
- bootfs, tmpfs, and a small FAT32 root-filesystem implementation;
- QEMU virtio block, GPU, input, and network paths;
- PCI/xHCI and basic USB HID support;
- native host tests for core kernel, VFS, filesystem, driver-parser, GUI, and ABI logic;
- an automated `tools/verify.sh` baseline that includes the EL0 user-copy permissions host and QEMU gates, the process-local VFS descriptor host gate, the KLI1 mutable-storage contract gate, the BOARD=rpi4 build-contract gate, and the GUI focus-syscall QEMU gate.

Important limitations remain. In particular, the updated GitHub Actions baseline still needs a successful hosted run with logs, Raspberry Pi support is not hardware-verified, and Editor currently appears to show one visible text line in the manual desktop workflow.

Read these before making or evaluating claims:

- [Current State](docs/CURRENT_STATE.md) - audited operational truth and verification evidence
- [Technical Risks](docs/TECHNICAL_RISKS.md) - active blockers and correctness risks
- [Documentation Policy](docs/DOCUMENTATION_POLICY.md) - evidence and maintenance rules
- [Study Guide](docs/STUDY_GUIDE.md) - suggested reading path for the educational core
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

The script records the current commit, runs the kernel build, binary-size gate, the BOARD=rpi4 build-contract gate, native host tests, process-local VFS descriptor host gate, the standalone user-copy permissions host gate, the KLI1 mutable-storage contract gate, userland stack check, the FAT32 QEMU serial-marker storage smoke test, the EL0 user-copy permissions QEMU regression, the GUI focus-syscall QEMU gate, the per-subsystem marker gates (framebuffer, USB, network) under `tools/qemu_marker_test.sh all`, and the visible-desktop FAT+GPU wiring gate. It stops on the first failure.

Equivalent individual commands are:

```bash
make BOARD=qemu_virt
make BOARD=qemu_virt size
bash tests/run_board_build_test.sh
make -C tests test
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tests/run_kli1_contract_test.sh
make stack-check
make qemu-fs-test
bash tools/qemu_usercopy_test.sh
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_fb_fat_test.sh
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

This target builds and attaches the generated FAT32 virtio block image together with GPU, USB keyboard, and mouse devices. rocco manually verified the complete create/edit/save/rename/reopen/delete workflow on 2026-07-17 against working-tree baseline `8c8400bcddd754d879e6e21b787b8d028a6c6036`.

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

At present these are runtime launch targets. Use `bash tools/qemu_marker_test.sh all` or `bash tools/verify.sh` when you need deterministic pass/fail evidence and captured serial logs.

## Current Verified Local Baseline

The latest local verification was run on 2026-07-17 against the working tree
based on `8c8400bcddd754d879e6e21b787b8d028a6c6036`:

```text
make                              passed
make size                         kernel.bin: 106524 bytes, limit 108000
make -C tests test                ALL TESTS PASSED (0)
tests/run_vfs_process_fd_test.sh  process-local VFS descriptors + exit cleanup PASS
tests/run_user_copy_permissions_test.sh  writable / ERR_PERM / mixed atomicity PASS
make stack-check                  maximum 368 bytes, limit 3072
make qemu-fs-test                 storage: initialized + FAT32: mounted + /fat mounted
tools/qemu_usercopy_test.sh       7 EL0 probes rejected, panel: ready + clock: starting
tools/qemu_focus_test.sh          6 focus transitions across 6 distinct windows
tools/qemu_marker_test.sh all     fb (VIRTIO gpu + panel: ready)
                                  usb (controller + enumeration + 2 HID devices)
                                  net (network: initialized + DHCP ack)
tools/qemu_fb_fat_test.sh         FAT32 + GPU + panel: ready in one boot
```

Record the exact commit and serial log when promoting the baseline. The visible
Files/Editor/FAT workflow is manually verified on the current baseline, with a
known Editor single-visible-line polish note. See `docs/CURRENT_STATE.md` for
the per-subsystem evidence and scope.

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

Shipping applications are linked as flat KLI1 images with a fixed header and entry table. The current format is tested for the seven included applications and explicitly forbids mutable static `.data` / `.bss`: `programs/apps/image.ld` `ASSERT`s both sections are empty, and `tests/run_kli1_contract_test.sh` confirms the seven shipping ELFs link clean and that a regression `.bss` source is rejected with a clear `KLI1 forbids .bss` message. Apps that need mutable state obtain it through `SYS_MMAP` at runtime.

## Raspberry Pi status

Raspberry Pi 4 and 5 are planned targets only.

The `rpi4` board backend now compiles and links cleanly under the v1.0 size gate via `tests/run_board_build_test.sh` (the `board-rpi4` gate in `tools/verify.sh`). virtio-input is wired with explicit safe-failure stubs because the hardware-track bring-up does not expose a virtio-input device yet; the kernel still works with serial input alone.

The repository does **not** claim that ArmoniOS boots on physical Raspberry Pi hardware. The eMMC driver remains experimental scaffolding and the storage milestone (RISK-007) still requires a physical serial capture and sector read on real hardware per `docs/DOCUMENTATION_POLICY.md` and `docs/PORTING.md`. Do not describe ArmoniOS as running on Raspberry Pi until those hardware criteria are satisfied.

## Release direction

The next release goal is a repeatable **v1.0 QEMU desktop release candidate**. The immediate work is correctness and reproducibility, not new multimedia or hardware scope.

Both syscall-boundary P0 risks (`RISK-001` and `RISK-002`) are closed; the deterministic QEMU gate scaffold (`RISK-005`) is covered by the current automated baseline; the visible-desktop FAT workflow and editor focus risks (`RISK-003` and `RISK-004`) are closed with manual evidence. See `docs/TECHNICAL_RISKS.md` for the recorded evidence.

The remaining v1.0 blocker is recording a successful GitHub Actions run of the updated baseline with QEMU serial-log artifacts (`RISK-011`).

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
