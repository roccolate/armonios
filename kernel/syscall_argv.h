#ifndef ARMONIOS_KERNEL_SYSCALL_ARGV_H
#define ARMONIOS_KERNEL_SYSCALL_ARGV_H

#include <stdint.h>

#include "kernel/panel_boot_argv.h"
#include "kernel/process.h"
#include "kernel/syscall_helpers.h"

/* Copy the EL0 pointer array and all strings into a pointer-free kernel block. */
static inline int64_t sys_copy_argv_from_user(
    const process_t *process, uint64_t argv_ptr, uint32_t argc,
    panel_boot_argv_t *kernel_argv) {
    uint32_t used = 0U;

    if (process == 0 || kernel_argv == 0 ||
        argc > PANEL_BOOT_ARGV_MAX_STRINGS ||
        (argc == 0U && argv_ptr != 0U) ||
        (argc != 0U && argv_ptr == 0U)) {
        return ERR_INVAL;
    }

    for (uint32_t i = 0; i < argc; i++) {
        uint64_t user_pointer;
        uint64_t offset = (uint64_t)i * sizeof(uint64_t);
        int64_t status;

        if (argv_ptr > UINT64_MAX - offset ||
            sys_copy_from_user(process, &user_pointer, argv_ptr + offset,
                               sizeof(user_pointer)) != 0 ||
            user_pointer == 0U || used >= PANEL_BOOT_ARGV_MAX_BYTES) {
            return ERR_INVAL;
        }

        kernel_argv->offsets[i] = (uint16_t)used;
        status = sys_user_copy_cstr(process, user_pointer,
                                    &kernel_argv->bytes[used],
                                    PANEL_BOOT_ARGV_MAX_BYTES - used);
        if (status != 0) {
            return status;
        }
        while (kernel_argv->bytes[used++] != '\0') {
        }
    }

    kernel_argv->bytes_used = (uint16_t)used;
    return 0;
}

#endif
