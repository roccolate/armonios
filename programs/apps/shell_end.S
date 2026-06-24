// programs/apps/shell_end.S
//
// End-of-image marker for the shell app. shell_image_end has to
// live at the very tail of the flat image so that
// shell_image_header's image_size field correctly reports how many
// bytes the loader should copy into per-process memory.

.section .user.image.text, "ax"

.global shell_image_end
shell_image_end:
