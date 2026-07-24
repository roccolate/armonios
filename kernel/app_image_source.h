#ifndef ARMONIOS_KERNEL_APP_IMAGE_SOURCE_H
#define ARMONIOS_KERNEL_APP_IMAGE_SOURCE_H

#include <stdint.h>

#include "kernel/vfs.h"

#define APP_IMAGE_NAME_MAX 32U

typedef enum {
    APP_IMAGE_SOURCE_BOOTFS = 0,
    APP_IMAGE_SOURCE_VFS = 1,
} app_image_source_kind_t;

typedef struct {
    app_image_source_kind_t kind;
    const char *path;
    char path_storage[VFS_MAX_PATH];
    char name[APP_IMAGE_NAME_MAX];
} app_image_source_t;

/*
 * Resolve one spawn path into its canonical image backend and process name.
 * `/armonios/<name>` selects the embedded bootfs registry. Every other valid
 * absolute file path selects the VFS backend and uses its final component as
 * the process name.
 */
int app_image_source_resolve(const char *path, app_image_source_t *source);

#endif
