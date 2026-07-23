#ifndef ARMONIOS_DRIVERS_STORAGE_BLOCK_DEVICE_H
#define ARMONIOS_DRIVERS_STORAGE_BLOCK_DEVICE_H

#include <stdint.h>

#define BLOCK_DEVICE_FLAG_READ_ONLY (1U << 0)

typedef int (*block_device_read_fn_t)(void *context, uint64_t first_block,
                                      uint32_t block_count, void *buffer);
typedef int (*block_device_write_fn_t)(void *context, uint64_t first_block,
                                       uint32_t block_count,
                                       const void *buffer);
typedef int (*block_device_flush_fn_t)(void *context);

typedef struct {
    block_device_read_fn_t read;
    block_device_write_fn_t write;
    block_device_flush_fn_t flush;
    void *context;
    uint64_t block_count;
    uint32_t block_size;
    uint32_t flags;
} block_device_t;

typedef struct {
    block_device_t device;
    const block_device_t *parent;
    uint64_t base_block;
} block_device_view_t;

int block_device_init(block_device_t *device, block_device_read_fn_t read,
                      block_device_write_fn_t write,
                      block_device_flush_fn_t flush, void *context,
                      uint64_t block_count, uint32_t block_size,
                      uint32_t flags);
int block_device_read(const block_device_t *device, uint64_t first_block,
                      uint32_t block_count, void *buffer);
int block_device_write(const block_device_t *device, uint64_t first_block,
                       uint32_t block_count, const void *buffer);
int block_device_flush(const block_device_t *device);
int block_device_is_read_only(const block_device_t *device);

int block_device_view_init(block_device_view_t *view,
                           const block_device_t *parent,
                           uint64_t base_block, uint64_t block_count,
                           uint32_t flags);
const block_device_t *block_device_view_device(
    const block_device_view_t *view);

#endif
