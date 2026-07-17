// programs/apps/control_header.S

.section .user.image.header, "a"
.global control_image_header
.type control_image_header, %function
control_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 1
    .quad control_image_end - control_image_header
    .quad _start - control_image_header
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
