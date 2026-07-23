#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/syscall_helpers.h"
#include "kernel/vfs.h"

#define SYSCALL_READDIR_V2_MAX_ENTRIES 8U

typedef struct {
    uint32_t version;
    uint32_t struct_size;
} vfs_metadata_header_t;

int64_t sys_stat_v2(process_t *process, uint64_t path_ptr,
                    uint64_t stat_ptr) {
    char path[VFS_MAX_PATH];
    vfs_metadata_header_t header;
    arm_stat_v2_t stat;

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_in(process, stat_ptr, sizeof(header)) != 0 ||
        sys_copy_from_user(process, &header, stat_ptr, sizeof(header)) != 0 ||
        header.version != ARM_VFS_METADATA_VERSION ||
        header.struct_size != sizeof(stat) ||
        sys_user_buf_out(process, stat_ptr, sizeof(stat)) != 0) {
        return ERR_INVAL;
    }

    if (vfs_stat_v2(path, &stat) != 0) {
        return ERR_NOENT;
    }

    return sys_copy_to_user(process, stat_ptr, &stat, sizeof(stat)) == 0
               ? 0
               : ERR_INVAL;
}

int64_t sys_readdir_v2(process_t *process, uint64_t path_ptr,
                       uint64_t start_index, uint64_t entries_ptr,
                       uint64_t max_entries) {
    char path[VFS_MAX_PATH];
    arm_dirent_v2_t entries[SYSCALL_READDIR_V2_MAX_ENTRIES];
    uint64_t written = 0;
    uint64_t bytes;

    if (max_entries == 0 ||
        max_entries > SYSCALL_READDIR_V2_MAX_ENTRIES ||
        max_entries > UINT64_MAX / sizeof(arm_dirent_v2_t)) {
        return ERR_INVAL;
    }
    bytes = max_entries * sizeof(arm_dirent_v2_t);

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_out(process, entries_ptr, bytes) != 0) {
        return ERR_INVAL;
    }

    if (vfs_readdir_v2(path, start_index, entries, max_entries, &written) != 0) {
        return ERR_NOENT;
    }
    if (written > max_entries) {
        return ERR_INVAL;
    }

    if (written != 0 &&
        sys_copy_to_user(process, entries_ptr, entries,
                         written * sizeof(arm_dirent_v2_t)) != 0) {
        return ERR_INVAL;
    }
    return (int64_t)written;
}
