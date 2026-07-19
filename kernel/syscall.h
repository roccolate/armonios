#ifndef ARMONIOS_KERNEL_SYSCALL_H
#define ARMONIOS_KERNEL_SYSCALL_H

#include "kernel/exceptions.h"

void syscall_dispatch(exception_frame_t *frame);

#endif
