#include "storage/block_view.h"

#include <stdint.h>

int block_view_init(block_view_t *view, block_view_read_fn_t read_sector,
                    block_view_write_fn_t write_sector, void *context,
                    uint32_t base_lba, uint32_t sector_count) {
    if (view == 0 || read_sector == 0 || sector_count == 0U ||
        base_lba > UINT32_MAX - (sector_count - 1U)) {
        return -1;
    }

    view->read_sector = read_sector;
    view->write_sector = write_sector;
    view->context = context;
    view->base_lba = base_lba;
    view->sector_count = sector_count;
    return 0;
}

int block_view_read_sector(void *context, uint32_t lba, uint8_t *buffer) {
    block_view_t *view = (block_view_t *)context;

    if (view == 0 || view->read_sector == 0 || buffer == 0 ||
        lba >= view->sector_count) {
        return -1;
    }

    return view->read_sector(view->context, view->base_lba + lba, buffer);
}

int block_view_write_sector(void *context, uint32_t lba,
                            const uint8_t *buffer) {
    block_view_t *view = (block_view_t *)context;

    if (view == 0 || view->write_sector == 0 || buffer == 0 ||
        lba >= view->sector_count) {
        return -1;
    }

    return view->write_sector(view->context, view->base_lba + lba, buffer);
}
