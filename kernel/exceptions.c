#include "kernel/exceptions.h"

#include <stdint.h>

#include "kernel/aarch64_state.h"
#include "kernel/print.h"
#include "kernel/process.h"
#include "kernel/syscall.h"
#include "kernel/panel_boot.h"
#include "kernel/user_exit.h"
#include "uart/pl011.h"

/*
 * EL1 exception diagnostics and lower-EL synchronous dispatch.
 *
 * SVC exceptions are syscall entries. Other lower-EL synchronous exceptions are
 * treated as user faults: the process becomes a zombie, owned resources are
 * released through process_mark_exited, and the scheduler gets a chance to run
 * the next ready process before falling back to the EL0 return trampoline.
 */

#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK  0x3fULL
#define ESR_EC_SVC64 0x15ULL

static const char *exception_name(uint64_t kind) {
    switch (kind) {
    case 0:
        return "current EL SP0 sync";
    case 1:
        return "current EL SP0 irq";
    case 2:
        return "current EL SP0 fiq";
    case 3:
        return "current EL SP0 serror";
    case 4:
        return "current EL SPx sync";
    case 5:
        return "current EL SPx irq";
    case 6:
        return "current EL SPx fiq";
    case 7:
        return "current EL SPx serror";
    case 8:
        return "lower EL AArch64 sync";
    case 9:
        return "lower EL AArch64 irq";
    case 10:
        return "lower EL AArch64 fiq";
    case 11:
        return "lower EL AArch64 serror";
    case 12:
        return "lower EL AArch32 sync";
    case 13:
        return "lower EL AArch32 irq";
    case 14:
        return "lower EL AArch32 fiq";
    case 15:
        return "lower EL AArch32 serror";
    default:
        return "unknown";
    }
}

static void print_saved_reg(const char *name, uint64_t value) {
    uart_puts(name);
    print_hex64(value);
    uart_puts("\n");
}

void exception_handler(exception_frame_t *frame, uint64_t esr, uint64_t far,
                       uint64_t kind) {
    uint64_t sp_val;
    uint64_t elr = frame == 0 ? 0 : frame->elr;

    __asm__ volatile("mov %0, sp" : "=r"(sp_val));

    uart_puts("\n");
    uart_puts("================================================================\n");
    uart_puts("KERNEL PANIC: unhandled exception in EL1\n");
    uart_puts("================================================================\n");
    uart_puts("kind:    ");
    uart_puts(exception_name(kind));
    uart_puts("\n");
    print_saved_reg("ESR_EL1: ", esr);
    print_saved_reg("ELR_EL1: ", elr);
    print_saved_reg("FAR_EL1: ", far);
    print_saved_reg("SP_EL1:  ", sp_val);

    if (frame != 0) {
        print_saved_reg("X0:      ", frame->x[0]);
        print_saved_reg("X1:      ", frame->x[1]);
        print_saved_reg("X2:      ", frame->x[2]);
        print_saved_reg("X3:      ", frame->x[3]);
        print_saved_reg("X19:     ", frame->x[19]);
        print_saved_reg("X20:     ", frame->x[20]);
        print_saved_reg("X21:     ", frame->x[21]);
        print_saved_reg("X29:     ", frame->x[29]);
        print_saved_reg("X30:     ", frame->x[30]);
        print_saved_reg("SPSR:    ", frame->spsr);
        print_saved_reg("SP_EL0:  ", frame->sp_el0);
    }

    uart_puts("================================================================\n");
    uart_puts("SYSTEM HALTED\n");
    uart_puts("================================================================\n");
    uart_puts("__PANIC_HALT__\n");
    uart_puts("\n");

    /* Halt every CPU. QEMU's -no-reboot or -action panic=shutdown will
     * turn this wfe loop into a clean guest exit; otherwise the wfe
     * burns cycles without observable side effects. */
    for (;;) {
        __asm__ volatile("wfe");
    }
}

static void handle_user_fault(exception_frame_t *frame, uint64_t esr,
                              uint64_t far) {
    process_t *current = process_current();

    if (current == 0 || frame == 0) {
        exception_handler(frame, esr, far, 8);
        return;
    }

    process_save_context(current, frame->x, frame->elr, frame->spsr,
                         frame->sp_el0);
    process_mark_exited(current, KERNEL_USER_FAULT_EXIT_CODE);

    uart_puts("USER fault pid: ");
    print_hex64(current->pid);
    uart_puts(" ESR: ");
    print_hex64(esr);
    uart_puts(" FAR: ");
    print_hex64(far);
    uart_puts("\n");

    /*
     * Note: the process's GUI windows are destroyed by
     * process_mark_exited above (centralised cleanup), so we do
     * not need to call gui_destroy_windows_for_pid here.
     */

    if (process_dispatch_next(current, frame, PROCESS_DISPATCH_EXIT) != 0) {
        return;
    }

    frame->x[0] = KERNEL_USER_FAULT_EXIT_CODE;
    frame->elr = el0_return_address();
    frame->spsr = AARCH64_SPSR_EL1H_DAIF_MASKED;
}

void exception_lower_sync_handler(exception_frame_t *frame, uint64_t esr,
                                  uint64_t far) {
    uint64_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;

    if (ec == ESR_EC_SVC64) {
        syscall_dispatch(frame);
        return;
    }

    handle_user_fault(frame, esr, far);
}
