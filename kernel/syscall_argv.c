#include "kernel/syscall_argv.h"

#include <stdint.h>

#include "kernel/syscall_helpers.h"

int64_t sys_copy_argv_from_user(const process_t *process, uint64_t argv_ptr,
                                uint32_t argc,
                                syscall_kernel_argv_t *kernel_argv) {
    uint64_t user_pointers[PANEL_BOOT_ARGV_MAX_STRINGS];
    uint32_t used = 0U;
    int64_t status;

    if (process == 0 || kernel_argv == 0 ||
        argc > PANEL_BOOT_ARGV_MAX_STRINGS) {
        return ERR_INVAL;
    }
    if (argc == 0U) {
        return argv_ptr == 0U ? 0 : ERR_INVAL;
    }
    if (argv_ptr == 0U) {
        return ERR_INVAL;
    }

    status = sys_copy_from_user(process, user_pointers, argv_ptr,
                                (uint64_t)argc * sizeof(uint64_t));
    if (status != 0) {
        return status;
    }

    for (uint32_t i = 0; i < argc; i++) {
        uint32_t length = 0U;
        uint32_t remaining;

        if (user_pointers[i] == 0U || used >= PANEL_BOOT_ARGV_MAX_BYTES) {
            return ERR_INVAL;
        }
        remaining = PANEL_BOOT_ARGV_MAX_BYTES - used;
        status = sys_user_copy_cstr(process, user_pointers[i],
                                    &kernel_argv->bytes[used], remaining);
        if (status != 0) {
            return status;
        }

        while (length < remaining &&
               kernel_argv->bytes[used + length] != '\0') {
            length++;
        }
        if (length >= remaining) {
            return ERR_INVAL;
        }
        length++;

        kernel_argv->pointers[i] =
            (uint64_t)(uintptr_t)&kernel_argv->bytes[used];
        used += length;
    }

    for (uint32_t i = argc; i < PANEL_BOOT_ARGV_MAX_STRINGS; i++) {
        kernel_argv->pointers[i] = 0U;
    }
    return 0;
}
