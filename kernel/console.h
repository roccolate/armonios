#ifndef KOLIBRIARM_KERNEL_CONSOLE_H
#define KOLIBRIARM_KERNEL_CONSOLE_H

#include <stdint.h>

#include "kernel/dtb.h"

void console_init(const dtb_memory_t *memory);
void console_start_interactive(void);
void console_set_storage_ready(int ready);
void console_set_framebuffer_ready(int ready);
void console_poll_char(char ch);

#endif
