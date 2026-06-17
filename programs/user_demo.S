.section .user.image.header, "a"
.global user_demo_image_header
user_demo_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 8
    .quad user_demo_image_end - user_demo_image_header
    .quad user_hello_start - user_demo_image_header
    .quad user_hello_second_start - user_demo_image_header
    .quad user_fault_start - user_demo_image_header
    .quad user_shell_start - user_demo_image_header
    .quad user_run_hello_start - user_demo_image_header
    .quad user_run_loop_start - user_demo_image_header
    .quad user_editor_start - user_demo_image_header
    .quad user_monitor_start - user_demo_image_header

.section .user.image.text, "ax"
.global user_hello_start
.type user_hello_start, %function
.global user_hello_second_start
.type user_hello_second_start, %function
.global user_fault_start
.type user_fault_start, %function
.global user_shell_start
.type user_shell_start, %function
.global user_run_hello_start
.type user_run_hello_start, %function
.global user_run_loop_start
.type user_run_loop_start, %function
.global user_editor_start
.type user_editor_start, %function
.global user_monitor_start
.type user_monitor_start, %function

user_hello_start:
    mov x8, #3
    svc #0
    cmp x0, #1
    b.eq 2f
    mov x8, #1
    svc #0

2:
    mov x0, #2
    adr x1, user_ipc_msg
    mov x2, #13
    mov x8, #60
    svc #0

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

    sub sp, sp, #32
    adr x0, user_bootfs_path
    mov x1, sp
    mov x8, #45
    svc #0
    mov x12, #0x41
    cbnz x0, 10f

    adr x0, user_bootfs_path
    mov x1, #0
    mov x8, #40
    svc #0
    mov x12, #0x42
    tbnz x0, #63, 10f
    mov x9, x0

    mov x0, x9
    mov x1, sp
    mov x2, #4
    mov x8, #42
    svc #0
    cmp x0, #4
    mov x12, x0
    b.ne 11f

    ldr w10, [sp]
    movz w11, #0x4c4b
    movk w11, #0x3149, lsl #16
    mov x12, #0x44
    cmp w10, w11
    b.ne 11f

    mov x0, x9
    mov x8, #41
    svc #0
    mov x12, #0x45
    cbnz x0, 10f

    mov x0, #1
    adr x1, user_vfs_msg
    mov x2, #13
    mov x8, #43
    svc #0
    add sp, sp, #32

    mov x0, #1
    adr x1, user_hello_msg
    mov x2, #15
    mov x8, #43
    svc #0

    mov x0, #0
    mov x8, #1
    svc #0

11:
    mov x0, x9
    mov x8, #41
    svc #0
    mov x0, x12
    b 12f
10:
    mov x0, x12
12:
    add sp, sp, #32
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
    sub sp, sp, #16
    mov x0, sp
    mov x1, #16
    mov x8, #61
    svc #0
    tbnz x0, #63, 9f
    mov x2, x0
    mov x0, #1
    mov x1, sp
    mov x8, #43
    svc #0

9:
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

user_streq:
    ldrb w2, [x0], #1
    ldrb w3, [x1], #1
    cmp w2, w3
    b.ne 13f
    cbnz w2, user_streq
    mov x0, #1
    ret
13:
    mov x0, #0
    ret

user_print_dec:
    sub sp, sp, #32
    mov x3, sp
    mov x4, #0
    cbnz x0, 27f
    mov w5, #'0'
    strb w5, [x3]
    mov x0, #1
    mov x1, x3
    mov x2, #1
    mov x8, #43
    svc #0
    add sp, sp, #32
    ret
27:
    mov x6, #10
28:
    udiv x7, x0, x6
    msub x5, x7, x6, x0
    add w5, w5, #'0'
    strb w5, [x3, x4]
    add x4, x4, #1
    mov x0, x7
    cbnz x0, 28b
29:
    sub x4, x4, #1
    mov x0, #1
    add x1, x3, x4
    mov x2, #1
    mov x8, #43
    svc #0
    cbnz x4, 29b
    add sp, sp, #32
    ret

user_print_cstr:
    mov x3, x0
34:
    ldrb w4, [x3]
    cbz w4, 35f
    mov x0, #1
    mov x1, x3
    mov x2, #1
    mov x8, #43
    svc #0
    add x3, x3, #1
    b 34b
35:
    ret

user_shell_start:
    mov x8, #3
    svc #0
    cmp x0, #4
    b.eq 14f
    mov x8, #1
    svc #0

14:
    sub sp, sp, #96
    mov x19, sp
    mov x26, #0
    mov x4, #16
24:
    mov x8, #2
    svc #0
    subs x4, x4, #1
    b.ne 24b

    mov x0, #1
    adr x1, user_shell_banner
    mov x2, #15
    mov x8, #43
    svc #0

15:
    mov x20, #0
    mov x28, #0
    mov x0, #1
    adr x1, user_shell_prompt
    mov x2, #3
    mov x8, #43
    svc #0

16:
    mov x0, #0
    add x1, sp, #80
    mov x2, #1
    mov x8, #42
    svc #0
    cmp x0, #1
    b.eq 17f
    mov x8, #2
    svc #0
    b 16b

17:
    ldrb w21, [sp, #80]
    cbz x28, 53f
    cmp w21, #'~'
    b.eq 54f
    cmp w21, #'A'
    b.lo 16b
    cmp w21, #'Z'
    b.ls 54f
    cmp w21, #'a'
    b.lo 16b
    cmp w21, #'z'
    b.hi 16b
54:
    mov x28, #0
    b 16b
53:
    cmp w21, #27
    b.ne 55f
    mov x28, #1
    b 16b
55:
    cmp w21, #13
    b.eq 19f
    cmp w21, #10
    b.eq 19f
    cmp w21, #8
    b.eq 18f
    cmp w21, #127
    b.eq 18f
    cmp w21, #32
    b.lo 16b
    cmp w21, #126
    b.hi 16b
    cmp x20, #63
    b.hs 16b
    strb w21, [x19, x20]
    add x20, x20, #1
    mov x0, #1
    add x1, sp, #80
    mov x2, #1
    mov x8, #43
    svc #0
    b 16b

18:
    cbz x20, 16b
    sub x20, x20, #1
    mov x0, #1
    adr x1, user_shell_backspace
    mov x2, #3
    mov x8, #43
    svc #0
    b 16b

19:
    mov w22, #0
    strb w22, [x19, x20]
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    cbz x20, 15b

    mov x0, x19
    adr x1, user_shell_help_cmd
    bl user_streq
    cbnz x0, 20f

    mov x0, x19
    adr x1, user_shell_cat_cmd
    bl user_streq
    cbnz x0, 21f

    mov x0, x19
    adr x1, user_shell_cat_edit_cmd
    bl user_streq
    cbnz x0, 48f

    mov x0, x19
    adr x1, user_shell_ls_cmd
    bl user_streq
    cbnz x0, 25f

    mov x0, x19
    adr x1, user_shell_ls_root_cmd
    bl user_streq
    cbnz x0, 25f

    mov x0, x19
    adr x1, user_shell_ls_fat_cmd
    bl user_streq
    cbnz x0, 56f

    mov x0, x19
    adr x1, user_shell_mem_cmd
    bl user_streq
    cbnz x0, 30f

    mov x0, x19
    adr x1, user_shell_ps_cmd
    bl user_streq
    cbnz x0, 36f

    mov x0, x19
    adr x1, user_shell_ticks_cmd
    bl user_streq
    cbnz x0, 40f

    mov x0, x19
    adr x1, user_shell_run_hello_cmd
    bl user_streq
    cbnz x0, 42f

    mov x0, x19
    adr x1, user_shell_run_loop_cmd
    bl user_streq
    cbnz x0, 45f

    mov x0, x19
    adr x1, user_shell_run_editor_cmd
    bl user_streq
    cbnz x0, 49f

    mov x0, x19
    adr x1, user_shell_edit_cmd
    bl user_streq
    cbnz x0, 49f

    mov x0, x19
    adr x1, user_shell_run_monitor_cmd
    bl user_streq
    cbnz x0, 50f

    mov x0, x19
    adr x1, user_shell_monitor_cmd
    bl user_streq
    cbnz x0, 50f

    mov x0, x19
    adr x1, user_shell_kill_last_cmd
    bl user_streq
    cbnz x0, 46f

    mov x0, x19
    adr x1, user_shell_pwd_cmd
    bl user_streq
    cbnz x0, 32f

    mov x0, x19
    adr x1, user_shell_cd_root_cmd
    bl user_streq
    cbnz x0, 33f

    mov x0, x19
    adr x1, user_shell_exit_cmd
    bl user_streq
    cbnz x0, 23f

    mov x0, #1
    adr x1, user_shell_unknown
    mov x2, #16
    mov x8, #43
    svc #0
    b 15b

20:
    mov x0, #1
    adr x1, user_shell_help_msg
    mov x2, #138
    mov x8, #43
    svc #0
    b 15b

21:
    adr x0, user_shell_note_path
    mov x1, #0
    mov x8, #40
    svc #0
    tbnz x0, #63, 22f
    mov x9, x0
    mov x0, x9
    mov x1, x19
    mov x2, #64
    mov x8, #42
    svc #0
    tbnz x0, #63, 22f
    mov x2, x0
    mov x0, #1
    mov x1, x19
    mov x8, #43
    svc #0
    mov x0, x9
    mov x8, #41
    svc #0
    b 15b

22:
    mov x0, #1
    adr x1, user_shell_cat_error
    mov x2, #11
    mov x8, #43
    svc #0
    b 15b

25:
    adr x0, user_shell_root_path
    mov x1, x19
    mov x2, #64
    mov x8, #46
    svc #0
    tbnz x0, #63, 26f
    mov x2, x0
    mov x0, #1
    mov x1, x19
    mov x8, #43
    svc #0
    b 15b

26:
    mov x0, #1
    adr x1, user_shell_ls_error
    mov x2, #10
    mov x8, #43
    svc #0
    b 15b

56:
    adr x0, user_shell_fat_path
    mov x1, x19
    mov x2, #64
    mov x8, #46
    svc #0
    tbnz x0, #63, 26b
    mov x2, x0
    mov x0, #1
    mov x1, x19
    mov x8, #43
    svc #0
    b 15b

30:
    mov x0, x19
    mov x8, #101
    svc #0
    tbnz x0, #63, 31f
    mov x0, #1
    adr x1, user_shell_mem_total
    mov x2, #12
    mov x8, #43
    svc #0
    ldr x0, [x19]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_mem_free
    mov x2, #6
    mov x8, #43
    svc #0
    ldr x0, [x19, #8]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    b 15b

31:
    mov x0, #1
    adr x1, user_shell_mem_error
    mov x2, #11
    mov x8, #43
    svc #0
    b 15b

32:
    mov x0, #1
    adr x1, user_shell_pwd_msg
    mov x2, #2
    mov x8, #43
    svc #0
    b 15b

33:
    mov x0, #1
    adr x1, user_shell_cd_ok
    mov x2, #1
    mov x8, #43
    svc #0
    b 15b

36:
    mov x0, x19
    mov x1, #4
    mov x8, #102
    svc #0
    tbnz x0, #63, 39f
    mov x23, x0
    mov x24, x19
    mov x25, #0
    mov x0, #1
    adr x1, user_shell_ps_header
    mov x2, #15
    mov x8, #43
    svc #0
37:
    cmp x25, x23
    b.hs 15b
    ldr w0, [x24]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_space
    mov x2, #1
    mov x8, #43
    svc #0
    ldr w0, [x24, #4]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_space
    mov x2, #1
    mov x8, #43
    svc #0
    add x0, x24, #8
    bl user_print_cstr
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    add x24, x24, #24
    add x25, x25, #1
    b 37b

39:
    mov x0, #1
    adr x1, user_shell_ps_error
    mov x2, #10
    mov x8, #43
    svc #0
    b 15b

40:
    mov x0, x19
    mov x8, #100
    svc #0
    tbnz x0, #63, 41f
    mov x0, #1
    adr x1, user_shell_ticks_timer
    mov x2, #6
    mov x8, #43
    svc #0
    ldr x0, [x19]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_ticks_sched
    mov x2, #7
    mov x8, #43
    svc #0
    ldr x0, [x19, #8]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_ticks_quantums
    mov x2, #10
    mov x8, #43
    svc #0
    ldr x0, [x19, #16]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    b 15b

41:
    mov x0, #1
    adr x1, user_shell_ticks_error
    mov x2, #13
    mov x8, #43
    svc #0
    b 15b

42:
    adr x0, user_shell_run_path
    mov x1, #4
    mov x8, #4
    svc #0
    tbnz x0, #63, 43f
    mov x23, x0
    mov x0, #1
    adr x1, user_shell_spawned
    mov x2, #12
    mov x8, #43
    svc #0
    mov x0, x23
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x8, #2
    svc #0
    mov x0, x23
    mov x8, #6
    svc #0
    tbnz x0, #63, 15b
    mov x23, x0
    mov x0, #1
    adr x1, user_shell_wait_exit
    mov x2, #5
    mov x8, #43
    svc #0
    mov x0, x23
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    b 15b

43:
    mov x0, #1
    adr x1, user_shell_run_error
    mov x2, #11
    mov x8, #43
    svc #0
    b 15b

45:
    adr x0, user_shell_run_path
    mov x1, #5
    mov x8, #4
    svc #0
    tbnz x0, #63, 43b
    mov x26, x0
    mov x0, #1
    adr x1, user_shell_spawned
    mov x2, #12
    mov x8, #43
    svc #0
    mov x0, x26
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x8, #2
    svc #0
    b 15b

46:
    cbz x26, 47f
    mov x0, x26
    mov x8, #7
    svc #0
    tbnz x0, #63, 47f
    mov x0, x26
    mov x8, #6
    svc #0
    tbnz x0, #63, 47f
    mov x23, x0
    mov x0, #1
    adr x1, user_shell_killed
    mov x2, #11
    mov x8, #43
    svc #0
    mov x0, x26
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_kill_exit
    mov x2, #6
    mov x8, #43
    svc #0
    mov x0, x23
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x26, #0
    b 15b

47:
    mov x0, #1
    adr x1, user_shell_kill_error
    mov x2, #12
    mov x8, #43
    svc #0
    b 15b

48:
    adr x0, user_shell_edit_path
    mov x1, #0
    mov x8, #40
    svc #0
    tbnz x0, #63, 22b
    mov x9, x0
    mov x0, x9
    mov x1, x19
    mov x2, #64
    mov x8, #42
    svc #0
    tbnz x0, #63, 22b
    mov x2, x0
    mov x0, #1
    mov x1, x19
    mov x8, #43
    svc #0
    mov x0, x9
    mov x8, #41
    svc #0
    b 15b

49:
    adr x0, user_shell_run_path
    mov x1, #6
    mov x8, #4
    svc #0
    tbnz x0, #63, 43b
    mov x23, x0
    mov x0, #1
    adr x1, user_shell_spawned
    mov x2, #12
    mov x8, #43
    svc #0
    mov x0, x23
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x27, x23
51:
    mov x0, x27
    mov x8, #6
    svc #0
    tbz x0, #63, 15b
    mov x8, #2
    svc #0
    b 51b

50:
    adr x0, user_shell_run_path
    mov x1, #7
    mov x8, #4
    svc #0
    tbnz x0, #63, 43b
    mov x23, x0
    mov x0, #1
    adr x1, user_shell_spawned
    mov x2, #12
    mov x8, #43
    svc #0
    mov x0, x23
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x27, x23
52:
    mov x0, x27
    mov x8, #6
    svc #0
    tbz x0, #63, 15b
    mov x8, #2
    svc #0
    b 52b

23:
    add sp, sp, #96
    mov x0, #0
    mov x8, #1
    svc #0

user_run_hello_start:
    mov x0, #1
    adr x1, user_run_hello_msg
    mov x2, #15
    mov x8, #43
    svc #0
    mov x0, #0
    mov x8, #1
    svc #0

user_run_loop_start:
44:
    mov x8, #2
    svc #0
    b 44b

user_editor_start:
    sub sp, sp, #768
    mov x19, sp
    add x20, sp, #512
    mov x21, #0

    mov x0, #1
    adr x1, user_editor_banner
    mov x2, #40
    mov x8, #43
    svc #0

    adr x0, user_shell_edit_path
    mov x1, #2
    mov x8, #40
    svc #0
    tbnz x0, #63, 70f
    mov x22, x0
    mov x0, x22
    mov x1, x19
    mov x2, #512
    mov x8, #42
    svc #0
    tbnz x0, #63, 70f
    mov x21, x0
    mov x0, x22
    mov x8, #41
    svc #0
    b 71f
70:
    mov x0, #1
    adr x1, user_editor_open_error
    mov x2, #19
    mov x8, #43
    svc #0
    add sp, sp, #768
    mov x0, #1
    mov x8, #1
    svc #0

71:
    mov x0, #1
    adr x1, user_editor_loaded
    mov x2, #7
    mov x8, #43
    svc #0
    mov x0, x21
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0

72:
    mov x23, #0
    mov x28, #0
    mov x0, #1
    adr x1, user_editor_prompt
    mov x2, #6
    mov x8, #43
    svc #0

73:
    mov x0, #0
    add x1, sp, #752
    mov x2, #1
    mov x8, #42
    svc #0
    cmp x0, #1
    b.eq 74f
    mov x8, #2
    svc #0
    b 73b

74:
    ldrb w24, [sp, #752]
    cbz x28, 83f
    cmp w24, #'~'
    b.eq 84f
    cmp w24, #'A'
    b.lo 73b
    cmp w24, #'Z'
    b.ls 84f
    cmp w24, #'a'
    b.lo 73b
    cmp w24, #'z'
    b.hi 73b
84:
    mov x28, #0
    b 73b
83:
    cmp w24, #27
    b.ne 85f
    mov x28, #1
    b 73b
85:
    cmp w24, #13
    b.eq 76f
    cmp w24, #10
    b.eq 76f
    cmp w24, #8
    b.eq 75f
    cmp w24, #127
    b.eq 75f
    cmp w24, #32
    b.lo 73b
    cmp w24, #126
    b.hi 73b
    cmp x23, #63
    b.hs 73b
    strb w24, [x20, x23]
    add x23, x23, #1
    mov x0, #1
    add x1, sp, #752
    mov x2, #1
    mov x8, #43
    svc #0
    b 73b

75:
    cbz x23, 73b
    sub x23, x23, #1
    mov x0, #1
    adr x1, user_shell_backspace
    mov x2, #3
    mov x8, #43
    svc #0
    b 73b

76:
    mov w24, #0
    strb w24, [x20, x23]
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0

    mov x0, x20
    adr x1, user_editor_quit_cmd
    bl user_streq
    cbnz x0, 80f
    mov x0, x20
    adr x1, user_editor_print_cmd
    bl user_streq
    cbnz x0, 77f
    mov x0, x20
    adr x1, user_editor_write_cmd
    bl user_streq
    cbnz x0, 78f

    add x25, x21, x23
    add x25, x25, #1
    cmp x25, #512
    b.hi 79f
    mov x26, #0
81:
    cmp x26, x23
    b.hs 82f
    ldrb w24, [x20, x26]
    strb w24, [x19, x21]
    add x21, x21, #1
    add x26, x26, #1
    b 81b
82:
    mov w24, #10
    strb w24, [x19, x21]
    add x21, x21, #1
    b 72b

77:
    mov x0, #1
    mov x1, x19
    mov x2, x21
    mov x8, #43
    svc #0
    b 72b

78:
    adr x0, user_shell_edit_path
    mov x1, #2
    mov x8, #40
    svc #0
    tbnz x0, #63, 79f
    mov x22, x0
    mov x0, x22
    mov x1, #0
    mov x2, #0
    mov x8, #44
    svc #0
    tbnz x0, #63, 79f
    mov x0, x22
    mov x1, x19
    mov x2, x21
    mov x8, #43
    svc #0
    tbnz x0, #63, 79f
    mov x0, x22
    mov x8, #41
    svc #0
    mov x0, #1
    adr x1, user_editor_saved
    mov x2, #6
    mov x8, #43
    svc #0
    b 72b

79:
    mov x0, #1
    adr x1, user_editor_error
    mov x2, #13
    mov x8, #43
    svc #0
    b 72b

80:
    add sp, sp, #768
    mov x0, #0
    mov x8, #1
    svc #0

user_monitor_start:
    sub sp, sp, #160
    mov x19, sp
    mov x26, #5
90:
    mov x0, #1
    adr x1, user_monitor_banner
    mov x2, #17
    mov x8, #43
    svc #0
    mov x0, x19
    mov x8, #101
    svc #0
    tbnz x0, #63, 95f
    mov x0, #1
    adr x1, user_shell_mem_free
    mov x2, #6
    mov x8, #43
    svc #0
    ldr x0, [x19, #8]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x0, x19
    mov x8, #100
    svc #0
    tbnz x0, #63, 95f
    mov x0, #1
    adr x1, user_shell_ticks_timer
    mov x2, #6
    mov x8, #43
    svc #0
    ldr x0, [x19]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    mov x0, x19
    mov x1, #4
    mov x8, #102
    svc #0
    tbnz x0, #63, 95f
    mov x23, x0
    mov x24, x19
    mov x25, #0
    mov x0, #1
    adr x1, user_shell_ps_header
    mov x2, #15
    mov x8, #43
    svc #0
91:
    cmp x25, x23
    b.hs 92f
    ldr w0, [x24]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_space
    mov x2, #1
    mov x8, #43
    svc #0
    ldr w0, [x24, #4]
    bl user_print_dec
    mov x0, #1
    adr x1, user_shell_space
    mov x2, #1
    mov x8, #43
    svc #0
    add x0, x24, #8
    bl user_print_cstr
    mov x0, #1
    adr x1, user_shell_newline
    mov x2, #1
    mov x8, #43
    svc #0
    add x24, x24, #24
    add x25, x25, #1
    b 91b
92:
    mov x0, #0
    add x1, sp, #144
    mov x2, #1
    mov x8, #42
    svc #0
    cmp x0, #1
    b.ne 93f
    ldrb w0, [sp, #144]
    cmp w0, #'q'
    b.eq 96f
93:
    mov x8, #2
    svc #0
    subs x26, x26, #1
    b.ne 90b
    b 96f
95:
    mov x0, #1
    adr x1, user_shell_ps_error
    mov x2, #10
    mov x8, #43
    svc #0
96:
    add sp, sp, #160
    mov x0, #0
    mov x8, #1
    svc #0

.section .user.image.rodata, "a"
user_hello_msg:
    .ascii "Hello from EL0\n"
user_hello_second_msg:
    .ascii "Hello again EL0\n"
user_ipc_msg:
    .ascii "IPC from EL0\n"
user_vfs_msg:
    .ascii "VFS read EL0\n"
user_bootfs_path:
    .asciz "/boot/user_demo"
user_shell_banner:
    .ascii "User shell EL0\n"
user_shell_prompt:
    .ascii "u> "
user_shell_backspace:
    .ascii "\b \b"
user_shell_newline:
    .ascii "\n"
user_shell_help_cmd:
    .asciz "help"
user_shell_cat_cmd:
    .asciz "cat /tmp/note"
user_shell_cat_edit_cmd:
    .asciz "cat /fat/edit.txt"
user_shell_ls_cmd:
    .asciz "ls"
user_shell_ls_root_cmd:
    .asciz "ls /"
user_shell_ls_fat_cmd:
    .asciz "ls /fat"
user_shell_mem_cmd:
    .asciz "mem"
user_shell_ps_cmd:
    .asciz "ps"
user_shell_ticks_cmd:
    .asciz "ticks"
user_shell_run_hello_cmd:
    .asciz "run hello"
user_shell_run_loop_cmd:
    .asciz "run loop"
user_shell_run_editor_cmd:
    .asciz "run editor"
user_shell_edit_cmd:
    .asciz "edit /fat/edit.txt"
user_shell_run_monitor_cmd:
    .asciz "run monitor"
user_shell_monitor_cmd:
    .asciz "monitor"
user_shell_kill_last_cmd:
    .asciz "kill last"
user_shell_pwd_cmd:
    .asciz "pwd"
user_shell_cd_root_cmd:
    .asciz "cd /"
user_shell_exit_cmd:
    .asciz "exit"
user_shell_note_path:
    .asciz "/tmp/note"
user_shell_root_path:
    .asciz "/"
user_shell_fat_path:
    .asciz "/fat"
user_shell_run_path:
    .asciz "/boot/user_demo"
user_shell_edit_path:
    .asciz "/fat/edit.txt"
user_shell_help_msg:
    .ascii "help, ls, ls /fat, ps, ticks, pwd, cd /, mem, cat /tmp/note, cat /fat/edit.txt, run hello, run loop, run editor, monitor, kill last, exit\n"
user_shell_space:
    .ascii " "
user_shell_unknown:
    .ascii "unknown command\n"
user_shell_cat_error:
    .ascii "cat failed\n"
user_shell_ls_error:
    .ascii "ls failed\n"
user_shell_mem_total:
    .ascii "pages total="
user_shell_mem_free:
    .ascii " free="
user_shell_mem_error:
    .ascii "mem failed\n"
user_shell_ps_header:
    .ascii "pid state name\n"
user_shell_ps_error:
    .ascii "ps failed\n"
user_shell_ticks_timer:
    .ascii "timer="
user_shell_ticks_sched:
    .ascii " sched="
user_shell_ticks_quantums:
    .ascii " quantums="
user_shell_ticks_error:
    .ascii "ticks failed\n"
user_shell_spawned:
    .ascii "spawned pid="
user_shell_wait_exit:
    .ascii "exit="
user_shell_kill_exit:
    .ascii " exit="
user_shell_killed:
    .ascii "killed pid="
user_shell_kill_error:
    .ascii "kill failed\n"
user_shell_run_error:
    .ascii "run failed\n"
user_shell_pwd_msg:
    .ascii "/\n"
user_shell_cd_ok:
    .ascii "\n"
user_run_hello_msg:
    .ascii "Hello from run\n"
user_editor_banner:
    .ascii "EL0 editor: .p print, .w write, .q quit\n"
user_editor_open_error:
    .ascii "editor open failed\n"
user_editor_loaded:
    .ascii "bytes="
user_editor_prompt:
    .ascii "edit> "
user_editor_quit_cmd:
    .asciz ".q"
user_editor_print_cmd:
    .asciz ".p"
user_editor_write_cmd:
    .asciz ".w"
user_editor_saved:
    .ascii "saved\n"
user_editor_error:
    .ascii "editor error\n"
user_monitor_banner:
    .ascii "\nprocess monitor\n"
user_demo_image_end:
