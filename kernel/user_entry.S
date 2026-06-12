.section .text
.global user_enter_el0
.type user_enter_el0, %function

// uint64_t user_enter_el0(uint64_t entry, uint64_t stack_top)
user_enter_el0:
    sub sp, sp, #16
    str x30, [sp]

    msr sp_el0, x1
    mov x2, #0x3c0
    msr spsr_el1, x2
    msr elr_el1, x0
    eret

.global user_enter_el0_return
.type user_enter_el0_return, %function
user_enter_el0_return:
    ldr x30, [sp]
    add sp, sp, #16
    ret

.section .user.text, "ax"
.global user_hello_start
.type user_hello_start, %function

user_hello_start:
    mov x0, #1
    adr x1, user_hello_msg
    mov x2, #15
    mov x8, #43
    svc #0

    mov x0, #0
    mov x8, #1
    svc #0

1:
    wfe
    b 1b

.section .user.rodata, "a"
user_hello_msg:
    .ascii "Hello from EL0\n"
