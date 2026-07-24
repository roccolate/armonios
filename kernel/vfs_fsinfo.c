#include "kernel/vfs.h"

#include <stdint.h>

#include "include/armonios/abi/errors.h"

static void vfs_public_clear_fsinfo(arm_fsinfo_t *info) {
    info->version = ARM_FSINFO_VERSION;
    info->struct_size = sizeof(*info);
    info->total_bytes = 0;
    info->free_bytes = 0;
    info->block_size = 0;
    info->max_name_length = 0;
    info->max_path_length = 0;
    info->flags = 0;
    for (uint32_t i = 0; i < ARM_FS_NAME_MAX; i++) {
        info->filesystem[i] = '\0';
    }
    info->reserved = 0;
}

static int vfs_public_copy_filesystem(char destination[ARM_FS_NAME_MAX],
                                      const char source[VFS_FILESYSTEM_NAME_MAX]) {
    uint32_t i = 0;

    if (destination == 0 || source == 0 || source[0] == '\0') {
        return ARMONIOS_ERR_RANGE;
    }
    while (i < VFS_FILESYSTEM_NAME_MAX && source[i] != '\0') {
        if (i + 1U >= ARM_FS_NAME_MAX) {
            return ARMONIOS_ERR_RANGE;
        }
        destination[i] = source[i];
        i++;
    }
    if (i == VFS_FILESYSTEM_NAME_MAX) {
        return ARMONIOS_ERR_RANGE;
    }
    destination[i] = '\0';
    return 0;
}

int vfs_fsinfo_v1(const char *path, arm_fsinfo_t *info) {
    vfs_fsinfo_t native;
    int status;

    if (path == 0 || info == 0) {
        return ARMONIOS_ERR_INVAL;
    }

    status = vfs_fsinfo(path, &native);
    if (status != 0) {
        return status;
    }

    vfs_public_clear_fsinfo(info);
    info->total_bytes = native.total_bytes;
    info->free_bytes = native.free_bytes;
    info->block_size = native.block_size;
    info->max_name_length = native.max_name_length;
    info->max_path_length = native.max_path_length;
    info->flags = native.flags;
    return vfs_public_copy_filesystem(info->filesystem, native.filesystem);
}
