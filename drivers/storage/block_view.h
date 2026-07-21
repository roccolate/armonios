#ifndef ARMONIOS_DRIVERS_STORAGE_BLOCK_VIEW_H
#define ARMONIOS_DRIVERS_STORAGE_BLOCK_VIEW_H

#include <stdint.h>

typedef int (*block_view_read_fn_t)(void *context, uint32_t lba,
                                    uint8_t *buffer);
typedef int (*block_view_write_fn_t)(void *context, uint32_t lba,
                                     const uint8_t *buffer);

typedef struct {
    block_view_read_fn_t read_sector;
    block_view_write_fn_t write_sector;
    void *context;
    uint32_t base_lba;
    uint32_t sector_count;
} block_view_t;

int block_view_init(block_view_t *view, block_view_read_fn_t read_sector,
                    block_view_write_fn_t write_sector, void *context,
                    uint32_t base_lba, uint32_t sector_count);
int block_view_read_sector(void *context, uint32_t lba, uint8_t *buffer);
int block_view_write_sector(void *context, uint32_t lba,
                            const uint8_t *buffer);

#endif
