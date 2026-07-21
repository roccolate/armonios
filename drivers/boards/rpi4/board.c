#include "boards/rpi4/board.h"

#include "firmware/rpi_mailbox.h"
#include "irq/gicv2.h"
#include "kernel/mm/vmm.h"
#include "kernel/print.h"
#include "uart/pl011.h"

static uint32_t g_emmc2_clock_hz;

const char *board_name(void) {
    return "rpi4-bcm2711";
}

uint32_t board_capabilities(void) {
    /*
     * The RPi4 backend remains a serial-only build target until physical
     * hardware evidence exists. The EMMC2 controller core is host-tested, but
     * storage must not be advertised before repeatable card reads on hardware.
     */
    return 0U;
}

void board_early_init(void) {
    uart_init(RPI4_UART0_BASE);

    if (rpi_mailbox_get_clock_rate(RPI4_MAILBOX_BASE,
                                   RPI_FIRMWARE_CLOCK_EMMC2,
                                   &g_emmc2_clock_hz) == RPI_MAILBOX_OK) {
        uart_puts("EMMC2 clock: ");
        print_dec64(g_emmc2_clock_hz);
        uart_puts("\n");
    } else {
        g_emmc2_clock_hz = 0U;
        uart_puts("EMMC2 clock: unavailable\n");
    }
}

void board_irq_init(void) {
    gicv2_init(RPI4_GIC_DIST_BASE, RPI4_GIC_CPU_BASE);
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
    return RPI4_UART0_IRQ;
}

uint64_t board_virtio_mmio_base(void) {
    return RPI4_VIRTIO_MMIO_BASE;
}

uint64_t board_virtio_mmio_size(void) {
    return RPI4_VIRTIO_MMIO_SIZE;
}

uint64_t board_virtio_mmio_stride(void) {
    return RPI4_VIRTIO_MMIO_STRIDE;
}

int board_map_mmio(uint64_t *pgd) {
    int status;

    status = vmm_map_page(pgd, RPI4_UART0_BASE, RPI4_UART0_BASE,
                          VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    if (status != 0) {
        return status;
    }

    status = vmm_map_range(pgd, RPI4_GIC_DIST_BASE,
                           RPI4_GIC_DIST_BASE,
                           RPI4_GIC_MMIO_SIZE,
                           VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    if (status != 0) {
        return status;
    }

    status = vmm_map_range(pgd, RPI4_MAILBOX_BASE,
                           RPI4_MAILBOX_BASE,
                           RPI4_MAILBOX_SIZE,
                           VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    if (status != 0) {
        return status;
    }

    return 0;
}

/*
 * Storage intentionally fails closed. The clock query is diagnostic only;
 * EMMC2 MMIO is not mapped and the generic storage path cannot invoke the
 * host-tested controller until physical read evidence exists.
 */
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

int board_storage_init(void) {
    return -1;
}

int board_display_init(board_display_draw_fn_t draw, void *context) {
    (void)draw;
    (void)context;
    return -1;
}

int board_display_redraw(board_display_draw_fn_t draw, void *context) {
    (void)draw;
    (void)context;
    return -1;
}

/*
 * RPi4 input is not wired yet. These safe failures keep the generic kernel
 * running on serial-only bring-up while the board contract stays complete.
 */
uint32_t board_input_irq(void) {
    return 0U;
}

int board_input_init(void) {
    return -1;
}

int board_input_poll(void) {
    return -1;
}
