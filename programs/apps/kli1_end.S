// Shared end marker for flat KLI1 images.
//
// Keep this object last in the application link line. The shared KLI1 header
// computes image_size from this symbol, so it must remain at the tail of the
// .user_image output section.

.section .user.image.end, "ax"
.global __arm_kli1_image_end
.type __arm_kli1_image_end, %function
__arm_kli1_image_end:
