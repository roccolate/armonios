#ifndef ARMONIOS_KERNEL_RUNTIME_SERVICE_H
#define ARMONIOS_KERNEL_RUNTIME_SERVICE_H

#include <stdint.h>

/*
 * Deferred runtime work published by hard-IRQ handlers and consumed once the
 * interrupt controller has received EOI. Repeated requests coalesce in the
 * pending bitmask; the single consumer performs one bounded pass before EL0
 * dispatch resumes.
 */
enum {
    RUNTIME_WORK_PERIODIC = 1U << 0,
    RUNTIME_WORK_ALL = RUNTIME_WORK_PERIODIC,
};

void runtime_service_request(uint32_t work);
uint32_t runtime_service_run_pending(void);
void runtime_service_reset(void);

#endif
