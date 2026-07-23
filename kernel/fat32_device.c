#include "kernel/fat32.h"

#include <stdint.h>

static int fat32_device_read_sector(void *context, uint32_t lba,
                                    uint8_t *buffer) {
    const block_device_t *device = (const block_device_t *)context;

    return block_device_read(device, lba, 1U, buffer);
}

static int fat32_device_write_sector(void *context, uint32_t lba,
                                     const uint8_t *buffer) {
    const block_device_t *device = (const block_device_t *)context;

    return block_device_write(device, lba, 1U, buffer);
}

int fat32_mount_device(fat32_fs_t *fs, const block_device_t *device) {
    if (fs == 0 || device == 0 || device->read == 0 ||
        device->block_count == 0U || device->block_size != FAT32_SECTOR_SIZE ||
        ((device->flags & BLOCK_DEVICE_FLAG_READ_ONLY) == 0U &&
         device->write == 0)) {
        return -1;
    }

    if (fat32_mount(fs, fat32_device_read_sector, (void *)device) != 0) {
        return -1;
    }

    if ((uint64_t)fs->total_sectors > device->block_count) {
        fs->mounted = 0U;
        return -1;
    }

    if (!block_device_is_read_only(device)) {
        fat32_set_write_sector(fs, fat32_device_write_sector);
    }
    return 0;
}

int fat32_flush(fat32_fs_t *fs) {
    if (fs == 0 || fs->mounted == 0U) {
        return -1;
    }

    /* Legacy callback mounts have no flush contract. */
    if (fs->read_sector != fat32_device_read_sector) {
        return 0;
    }

    return block_device_flush((const block_device_t *)fs->context);
}
