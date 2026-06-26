/* KLI1 flat-image layout; see kernel/user_image_format.h. */
SECTIONS {
    . = 0;

    .user_image : ALIGN(16) {
        KEEP(*(.user.image.header*))
        *(.user.image.text* .text .text.*)
        *(.user.image.rodata* .rodata .rodata.*)
        KEEP(*(.user.image.end*))
    }
}
