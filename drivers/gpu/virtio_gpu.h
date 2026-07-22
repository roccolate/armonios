#ifndef ARMONIOS_DRIVERS_GPU_VIRTIO_GPU_H
#define ARMONIOS_DRIVERS_GPU_VIRTIO_GPU_H

#include <stdint.h>

#include "fb/fb.h"

typedef void (*virtio_gpu_render_fn_t)(fb_t *fb, void *context);

int virtio_gpu_probe_range(uint64_t base, uint64_t size, uint64_t stride,
                           uint64_t *found_base);
int virtio_gpu_draw(uint64_t base, virtio_gpu_render_fn_t render,
                    void *context);
int virtio_gpu_draw_test_pattern(uint64_t base);

#endif
