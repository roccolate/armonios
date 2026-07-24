#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/syscall_helpers.h"
#include "kernel/vfs.h"

typedef struct {
    uint32_t version;
    uint32_t struct_size;
} vfs_fsinfo_header_t;

int64_t sys_fsinfo(process_t *process, uint64_t path_ptr,
                   uint64_t info_ptr) {
    char path[VFS_MAX_PATH];
    vfs_fsinfo_header_t header;
    arm_fsinfo_t info;
    int status;

    if (sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0 ||
        sys_user_buf_in(process, info_ptr, sizeof(header)) != 0 ||
        sys_copy_from_user(process, &header, info_ptr, sizeof(header)) != 0 ||
        header.version != ARM_FSINFO_VERSION ||
        header.struct_size != sizeof(info) ||
        sys_user_buf_out(process, info_ptr, sizeof(info)) != 0) {
        return ERR_INVAL;
    }

    status = vfs_fsinfo_v1(path, &info);
    if (status != 0) {
        return status;
    }

    return sys_copy_to_user(process, info_ptr, &info, sizeof(info)) == 0
               ? 0
               : ERR_INVAL;
}
