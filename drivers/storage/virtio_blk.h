#ifndef ARMONIOS_DRIVERS_STORAGE_VIRTIO_BLK_H
#define ARMONIOS_DRIVERS_STORAGE_VIRTIO_BLK_H

#include <stdint.h>

typedef struct {
    uint64_t capacity_sectors;
    uint32_t version;
    uint32_t vendor_id;
} virtio_blk_info_t;

typedef struct {
    uint64_t base;
    uint32_t queue_size;
    uint16_t last_used_idx;
    uint8_t ready;
} virtio_blk_device_t;

int virtio_blk_probe(uint64_t base, virtio_blk_info_t *info);
int virtio_blk_probe_range(uint64_t base, uint64_t size, uint64_t stride,
                           uint64_t *found_base, virtio_blk_info_t *info);
int virtio_blk_init(virtio_blk_device_t *device, uint64_t base);
int virtio_blk_read_sector(virtio_blk_device_t *device, uint64_t sector,
                           void *buffer);
int virtio_blk_write_sector(virtio_blk_device_t *device, uint64_t sector,
                            const void *buffer);

#endif
