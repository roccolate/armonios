#ifndef ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H
#define ARMONIOS_KERNEL_USER_IMAGE_FORMAT_H

#include "include/armonios/abi/kli.h"

/*
 * Kernel-private spellings retained as aliases of the public KLI1 ABI.
 *
 * Shipping applications are still emitted by programs/apps/image.ld and the
 * per-app *_header.S / *_end.S files. The future SDK may replace those assembly
 * wrappers with a generic packager without changing this on-disk layout.
 */
#define USER_IMAGE_MAGIC       ARM_KLI1_MAGIC
#define USER_IMAGE_MAX_ENTRIES ARM_KLI1_MAX_ENTRIES
#define USER_IMAGE_HEADER_SIZE ARM_KLI1_HEADER_SIZE

typedef arm_kli1_header_t user_flat_image_header_t;

_Static_assert(sizeof(user_flat_image_header_t) == USER_IMAGE_HEADER_SIZE,
               "KLI1 header size");
_Static_assert(USER_IMAGE_MAGIC == ARM_KLI1_MAGIC,
               "kernel/public KLI1 magic drift");
_Static_assert(USER_IMAGE_MAX_ENTRIES == ARM_KLI1_MAX_ENTRIES,
               "kernel/public KLI1 entry-count drift");

#endif
