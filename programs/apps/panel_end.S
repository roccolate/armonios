// programs/apps/panel_end.S
//
// End-of-image marker for the panel app. panel_image_end has to
// live at the very tail of the flat image so that
// panel_image_header's image_size field correctly reports how many
// bytes the loader should copy into per-process memory. The KLI1
// layout is defined in kernel/user_image_format.h.
//
// The marker is placed in its own .user.image.end section so the
// linker script can position it after every other .user_image
// rodata/text fragment — otherwise the strings that follow the
// code in panel.c's .rodata would land past panel_image_end and
// never be copied into per-process memory.

.section .user.image.end, "ax"

.global panel_image_end
panel_image_end:
