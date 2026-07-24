# Porting ArmoniOS

ArmoniOS currently has one verified runtime platform: QEMU `virt`.

Raspberry Pi 4 support is a build- and host-contract-verified hardware scaffold.
It is not a verified physical boot or desktop port.

Use this document for board responsibilities and evidence stages. Use:

- `CURRENT_STATE.md` for current capability;
- `TECHNICAL_RISKS.md` for the physical-board risk boundary;
- `MEMORY_MAP.md` for current address-space assumptions;
- `DEVELOPMENT_GUIDE.md` for normal change workflow;
- `DOCUMENTATION_POLICY.md` for support-claim rules.

## Portability boundary

Generic kernel code should not know board physical addresses, firmware protocols,
interrupt-controller registers, storage-controller commands, or display mailbox
formats.

```text
generic kernel and subsystems
  -> drivers/board.h capability boundary
  -> drivers/boards/<board>/ implementation
  -> controller and firmware drivers
  -> hardware or emulator
```

Portable areas include:

- process lifecycle and scheduling policy;
- syscall dispatch and user-copy validation;
- PMM/VMM policy after the board supplies a valid memory description;
- IPC;
- generic VFS and filesystem logic;
- GUI compositor and public application ABI;
- device-neutral input events;
- network protocols above a proven frame transport.

Board/platform code owns:

- firmware entry state and exception-level transition;
- boot-core and secondary-core policy;
- kernel load address and linker placement;
- DTB or firmware memory description;
- UART, pins, clocks, and early diagnostics;
- interrupt controller and IRQ routing;
- timer source;
- framebuffer/display acquisition;
- storage controller and DMA/cache behavior;
- PCIe host bridge and USB host discovery;
- network-controller transport;
- board-specific MMIO mappings and memory attributes.

## Current board contract

The authoritative declarations live in `drivers/board.h`.

A selected board must provide the required operations for:

- board identity and capability reporting;
- early initialization and MMIO mapping;
- interrupt-controller initialization, acknowledge, spurious detection, and EOI;
- timer and UART interrupt identity;
- storage discovery and bounded I/O;
- display initialization and redraw submission;
- input initialization, polling, and interrupt routing;
- optional emulator-specific transports where declared.

Rules:

1. Generic code does not contain board physical addresses.
2. Unsupported capabilities fail explicitly and safely.
3. A capability is not advertised merely because source code exists.
4. Silent success stubs are prohibited.
5. Board implementations preserve generic ownership, ABI, W^X, runtime-service,
   and failure contracts.
6. Physical support claims require physical evidence for each named subsystem.
7. Storage writes begin only after read-only bring-up and an explicit corruption
   and recovery plan.

The interface is still evolving. Display and input are already routed through
board operations. Some network and virtio assumptions remain QEMU-oriented and
must be isolated before a second runtime platform supplies equivalent facilities.

## Current platforms

### QEMU `virt`

QEMU is the default development and regression platform.

Current verified scope includes:

- AArch64 kernel boot and EL0 process execution;
- GICv2 and physical timer integration;
- PL011 serial output;
- virtio block, GPU, input, and network devices;
- PCI ECAM and xHCI paths used by the existing USB tests;
- framebuffer desktop, keyboard, mouse, storage, and DHCP workflows;
- runtime-service count, routing, deadline, and stress contracts.

QEMU-specific constants and device wiring belong under
`drivers/boards/qemu_virt/` or clearly named emulator-only driver paths.

### Raspberry Pi 4

Promoted scope:

- AArch64 cross-build and link;
- board symbols required by the generic interface;
- linker placement at the current experimental load address;
- EMMC2/SDHCI and mailbox-clock source code;
- MBR and bounded block-view host tests;
- opt-in read-only EMMC2 diagnostic image packaging;
- fail-closed normal capabilities where physical evidence is missing.

Not verified on physical hardware:

- firmware entry and EL2-to-EL1 transition;
- secondary-core parking;
- repeatable UART boot output;
- physical memory map and timer behavior;
- SD/eMMC initialization and sector reads;
- framebuffer acquisition;
- USB host, keyboard, and mouse;
- BCM2711 PCIe/VL805 xHCI initialization;
- networking;
- graphical desktop workflow;
- storage writes or reboot persistence.

`make BOARD=rpi4` proves only that the target builds and links.

## Evidence levels

Use these scopes precisely:

| Level | Meaning |
|---|---|
| Source present | Board files or controller code exist. |
| Build verified | The target compiles and links. |
| Host-contract verified | Pure-C or mocked helpers pass on the development host. |
| Serial bring-up verified | A named physical board reaches repeatable UART markers. |
| Subsystem verified | One named physical subsystem passes a documented workflow. |
| Desktop verified | Display, input, processes, storage, and applications complete a documented physical workflow. |
| Supported | Setup is repeatable and required risks are closed or explicitly accepted. |

ArmoniOS currently claims QEMU development support and Raspberry Pi 4
build/host-contract scaffolding only.

## Adding a board

### 1. Establish the source and linker boundary

Typical locations:

```text
drivers/boards/<board>/
linker/linker_<board>.ld
```

Keep controller-specific constants, firmware calls, and MMIO policy in the board
or controller layer.

### 2. Land a fail-closed build target

```sh
make BOARD=<board>
```

The build must provide every required symbol. Unsupported optional facilities may
return explicit errors; they must not report success or advertise capability.

This stage is `BUILD-VERIFIED`, not boot support.

### 3. Define controlled CPU entry

Document and verify:

- exact board revision;
- firmware or bootloader;
- initial exception level;
- boot-core selection;
- secondary-core behavior;
- kernel load address;
- DTB or firmware data address and lifetime;
- cache and MMU state on entry;
- early stack and exception-vector installation.

If entry occurs at EL2, add an explicit reviewed transition to EL1. Keep secondary
cores parked until SMP is deliberately designed.

### 4. Obtain repeatable serial output

Record:

- board and firmware versions;
- boot files and configuration;
- UART type, pins, clock, and baud;
- power supply and serial adapter;
- exact image identity;
- first stable marker;
- successful cold boots and attempts.

Do not claim display, storage, USB, or desktop behavior before serial bring-up is
repeatable.

### 5. Validate memory, exceptions, IRQs, and timer

Required boundaries:

- correct RAM and reservation discovery;
- kernel, DTB, firmware, stack, and MMIO reservations;
- reviewed PMM capacity policy;
- correct exception-level transition and return;
- interrupt acknowledge and EOI;
- timer frequency and repeated ticks;
- successful return to EL0 after IRQ;
- preserved kernel W^X and page ownership.

### 6. Add display as an isolated milestone

Validate raw display output before compositor integration:

1. obtain the framebuffer or display surface;
2. verify physical/CPU-visible address conversion;
3. verify dimensions, pitch, and pixel order;
4. choose correct memory attributes and cache policy;
5. draw stable full-screen patterns;
6. repeat redraws;
7. connect the generic compositor only after the transport is stable.

For Raspberry Pi 4, a firmware-mailbox framebuffer is the preferred first path.

### 7. Add storage read-only first

Recommended order:

1. controller reset and clock setup;
2. bounded command/response timeout behavior;
3. card identification and addressing mode;
4. one repeatable sector read;
5. multiple bounded reads;
6. whole-device capacity reporting;
7. MBR or volume discovery;
8. FAT boot-sector parsing;
9. read-only file access;
10. writes only after explicit corruption and recovery testing.

Use disposable media for destructive experiments. Do not promote a normal writable
capability from build or diagnostic-probe evidence.

### 8. Add USB and PCIe in layers

For Raspberry Pi 4, validate:

1. BCM2711 PCIe host bridge;
2. link state;
3. ECAM and BAR access;
4. PCI enumeration;
5. VL805/xHCI reset and rings;
6. one directly attached keyboard;
7. mouse;
8. hubs as a separate feature.

### 9. Add networking last

Prove controller/DMA frame send and receive before reusing generic Ethernet, ARP,
IPv4, UDP, and DHCP code. Keep transport behavior behind the board boundary.

A board bring-up does not require creating a public application network ABI.

## Current board checks

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

These commands provide build or host-contract evidence only.

## Physical verification record

Every physical milestone should record:

```text
Date:
Commit and image hash:
Board model and revision:
Firmware or bootloader:
Boot files and configuration:
Power supply:
Serial adapter and settings:
Display, storage, input, and USB hardware:

Build command and result:
Hardware procedure:
Expected markers:
Observed markers:
Successful cold boots / attempts:
Artifacts:
Known failures and limitations:
Evidence classification:
```

Photos and logs are useful artifacts, but reproduction instructions must remain
sufficient without them.

## Porting invariants

- keep physical addresses outside generic code;
- fail unsupported capabilities closed;
- separate source, build, host, serial, subsystem, desktop, and support claims;
- preserve single-core assumptions until SMP work is explicit;
- preserve page ownership, W^X, public ABI, and runtime-service contracts;
- start storage read-only;
- update `RISK-007`, `CURRENT_STATE.md`, and this document as physical evidence
  advances.
