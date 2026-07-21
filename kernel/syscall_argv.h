#ifndef ARMONIOS_KERNEL_SYSCALL_ARGV_H
#define ARMONIOS_KERNEL_SYSCALL_ARGV_H

#include <stdint.h>

#include "kernel/panel_boot_argv.h"
#include "kernel/process.h"

typedef struct {
    uint64_t pointers[PANEL_BOOT_ARGV_MAX_STRINGS];
    char bytes[PANEL_BOOT_ARGV_MAX_BYTES];
} syscall_kernel_argv_t;

/*
 * Copy an EL0 argv pointer array and every referenced string into bounded,
 * kernel-owned storage. `pointers` is then safe to pass synchronously to the
 * app loader; none of its entries reference the source process.
 */
int64_t sys_copy_argv_from_user(const process_t *process, uint64_t argv_ptr,
                                uint32_t argc,
                                syscall_kernel_argv_t *kernel_argv);

#endif
