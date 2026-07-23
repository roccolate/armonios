#include "storage/block_device.h"

#include <stdint.h>

static int block_device_range_valid(const block_device_t *device,
                                    uint64_t first_block,
                                    uint32_t block_count) {
    if (device == 0 || device->read == 0 || device->block_count == 0U ||
        device->block_size == 0U) {
        return 0;
    }

    if (block_count == 0U) {
        return first_block <= device->block_count;
    }

    return first_block < device->block_count &&
           (uint64_t)block_count <= device->block_count - first_block;
}

int block_device_init(block_device_t *device, block_device_read_fn_t read,
                      block_device_write_fn_t write,
                      block_device_flush_fn_t flush, void *context,
                      uint64_t block_count, uint32_t block_size,
                      uint32_t flags) {
    if (device == 0 || read == 0 || block_count == 0U || block_size == 0U ||
        (flags & ~BLOCK_DEVICE_FLAG_READ_ONLY) != 0U ||
        ((flags & BLOCK_DEVICE_FLAG_READ_ONLY) == 0U && write == 0)) {
        return -1;
    }

    device->read = read;
    device->write = write;
    device->flush = flush;
    device->context = context;
    device->block_count = block_count;
    device->block_size = block_size;
    device->flags = flags;
    return 0;
}

int block_device_read(const block_device_t *device, uint64_t first_block,
                      uint32_t block_count, void *buffer) {
    if (!block_device_range_valid(device, first_block, block_count) ||
        (block_count != 0U && buffer == 0)) {
        return -1;
    }

    if (block_count == 0U) {
        return 0;
    }

    return device->read(device->context, first_block, block_count, buffer);
}

int block_device_write(const block_device_t *device, uint64_t first_block,
                       uint32_t block_count, const void *buffer) {
    if (!block_device_range_valid(device, first_block, block_count) ||
        (block_count != 0U && buffer == 0) ||
        (device->flags & BLOCK_DEVICE_FLAG_READ_ONLY) != 0U ||
        device->write == 0) {
        return -1;
    }

    if (block_count == 0U) {
        return 0;
    }

    return device->write(device->context, first_block, block_count, buffer);
}

int block_device_flush(const block_device_t *device) {
    if (device == 0 || device->read == 0 || device->block_count == 0U ||
        device->block_size == 0U) {
        return -1;
    }

    if (device->flush == 0) {
        return 0;
    }

    return device->flush(device->context);
}

int block_device_is_read_only(const block_device_t *device) {
    return device == 0 ||
           (device->flags & BLOCK_DEVICE_FLAG_READ_ONLY) != 0U;
}

static int block_device_view_read(void *context, uint64_t first_block,
                                  uint32_t block_count, void *buffer) {
    block_device_view_t *view = (block_device_view_t *)context;

    if (view == 0 || view->parent == 0 ||
        first_block > UINT64_MAX - view->base_block) {
        return -1;
    }

    return block_device_read(view->parent, view->base_block + first_block,
                             block_count, buffer);
}

static int block_device_view_write(void *context, uint64_t first_block,
                                   uint32_t block_count,
                                   const void *buffer) {
    block_device_view_t *view = (block_device_view_t *)context;

    if (view == 0 || view->parent == 0 ||
        first_block > UINT64_MAX - view->base_block) {
        return -1;
    }

    return block_device_write(view->parent, view->base_block + first_block,
                              block_count, buffer);
}

static int block_device_view_flush(void *context) {
    block_device_view_t *view = (block_device_view_t *)context;

    if (view == 0 || view->parent == 0) {
        return -1;
    }

    return block_device_flush(view->parent);
}

int block_device_view_init(block_device_view_t *view,
                           const block_device_t *parent,
                           uint64_t base_block, uint64_t block_count,
                           uint32_t flags) {
    uint32_t child_flags;
    block_device_write_fn_t write;
    block_device_flush_fn_t flush;

    if (view == 0 || parent == 0 || parent->read == 0 ||
        parent->block_count == 0U || parent->block_size == 0U ||
        (parent->flags & ~BLOCK_DEVICE_FLAG_READ_ONLY) != 0U ||
        ((parent->flags & BLOCK_DEVICE_FLAG_READ_ONLY) == 0U &&
         parent->write == 0) ||
        (flags & ~BLOCK_DEVICE_FLAG_READ_ONLY) != 0U || block_count == 0U ||
        base_block >= parent->block_count ||
        block_count > parent->block_count - base_block) {
        return -1;
    }

    child_flags = parent->flags | flags;
    write = (child_flags & BLOCK_DEVICE_FLAG_READ_ONLY) != 0U
                ? 0
                : block_device_view_write;
    flush = parent->flush == 0 ? 0 : block_device_view_flush;

    view->parent = parent;
    view->base_block = base_block;
    return block_device_init(&view->device, block_device_view_read, write,
                             flush, view, block_count, parent->block_size,
                             child_flags);
}

const block_device_t *block_device_view_device(
    const block_device_view_t *view) {
    return view == 0 ? 0 : &view->device;
}
