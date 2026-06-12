#ifndef KOLIBRIARM_KERNEL_SYSCALL_H
#define KOLIBRIARM_KERNEL_SYSCALL_H

#include "kernel/exceptions.h"

void syscall_dispatch(exception_frame_t *frame);

#endif
