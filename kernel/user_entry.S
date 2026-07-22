.section .text

// EL1-to-EL0 launch trampoline.
//
// The caller passes entry, stack_top, and the SPSR value prepared for EL0t.
// Callee-saved EL1 registers are parked on the kernel stack before eret so
// el0_return_address() can later return through user_enter_el0_return and
// restore the original kernel context after the user process exits or faults.
.global user_enter_el0
.type user_enter_el0, %function

// uint64_t user_enter_el0(uint64_t entry, uint64_t stack_top, uint64_t pstate)
user_enter_el0:
    stp x19, x20, [sp, #-96]!
    stp x21, x22, [sp, #16]
    stp x23, x24, [sp, #32]
    stp x25, x26, [sp, #48]
    stp x27, x28, [sp, #64]
    stp x29, x30, [sp, #80]

    msr sp_el0, x1
    msr spsr_el1, x2
    msr elr_el1, x0
    eret

.global user_enter_el0_return
.type user_enter_el0_return, %function
user_enter_el0_return:
    ldp x21, x22, [sp, #16]
    ldp x23, x24, [sp, #32]
    ldp x25, x26, [sp, #48]
    ldp x27, x28, [sp, #64]
    ldp x29, x30, [sp, #80]
    ldp x19, x20, [sp], #96
    ret
