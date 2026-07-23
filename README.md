# ArmoniOS

> **Implementation update — 2026-07-23:** The older audit sections in this document predate merged v0.3 PRs #80, #81, #82, #90, and #93. Use `V03_IMPLEMENTATION_STATUS.md` for the current storage/VFS checkpoint. Issue #63 is closed; issue #76 remains the manual v0.2 validation and release-record task.

> A small freestanding AArch64 operating system with a graphical QEMU desktop,
> inspired by the compact, direct design of KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang-C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-v0.2%20promotion%20candidate-blue.svg)]()

<p align="center">
  <img src="docs/assets/armin.png" alt="Armin, the ArmoniOS mascot" width="128">
</p>

## Preview

![ArmoniOS QEMU desktop preview](docs/assets/qemu-desktop-preview.png)

## Project status

ArmoniOS is a **real freestanding bare-metal AArch64 operating system**. It is not
a hosted Linux application or distribution.

- **verified public baseline:** v0.1 QEMU desktop;
- **current phase:** v0.2 promotion candidate after cleanup and runtime hardening;
- **formal v0.2 blockers:** residual-risk disposition, issue #63, a final dated
  visible workflow, and the tag/release record;
- **current architecture work:** v0.3 storage/VFS foundations are partly landed; structured metadata is active in PR #95;
- **v1 target:** a small usable QEMU desktop;
- **physical hardware:** Raspberry Pi 4 build/host scaffolding only.

The current tree must not yet be described as a release candidate. The project
policy reserves that term for a tree whose affected P1 risks are closed or
explicitly accepted and whose required manual evidence is recorded.

Read these first:

1. [Current State](docs/CURRENT_STATE.md) — what is actually promoted and verified;
2. [Technical Risks](docs/TECHNICAL_RISKS.md) — open correctness and release risks;
3. [Roadmap](docs/ROADMAP.md) — milestone order and exit criteria;
4. [Development Guide](docs/DEVELOPMENT_GUIDE.md) — codebase map and practical work workflow;
5. [Architecture](docs/ARCHITECTURE.md) — implemented design;
6. [v0.3 Implementation Status](docs/V03_IMPLEMENTATION_STATUS.md) — current storage/VFS checkpoint.

## What works

The verified QEMU tree includes:

- AArch64 EL1 entry with DTB handoff;
- fixed 128 MiB PMM, heap, 4 KiB page tables, MMU, and kernel W^X;
- preemptive freestanding EL0 processes;
- private images, stacks, anonymous mappings, parent/wait, kill, exit, zombie, and
  cleanup behavior;
- a small append-only non-POSIX syscall ABI;
- permission-aware user pointers and kernel-owned syscall payloads;
- process-local VFS descriptors;
- bootfs, tmpfs, and a writable root-only FAT32 8.3 bridge;
- a kernel compositor/window manager with backing buffers, damage, focus,
  dragging, minimize/restore, and event queues;
- Panel, Shell, Editor, Files, Monitor, Control, and Clock;
- QEMU virtio block, GPU, input, and network;
- PCI/xHCI with directly attached boot-protocol keyboard and mouse;
- Ethernet, ARP, IPv4, UDP, and DHCP sufficient for a QEMU lease;
- a count- and time-bounded post-EOI runtime service;
- deterministic host, build, QEMU, stress, size, stack, ABI, storage, GUI, USB,
  network, and board-contract gates.

## Runtime hardening

Timer-originated desktop and network work is centralized after EOI but before
process dispatch and `eret`:

```text
physical timer IRQ
  -> fixed account/rearm/publish callback
  -> interrupt-controller EOI
  -> measured post-EOI EL1 runtime pass
       -> deadline = start + one nominal timer interval
       -> PERIODIC / INPUT phase
       -> NETWORK phase if time remains
       -> on expiry: count once, republish original work, skip later work
  -> process dispatch
  -> eret
```

EOI is not exception return. EL0 remains paused during the service pass, so both
count and time bounds matter.

### Enforced bounds

| Work class | Current rule |
|---|---:|
| Whole service | One nominal timer interval, checked at safe boundaries |
| Virtio-input producer | At most one negotiated ring length and no more than 16 descriptors/call |
| USB HID producer | At most four fixed device visits/call |
| Shared input consumer | At most 16 queued events/active pass |
| Partial compositor damage | At most eight ordered rectangles/successful submission |
| Virtio-net RX | At most 16 valid frames/active NETWORK pass |

Network polling and descriptor receive consume nothing outside the NETWORK phase.
Native continuation remains in device rings, the input queue, or the compositor
damage list.

The deadline is cooperative, not asynchronous preemption. One already-started
full redraw or driver call may cross the nominal interval before the next
checkpoint.

### Automated stress evidence

A deterministic stress image forces deadline expirations and proves repeated EL0
heartbeats while input, redraw, DHCP, and network activity occur.

A separate natural-deadline image saturates software-visible virtio-net RX while
input and redraw continue:

```text
EL0 yields:                       38,912
input events consumed:                 16
redraw submissions:                   738
virtio-net frames consumed:        29,234
maximum frames/pass:                   16
network cap exhaustions:             1,827
runtime requeues:                    1,827
natural deadline overruns:               0
maximum duration:                  385,763 / 625,000 ticks (61.7%)
observable input overflow:               0
panic markers:                           0
```

Evidence provenance:

- original GitHub PR #62 merge metadata:
  `7ea3d309047659c8bbe9c601c3d98217bcaafb02`;
- current-main runtime replay commit:
  `d5c104a0badc3a2d553516159b2b745737dd252f`;
- final PR #62 head:
  `04f65776d1bbe07545113652342c32f2448bfc7b`;
- original final PR validation:
  - `Verify ArmoniOS` `29896952424` (#295): success;
  - `CI - Tests` `29896952435` (#435): success;
- production kernel: **107918 / 108000 bytes**;
- remaining margin: **82 bytes**.

The repository history was replayed after the original PR merge. `CURRENT_STATE.md`
records the current audited `main` tree and current-tree workflow evidence.

Consumed-frame metrics prove progress after frames reach software. The current
virtio-net interface does not expose trustworthy device/ring-drop telemetry.

## Open v0.2 work

### Runtime residuals

`RISK-017` requires explicit release decisions about:

- missing device-level RX-drop telemetry;
- the one-operation full-redraw boundary;
- final visible evidence on the promotion tree.

### Intermittent VMM fault

One earlier FAT32 smoke run suffered an intermittent EL1 data abort resolved to
the `table[index]` load in `next_table()`. Subsequent runs passed, but a green
rerun is not a root-cause explanation.

Issue #63 / `RISK-018` tracks the investigation. Draft PR #64 is developing a
repeated production FAT32 soak and live GDB capture. That branch is investigation
evidence until it is finalized, merged, and promoted into the canonical state.

## Important limits

| Area | Current limit |
|---|---|
| Runtime platform | QEMU `virt` is the only verified runtime target. |
| Production kernel | 108000-byte ceiling; current audited image 107918 bytes. |
| Physical memory | PMM manages at most 128 MiB. |
| Processes | 16 slots and eight user regions/process. |
| VFS | 24 nodes, four mounts, eight descriptors/process, 64-byte paths. |
| FAT32 | Existing nested 8.3 trees are readable/listable; creation, deletion, and rename remain root-only; no long names. |
| Editor | 512-byte buffer and caret-line viewport. |
| Files | `/fat` only; at most eight displayed root entries. |
| GUI | 16 windows, 32 events/window, 32 damage rectangles, eight partial rectangles/submission. |
| Input | 64-event shared queue; overflow counted but not prevented. |
| USB HID | Four direct devices; no hubs. |
| Network | No socket ABI, TCP, DNS API, HTTP, or general user UDP API. |
| User copy | Permission-aware but not fault-recoverable. |
| Raspberry Pi | Build/host scaffolding only; no physical support claim. |

## Quick start

### Requirements

On Ubuntu or WSL2:

```sh
sudo apt update && sudo apt install -y \
  qemu-system-arm \
  gcc-aarch64-linux-gnu \
  binutils-aarch64-linux-gnu \
  gdb-multiarch \
  make
```

### Build and verify

```sh
git clone https://github.com/roccolate/armonios
cd armonios
bash tools/verify.sh
```

The complete gate covers builds, `.data`, size, RPi4 fail-closed contracts, host
suites, runtime counts/routing/deadline, process and ABI contracts, stack, FAT32,
user-copy/focus, devices, both runtime stress modes, and FAT + GPU wiring.

Useful focused commands:

```sh
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
bash tests/run_runtime_service_test.sh
bash tests/run_input_queue_stats_test.sh
bash tests/run_process_parent_wait_test.sh
bash tests/run_vfs_process_fd_test.sh
bash tests/run_user_copy_permissions_test.sh
bash tests/run_kli1_contract_test.sh
make stack-check
make qemu-fs-test
bash tools/qemu_usercopy_test.sh
bash tools/qemu_focus_test.sh
bash tools/qemu_marker_test.sh all
bash tools/qemu_runtime_stress_test.sh
bash tools/qemu_runtime_net_stress_test.sh
bash tools/qemu_fb_fat_test.sh
```

### Run QEMU

Serial target:

```sh
make qemu
```

Exit with `Ctrl+A`, then `X`.

Visible desktop with FAT32, GPU, keyboard, and mouse:

```sh
make qemu-fb-visible
```

Visible use is separate manual evidence. The last recorded full FAT workflow was
performed by Rocco on 2026-07-17. A new dated pass is required on the final v0.2
promotion tree.

## Storage and applications

The current FAT32 path supports 512-byte sectors, whole-device or primary-MBR
mounting, bounded nested 8.3 traversal, and root-level create/write/rename/delete.
Nested mutation, long names, and durable reboot semantics remain incomplete; it is
still not a general FAT implementation.

Current applications are real demonstrations, not complete daily tools:

- **Files:** root-only `/fat` browser and basic 8.3 operations;
- **Editor:** small buffer, caret editing, save, and one-line viewport;
- **Shell:** history, scrollback, and basic process/file/system commands;
- **Panel:** launcher, taskbar, focus, minimize, and restore;
- **Monitor, Control, Clock:** compact system/desktop demonstrations.

Issue #2 tracks v0.6 useful applications after v0.3 storage/VFS, v0.4 real FAT,
and v0.5 shared userland runtime/widgets.

## Raspberry Pi

The RPi4 backend and opt-in read-only EMMC2 diagnostic package build and pass host
contracts. Normal unsupported capabilities fail closed.

ArmoniOS does not claim physical Raspberry Pi boot, storage, framebuffer, input,
USB, network, or desktop support.

## Road to v1

1. Complete or disposition issue #63 and the remaining v0.2 residuals.
2. Run final automated and visible v0.2 evidence and create the release record.
3. Build v0.3 block-device, path, mount, and structured filesystem foundations.
4. Add real FAT directories and long names.
5. Add a shared userland runtime and widget layer.
6. Complete the seven applications around the v1 workflow.
7. Mount ext2 read-only.
8. Stabilize, fuzz, verify reboot persistence, and record final evidence.

## Project structure

```text
boot/                 AArch64 entry and early setup
kernel/               exceptions, processes, syscalls, VFS, GUI, runtime
kernel/mm/            PMM, VMM, MMU, heap
kernel/sched/         cooperative EL1 helper scheduler
drivers/              boards and device drivers
programs/apps/         freestanding KLI1 applications
programs/libkarm/      syscall and userland helpers
programs/libkarmdesk/  GUI wrappers
tests/                host and contract tests
tools/                build, QEMU, stress, and verification utilities
docs/                 status, risks, architecture, ABI, workflow, roadmap
```

## Design principles

- Keep the kernel small, explicit, and readable.
- Prefer direct C and narrow assembly boundaries.
- Treat QEMU as the regression platform until hardware is proven.
- Separate implementation, promoted evidence, investigation, and future intent.
- Keep hard IRQ callbacks fixed and bounded.
- Preserve post-EOI count, deadline, routing, and continuation contracts.
- Compact or redesign before raising fixed limits.
- Add tests and documentation with behavior changes.

## License

ArmoniOS is licensed under [GNU GPL-2.0](LICENSE).
