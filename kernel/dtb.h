#ifndef ARMONIOS_KERNEL_DTB_H
#define ARMONIOS_KERNEL_DTB_H

#include <stdint.h>

typedef struct {
    uint64_t base;
    uint64_t size;
} dtb_memory_t;

typedef struct {
    uint64_t base;
    uint64_t size;
    uint32_t width;
    uint32_t height;
    uint32_t stride_bytes;
} dtb_framebuffer_t;

int dtb_get_memory(uint64_t dtb_addr, dtb_memory_t *memory);
int dtb_get_framebuffer(uint64_t dtb_addr, dtb_framebuffer_t *framebuffer);
uint32_t dtb_total_size(uint64_t dtb_addr);

#endif
