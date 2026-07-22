# ArmoniOS

> A small freestanding AArch64 operating system with a graphical QEMU desktop,
> inspired by the compact, direct design of KolibriOS and MenuetOS.

[![License: GPL-2.0](https://img.shields.io/badge/License-GPL--2.0-blue.svg)](LICENSE)
[![Architecture](https://img.shields.io/badge/arch-AArch64-green.svg)]()
[![Language](https://img.shields.io/badge/lang-C%20%2B%20ASM-orange.svg)]()
[![Status](https://img.shields.io/badge/status-v0.2%20final%20candidate-blue.svg)]()

<p align="center">
  <img src="docs/assets/armin.png" alt="Armin, the ArmoniOS mascot" width="128">
</p>

## Preview

![ArmoniOS QEMU desktop preview](docs/assets/qemu-desktop-preview.png)

## Current status

ArmoniOS is a **real bare-metal AArch64 operating system** with a verified QEMU
`virt` desktop baseline. It is not a hosted Linux application or distribution.

- **public baseline:** v0.1 QEMU desktop;
- **engineering phase:** v0.2 final cleanup/runtime-hardening candidate;
- **remaining promotion work:** visible pass, residual-risk disposition, issue #63,
  and the release record;
- **product target:** v1.0 usable QEMU mini desktop;
- **hardware:** Raspberry Pi scaffolding only, not hardware-supported.

The post-EOI runtime service enforces:

| Work class | Current bound |
|---|---:|
| Whole service | One nominal timer interval at safe checkpoints |
| Virtio-input producer | At most 16 used descriptors per call |
| USB HID producer | At most four registered device visits per call |
| Shared input consumer | At most 16 queued events per pass |
| Partial compositor damage | At most eight rectangles per successful redraw |
| Virtio-net RX | At most 16 valid frames per active NETWORK pass |

Virtio-input leaves later descriptors in its ring. Partial damage remains ordered
and dirty until later batches complete; failed GPU submissions consume nothing.
Input and network readiness use independent pending bits. Deadline exhaustion is
counted once and conservatively republishes the original work snapshot.

Network polling and receive now consume nothing outside the active NETWORK phase,
so the legacy cooperative console poll cannot bypass the post-EOI count and time
budgets.

Two automated QEMU stress modes exist:

- PR #61 forces deterministic deadline expirations and proves repeated EL0
  heartbeats while input, redraw, DHCP, and network activity occur;
- PR #62 preserves the natural production deadline and saturates virtio-net RX
  while input and redraw work continue.

The natural saturation run recorded:

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
input overflow:                          0
kernel panic:                            0
```

Final validated PR #62 head:
`eac4ff990baddbf83406567b4a20e58bcae6600d`.

- `Verify ArmoniOS` run `29896102906` (#290): success;
- `CI - Tests` run `29896102904` (#430): success;
- production loadable QEMU kernel: **107918 / 108000 bytes**;
- remaining production margin: **82 bytes**.

The virtio-net path still exposes no trustworthy device-drop or ring-overflow
counter. Consumed-frame counts prove software-visible progress, not delivery of
every host-submitted packet. A full redraw also remains one cooperatively bounded
operation rather than an asynchronously preemptible task.

One intermittent EL1 VMM data abort observed during an existing FAT32 smoke run is
tracked separately in issue #63. Subsequent reruns passed, but the observation is
not classified as infrastructure-only without investigation.

## What works

The QEMU codebase includes:

- AArch64 EL1 boot with DTB handoff;
- PMM, heap, 4 KiB page tables, MMU, and kernel W^X;
- preemptive freestanding EL0 processes with private image, stack, anonymous
  mappings, parent/wait, kill, exit, and zombie cleanup;
- a small non-POSIX syscall ABI with permission-aware user pointers and
  kernel-owned syscall payloads;
- process-local VFS descriptors;
- bootfs, tmpfs, and a writable root-only FAT32 bridge;
- a kernel-owned compositor with focus, dragging, minimize/restore, backing
  buffers, damage tracking, and event queues;
- `panel`, `shell`, `editor`, `files`, `monitor`, `control`, and `clock`;
- QEMU virtio block, GPU, input, and network;
- PCI/xHCI and directly attached boot-protocol USB HID;
- Ethernet, ARP, IPv4, UDP, and DHCP for a QEMU user-network lease;
- one post-EOI runtime service with count bounds, a cooperative global deadline,
  strict network routing, and aggregate/per-class telemetry;
- deterministic host, QEMU, stress, size, stack, ABI, storage, GUI, USB, network,
  and board-build gates.

## Important limits

| Area | Current limit |
|---|---|
| Runtime platform | QEMU `virt` is the only verified runtime target. |
| Physical memory | PMM manages at most 128 MiB. |
| Processes | 16 slots and eight tracked user regions each. |
| VFS | 24 nodes, four mounts, eight descriptors/process, 64-byte paths. |
| FAT32 | Root directory, short 8.3 names, no subdirectories or LFN. |
| Editor | 512-byte buffer; only the caret line is rendered. |
| Files | `/fat` only; at most eight displayed root entries. |
| GUI | 16 windows, 32 events/window, 32 damage rectangles; eight partial rectangles/redraw. |
| Input queue | 64 events; overflow is counted but not prevented. |
| Virtio input | Negotiated ring up to 16; at most one ring-length drained/call. |
| USB HID | Four devices; at most four device polls/call; no hubs. |
| Network RX | 16 valid frames/NETWORK pass; outside-phase receive suppressed; device drops unavailable. |
| Networking API | No socket ABI, TCP, DNS API, or HTTP. |
| Scheduling | EL0 preemptive; EL1 helper threads cooperative. |
| Runtime service | Runs after EOI but before `eret`; deadline checks occur at safe boundaries and cannot interrupt one active full redraw. |
| User copy | Permission checked, but final copies are not fault-recoverable. |
| Raspberry Pi | Build/host scaffolding only; no physical support claim. |

Read [Current State](docs/CURRENT_STATE.md) and
[Technical Risks](docs/TECHNICAL_RISKS.md) before making release claims.

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

### Verify the automated baseline

```bash
git clone https://github.com/roccolate/armonios
cd armonios
bash tools/verify.sh
```

The gate covers QEMU/RPi4 builds, `.data == 0`, the 108000-byte ceiling,
native subsystem tests, runtime timing/count/routing contracts, userland stack
usage, FAT32 smoke, user-copy/focus, framebuffer, USB, DHCP, forced-expiry
heartbeat stress, and natural RX saturation.

Useful focused commands:

```bash
make BOARD=qemu_virt
make BOARD=qemu_virt size
make -C tests test
bash tests/run_runtime_service_test.sh
bash tests/run_input_queue_stats_test.sh
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

```bash
make qemu
```

Exit with `Ctrl+A`, then `X`.

### Run the visible desktop

```bash
make qemu-fb-visible
```

The latest recorded manual workflow is from 2026-07-17. Rocco listed `/fat`,
created an 8.3 file, opened Editor, typed and saved, closed, renamed, reopened
with content intact, deleted, and refreshed. A new dated pass is required after
the final v0.2 runtime boundary. Manual evidence is separate from serial gates.

## Runtime architecture

```text
physical timer IRQ
  -> fixed account/rearm/publish PERIODIC | INPUT | NETWORK readiness
  -> EOI
  -> measured post-EOI runtime pass
       -> deadline = start + one timer interval
       -> periodic producer/GUI phase
            -> virtio-input: <=16 used descriptors/call
            -> USB HID: <=4 device visits/call
            -> shared input queue: <=16 events/pass when INPUT is pending
            -> partial damage: <=8 rectangles/successful redraw
            -> full redraw: one non-preemptible operation
       -> independently pending NETWORK phase
            -> <=16 valid RX frames
            -> conservative requeue at cap
       -> expiry: count once, republish original work, skip later work
  -> process dispatch
  -> eret
```

EOI completes the interrupt-controller transaction but does not leave the CPU
exception. EL0 remains paused during the service pass. The complete pass has a
cooperative generic-counter deadline, but one already-started operation can cross
it before the next checkpoint.

See [Deferred Runtime Service](docs/RUNTIME_SERVICE.md).

## Storage and applications

The current FAT32 path supports 512-byte sectors, root 8.3 files, cluster growth,
create/read/write/list/rename/delete, and `/fat/<name>` VFS nodes. It is not a
general FAT implementation.

The applications are useful demonstrations, not complete daily tools:

- **Files:** root-only `/fat` browser and basic 8.3 operations;
- **Editor:** small text buffer, caret editing, save, and a one-line viewport;
- **Shell:** history, scrollback, and basic file/process/system commands;
- **Panel:** launcher, taskbar, focus, minimize, and restore;
- **Monitor, Control, Clock:** compact system and desktop demonstrations.

Issue #2 tracks **v0.6 useful desktop applications**. It depends on v0.3
storage/path infrastructure, v0.4 real FAT, and v0.5 shared runtime/widgets.

## Raspberry Pi

The `rpi4` backend and opt-in read-only EMMC2 diagnostic image build and pass host
tests. Normal board capabilities remain fail closed.

ArmoniOS does not claim physical Raspberry Pi boot, working SD/eMMC, framebuffer,
input, or an installable Raspberry Pi desktop image.

## Documentation

- [Current State](docs/CURRENT_STATE.md)
- [Technical Risks](docs/TECHNICAL_RISKS.md)
- [Roadmap](docs/ROADMAP.md)
- [Architecture](docs/ARCHITECTURE.md)
- [Deferred Runtime Service](docs/RUNTIME_SERVICE.md)
- [Memory Map](docs/MEMORY_MAP.md)
- [Syscalls](docs/SYSCALLS.md)
- [GUI ABI Notes](docs/GUI_ABI_NOTES.md)
- [Documentation Policy](docs/DOCUMENTATION_POLICY.md)
- [Contributing](docs/CONTRIBUTING.md)
- [Porting](docs/PORTING.md)

## Road to v1.0

1. Investigate issue #63 and complete the final v0.2 visible/release record.
2. Build v0.3 block/VFS/path infrastructure.
3. Add real FAT support.
4. Add shared userland runtime/widgets.
5. Complete v0.6 Files, Editor, Shell, Settings, Monitor, Panel, and Clock.
6. Mount ext2 read-only.
7. Stabilize, fuzz, document, and record the final workflow.

## Project structure

```text
boot/                 AArch64 entry and early setup
kernel/               kernel, exceptions, processes, syscalls, VFS, GUI
kernel/mm/            PMM, VMM, heap
kernel/sched/         cooperative EL1 helper scheduler
drivers/              board boundary and device drivers
programs/apps/         freestanding KLI1 applications
programs/libkarm/      syscall and small userland helpers
programs/libkarmdesk/  GUI wrappers
tests/                host and contract tests
tools/                build, QEMU, and verification utilities
docs/                 architecture, ABI, status, risks, policy
```

## Design principles

- Keep the kernel small, explicit, and readable.
- Prefer direct C and narrow AArch64 assembly boundaries.
- Treat QEMU as the regression platform until hardware is proven.
- Separate implementation from verified behavior.
- Keep hard IRQ callbacks bounded.
- Compact or redesign before raising size limits.
- Add tests and documentation with behavior changes.

## License

ArmoniOS is licensed under [GNU GPL-2.0](LICENSE).
