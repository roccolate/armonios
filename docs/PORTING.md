# Porting Guide

ArmoniOS is designed so that porting to a new ARM64 board keeps generic kernel
code board-agnostic. Board-specific physical addresses, interrupt routing,
storage, input, display, and bus details belong behind `drivers/board.h` and
`drivers/boards/<board>/`.

---

## Architecture Goal

ArmoniOS should be structured so that the following components remain portable
and avoid board-specific constants:

- `kernel/mm/` — physical and virtual memory management
- `kernel/sched/` — scheduler and context switch
- `kernel/ipc/` — message passing
- `kernel/vfs.c`, `kernel/tmpfs.c`, `kernel/fat32.c` — VFS and filesystems
- `kernel/gui_*.{c,h}` — window manager and compositor
- syscall dispatch and user-pointer validation

The following components are board-specific and should live behind
`drivers/boards/<board>/` or an equivalent platform layer:

- UART base address and register type
- interrupt controller and IRQ numbers
- timer source
- display / framebuffer discovery
- storage backend (virtio-blk, eMMC/SD, NVMe, etc.)
- USB host controller and PCIe host bridge, when present
- network controller
- virtio-mmio and virtio-input discovery, when present

**Current reference board:** QEMU `virt` lives under
`drivers/boards/qemu_virt/`. Its board layer owns early UART init, GIC setup,
UART IRQ number, MMIO mappings, virtio-mmio range constants, PCIe ECAM/MMIO
mappings for xHCI, virtio-blk storage, and virtio-input polling.

**USB note:** the active USB backend is the generic PCI xHCI driver under
`drivers/usb/`. QEMU `virt` reaches it through ECAM + PCI BAR assignment.
Raspberry Pi 4 still needs BCM2711 PCIe host bridge setup in
`drivers/boards/rpi4/` before the VL805 xHCI controller can be discovered.

**Next portability rule:** keep moving new board-specific devices behind
`drivers/boards/<board>/` helpers instead of adding raw MMIO addresses to
generic kernel code.

---

## Current Board Contract

Generic kernel code includes `drivers/board.h`. Every board build must provide
these functions:

```c
const char *board_name(void);
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

uint32_t board_virtio_input_irq(void);
int board_virtio_input_init(void);
int board_virtio_input_poll(void);
```

A board may stub unsupported features with safe failure values, but it must do so
explicitly. For example, a non-virtio board can return `0` from the virtio range
functions and `-1` from `board_virtio_input_init()` / `board_virtio_input_poll()`.

---

## How to Add a New Board

### Step 1: Create a board directory

```
drivers/
└── boards/
    ├── qemu_virt/       ← reference implementation
    │   ├── board.h
    │   └── board.c
    ├── rpi4/
    │   ├── board.h
    │   └── board.c
    └── your_board/
        ├── board.h      ← you create this
        └── board.c
```

### Step 2: Keep the board interface split

Generic kernel code includes `drivers/board.h`. A board-specific header under
`drivers/boards/<board>/board.h` should include that generic contract and keep
only private constants for the board implementation.

```c
// drivers/boards/your_board/board.h

#pragma once
#include <stdint.h>

#include "drivers/board.h"

#define YOUR_BOARD_UART_BASE       0x09000000ULL
#define YOUR_BOARD_UART_IRQ        33
#define YOUR_BOARD_GIC_DIST_BASE   0x08000000ULL
#define YOUR_BOARD_GIC_CPU_BASE    0x08010000ULL
#define YOUR_BOARD_VIRTIO_BASE     0x00000000ULL
#define YOUR_BOARD_VIRTIO_SIZE     0x00000000ULL
#define YOUR_BOARD_VIRTIO_STRIDE   0x00001000ULL
```

### Step 3: Implement `board.c`

Use `drivers/boards/qemu_virt/board.c` as the live reference. A minimal board
skeleton looks like this:

```c
// drivers/boards/your_board/board.c

#include "boards/your_board/board.h"

#include "irq/gicv2.h"       // or your interrupt controller
#include "kernel/mm/vmm.h"
#include "uart/pl011.h"      // or your UART type

const char *board_name(void) {
    return "your-board";
}

void board_early_init(void) {
    uart_init(YOUR_BOARD_UART_BASE);
}

int board_map_mmio(uint64_t *pgd) {
    int status = vmm_map_page(pgd, YOUR_BOARD_UART_BASE,
                              YOUR_BOARD_UART_BASE,
                              VMM_FLAG_READ | VMM_FLAG_WRITE |
                                  VMM_FLAG_DEVICE);
    if (status == 0) {
        status = vmm_map_range(pgd, YOUR_BOARD_GIC_DIST_BASE,
                               YOUR_BOARD_GIC_DIST_BASE, 0x20000,
                               VMM_FLAG_READ | VMM_FLAG_WRITE |
                                   VMM_FLAG_DEVICE);
    }
    return status;
}

void board_irq_init(void) {
    gicv2_init(YOUR_BOARD_GIC_DIST_BASE, YOUR_BOARD_GIC_CPU_BASE);
}

void board_irq_enable(uint32_t irq) {
    gicv2_enable_irq(irq);
}

uint32_t board_irq_ack(void) {
    return gicv2_ack_irq();
}

void board_irq_end(uint32_t irq) {
    gicv2_end_irq(irq);
}

int board_irq_is_spurious(uint32_t irq) {
    return irq == GIC_SPURIOUS_IRQ;
}

uint32_t board_uart0_irq(void) {
    return YOUR_BOARD_UART_IRQ;
}

uint64_t board_virtio_mmio_base(void) {
    return YOUR_BOARD_VIRTIO_BASE;
}

uint64_t board_virtio_mmio_size(void) {
    return YOUR_BOARD_VIRTIO_SIZE;
}

uint64_t board_virtio_mmio_stride(void) {
    return YOUR_BOARD_VIRTIO_STRIDE;
}

int board_emmc_read(uint32_t lba, uint32_t count, void *buffer) {
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

int board_emmc_write(uint32_t lba, uint32_t count, const void *buffer) {
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

int board_storage_init(void) {
    return -1;
}

int board_storage_read(uint32_t lba, uint32_t count, void *buffer) {
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

int board_storage_write(uint32_t lba, uint32_t count, const void *buffer) {
    (void)lba;
    (void)count;
    (void)buffer;
    return -1;
}

uint32_t board_virtio_input_irq(void) {
    return 0;
}

int board_virtio_input_init(void) {
    return -1;
}

int board_virtio_input_poll(void) {
    return -1;
}
```

Replace the storage and input stubs only after the board has a proven boot +
UART milestone. Keep unsupported optional paths failing cleanly instead of
pretending they work.

### Step 4: Select the board at build time

```bash
# qemu_virt is the default. Override it on the command line:
make BOARD=your_board
```

The Makefile compiles `drivers/boards/$(BOARD)/board.o`. Do not add the board
directory to the global include path unless there is a specific reason; generic
kernel files should keep resolving `#include "drivers/board.h"` to the generic
contract.

---

## Porting Checklist

Work through this list in order. Each item depends on the previous.

### Phase A: Boot

- [ ] Identify where your board's boot ROM or bootloader loads the kernel.
- [ ] Add or update the board linker script under `linker/`.
- [ ] For QEMU `virt`, the current link/load address is `0x40080000`.
- [ ] For the planned Raspberry Pi 4 path, the current linker script uses
      `0x80000`.
- [ ] Verify entry point: does your bootloader jump to `_start`?
- [ ] Check initial exception level: are you at EL2 or EL1? EL2 needs a drop.

**EL2 to EL1 drop sketch (if needed, add carefully to the boot path):**

```asm
// Check current EL
mrs x0, CurrentEL
lsr x0, x0, #2
cmp x0, #2
bne .not_el2

// Drop to EL1
msr elr_el2, x9        // return address
mov x0, #0x3c5         // SPSR: EL1h, interrupts masked
msr spsr_el2, x0
eret
.not_el2:
```

### Phase B: UART Output

- [ ] Find your board's UART base address from the datasheet, firmware docs, or
      Linux DTS.
- [ ] Check UART type: PL011, 8250/16550, BCM mini UART, or custom.
- [ ] Implement the board's early UART path.
- [ ] Verify boot prints the earliest banner or a controlled failure marker.

This is the most important hardware milestone. Nothing else matters until text
output is reliable.

### Phase C: Interrupts

- [ ] Identify interrupt controller: GIC-400, GIC-500, or custom.
- [ ] Find GIC distributor and CPU interface base addresses.
- [ ] Initialize the GIC.
- [ ] Verify timer interrupt fires at the expected rate.

### Phase D: Memory

- [ ] Find the board's RAM size from DTB, firmware API, or a board-specific
      fallback.
- [ ] Verify `dtb_get_memory()` sees the RAM map, or add a board-specific
      fallback.
- [ ] Verify PMM initializes without overwriting kernel or firmware regions.

### Phase E: Storage

The generic filesystem path now goes through `board_storage_*`.

**QEMU `virt`:**
- `board_storage_init/read/write` route to virtio-blk.
- `make qemu-fs-test` covers the storage/VFS integration path.

**Raspberry Pi 4 / SD or eMMC:**
- Implement the eMMC/SD driver behind the board storage functions.
- Keep raw eMMC helpers board-owned and expose only `board_storage_*` to generic
  VFS code.

### Phase F: Input

The common input queue can be filled by UART, virtio-input, or USB HID.

- [ ] Stub unsupported virtio-input paths with clean failure values.
- [ ] For QEMU-like boards, implement `board_virtio_input_init()` and
      `board_virtio_input_poll()` through the virtio-input driver.
- [ ] For real boards, prefer USB HID once the host controller is visible.

### Phase G: Display

**Option 1: Linear framebuffer (simplest)**
- Find framebuffer base address from DTB or firmware.
- Feed the resolved base address, dimensions, pitch, and pixel format into the
  framebuffer driver.

**Option 2: Raspberry Pi mailbox (RPi 4/5)**
```c
uint32_t mailbox_fb_init(uint32_t width, uint32_t height) {
    // Query framebuffer from VideoCore firmware via mailbox properties.
}
```

**Option 3: DRM/KMS-like display controller work**
- Parse the display controller from DTB.
- Implement a minimal mode-set path only for hardware with open documentation.
- Do not start here for the first hardware boot.

---

## Device Tree (DTB)

QEMU and most ARM boards pass a Device Tree Blob (DTB) to the kernel. ArmoniOS
currently uses minimal DTB parsing for RAM and selected board/runtime discovery.
Keep board-specific fallbacks in the board layer when a device tree is missing
or incomplete.

The DTB parser lives in `kernel/dtb.c`. Keep it small and add host tests before
expanding its supported FDT surface.

---

## Release Safety For Porting Work

Porting must not regress the current QEMU baseline. Before merging board-layer,
linker, storage, input, or boot changes, run the relevant gates from
`docs/ROADMAP.md`:

```sh
make
make size
make -C tests test
make stack-check
make qemu-fs-test
timeout 25s make qemu-fb
timeout 25s make qemu-usb
timeout 25s make qemu-net
```

For real hardware work, add a short serial log note to the relevant issue or
document. Do not claim Raspberry Pi support in README until a real board reaches
a documented boot milestone.
