#include "boards/rpi4/board.h"

#include "firmware/rpi_mailbox.h"
#include "irq/gicv2.h"
#include "kernel/mm/vmm.h"
#include "kernel/print.h"
#include "uart/pl011.h"

#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
#include "boards/rpi4/emmc2_probe_diag.h"
#include "storage/emmc.h"
#endif

static uint32_t g_emmc2_clock_hz;

#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
static emmc_device_t g_emmc2_probe_device;
static rpi4_emmc2_probe_diag_t g_emmc2_probe_diag;
static uint8_t g_emmc2_sector0[EMMC_BLKSZ] __attribute__((aligned(16)));

static void rpi4_emmc2_probe_delay_us(void *context, uint32_t usec) {
    uint64_t frequency;
    uint64_t start;
    uint64_t now;
    uint64_t ticks;

    (void)context;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(frequency));
    __asm__ volatile("mrs %0, cntpct_el0" : "=r"(start));
    ticks = (frequency * usec + 999999U) / 1000000U;
    do {
        __asm__ volatile("mrs %0, cntpct_el0" : "=r"(now));
    } while (now - start < ticks);
}

static const char *rpi4_emmc2_probe_command_name(uint32_t command) {
    switch (command) {
    case 0U:
        return "CMD0";
    case 2U:
        return "CMD2";
    case 3U:
        return "CMD3";
    case 7U:
        return "CMD7";
    case 8U:
        return "CMD8";
    case 9U:
        return "CMD9";
    case 16U:
        return "CMD16";
    case 17U:
        return "CMD17";
    case 41U:
        return "ACMD41";
    case 55U:
        return "CMD55";
    default:
        return "CMD?";
    }
}

static void rpi4_emmc2_probe_print_diag(void) {
    rpi4_emmc2_probe_diag_refresh(&g_emmc2_probe_diag);

    uart_puts("EMMC2 probe: diag command ");
    if (g_emmc2_probe_diag.last_command == RPI4_EMMC2_DIAG_NO_COMMAND) {
        uart_puts("none");
    } else {
        uart_puts(rpi4_emmc2_probe_command_name(
            g_emmc2_probe_diag.last_command));
        uart_puts(" ");
        print_dec64(g_emmc2_probe_diag.last_command);
    }
    uart_puts(" argument ");
    print_hex64(g_emmc2_probe_diag.last_argument);
    uart_puts("\n");

    uart_puts("EMMC2 probe: diag read-offset ");
    print_hex64(g_emmc2_probe_diag.last_read_offset);
    uart_puts(" write-offset ");
    print_hex64(g_emmc2_probe_diag.last_write_offset);
    uart_puts(" write-value ");
    print_hex64(g_emmc2_probe_diag.last_write_value);
    uart_puts("\n");

    uart_puts("EMMC2 probe: diag present ");
    print_hex64(g_emmc2_probe_diag.present_state);
    uart_puts(" clock-reset ");
    print_hex64(g_emmc2_probe_diag.clock_reset);
    uart_puts(" host-power ");
    print_hex64(g_emmc2_probe_diag.host_power);
    uart_puts(" actual-clock ");
    print_dec64(g_emmc2_probe_device.actual_clock_hz);
    uart_puts("\n");

    uart_puts("EMMC2 probe: diag int-now ");
    print_hex64(g_emmc2_probe_diag.interrupt_status);
    uart_puts(" int-last ");
    print_hex64(g_emmc2_probe_diag.last_nonzero_interrupt_status);
    uart_puts("\n");
}

static void rpi4_emmc2_probe(void) {
    emmc_io_t io;
    int result;

    if (g_emmc2_clock_hz == 0U) {
        uart_puts("EMMC2 probe: clock unavailable\n");
        return;
    }

    rpi4_emmc2_probe_diag_init(&g_emmc2_probe_diag,
                               (uintptr_t)RPI4_EMMC_BASE);
    io.read32 = rpi4_emmc2_probe_diag_read32;
    io.write32 = rpi4_emmc2_probe_diag_write32;
    io.delay_us = rpi4_emmc2_probe_delay_us;
    io.context = &g_emmc2_probe_diag;

    uart_puts("EMMC2 probe: begin\n");
    uart_puts("EMMC2 probe: broken-cd assume-present\n");
    result = emmc_init_with_io(&g_emmc2_probe_device, &io,
                               g_emmc2_clock_hz);
    uart_puts("EMMC2 probe: init ");
    print_signed32(result);
    uart_puts("\n");
    if (result != EMMC_OK) {
        rpi4_emmc2_probe_print_diag();
        return;
    }

    result = emmc_read_sector(&g_emmc2_probe_device, 0U, 1U,
                              g_emmc2_sector0);
    uart_puts("EMMC2 probe: read0 ");
    print_signed32(result);
    uart_puts("\n");
    if (result != EMMC_OK) {
        rpi4_emmc2_probe_print_diag();
        return;
    }

    uart_puts("EMMC2 probe: first16 ");
    for (uint32_t i = 0; i < 16U; i++) {
        print_hex8(g_emmc2_sector0[i]);
    }
    uart_puts("\n");
    uart_puts("EMMC2 probe: signature ");
    print_hex8(g_emmc2_sector0[510]);
    print_hex8(g_emmc2_sector0[511]);
    uart_puts("\n");
}
#endif

const char *board_name(void) {
    return "rpi4-bcm2711";
}

uint32_t board_capabilities(void) {
    /*
     * The RPi4 backend remains a serial-only build target until physical
     * hardware evidence exists. The opt-in probe never advertises storage to
     * generic kernel code.
     */
    return 0U;
}

void board_early_init(void) {
    int mailbox_result;

    uart_init(RPI4_UART0_BASE);

    mailbox_result = rpi_mailbox_get_clock_rate(
        RPI4_MAILBOX_BASE, RPI4_MAILBOX_BUS_ALIAS,
        RPI_FIRMWARE_CLOCK_EMMC2, &g_emmc2_clock_hz);
    if (mailbox_result == RPI_MAILBOX_OK) {
        uart_puts("EMMC2 clock: ");
        print_dec64(g_emmc2_clock_hz);
        uart_puts("\n");
    } else {
        g_emmc2_clock_hz = 0U;
        uart_puts("EMMC2 clock: unavailable ");
        print_signed32(mailbox_result);
        uart_puts("\n");
    }

#if defined(ARMONIOS_RPI4_EMMC2_PROBE)
    rpi4_emmc2_probe();
#endif
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
 * Storage intentionally fails closed. Even the opt-in sector probe runs only
 * during early board diagnostics and never exposes the device through these
 * generic entry points.
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
