#ifndef ARMONIOS_KERNEL_SYSCALL_INTERNAL_H
#define ARMONIOS_KERNEL_SYSCALL_INTERNAL_H

#include <stdint.h>

#include "include/armonios/abi/vfs.h"
#include "kernel/exceptions.h"
#include "kernel/process.h"

#define FD_STDIN     ARM_FD_STDIN
#define FD_STDOUT    ARM_FD_STDOUT
#define FD_STDERR    ARM_FD_STDERR
#define FD_FILE_BASE ARM_FD_FILE_BASE

int64_t sys_write(process_t *process, uint64_t fd, uint64_t buf,
                  uint64_t len);
int64_t sys_open(process_t *process, uint64_t path_ptr, uint64_t flags);
int64_t sys_close(uint64_t fd);
int64_t sys_read(process_t *process, uint64_t fd, uint64_t buf,
                 uint64_t len);
int64_t sys_seek(uint64_t fd, uint64_t offset, uint64_t whence);
int64_t sys_stat(process_t *process, uint64_t path_ptr, uint64_t stat_ptr);
int64_t sys_readdir(process_t *process, uint64_t path_ptr, uint64_t buf,
                    uint64_t len);
int64_t sys_stat_v2(process_t *process, uint64_t path_ptr,
                    uint64_t stat_ptr);
int64_t sys_readdir_v2(process_t *process, uint64_t path_ptr,
                       uint64_t start_index, uint64_t entries_ptr,
                       uint64_t max_entries);
int64_t sys_unlink(process_t *process, uint64_t path_ptr);
int64_t sys_rename(process_t *process, uint64_t old_ptr, uint64_t new_ptr);

int64_t sys_spawn(process_t *process, uint64_t path_ptr, uint64_t entry_index);
int64_t sys_spawn_argv(process_t *process, uint64_t path_ptr,
                       uint64_t entry_index, uint64_t argv_ptr,
                       uint64_t argc);
int64_t sys_wait(process_t *process, uint64_t pid);
int64_t sys_kill(uint64_t pid);
int64_t sys_munmap(process_t *process, uint64_t addr, uint64_t size);
int64_t sys_mmap(process_t *process, uint64_t hint, uint64_t size,
                 uint64_t flags);
int sys_yield_process(exception_frame_t *frame);
void sys_exit(exception_frame_t *frame, uint64_t code);

int64_t sys_ipc_send(process_t *process, uint64_t target_pid, uint64_t buf,
                     uint64_t len);
int64_t sys_ipc_recv(process_t *process, uint64_t buf, uint64_t capacity);

int64_t sys_window_create(process_t *process, uint64_t x, uint64_t y,
                          uint64_t w, uint64_t h, uint64_t bg,
                          uint64_t border, uint64_t title_ptr);
int64_t sys_window_destroy(process_t *process, uint64_t window_id);
int64_t sys_window_draw_text(process_t *process, uint64_t window_id,
                             uint64_t x, uint64_t y, uint64_t color,
                             uint64_t str_ptr);
int64_t sys_window_draw_rect(process_t *process, uint64_t window_id,
                             uint64_t x, uint64_t y, uint64_t w,
                             uint64_t h, uint64_t color);
int64_t sys_window_set_title(process_t *process, uint64_t window_id,
                             uint64_t title_ptr, uint64_t title_h);
int64_t sys_window_redraw(process_t *process, uint64_t window_id);
int64_t sys_window_focus(process_t *process, uint64_t window_id);
int64_t sys_window_for_pid(process_t *process, uint64_t owner_pid,
                           uint64_t index);
int64_t sys_cursor_set_shape(process_t *process, uint64_t shape);
int64_t sys_cursor_register_region(process_t *process, uint64_t win,
                                   uint64_t slot, uint64_t x, uint64_t y,
                                   uint64_t w, uint64_t h, uint64_t shape);
int64_t sys_window_flush(process_t *process, uint64_t window_id,
                         uint64_t x, uint64_t y, uint64_t w, uint64_t h);
int64_t sys_window_get_bounds(process_t *process, uint64_t window_id,
                              uint64_t out_ptr);
int64_t sys_window_set_bounds(process_t *process, uint64_t window_id,
                              uint64_t x, uint64_t y, uint64_t w,
                              uint64_t h);
int64_t sys_window_minimize(process_t *process, uint64_t window_id);
int64_t sys_window_restore(process_t *process, uint64_t window_id);
int64_t sys_window_state(process_t *process, uint64_t window_id,
                         uint64_t out_ptr);
int64_t sys_window_event(process_t *process, uint64_t window_id,
                         uint64_t buf_ptr, uint64_t buf_count);

int64_t sys_timeinfo(process_t *process, uint64_t info_ptr);
int64_t sys_meminfo(process_t *process, uint64_t info_ptr);
int64_t sys_proclist(process_t *process, uint64_t entries_ptr,
                     uint64_t max_entries);

#endif
