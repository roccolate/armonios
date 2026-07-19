# Porting Guide

ArmoniOS currently has one verified development platform: QEMU `virt`.

The repository also contains experimental Raspberry Pi 4 files. They are build-verified scaffolding for future bring-up, not a working hardware port. Hardware claims must follow `DOCUMENTATION_POLICY.md`.

The v1.0 roadmap is a QEMU desktop release line. Raspberry Pi work remains a
separate hardware track until physical serial, memory/timer, and storage
evidence exist.

Active board risks:

- `RISK-007` — experimental and contradictory eMMC implementation.

## Portability goal

Generic kernel code should remain independent of board physical addresses and controller-specific initialization.

Portable subsystems should include:

- process and scheduler logic;
- PMM/VMM policy where board memory maps are supplied correctly;
- syscall dispatch and user-copy helpers;
- IPC;
- VFS, tmpfs, and FAT32 logic;
- GUI compositor and application ABI;
- device-independent input events.

Board or platform code must own:

- initial exception level and CPU bring-up;
- kernel load address;
- UART selection and pin/clock setup;
- interrupt controller and IRQ routing;
- timer source;
- memory-map fallback and firmware reservations;
- framebuffer or display-controller setup;
- storage controller;
- PCIe host bridge;
- USB host controller discovery;
- network controller;
- board-specific DMA/cache requirements.

## Current board contract

Generic code includes `drivers/board.h`. The present interface requires each selected board build to provide:

```c
const char *board_name(void);
uint32_t board_capabilities(void);
void board_early_init(void);
int board_map_mmio(uint64_t *pgd);

void board_irq_init(void);
void board_irq_enable(uint32_t irq);
uint32_t board_irq_ack(void);
void board_irq_end(uint32_t irq);
int board_irq_is_spurious(uint32_t irq);

uint32_t board_uart0_irq(void);

uint64_t board_virtio_mmio_base(void);
uint64_t board_virtio_mmio_size(void);
uint64_t board_virtio_mmio_stride(void);

int board_emmc_read(uint32_t lba, uint32_t count, void *buffer);
int board_emmc_write(uint32_t lba, uint32_t count, const void *buffer);

int board_storage_read(uint32_t lba, uint32_t count, void *buffer);
int board_storage_write(uint32_t lba, uint32_t count, const void *buffer);
int board_storage_init(void);

int board_display_init(board_display_draw_fn_t draw, void *context);
int board_display_redraw(board_display_draw_fn_t draw, void *context);

uint32_t board_input_irq(void);
int board_input_init(void);
int board_input_poll(void);
```

This contract is transitional. Generic input/display calls are capability-shaped, while network still reaches the QEMU virtio transport through the current direct stack.

Every board build must define all required symbols. Unsupported operations must return explicit safe failures rather than remaining undefined.

## Current board status

### QEMU `virt`

Status:

- default build target;
- locally build-verified;
- storage path QEMU-verified through `qemu-fs-test`;
- visible desktop wiring QEMU-verified through `tools/qemu_fb_fat_test.sh`;
- reference implementation for the current board contract.

QEMU-specific responsibilities include:

- PL011 UART;
- GICv2;
- virtio-mmio range;
- PCI ECAM/MMIO mapping;
- virtio block, GPU, input, and network discovery behind the QEMU board or QEMU-only stack;
- xHCI discovery through PCI BAR assignment.

### Raspberry Pi 4

Status:

- experimental source files exist;
- build-verified and link-verified by `tests/run_board_build_test.sh`;
- not serial-verified;
- not hardware-verified;
- not storage-verified;
- not display-verified.

Known facts from static inspection:

- the backend implements every function required by `drivers/board.h`;
- unsupported display/input operations return explicit safe failures;
- the eMMC code must not be treated as a functional driver;
- storage should fail closed until the eMMC path is rewritten or hardware-validated;
- there is no documented EL2-to-EL1 bring-up path for physical firmware entry;
- there is no secondary-core parking milestone;
- there is no validated framebuffer acquisition path;
- BCM2711 PCIe host initialization is not implemented for VL805 xHCI discovery.

Therefore `make BOARD=rpi4` is build evidence only, not evidence of hardware support.

## Required board capability direction

Before adding more boards, prefer a capability-oriented interface such as:

```c
typedef struct {
    int (*storage_init)(void);
    int (*storage_read)(uint32_t lba, uint32_t count, void *buffer);
    int (*storage_write)(uint32_t lba, uint32_t count, const void *buffer);

    int (*input_init)(void);
    int (*input_poll)(void);

    int (*display_init)(void);
    int (*network_init)(void);
} board_ops_t;
```

The exact design is not fully frozen. Input and display already use `board_input_*` and `board_display_*`; future work should move network and any remaining transport-specific assumptions behind the same capability style.

## Adding a board

### 1. Create board files

```text
drivers/boards/<board>/board.h
drivers/boards/<board>/board.c
linker/linker_<board>.ld
```

Keep physical addresses in the board-specific header or implementation.

### 2. Establish a build-only milestone

Before hardware work:

```sh
make BOARD=<board>
```

must compile and link in CI.

At this stage unsupported operations may return safe errors, but every required symbol must exist. Record the result as `BUILD-VERIFIED`, not hardware support.

### 3. Establish controlled CPU entry

Document:

- firmware or bootloader;
- initial exception level;
- core ID behavior;
- kernel load address;
- DTB location and ownership;
- cache/MMU state on entry.

If the board enters at EL2, add a reviewed EL2-to-EL1 path before using EL1 exception registers. Park non-boot cores until SMP is deliberately implemented.

### 4. Establish serial output

Serial is the first hardware runtime milestone.

Record:

- exact board model and revision;
- UART type;
- GPIO alternate-function setup;
- clock source and baud configuration;
- cable/adapter details;
- earliest stable marker;
- repeat count across cold boots.

No display, storage, or USB claim matters until serial is repeatable.

### 5. Validate memory and timer

- confirm DTB memory ranges;
- reserve firmware, DTB, kernel, and MMIO correctly;
- remove or explicitly document the current 128 MiB PMM limit;
- verify interrupt acknowledge/end paths;
- verify timer frequency and repeated ticks;
- test process return from IRQ on hardware before running a desktop.

### 6. Add display

For Raspberry Pi 4, a firmware mailbox framebuffer is the preferred first display milestone.

Validate:

- physical framebuffer address;
- ARM-visible address translation;
- width, height, pitch, and pixel order;
- cache attributes;
- full-screen fill before attempting the compositor.

Do not begin with a complex DRM/KMS-style stack.

### 7. Add storage read-only first

The generic filesystem path expects 512-byte logical sectors from `board_storage_*`.

Required order:

1. controller reset and clock setup;
2. card identification and voltage negotiation;
3. addressing-mode detection;
4. one repeatable sector read;
5. multiple sector reads;
6. FAT boot-sector parse;
7. read-only file access;
8. writes only after corruption/recovery testing.

Do not use the current `drivers/storage/emmc.c` as a known-good reference. It contains unresolved register-size, indexing, command-construction, and initialization issues tracked by `RISK-007`.

### 8. Add USB and PCIe

On Raspberry Pi 4, the VL805 xHCI controller requires BCM2711 PCIe host-bridge initialization before generic PCI/xHCI code can discover it.

Validate the host bridge first, then PCI enumeration, then xHCI reset, then one directly attached keyboard. Hub support is a separate feature.

### 9. Add network last

Keep network controller work behind the board layer. Reuse generic protocol code only after frame send/receive is proven.

## Port verification record

Every hardware milestone must include:

```text
Date:
Commit:
Board model/revision:
Firmware/bootloader:
Boot files/config:
Power supply:
Serial adapter and settings:

Build command and result:
Hardware procedure:
Observed markers:
Number of successful cold boots:
Known failures:
```

Photos or serial logs are useful evidence, but the reproduction text must remain sufficient without them.

## Support claim levels

Use these exact scopes:

- **source present** — board files exist;
- **build-verified** — target compiles and links;
- **serial bring-up verified** — physical board reaches documented UART marker;
- **subsystem verified** — one named subsystem passes a documented hardware workflow;
- **desktop verified** — framebuffer, input, processes, and applications complete a named workflow;
- **supported** — documented setup is repeatable and mandatory known risks are closed or accepted.

ArmoniOS currently claims only QEMU development support. Raspberry Pi 4 is at the build-verified experimental stage.
