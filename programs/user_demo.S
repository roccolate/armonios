.section .user.image.header, "a"
.global user_demo_image_header
user_demo_image_header:
    .word 0x31494c4b
    .hword 48
    .hword 3
    .quad user_demo_image_end - user_demo_image_header
    .quad user_hello_start - user_demo_image_header
    .quad user_hello_second_start - user_demo_image_header
    .quad user_fault_start - user_demo_image_header
    .quad 0

.section .user.image.text, "ax"
.global user_hello_start
.type user_hello_start, %function
.global user_hello_second_start
.type user_hello_second_start, %function
.global user_fault_start
.type user_fault_start, %function

user_hello_start:
    mov x8, #3
    svc #0
    cmp x0, #1
    b.eq 2f
    mov x8, #1
    svc #0

2:
    movz x4, #0x4000
    movk x4, #0x0060, lsl #16
4:
    subs x4, x4, #1
    b.ne 4b

    mov x0, #0
    mov x1, #4096
    mov x2, #0
    mov x8, #20
    svc #0
    tbnz x0, #63, 3f
    mov x3, x0
    mov w5, #0x41
    strb w5, [x3]
    ldrb w6, [x3]
    cmp w6, w5
    b.ne 3f

    mov x0, x3
    mov x1, #4096
    mov x8, #21
    svc #0
    cbnz x0, 3f

    mov x0, #1
    adr x1, user_hello_msg
    mov x2, #15
    mov x8, #43
    svc #0

    mov x0, #0
    mov x8, #1
    svc #0

3:
    mov x8, #1
    svc #0

1:
    wfe
    b 1b

user_hello_second_start:
    mov x8, #3
    svc #0
    cmp x0, #2
    b.eq 5f
    mov x8, #1
    svc #0

5:
    mov x0, #1
    adr x1, user_hello_second_msg
    mov x2, #16
    mov x8, #43
    svc #0

    mov x0, #0
    mov x8, #1
    svc #0

6:
    wfe
    b 6b

user_fault_start:
    mov x8, #3
    svc #0
    cmp x0, #3
    b.eq 7f
    mov x8, #1
    svc #0

7:
    movz x0, #0x0000
    movk x0, #0x00f0, lsl #16
    str xzr, [x0]

    mov x0, #0x55
    mov x8, #1
    svc #0

8:
    wfe
    b 8b

.section .user.image.rodata, "a"
user_hello_msg:
    .ascii "Hello from EL0\n"
user_hello_second_msg:
    .ascii "Hello again EL0\n"
user_demo_image_end:
