#ifndef ARMONIOS_DRIVERS_STORAGE_VIRTIO_BLK_BLOCK_DEVICE_H
#define ARMONIOS_DRIVERS_STORAGE_VIRTIO_BLK_BLOCK_DEVICE_H

#include <stdint.h>

#include "storage/block_device.h"
#include "storage/virtio_blk.h"

#define VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE 512U

static inline int virtio_blk_block_device_read(void *context,
                                                uint64_t first_block,
                                                uint32_t block_count,
                                                void *buffer) {
    virtio_blk_device_t *device = (virtio_blk_device_t *)context;
    uint8_t *bytes = (uint8_t *)buffer;

    if (device == 0 || device->ready == 0U || buffer == 0 ||
        block_count == 0U ||
        first_block > UINT64_MAX - ((uint64_t)block_count - 1U)) {
        return -1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        if (virtio_blk_read_sector(
                device, first_block + i,
                bytes + (uint64_t)i * VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

static inline int virtio_blk_block_device_write(void *context,
                                                 uint64_t first_block,
                                                 uint32_t block_count,
                                                 const void *buffer) {
    virtio_blk_device_t *device = (virtio_blk_device_t *)context;
    const uint8_t *bytes = (const uint8_t *)buffer;

    if (device == 0 || device->ready == 0U || buffer == 0 ||
        block_count == 0U ||
        first_block > UINT64_MAX - ((uint64_t)block_count - 1U)) {
        return -1;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        if (virtio_blk_write_sector(
                device, first_block + i,
                bytes + (uint64_t)i * VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE) != 0) {
            return -1;
        }
    }
    return 0;
}

static inline int virtio_blk_block_device_init(block_device_t *out,
                                                virtio_blk_device_t *device,
                                                uint64_t capacity_sectors) {
    if (out == 0 || device == 0 || device->ready == 0U ||
        capacity_sectors == 0U) {
        return -1;
    }

    out->read = virtio_blk_block_device_read;
    out->write = virtio_blk_block_device_write;
    out->flush = 0;
    out->context = device;
    out->block_count = capacity_sectors;
    out->block_size = VIRTIO_BLK_BLOCK_DEVICE_BLOCK_SIZE;
    out->flags = 0U;
    return 0;
}

#endif
