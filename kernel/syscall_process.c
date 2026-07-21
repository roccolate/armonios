#include "kernel/syscall_internal.h"

#include <stdint.h>

#include "kernel/aarch64_state.h"
#include "kernel/panel_boot.h"
#include "kernel/panel_boot_argv.h"
#include "kernel/print.h"
#include "kernel/sched/sched.h"
#include "kernel/syscall_helpers.h"
#include "kernel/user_exit.h"
#include "kernel/user_vm.h"
#include "kernel/vfs.h"
#include "uart/pl011.h"

int64_t sys_spawn(process_t *process, uint64_t path_ptr,
                  uint64_t entry_index) {
    char path[VFS_MAX_PATH];
    int pid;

    if (entry_index > UINT32_MAX ||
        sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0) {
        return ERR_INVAL;
    }

    pid = app_spawn_vfs(path, (uint32_t)entry_index, 0, 0);
    if (pid < 0) {
        return ERR_NOENT;
    }

    return pid;
}

int64_t sys_spawn_argv(process_t *process, uint64_t path_ptr,
                       uint64_t entry_index, uint64_t argv_ptr,
                       uint64_t argc) {
    panel_boot_argv_t kernel_argv;
    char path[VFS_MAX_PATH];
    int64_t status;
    int pid;

    if (entry_index > UINT32_MAX || argc > PANEL_BOOT_ARGV_MAX_STRINGS ||
        sys_user_copy_cstr(process, path_ptr, path, sizeof(path)) != 0) {
        return ERR_INVAL;
    }

    status = sys_copy_argv_from_user(process, argv_ptr, (uint32_t)argc,
                                     &kernel_argv);
    if (status != 0) {
        return status;
    }

    pid = app_spawn_vfs(path, (uint32_t)entry_index,
                        argc == 0U ? 0 : &kernel_argv,
                        (uint32_t)argc);
    if (pid < 0) {
        return ERR_NOENT;
    }

    return pid;
}

int64_t sys_wait(uint64_t pid) {
    uint64_t exit_code = 0;
    process_t *process;

    if (pid == 0 || pid > UINT32_MAX) {
        return ERR_INVAL;
    }

    process = process_find((uint32_t)pid);
    if (process == 0) {
        return ERR_NOENT;
    }

    if (process->state != PROCESS_ZOMBIE) {
        return ERR_AGAIN;
    }

    if (process_wait_zombie((uint32_t)pid, &exit_code) != 0) {
        return ERR_INVAL;
    }

    return (int64_t)exit_code;
}

int64_t sys_kill(uint64_t pid) {
    if (pid == 0 || pid > UINT32_MAX) {
        return ERR_INVAL;
    }

    if (process_find((uint32_t)pid) == 0) {
        return ERR_NOENT;
    }

    if (process_kill((uint32_t)pid, KERNEL_USER_KILL_EXIT_CODE) != 0) {
        return ERR_INVAL;
    }

    return 0;
}

int64_t sys_munmap(process_t *process, uint64_t addr, uint64_t size) {
    return user_vm_unmap_anonymous(process, addr, size);
}

int64_t sys_mmap(process_t *process, uint64_t hint, uint64_t size,
                 uint64_t flags) {
    return user_vm_map_anonymous(process, hint, size, flags);
}

int sys_yield_process(exception_frame_t *frame) {
    process_t *current = process_current();

    if (current == 0) {
        return 0;
    }
    current->regs[0] = 0;
    return process_dispatch_next(current, frame, PROCESS_DISPATCH_PREEMPT);
}

void sys_exit(exception_frame_t *frame, uint64_t code) {
    process_t *current = process_current();

    process_mark_exited(current, code);

    uart_puts("USER exit: ");
    print_hex64(code);
    uart_puts("\n");

    if (process_dispatch_next(current, frame, PROCESS_DISPATCH_EXIT) != 0) {
        return;
    }

    frame->x[0] = code;
    frame->elr = el0_return_address();
    frame->spsr = AARCH64_SPSR_EL1H_DAIF_MASKED;
}
