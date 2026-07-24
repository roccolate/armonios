#ifndef ARMONIOS_KERNEL_SYSCALL_HELPERS_H
#define ARMONIOS_KERNEL_SYSCALL_HELPERS_H

/*
 * Public helper contract for syscall implementations.
 *
 * Error values are shared by all syscall bodies and documented in
 * docs/SYSCALLS.md. The helper functions below keep user-buffer validation,
 * checked EL0/EL1 copies, argv import, and GUI window ownership centralized
 * instead of duplicated in each syscall case.
 */

#include <stdint.h>

#include "include/armonios/abi/errors.h"
#include "kernel/gui.h"
#include "kernel/panel_boot_argv.h"
#include "kernel/process.h"

/*
 * Compatibility names for existing kernel syscall implementations. Public
 * userland code should use ARMONIOS_ERR_* through libkarm rather than including
 * this kernel-private helper header.
 */
#define ERR_NOENT    ARMONIOS_ERR_NOENT
#define ERR_BADF     ARMONIOS_ERR_BADF
#define ERR_INVAL    ARMONIOS_ERR_INVAL
#define ERR_AGAIN    ARMONIOS_ERR_AGAIN
#define ERR_PERM     ARMONIOS_ERR_PERM
#define ERR_EXIST    ARMONIOS_ERR_EXIST
#define ERR_NOTDIR   ARMONIOS_ERR_NOTDIR
#define ERR_ISDIR    ARMONIOS_ERR_ISDIR
#define ERR_NOTEMPTY ARMONIOS_ERR_NOTEMPTY
#define ERR_NOSPC    ARMONIOS_ERR_NOSPC
#define ERR_ROFS     ARMONIOS_ERR_ROFS
#define ERR_NOTSUP   ARMONIOS_ERR_NOTSUP
#define ERR_RANGE    ARMONIOS_ERR_RANGE

int64_t sys_owner_window(process_t *process, uint64_t window_id,
                          gui_desktop_t **out_desktop,
                          gui_window_t **out_window);
int64_t sys_owner_window_badf(process_t *process, uint64_t window_id,
                               gui_desktop_t **out_desktop,
                               gui_window_t **out_window);
int64_t sys_user_buf_in(const process_t *process, uint64_t ptr, uint64_t len);
int64_t sys_user_buf_out(const process_t *process, uint64_t ptr, uint64_t len);
int64_t sys_copy_from_user(const process_t *process, void *out, uint64_t ptr,
                            uint64_t len);
int64_t sys_copy_to_user(const process_t *process, uint64_t ptr,
                          const void *input, uint64_t len);
int64_t sys_user_copy_cstr(const process_t *process, uint64_t ptr,
                            char *out, uint64_t capacity);
int64_t sys_copy_argv_from_user(const process_t *process, uint64_t argv_ptr,
                                 uint32_t argc, panel_boot_argv_t *kernel_argv);

#endif
