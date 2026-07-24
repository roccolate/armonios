/* KLI1 flat-image layout; see include/armonios/abi/kli.h.
 * Only the KLI1 header bytes, text, rodata, and the end marker are emitted into
 * the image. Mutable static state must be obtained through SYS_MMAP at runtime,
 * not embedded in the image. KLI1 contains no relocation table, so SDK
 * validation must also reject load-time relocations or absolute pointers that
 * need fixups. The empty .data / .bss capture sections below let us ASSERT that
 * no application accidentally emits either mutable section. */
SECTIONS {
    . = 0;

    .user_image : ALIGN(16) {
        KEEP(*(.user.image.header*))
        *(.user.image.text* .text .text.*)
        *(.user.image.rodata* .rodata .rodata.*)
        KEEP(*(.user.image.end*))
    }

    /* KLI1 mutable-storage contract. Any input that would otherwise land in
     * .data or .bss is captured into a private section so we can assert it
     * is empty; nothing is placed into the flat image. */
    .kli1.data_dummy (NOLOAD) : { *(.data .data.*) }
    ASSERT(SIZEOF(.kli1.data_dummy) == 0,
           "KLI1 forbids .data: use SYS_MMAP for initialised mutable state")

    .kli1.bss_dummy (NOLOAD) : { *(.bss .bss.* .bss.*.* COMMON) }
    ASSERT(SIZEOF(.kli1.bss_dummy) == 0,
           "KLI1 forbids .bss: use SYS_MMAP for zero-initialised mutable state")
}
