#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "include/armonios/abi/errors.h"
#include "include/armonios/abi/vfs.h"
#include "kernel/process.h"
#include "kernel/syscall_internal.h"
#include "kernel/vfs.h"

static int g_allow_input = 1;
static int g_allow_output = 1;
static int g_backend_status = 0;
static char g_backend_path[VFS_MAX_PATH];

int64_t sys_user_copy_cstr(const process_t *process, uint64_t ptr,
                           char *out, uint64_t capacity) {
    const char *source = (const char *)(uintptr_t)ptr;
    uint64_t i = 0;

    (void)process;
    if (!g_allow_input || source == 0 || out == 0 || capacity == 0) {
        return ARMONIOS_ERR_INVAL;
    }
    while (source[i] != '\0') {
        if (i + 1U >= capacity) {
            return ARMONIOS_ERR_RANGE;
        }
        out[i] = source[i];
        i++;
    }
    out[i] = '\0';
    return 0;
}

int64_t sys_user_buf_in(const process_t *process, uint64_t ptr, uint64_t len) {
    (void)process;
    (void)len;
    return g_allow_input && ptr != 0 ? 0 : ARMONIOS_ERR_INVAL;
}

int64_t sys_user_buf_out(const process_t *process, uint64_t ptr, uint64_t len) {
    (void)process;
    (void)len;
    return g_allow_output && ptr != 0 ? 0 : ARMONIOS_ERR_PERM;
}

int64_t sys_copy_from_user(const process_t *process, void *out, uint64_t ptr,
                           uint64_t len) {
    (void)process;
    if (!g_allow_input || out == 0 || ptr == 0) {
        return ARMONIOS_ERR_INVAL;
    }
    (void)memcpy(out, (const void *)(uintptr_t)ptr, (size_t)len);
    return 0;
}

int64_t sys_copy_to_user(const process_t *process, uint64_t ptr,
                         const void *input, uint64_t len) {
    (void)process;
    if (!g_allow_output || ptr == 0 || input == 0) {
        return ARMONIOS_ERR_PERM;
    }
    (void)memcpy((void *)(uintptr_t)ptr, input, (size_t)len);
    return 0;
}

int vfs_fsinfo_v1(const char *path, arm_fsinfo_t *info) {
    if (path == 0 || info == 0 || g_backend_status != 0) {
        return g_backend_status != 0 ? g_backend_status : ARMONIOS_ERR_INVAL;
    }
    (void)snprintf(g_backend_path, sizeof(g_backend_path), "%s", path);
    info->version = ARM_FSINFO_VERSION;
    info->struct_size = sizeof(*info);
    info->total_bytes = 4096;
    info->free_bytes = 0;
    info->block_size = 512;
    info->max_name_length = 12;
    info->max_path_length = 63;
    info->flags = ARM_FS_FLAG_READ_ONLY | ARM_FS_FLAG_DIRECTORIES;
    (void)memset(info->filesystem, 0, sizeof(info->filesystem));
    (void)memcpy(info->filesystem, "fat32", 5);
    info->reserved = 0;
    return 0;
}

static int require(int condition, const char *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        return 0;
    }
    return 1;
}

static void reset_state(void) {
    g_allow_input = 1;
    g_allow_output = 1;
    g_backend_status = 0;
    g_backend_path[0] = '\0';
}

int main(void) {
    process_t process;
    arm_fsinfo_t info;
    int64_t status;

    (void)memset(&process, 0, sizeof(process));
    reset_state();
    (void)memset(&info, 0, sizeof(info));
    info.version = ARM_FSINFO_VERSION;
    info.struct_size = sizeof(info);

    status = sys_fsinfo(&process, (uint64_t)(uintptr_t)"/fat/docs",
                        (uint64_t)(uintptr_t)&info);
    if (!require(status == 0, "valid syscall") ||
        !require(strcmp(g_backend_path, "/fat/docs") == 0,
                 "backend path") ||
        !require(info.version == ARM_FSINFO_VERSION &&
                     info.struct_size == sizeof(info),
                 "returned header") ||
        !require(info.total_bytes == 4096 && info.block_size == 512,
                 "returned geometry") ||
        !require(strcmp(info.filesystem, "fat32") == 0,
                 "returned filesystem")) {
        return 1;
    }

    reset_state();
    info.version = ARM_FSINFO_VERSION + 1U;
    info.struct_size = sizeof(info);
    if (!require(sys_fsinfo(&process, (uint64_t)(uintptr_t)"/fat",
                            (uint64_t)(uintptr_t)&info) ==
                     ARMONIOS_ERR_INVAL,
                 "unknown version rejected")) {
        return 1;
    }

    reset_state();
    info.version = ARM_FSINFO_VERSION;
    info.struct_size = sizeof(info) - 8U;
    if (!require(sys_fsinfo(&process, (uint64_t)(uintptr_t)"/fat",
                            (uint64_t)(uintptr_t)&info) ==
                     ARMONIOS_ERR_INVAL,
                 "wrong size rejected")) {
        return 1;
    }

    reset_state();
    info.version = ARM_FSINFO_VERSION;
    info.struct_size = sizeof(info);
    g_allow_output = 0;
    if (!require(sys_fsinfo(&process, (uint64_t)(uintptr_t)"/fat",
                            (uint64_t)(uintptr_t)&info) ==
                     ARMONIOS_ERR_INVAL,
                 "read-only output rejected before backend")) {
        return 1;
    }

    reset_state();
    info.version = ARM_FSINFO_VERSION;
    info.struct_size = sizeof(info);
    g_backend_status = ARMONIOS_ERR_NOTSUP;
    if (!require(sys_fsinfo(&process, (uint64_t)(uintptr_t)"/legacy",
                            (uint64_t)(uintptr_t)&info) ==
                     ARMONIOS_ERR_NOTSUP,
                 "backend error propagated")) {
        return 1;
    }

    puts("PASS: filesystem information syscall boundary");
    return 0;
}
