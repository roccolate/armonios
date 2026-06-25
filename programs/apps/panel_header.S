// programs/apps/panel_header.S
//
// Image header for the panel app. The panel logic now lives in
// programs/apps/panel.c and enters through libkarm's _start, so the
// only thing the header needs is the magic, the size, and the
// entry_offsets[0] pointer at libkarm's _start.
//
// panel_image_end lives in panel_end.S so it sits at the very tail
// of the flat image and panel_image_header's image_size field covers
// every byte of code the loader has to copy.

.section .user.image.header, "a"
.global panel_image_header
.type panel_image_header, %function
panel_image_header:
    .word 0x31494c4b
    .hword 80
    .hword 1
    .quad panel_image_end - panel_image_header
    .quad _start - panel_image_header
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
    .quad 0
