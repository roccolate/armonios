#ifndef ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H
#define ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H

#include <stdint.h>

/*
 * KLI1 is the flat user-image format emitted by programs/apps/image.ld and
 * the per-app *_header.S / *_end.S files. Mutable static state is forbidden
 * in the image: programs/apps/image.ld ASSERTs that SIZEOF(.data) and
 * SIZEOF(.bss) are zero, and tools/verify.sh runs tests/run_kli1_contract_test.sh
 * to confirm both the shipping apps and a regression .bss source are rejected.
 * Apps obtain mutable storage at runtime through SYS_MMAP.
 */
#define USER_IMAGE_MAGIC       0x31494c4bU
#define USER_IMAGE_MAX_ENTRIES 8U
#define USER_IMAGE_HEADER_SIZE 80U

typedef struct {
    uint32_t magic;
    uint16_t header_size;
    uint16_t entry_count;
    uint64_t image_size;
    uint64_t entry_offsets[USER_IMAGE_MAX_ENTRIES];
} user_flat_image_header_t;

_Static_assert(sizeof(user_flat_image_header_t) == USER_IMAGE_HEADER_SIZE,
               "KLI1 header size");

#endif
