#ifndef ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H
#define ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H

#include <stdint.h>

/*
 * KLI1 is the flat user-image format emitted by programs/apps/image.ld and
 * the per-app *_header.S / *_end.S files.
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
