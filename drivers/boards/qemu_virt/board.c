#include "boards/qemu_virt/board.h"

#include "irq/gicv2.h"
#include "kernel/mm/vmm.h"
#include "uart/pl011.h"

const char *board_name(void) {
    return "qemu-virt";
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

    return status;
}
