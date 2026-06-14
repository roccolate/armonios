#ifndef KOLIBRIARM_KERNEL_IRQ_H
#define KOLIBRIARM_KERNEL_IRQ_H

#include <stdint.h>

#include "kernel/exceptions.h"

typedef void (*irq_handler_fn_t)(void *context);

int irq_register_handler(uint32_t irq, irq_handler_fn_t handler, void *context);
void irq_unregister_handler(uint32_t irq);
void irq_handler(void);
void irq_handler_frame(exception_frame_t *frame);
void irq_enable(void);
void irq_disable(void);

#endif
