# Porting Guide

ArmoniOS currently has one verified runtime platform: QEMU `virt`.

The Raspberry Pi 4 tree is build/host-verified, fail-closed scaffolding for a
future physical port. It is not a working hardware port.

- Current board evidence: `CURRENT_STATE.md`
- Hardware risks: `RISK-007` and `RISK-008` in `TECHNICAL_RISKS.md`
- Evidence rules: `DOCUMENTATION_POLICY.md`
- Development workflow: `DEVELOPMENT_GUIDE.md`

The v1 product roadmap is a QEMU desktop line. Physical-board work remains a
separate track until serial, memory/timer, storage, display, and input evidence is
recorded.

## Portability goal

Generic kernel code should remain independent of board physical addresses,
interrupt-controller details, and controller-specific initialization.

Portable subsystems should include:

- process and scheduling logic;
- PMM/VMM policy once a correct board memory map is supplied;
- syscall and user-copy helpers;
- IPC;
- VFS and filesystem logic;
- GUI compositor and application ABI;
- device-independent input events;
- generic protocol code after a board-specific frame transport exists.

Board/platform code owns:

- initial exception level and CPU bring-up;
- secondary-core policy;
- kernel load address;
- UART, pins, and clocks;
- interrupt controller and routing;
- timer source;
- memory-map fallback and firmware reservations;
- framebuffer/display acquisition;
- storage controller and DMA/cache behavior;
- PCIe host bridge;
- USB host discovery;
- network controller;
- board-specific MMIO mappings.

## Current board contract

Generic code includes `drivers/board.h`. Every selected board must provide the
required symbols for:

- name and capability reporting;
- early initialization and MMIO mapping;
- IRQ initialize, enable, acknowledge, spurious detection, and EOI;
- UART IRQ identity;
- virtio range where applicable;
- storage initialize/read/write;
- display initialize/redraw;
- input initialize/poll/IRQ.

The exact declarations in `drivers/board.h` are authoritative.

Rules:

- generic code must not contain board physical addresses;
- unsupported optional operations return explicit safe failures;
- a board must not advertise a capability without matching runtime evidence;
- silent success stubs are not acceptable;
- board code must preserve the generic runtime and ownership contracts;
- transport-specific assumptions should move behind capability-oriented interfaces
  over time.

The current interface is transitional. Input and display are already expressed as
board operations. Network still contains QEMU-oriented direct virtio assumptions
that should be isolated before another board supplies networking.

## Current board status

### QEMU `virt`

Current scope:

- default build and regression target;
- QEMU boot, storage, display, input, USB, and DHCP evidence;
- reference GICv2, PL011, virtio-mmio, PCI ECAM, virtio block/GPU/input/network,
  and xHCI behavior;
- current runtime count/deadline/stress evidence.

QEMU-specific constants and device wiring belong under
`drivers/boards/qemu_virt/` or explicitly QEMU-only driver paths.

### Raspberry Pi 4

Promoted scope:

- cross-build and link contract;
- every required board symbol exists;
- unsupported normal display/input/storage capabilities fail closed;
- EMMC2/SDHCI and mailbox-clock code exists;
- MBR and bounded block-view components build and pass host tests;
- an opt-in read-only diagnostic/probe package builds;
- no physical behavior is claimed.

Unverified scope:

- firmware entry and EL2-to-EL1 transition;
- secondary-core parking;
- repeatable UART output;
- physical memory/timer behavior;
- card initialization and sector reads;
- framebuffer acquisition;
- GPIO keyboard/mouse or USB host operation;
- BCM2711 PCIe/VL805 xHCI initialization;
- network controller;
- desktop workflow.

`make BOARD=rpi4` is build evidence only.

## Support claim levels

Use these exact scopes:

- **source present** — board files exist;
- **build-verified** — target compiles and links;
- **host-contract-verified** — mocked/pure-C board helper tests pass;
- **serial bring-up verified** — physical board reaches documented UART markers;
- **subsystem verified** — one named physical subsystem passes a documented
  workflow;
- **desktop verified** — framebuffer, input, process, storage, and applications
  complete a named physical workflow;
- **supported** — setup is repeatable and mandatory risks are closed or accepted.

ArmoniOS currently claims QEMU development support and Raspberry Pi 4
build/host-contract scaffolding only.

## Adding or advancing a board

### 1. Create the board boundary

Typical files:

```text
drivers/boards/<board>/board.h
drivers/boards/<board>/board.c
linker/linker_<board>.ld
```

Keep physical addresses and controller-specific policy in the board tree.

### 2. Establish a build-only milestone

```sh
make BOARD=<board>
```

must compile and link in CI. Unsupported operations may return explicit errors,
but every required symbol must exist.

Record this only as `BUILD-VERIFIED`.

### 3. Establish controlled CPU entry

Document:

- firmware/bootloader;
- initial exception level;
- boot-core selection;
- secondary-core behavior;
- kernel load address;
- DTB address, lifetime, and reservations;
- cache and MMU state on entry;
- stack and exception-vector installation.

If firmware enters at EL2, add a reviewed EL2-to-EL1 transition before using EL1
state. Park non-boot cores until SMP is deliberately designed.

### 4. Establish repeatable serial output

Record:

- board model and revision;
- firmware and boot files;
- UART type;
- GPIO alternate function;
- clock source and baud;
- power supply;
- cable/adapter and host settings;
- first stable marker;
- cold-boot repeat count;
- exact image hash.

No display, storage, or USB claim should precede repeatable serial.

### 5. Validate memory, exceptions, IRQs, and timer

- confirm DTB memory ranges;
- reserve firmware, DTB, kernel, stacks, and MMIO correctly;
- document or revise the 128 MiB PMM cap;
- validate exception-level transitions and return;
- validate interrupt acknowledge/EOI;
- validate timer frequency and repeated ticks;
- prove EL0 return from IRQ before running the desktop;
- preserve kernel W^X and page ownership.

### 6. Add display

For Raspberry Pi 4, a firmware-mailbox framebuffer is the preferred first display
milestone.

Validate:

- physical framebuffer address;
- ARM-visible address conversion;
- width, height, pitch, and pixel order;
- memory attributes/cache policy;
- full-screen fill;
- repeated redraw;
- compositor integration only after basic output is stable.

### 7. Add storage read-only first

The generic path currently expects 512-byte logical sectors.

Recommended order:

1. controller reset and clock setup;
2. command/response and timeout handling;
3. card identification and voltage negotiation;
4. addressing-mode detection;
5. one repeatable sector read;
6. multiple bounded reads;
7. MBR/volume discovery;
8. FAT boot-sector parse;
9. read-only file access;
10. writes only after explicit corruption/recovery testing.

Use disposable media for destructive experiments. Do not enable normal writable
capabilities from build or probe evidence.

### 8. Add PCIe and USB

On Raspberry Pi 4, VL805 xHCI requires BCM2711 PCIe host-bridge initialization.
Validate in order:

1. host bridge;
2. link state;
3. ECAM and BAR access;
4. PCI enumeration;
5. xHCI reset and rings;
6. one directly attached keyboard;
7. mouse;
8. hubs only as a separate feature.

### 9. Add network last

Keep controller/DMA behavior behind the board boundary. Reuse generic Ethernet,
ARP, IPv4, UDP, and DHCP code only after frame send/receive is separately proven.
Do not expose an application network ABI as part of board bring-up.

## Board verification commands

Current repository checks:

```sh
make BOARD=rpi4
make rpi4-emmc2-probe
bash tests/run_board_build_test.sh
bash tests/run_emmc_sdhci_test.sh
bash tests/run_rpi4_emmc2_probe_diag_test.sh
bash tests/run_mbr_fat32_test.sh
bash tests/run_block_view_fat32_test.sh
bash tests/run_rpi_mailbox_test.sh
```

These are build/host evidence only.

## Physical verification record

Every physical milestone must record:

```text
Date:
Commit and image hash:
Board model/revision:
Firmware/bootloader:
Boot files/config:
Power supply:
Serial adapter and settings:
Display/storage/input hardware:

Build command and result:
Hardware procedure:
Expected markers:
Observed markers:
Successful cold boots / attempts:
Artifacts:
Known failures and limitations:
Evidence classification:
```

Photos and logs are useful artifacts, but reproduction text must remain sufficient
without them.

## Porting invariants

- Keep board-specific addresses out of generic code.
- Fail unsupported capabilities closed.
- Do not advertise capabilities from source/build evidence.
- Preserve single-core assumptions until SMP is deliberate.
- Preserve memory ownership, W^X, ABI, and runtime-service contracts.
- Start storage read-only.
- Separate build, serial, subsystem, desktop, and support claims.
- Update `RISK-007`, `CURRENT_STATE.md`, and this guide as evidence advances.
