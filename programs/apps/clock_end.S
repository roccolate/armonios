// programs/apps/clock_end.S
//
// End-of-image marker for the clock app. clock_image_end has to live
// at the very tail of the flat image so that clock_image_header's
// image_size field correctly reports how many bytes the loader should
// copy into per-process memory. The KLI1 layout is defined in
// kernel/user_image_format.h. Defining it here (and placing this
// file last in the link line) keeps the marker out of clock.c's
// inline-asm hacks.

.section .user.image.end, "ax"

.global clock_image_end
clock_image_end:
