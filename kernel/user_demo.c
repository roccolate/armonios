#include "kernel/user_demo.h"

#include <stdint.h>

#include "uart/pl011.h"

#define USER_STACK_SIZE 4096ULL

extern char __user_demo_start[];
extern char __user_demo_end[];
extern void user_hello_start(void);
extern uint64_t user_enter_el0(uint64_t entry, uint64_t stack_top);
extern char user_enter_el0_return[];

static uint8_t g_user_stack[USER_STACK_SIZE] __attribute__((aligned(16)));

static int range_contains(uint64_t range_start, uint64_t range_end,
                          uint64_t start, uint64_t end) {
    return start >= range_start && end >= start && end <= range_end;
}

uint64_t user_demo_return_address(void) {
    return (uint64_t)(uintptr_t)user_enter_el0_return;
}

int user_demo_range_contains(uint64_t start, uint64_t end) {
    uint64_t image_start = (uint64_t)(uintptr_t)__user_demo_start;
    uint64_t image_end = (uint64_t)(uintptr_t)__user_demo_end;
    uint64_t stack_start = (uint64_t)(uintptr_t)g_user_stack;
    uint64_t stack_end = stack_start + USER_STACK_SIZE;

    return range_contains(image_start, image_end, start, end) ||
           range_contains(stack_start, stack_end, start, end);
}

uint64_t user_demo_run(void) {
    uint64_t stack_top = (uint64_t)(uintptr_t)g_user_stack + USER_STACK_SIZE;
    uint64_t exit_code;

    uart_puts("USER demo: entering EL0\n");
    exit_code = user_enter_el0((uint64_t)(uintptr_t)user_hello_start, stack_top);
    uart_puts("USER demo: returned to EL1\n");

    return exit_code;
}
