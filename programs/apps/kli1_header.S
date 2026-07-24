// Shared KLI1 image header for every user application.
//
// The public wire format is defined by include/armonios/abi/kli.h. This
// assembly object deliberately uses only fixed KLI1 constants and generic
// linker symbols so the same object can package built-in and external apps.
//
// __arm_kli1_image_end is provided by kli1_end.S and must remain at the tail
// of .user_image. _start is provided by libkarm/crt0.o.

.section .user.image.header, "a"
.global __arm_kli1_image_header
.type __arm_kli1_image_header, %object
__arm_kli1_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 1
    .quad __arm_kli1_image_end - __arm_kli1_image_header
    .quad _start - __arm_kli1_image_header
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
