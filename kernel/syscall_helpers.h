#ifndef KOLIBRIARM_KERNEL_SYSCALL_HELPERS_H
#define KOLIBRIARM_KERNEL_SYSCALL_HELPERS_H

#include <stdint.h>

#include "kernel/gui.h"
#include "kernel/process.h"

#define ERR_NOENT (-3LL)
#define ERR_BADF  (-5LL)
#define ERR_INVAL (-7LL)
#define ERR_AGAIN (-11LL)
#define ERR_PERM  (-13LL)

int64_t sys_owner_window(process_t *process, uint64_t window_id,
                         gui_desktop_t **out_desktop,
                         gui_window_t **out_window);
int64_t sys_owner_window_badf(process_t *process, uint64_t window_id,
                              gui_desktop_t **out_desktop,
                              gui_window_t **out_window);
int64_t sys_user_buf_in(const process_t *process, uint64_t ptr, uint64_t len);
int64_t sys_user_buf_out(const process_t *process, uint64_t ptr, uint64_t len);
int64_t sys_user_copy_cstr(const process_t *process, uint64_t ptr,
                           char *out, uint64_t capacity);

#endif
