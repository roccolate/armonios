#ifndef KOLIBRIARM_DRIVERS_BOARD_H
#define KOLIBRIARM_DRIVERS_BOARD_H

#include <stdint.h>

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

#endif
