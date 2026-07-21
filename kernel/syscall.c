/*
 * EL0 syscall dispatcher.
 *
 * Syscall numbers and argument registers are ABI. Domain-specific syscall
 * bodies live in syscall_*.c; this file stays at the trap boundary: pump
 * asynchronous work, save the current context, dispatch, and write back x0.
 */

#include "kernel/syscall.h"

#include <stdint.h>

#include "input/input.h"
#include "kernel/console.h"
#include "kernel/gui.h"
#include "kernel/io_service.h"
#include "kernel/net/dhcp.h"
#include "kernel/process.h"
#include "kernel/sched/sched.h"
#include "kernel/syscall_helpers.h"
#include "kernel/syscall_internal.h"
#include "kernel/syscall_numbers.h"
#include "uart/pl011.h"

static void syscall_pump_input(uint64_t syscall_number) {
    uart_pump_input();
    kernel_io_poll_input_sources(1);

    if (syscall_number != SYS_READ) {
        input_event_t drain_event;

        while (input_queue_poll(&drain_event) == 0) {
            if (drain_event.type == INPUT_EVENT_KEY_PRESS) {
                char c = (char)drain_event.data.key.key;
                gui_desktop_t *desktop = gui_desktop();

                if (desktop == 0 ||
                    desktop->focused_window_id == GUI_NO_WINDOW) {
                    console_poll_char(c);
                }
                (void)gui_handle_input(&drain_event);
            } else {
                (void)gui_handle_input(&drain_event);
            }
        }
    }
}

static uint64_t syscall_call(process_t *current, const uint64_t x[31]) {
    switch (x[8]) {
    case SYS_GETPID:
        return current == 0 ? (uint64_t)ERR_INVAL : current->pid;
    case SYS_SPAWN:
        return (uint64_t)sys_spawn(current, x[0], x[1]);
    case SYS_SPAWN_ARGV:
        return (uint64_t)sys_spawn_argv(current, x[0], x[1], x[2], x[3]);
    case SYS_WAIT:
        return (uint64_t)sys_wait(x[0]);
    case SYS_KILL:
        return (uint64_t)sys_kill(x[0]);
    case SYS_OPEN:
        return (uint64_t)sys_open(current, x[0], x[1]);
    case SYS_CLOSE:
        return (uint64_t)sys_close(x[0]);
    case SYS_READ:
        return (uint64_t)sys_read(current, x[0], x[1], x[2]);
    case SYS_MMAP:
        return (uint64_t)sys_mmap(current, x[0], x[1], x[2]);
    case SYS_MUNMAP:
        return (uint64_t)sys_munmap(current, x[0], x[1]);
    case SYS_WRITE:
        return (uint64_t)sys_write(current, x[0], x[1], x[2]);
    case SYS_SEEK:
        return (uint64_t)sys_seek(x[0], x[1], x[2]);
    case SYS_STAT:
        return (uint64_t)sys_stat(current, x[0], x[1]);
    case SYS_READDIR:
        return (uint64_t)sys_readdir(current, x[0], x[1], x[2]);
    case SYS_UNLINK:
        return (uint64_t)sys_unlink(current, x[0]);
    case SYS_RENAME:
        return (uint64_t)sys_rename(current, x[0], x[1]);
    case SYS_IPC_SEND:
        return (uint64_t)sys_ipc_send(current, x[0], x[1], x[2]);
    case SYS_IPC_RECV:
        return (uint64_t)sys_ipc_recv(current, x[0], x[1]);
    case SYS_WINDOW_CREATE:
        return (uint64_t)sys_window_create(current, x[0], x[1], x[2], x[3],
                                           x[4], x[5], x[6]);
    case SYS_WINDOW_DESTROY:
        return (uint64_t)sys_window_destroy(current, x[0]);
    case SYS_WINDOW_DRAW_TEXT:
        return (uint64_t)sys_window_draw_text(current, x[0], x[1], x[2],
                                              x[3], x[4]);
    case SYS_WINDOW_DRAW_RECT:
        return (uint64_t)sys_window_draw_rect(current, x[0], x[1], x[2],
                                              x[3], x[4], x[5]);
    case SYS_WINDOW_EVENT:
        return (uint64_t)sys_window_event(current, x[0], x[1], x[2]);
    case SYS_WINDOW_SET_TITLE:
        return (uint64_t)sys_window_set_title(current, x[0], x[1], x[2]);
    case SYS_WINDOW_REDRAW:
        return (uint64_t)sys_window_redraw(current, x[0]);
    case SYS_WINDOW_FOCUS:
        return (uint64_t)sys_window_focus(current, x[0]);
    case SYS_WINDOW_FOR_PID:
        return (uint64_t)sys_window_for_pid(current, x[0], x[1]);
    case SYS_CURSOR_SET_SHAPE:
        return (uint64_t)sys_cursor_set_shape(current, x[0]);
    case SYS_WINDOW_FLUSH:
        return (uint64_t)sys_window_flush(current, x[0], x[1], x[2],
                                          x[3], x[4]);
    case SYS_WINDOW_GET_BOUNDS:
        return (uint64_t)sys_window_get_bounds(current, x[0], x[1]);
    case SYS_WINDOW_SET_BOUNDS:
        return (uint64_t)sys_window_set_bounds(current, x[0], x[1], x[2],
                                               x[3], x[4]);
    case SYS_WINDOW_MINIMIZE:
        return (uint64_t)sys_window_minimize(current, x[0]);
    case SYS_WINDOW_RESTORE:
        return (uint64_t)sys_window_restore(current, x[0]);
    case SYS_WINDOW_STATE:
        return (uint64_t)sys_window_state(current, x[0], x[1]);
    case SYS_CURSOR_REGISTER_REGION:
        return (uint64_t)sys_cursor_register_region(current, x[0], x[1],
                                                    x[2], x[3], x[4],
                                                    x[5], x[6]);
    case SYS_TIMEINFO:
        return (uint64_t)sys_timeinfo(current, x[0]);
    case SYS_MEMINFO:
        return (uint64_t)sys_meminfo(current, x[0]);
    case SYS_PROCLIST:
        return (uint64_t)sys_proclist(current, x[0], x[1]);
    default:
        return (uint64_t)ERR_INVAL;
    }
}

void syscall_dispatch(exception_frame_t *frame) {
    process_t *current = process_current();

    syscall_pump_input(frame->x[8]);
    net_poll();

    if (current != 0) {
        process_save_context(current, frame->x, frame->elr, frame->spsr,
                             frame->sp_el0);
    }

    if (frame->x[8] == SYS_EXIT) {
        sys_exit(frame, frame->x[0]);
    } else if (frame->x[8] == SYS_YIELD) {
        if (!sys_yield_process(frame)) {
            sched_yield();
            frame->x[0] = 0;
        }
    } else {
        frame->x[0] = syscall_call(current, frame->x);
    }

    process_t *after = process_current();
    if (after != 0) {
        after->regs[0] = frame->x[0];
        after->pc = frame->elr;
        after->pstate = frame->spsr;
        after->sp = frame->sp_el0;
    }
}
