#ifndef ARMONIOS_DRIVERS_BOARD_H
#define ARMONIOS_DRIVERS_BOARD_H

#include <stdint.h>

#include "fb/fb.h"

#define BOARD_CAP_STORAGE  (1U << 0)
#define BOARD_CAP_DISPLAY  (1U << 1)
#define BOARD_CAP_INPUT    (1U << 2)
#define BOARD_CAP_NET      (1U << 3)
#define BOARD_CAP_PCI      (1U << 4)
#define BOARD_CAP_USB      (1U << 5)

typedef void (*board_display_draw_fn_t)(fb_t *fb, void *context);

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

#endif
