# ArmoniOS

> A small freestanding AArch64 operating system with a graphical QEMU desktop,
> inspired by the compact, direct design of KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang-C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-v0.1%20QEMU%20baseline-blue.svg)]()

<p align="center">
  <img src="docs/assets/armin.png" alt="Armin, the ArmoniOS mascot" width="128">
</p>

## Preview

![ArmoniOS QEMU desktop preview](docs/assets/qemu-desktop-preview.png)

## Current status

ArmoniOS is a **real bare-metal AArch64 operating system** with a verified QEMU
`virt` desktop baseline. It is not a hosted Linux application and it is not a
Linux distribution.

The honest release classification is:

- **current public baseline:** v0.1 QEMU desktop;
- **current engineering phase:** v0.2 cleanup and runtime hardening;
- **product target:** v1.0 usable QEMU mini desktop;
- **hardware status:** Raspberry Pi scaffolding only, not hardware-supported.

The validated runtime tree from PR #41 was merged into `main` by `ea13f0e`.
GitHub Actions validated the exact PR head `fedb070` before merge:

- `Verify ArmoniOS` run `29824050151`: success;
- `CI - Tests` run `29824050165`: success.

The merge commit did not receive a separate workflow run. The evidence therefore
applies to the merged code tree, not to an independently rerun merge commit.

The active v0.2 candidate adds aggregate deferred-runtime telemetry using the
AArch64 generic counter. It measures requests, coalescing, requeues, pass count,
last/maximum/cumulative duration, and passes exceeding one timer interval. This
is measurement, not completion: work per pass remains unbounded and RISK-017
still blocks formal v0.2 promotion.

## What works now

The current QEMU codebase includes:

- AArch64 EL1 boot with DTB handoff;
- physical memory, page tables, heap allocation, and kernel W^X mappings;
- preemptive freestanding EL0 processes with private image, stack, anonymous
  mappings, saved trap frames, parent PID, wait, kill, and zombie cleanup;
- a small non-POSIX syscall ABI with permission-aware user-pointer validation;
- process-local VFS descriptors with cleanup on exit, fault, and kill;
- bootfs, tmpfs, and a writable root-only FAT32 bridge;
- a kernel-owned window compositor with focus, dragging, minimize/restore,
  backing buffers, damage tracking, input routing, and per-window event queues;
- `panel`, `shell`, `editor`, `files`, `monitor`, `control`, and `clock` EL0
  applications;
- QEMU virtio block, GPU, input, and network paths;
- PCI/xHCI enumeration and directly attached boot-protocol USB HID devices;
- Ethernet, ARP, IPv4, UDP, and DHCP sufficient for a QEMU user-network lease;
- one post-EOI deferred runtime service for timer-originated input, GUI, device,
  and network work, with kernel-internal aggregate timing telemetry;
- deterministic host, QEMU, size, stack, ABI, storage, GUI, USB, network, and
  board-build gates under `tools/verify.sh`.

## Important limits

ArmoniOS is an alpha-quality educational desktop kernel, not a production OS.
The most important current limits are:

| Area | Current limit |
|---|---|
| Runtime platform | QEMU `virt` is the only verified runtime target. |
| Physical memory | PMM manages at most 128 MiB. |
| Processes | Fixed table of 16 process slots. |
| User regions | Eight tracked regions per process. |
| VFS | 24 nodes, four mounts, eight descriptors per process, 64-byte paths. |
| FAT32 | Root directory only, short 8.3 names, no subdirectories or LFN. |
| Editor | 512-byte buffer and only the caret line is rendered. |
| Files | `/fat` only, at most eight displayed root entries. |
| GUI | 16 windows, 32 queued events per window, no shared widget toolkit. |
| Networking | No socket ABI, TCP, DNS API, or HTTP client. |
| USB | Direct keyboard/mouse HID only; no hub support claim. |
| Scheduling | EL0 is preemptive; EL1 helper threads are cooperative. |
| Runtime service | Runs after EOI but before exception return. Aggregate duration is measured, but input/device/network/redraw work has no per-pass budget. |
| User copy | Permission checked, but final copies are not fault-recoverable. |
| Raspberry Pi | Build/host verified scaffolding; no physical boot or storage claim. |

See [Current State](docs/CURRENT_STATE.md) and
[Technical Risks](docs/TECHNICAL_RISKS.md) for exact evidence and exit criteria.

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

### Build and run the verified baseline

```bash
git clone https://github.com/roccolate/armonios
cd armonios
bash tools/verify.sh
```

`tools/verify.sh` stops on the first failure and covers:

- QEMU and RPi4 build contracts;
- `.data == 0` and the 108000-byte kernel limit;
- RPi4 EMMC2 diagnostic, MBR, and partition-view host tests;
- the native host suite;
- deferred runtime-service EOI ordering, coalescing, deterministic duration,
  maximum/total tracking, overrun, requeue, snapshot, and reset behavior;
- parent/wait lifecycle, process-local FDs, user-copy permissions, and KLI1;
- userland stack usage;
- FAT32 QEMU smoke;
- usercopy and focus QEMU regressions;
- framebuffer, USB, and DHCP marker gates;
- visible-target FAT + GPU wiring.

Useful individual commands:

```bash
make BOARD=qemu_virt
make BOARD=qemu_virt size
bash tests/run_board_build_test.sh
make -C tests test
bash tests/run_runtime_service_test.sh
bash tests/run_process_parent_wait_test.sh
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

The target attaches the generated FAT32 image with GPU, USB keyboard, and mouse.
The latest recorded manual workflow is from 2026-07-17: Files listed `/fat`,
created an 8.3 file, opened Editor, typed and saved content, closed, renamed,
reopened with content intact, deleted, and refreshed without stale title-bar
artifacts. That evidence is intentionally separate from automated serial-marker
checks.

## Architecture summary

```text
boot/start.S
  -> early stack, vectors, BSS clear
  -> kernel_main
       -> board/UART and DTB memory discovery
       -> PMM, heap, VMM/MMU
       -> VFS, bootfs, tmpfs
       -> timer, IRQ, process dispatch, runtime-service state
       -> optional storage, display, network, input, PCI, USB
       -> launch panel in EL0
       -> cooperative EL1 helper scheduler

physical timer IRQ
  -> account and rearm
  -> publish coalescible periodic work
  -> scheduler accounting
  -> EOI
  -> read generic counter
  -> non-preemptible runtime-service pass
  -> read counter and update telemetry
  -> EL0 dispatch
  -> exception return
```

EOI releases the interrupt controller, but the runtime service still executes in
EL1 exception context. IRQs remain masked and EL0 remains paused until that pass,
process dispatch, and `eret` complete. Aggregate timing makes the duration
observable; it does not bound the pass. This distinction is tracked by
`RISK-017`.

## Storage and applications

The current FAT32 path supports 512-byte sectors, root-directory 8.3 files,
cluster-chain growth, create/read/write/list/rename/delete, and dynamic
`/fat/<name>` VFS nodes. It is deliberately not described as general FAT.

The applications are useful demonstrations, not complete daily tools:

- **Files:** root-only `/fat` browser and 8.3 create/rename/delete/open flow;
- **Editor:** small text buffer with caret movement and save, but only one line is
  rendered at a time;
- **Shell:** history, scrollback, `pwd`, `cd`, `ls`, `cat`, `run`, `kill`, `mem`,
  `ps`, and `ticks`;
- **Panel:** launcher, taskbar, focus, minimize, and restore integration;
- **Monitor, Control, Clock:** compact system and desktop demonstrations.

Issue #2 tracks **v0.6 useful desktop applications**. It depends on the v0.3
storage/path platform, v0.4 real FAT, and v0.5 shared userland runtime/widgets.
The project should not jump directly to broad app polish around temporary
root-only and fixed-buffer foundations.

## Raspberry Pi status

The `rpi4` backend and an opt-in read-only EMMC2 diagnostic image build and pass
host tests. The normal board advertises no storage, display, or input capability
that has not been proven on hardware.

ArmoniOS does **not** currently claim physical Raspberry Pi boot support, working
SD/eMMC on real hardware, Raspberry Pi framebuffer/input, or an installable
Raspberry Pi desktop image.

Physical evidence must follow [Porting](docs/PORTING.md) and the
[Documentation Policy](docs/DOCUMENTATION_POLICY.md).

## Documentation map

- [Current State](docs/CURRENT_STATE.md) — operational truth and verification record
- [Technical Risks](docs/TECHNICAL_RISKS.md) — active risks and exit criteria
- [Roadmap](docs/ROADMAP.md) — ordered work toward v1.0
- [Architecture](docs/ARCHITECTURE.md) — implemented kernel structure
- [Deferred Runtime Service](docs/RUNTIME_SERVICE.md) — timer/bottom-half contract
- [Memory Map](docs/MEMORY_MAP.md) — mappings and address-space policy
- [Syscalls](docs/SYSCALLS.md) — current non-POSIX ABI
- [GUI ABI Notes](docs/GUI_ABI_NOTES.md) — windows, events, and ownership
- [Documentation Policy](docs/DOCUMENTATION_POLICY.md) — evidence terminology
- [Study Guide](docs/STUDY_GUIDE.md) — educational reading path
- [Contributing](docs/CONTRIBUTING.md) — development workflow
- [Porting](docs/PORTING.md) — board-boundary and hardware evidence rules

## Road to v1.0

The next sequence is deliberate:

1. add per-class runtime metrics and explicit work budgets;
2. prove EL0 progress under sustained load and complete v0.2 evidence;
3. build the v0.3 block/VFS/path platform;
4. replace the root-only bridge with real FAT support;
5. add a shared userland runtime and widgets;
6. complete the v0.6 Files, Editor, Shell, Settings, Monitor, Panel, and Clock workflows;
7. mount ext2 read-only;
8. stabilize, fuzz, document, and record a final manual workflow.

See [Roadmap](docs/ROADMAP.md) for milestone gates and dependencies.

## Project structure

```text
boot/                 AArch64 entry and early setup
kernel/               kernel core, exceptions, processes, syscalls, VFS, GUI
kernel/mm/            PMM, VMM, and heap
kernel/sched/         cooperative EL1 helper-thread scheduler
drivers/              board boundary and device drivers
drivers/boards/       QEMU reference backend and experimental board ports
programs/apps/         freestanding KLI1 desktop applications
programs/libkarm/      syscall and small userland helpers
programs/libkarmdesk/  GUI wrappers
tests/                native host and contract tests
tools/                build, QEMU, and verification utilities
linker/               kernel linker scripts
docs/                 architecture, ABI, status, risks, and policy
```

## Design principles

- Keep the kernel small, explicit, and readable.
- Prefer direct C and narrow AArch64 assembly boundaries.
- Treat QEMU as the regression platform until hardware is proven.
- Separate implemented code from verified runtime behavior.
- Keep hard IRQ callbacks bounded.
- Add tests and documentation in the same change as public behavior.
- Port design ideas from classic small operating systems, not architecture-specific source code.

## License

ArmoniOS is licensed under [GNU GPL-2.0](LICENSE).
