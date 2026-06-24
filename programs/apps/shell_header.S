// programs/apps/shell_header.S
//
// Image header for the shell app. The shell logic now lives in
// programs/apps/shell.c and enters through libkarm's _start, so the
// only thing the header needs is the magic, the size, and the
// entry_offsets[0] pointer at libkarm's _start.
//
// shell_image_end lives in shell_end.S so it sits at the very tail
// of the flat image and shell_image_header's image_size field covers
// every byte of code the loader has to copy.

.section .user.image.header, "a"
.global shell_image_header
.type shell_image_header, %function
shell_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 1
    .quad shell_image_end - shell_image_header
    .quad _start - shell_image_header
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
