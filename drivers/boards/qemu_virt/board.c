#include "boards/qemu_virt/board.h"

#include "gpu/virtio_gpu.h"
#include "input/virtio_input.h"
#include "irq/gicv2.h"
#include "kernel/mm/vmm.h"
#include "kernel/runtime_service.h"
#include "storage/virtio_blk.h"
#include "uart/pl011.h"

static virtio_blk_device_t g_blk_dev;
static virtio_input_device_t g_input_dev;
static uint64_t g_display_base;
static uint8_t g_display_ready;

int board_storage_read(uint32_t lba, uint32_t count, void *buffer) {
    uint8_t *bytes = (uint8_t *)buffer;

    if (buffer == 0 && count != 0) {
        return -1;
    }
    if (count != 0 && lba > UINT32_MAX - (count - 1U)) {
        return -1;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (virtio_blk_read_sector(&g_blk_dev, lba + i,
                                   bytes + i * 512U) != 0) {
            return -1;
        }
    }
    return 0;
}

int board_storage_write(uint32_t lba, uint32_t count, const void *buffer) {
    const uint8_t *bytes = (const uint8_t *)buffer;

    if (buffer == 0 && count != 0) {
        return -1;
    }
    if (count != 0 && lba > UINT32_MAX - (count - 1U)) {
        return -1;
    }
    for (uint32_t i = 0; i < count; i++) {
        if (virtio_blk_write_sector(&g_blk_dev, lba + i,
                                    bytes + i * 512U) != 0) {
            return -1;
        }
    }
    return 0;
}

int board_storage_init(void) {
    virtio_blk_info_t info;
    uint64_t base;

    if (virtio_blk_probe_range(board_virtio_mmio_base(),
                               board_virtio_mmio_size(),
                               board_virtio_mmio_stride(), &base,
                               &info) != 0) {
        return -1;
    }

    return virtio_blk_init(&g_blk_dev, base);
}

const char *board_name(void) {
    return "qemu-virt";
}

uint32_t board_capabilities(void) {
    return BOARD_CAP_STORAGE | BOARD_CAP_DISPLAY | BOARD_CAP_INPUT |
           BOARD_CAP_NET | BOARD_CAP_PCI | BOARD_CAP_USB;
}

void board_early_init(void) {
    uart_init(QEMU_VIRT_UART0_BASE);
}

void board_irq_init(void) {
    gicv2_init(QEMU_VIRT_GIC_DIST_BASE, QEMU_VIRT_GIC_CPU_BASE);
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
    return QEMU_VIRT_UART0_IRQ;
}

uint64_t board_virtio_mmio_base(void) {
    return QEMU_VIRT_VIRTIO_MMIO_BASE;
}

uint64_t board_virtio_mmio_size(void) {
    return QEMU_VIRT_VIRTIO_MMIO_SIZE;
}

uint64_t board_virtio_mmio_stride(void) {
    return QEMU_VIRT_VIRTIO_MMIO_STRIDE;
}

int board_map_mmio(uint64_t *pgd) {
    int status = vmm_map_page(pgd, QEMU_VIRT_UART0_BASE, QEMU_VIRT_UART0_BASE,
                              VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);

    if (status == 0) {
        status = vmm_map_range(pgd, QEMU_VIRT_GIC_DIST_BASE,
                               QEMU_VIRT_GIC_DIST_BASE,
                               QEMU_VIRT_GIC_MMIO_SIZE,
                               VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    }

    if (status == 0) {
        status = vmm_map_range(pgd, QEMU_VIRT_VIRTIO_MMIO_BASE,
                               QEMU_VIRT_VIRTIO_MMIO_BASE,
                               QEMU_VIRT_VIRTIO_MMIO_SIZE,
                               VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    }

    if (status == 0) {
        status = vmm_map_range(pgd, QEMU_VIRT_PCIE_ECAM_BASE,
                               QEMU_VIRT_PCIE_ECAM_BASE,
                               QEMU_VIRT_PCIE_ECAM_SIZE,
                               VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    }
    if (status != 0) {
        uart_puts("ECAM: fail\n");
    } else {
        uart_puts("ECAM: ok\n");
    }

    if (status == 0) {
        status = vmm_map_range(pgd, QEMU_VIRT_PCIE_MMIO_BASE,
                               QEMU_VIRT_PCIE_MMIO_BASE,
                               QEMU_VIRT_PCIE_MMIO_SIZE,
                               VMM_FLAG_READ | VMM_FLAG_WRITE | VMM_FLAG_DEVICE);
    }

    return status;
}

int board_display_init(board_display_draw_fn_t draw, void *context) {
    uint64_t gpu_base;

    g_display_base = 0;
    g_display_ready = 0;
    if (draw == 0) {
        return -1;
    }
    if (virtio_gpu_probe_range(QEMU_VIRT_VIRTIO_MMIO_BASE,
                               QEMU_VIRT_VIRTIO_MMIO_SIZE,
                               QEMU_VIRT_VIRTIO_MMIO_STRIDE,
                               &gpu_base) != 0) {
        return -1;
    }
    if (virtio_gpu_draw(gpu_base, draw, context) != 0) {
        return -1;
    }
    g_display_base = gpu_base;
    g_display_ready = 1;
    return 0;
}

int board_display_redraw(board_display_draw_fn_t draw, void *context) {
    int status;

    if (draw == 0 || g_display_ready == 0U) {
        return -1;
    }
    status = virtio_gpu_draw(g_display_base, draw, context);
    if (status == 0) {
        runtime_service_report_metric(RUNTIME_METRIC_REDRAW, 1U);
    }
    return status;
}

uint32_t board_input_irq(void) {
    return QEMU_VIRT_VIRTIO_INPUT_IRQ;
}

int board_input_init(void) {
    uint64_t input_base;

    if (virtio_input_probe_range(QEMU_VIRT_VIRTIO_MMIO_BASE,
                                 QEMU_VIRT_VIRTIO_MMIO_SIZE,
                                 QEMU_VIRT_VIRTIO_MMIO_STRIDE,
                                 &input_base) != 0) {
        return -1;
    }

    return virtio_input_init(&g_input_dev, input_base);
}

int board_input_poll(void) {
    int events = virtio_input_poll(&g_input_dev);

    if (events > 0) {
        runtime_service_report_metric(RUNTIME_METRIC_INPUT_PRODUCED,
                                      (uint32_t)events);
    }
    return events;
}
