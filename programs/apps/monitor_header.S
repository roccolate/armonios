// programs/apps/monitor_header.S
//
// Image header for the monitor app. The monitor logic now lives in
// programs/apps/monitor.c and enters through libkarm's _start, so
// the only thing the header needs is the magic, the size, and the
// entry_offsets[0] pointer at libkarm's _start.
//
// monitor_image_end lives in monitor_end.S so it sits at the very
// tail of the flat image and monitor_image_header's image_size field
// covers every byte of code the loader has to copy.

.section .user.image.header, "a"
.global monitor_image_header
.type monitor_image_header, %function
monitor_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 1
    .quad monitor_image_end - monitor_image_header
    .quad _start - monitor_image_header
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
