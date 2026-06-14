#ifndef KOLIBRIARM_DRIVERS_GPU_VIRTIO_GPU_H
#define KOLIBRIARM_DRIVERS_GPU_VIRTIO_GPU_H

#include <stdint.h>

int virtio_gpu_probe_range(uint64_t base, uint64_t size, uint64_t stride,
                           uint64_t *found_base);
int virtio_gpu_draw_test_pattern(uint64_t base);

#endif
