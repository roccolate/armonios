#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "include/armonios/abi/errors.h"
#include "include/armonios/abi/syscall_numbers.h"
#include "include/armonios/abi/version.h"
#include "include/armonios/abi/vfs.h"
#include "kernel/process.h"
#include "kernel/vfs.h"

process_t *process_current(void) {
    return 0;
}

process_t *process_find(uint32_t pid) {
    (void)pid;
    return 0;
}

typedef struct {
    uint32_t calls;
    char last_path[VFS_MAX_PATH];
} fsinfo_mount_t;

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static int copy_text(char *destination, uint32_t capacity,
                     const char *source) {
    uint32_t i = 0;

    if (destination == 0 || source == 0 || capacity == 0) {
        return -1;
    }
    while (source[i] != '\0') {
        if (i + 1U >= capacity) {
            return -1;
        }
        destination[i] = source[i];
        i++;
    }
    destination[i] = '\0';
    return 0;
}

static int mock_fsinfo(void *context, const char *path,
                       vfs_fsinfo_t *info) {
    fsinfo_mount_t *mount = (fsinfo_mount_t *)context;

    if (mount == 0 || path == 0 || info == 0 ||
        copy_text(mount->last_path, sizeof(mount->last_path), path) != 0 ||
        copy_text(info->filesystem, sizeof(info->filesystem), "mockfs") != 0) {
        return ARMONIOS_ERR_INVAL;
    }
    mount->calls++;
    info->total_bytes = 8192;
    info->free_bytes = 2048;
    info->block_size = 512;
    info->max_name_length = 63;
    info->max_path_length = 63;
    info->flags = VFS_FS_FLAG_READ_ONLY |
                  VFS_FS_FLAG_DIRECTORIES |
                  VFS_FS_FLAG_FREE_BYTES_VALID;
    return 0;
}

static int legacy_list(void *context, uint64_t offset, uint8_t *buffer,
                       uint64_t capacity, uint64_t *bytes_written) {
    (void)context;
    (void)offset;
    (void)buffer;
    (void)capacity;
    if (bytes_written != 0) {
        *bytes_written = 0;
    }
    return 0;
}

int main(void) {
    fsinfo_mount_t mount = {0};
    vfs_mount_ops_t ops = {
        .fsinfo = mock_fsinfo,
    };
    vfs_fsinfo_t native;
    arm_fsinfo_t public_info;

    if (!require(ARMONIOS_ABI_VERSION == 0x00010000U,
                 "global ABI remains 1.0") ||
        !require(SYS_FSINFO == 51ULL, "fsinfo syscall number") ||
        !require(sizeof(arm_fsinfo_t) == 64U, "fsinfo ABI layout") ||
        !require(ARMONIOS_ERR_EXIST == -14LL, "EXIST value") ||
        !require(ARMONIOS_ERR_NOTDIR == -15LL, "NOTDIR value") ||
        !require(ARMONIOS_ERR_ISDIR == -16LL, "ISDIR value") ||
        !require(ARMONIOS_ERR_NOTEMPTY == -17LL, "NOTEMPTY value") ||
        !require(ARMONIOS_ERR_NOSPC == -18LL, "NOSPC value") ||
        !require(ARMONIOS_ERR_ROFS == -19LL, "ROFS value") ||
        !require(ARMONIOS_ERR_NOTSUP == -20LL, "NOTSUP value") ||
        !require(ARMONIOS_ERR_RANGE == -21LL, "RANGE value")) {
        return 1;
    }

    vfs_reset();
    if (!require(vfs_mount("/disk", &ops, &mount) == 0, "mount fsinfo") ||
        !require(vfs_mount_list("/legacy", legacy_list, 0) == 0,
                 "legacy mount") ||
        !require(vfs_fsinfo("/disk//folder/../item", &native) == 0,
                 "native fsinfo") ||
        !require(strcmp(mount.last_path, "/disk/item") == 0,
                 "canonical callback path") ||
        !require(native.total_bytes == 8192 && native.free_bytes == 2048,
                 "native capacity") ||
        !require(native.block_size == 512 &&
                     native.max_name_length == 63 &&
                     native.max_path_length == 63,
                 "native limits") ||
        !require(strcmp(native.filesystem, "mockfs") == 0,
                 "native filesystem name") ||
        !require((native.flags & VFS_FS_FLAG_READ_ONLY) != 0 &&
                     (native.flags & VFS_FS_FLAG_DIRECTORIES) != 0 &&
                     (native.flags & VFS_FS_FLAG_FREE_BYTES_VALID) != 0,
                 "native flags")) {
        return 1;
    }

    if (!require(vfs_fsinfo_v1("/disk/item", &public_info) == 0,
                 "public fsinfo adapter") ||
        !require(public_info.version == ARM_FSINFO_VERSION,
                 "public version") ||
        !require(public_info.struct_size == sizeof(public_info),
                 "public struct size") ||
        !require(public_info.total_bytes == 8192 &&
                     public_info.free_bytes == 2048,
                 "public capacity") ||
        !require(public_info.flags == native.flags,
                 "public flags") ||
        !require(strcmp(public_info.filesystem, "mockfs") == 0,
                 "public filesystem name") ||
        !require(public_info.reserved == 0, "reserved zero")) {
        return 1;
    }

    if (!require(vfs_fsinfo("/missing", &native) == ARMONIOS_ERR_NOENT,
                 "missing mount error") ||
        !require(vfs_fsinfo("/diskette", &native) == ARMONIOS_ERR_NOENT,
                 "component boundary") ||
        !require(vfs_fsinfo("/legacy", &native) == ARMONIOS_ERR_NOTSUP,
                 "unsupported mount error") ||
        !require(vfs_fsinfo("relative", &native) == ARMONIOS_ERR_INVAL,
                 "invalid path error")) {
        return 1;
    }

    puts("PASS: filesystem errors and fsinfo ABI");
    return 0;
}
