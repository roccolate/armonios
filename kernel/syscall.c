#include "kernel/syscall.h"

#include <stdint.h>

#include "kernel/user_demo.h"
#include "uart/pl011.h"

#define SYS_EXIT  1ULL
#define SYS_WRITE 43ULL

#define FD_STDOUT 1ULL
#define FD_STDERR 2ULL

#define ERR_BADF  (-5LL)
#define ERR_INVAL (-7LL)

#define SPSR_EL1H_MASKED 0x3c5ULL

static int user_range_contains(uint64_t ptr, uint64_t len) {
    uint64_t end;

    if (len == 0) {
        return 1;
    }

    end = ptr + len;
    if (end < ptr) {
        return 0;
    }

    return user_demo_range_contains(ptr, end);
}

static int64_t sys_write(uint64_t fd, uint64_t buf, uint64_t len) {
    const char *text = (const char *)(uintptr_t)buf;

    if (fd != FD_STDOUT && fd != FD_STDERR) {
        return ERR_BADF;
    }

    if (!user_range_contains(buf, len)) {
        return ERR_INVAL;
    }

    for (uint64_t i = 0; i < len; i++) {
        uart_putc(text[i]);
    }

    return (int64_t)len;
}

static void sys_exit(exception_frame_t *frame, uint64_t code) {
    uart_puts("USER exit: ");
    static const char digits[] = "0123456789abcdef";
    uart_puts("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        uart_putc(digits[(code >> shift) & 0xf]);
    }
    uart_puts("\n");

    frame->x[0] = code;
    frame->elr = user_demo_return_address();
    frame->spsr = SPSR_EL1H_MASKED;
}

void syscall_dispatch(exception_frame_t *frame) {
    switch (frame->x[8]) {
    case SYS_EXIT:
        sys_exit(frame, frame->x[0]);
        break;
    case SYS_WRITE:
        frame->x[0] = (uint64_t)sys_write(frame->x[0], frame->x[1], frame->x[2]);
        break;
    default:
        frame->x[0] = (uint64_t)ERR_INVAL;
        break;
    }
}
